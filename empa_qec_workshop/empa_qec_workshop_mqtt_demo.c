#include "string.h"
#include "ble_ui.h"
#include "app_ble.h"
#include "app_sdp.h"

#if (BLE_APP_PRESENT && (BLE_CENTRAL))
#include "app_ble_init.h"
#if BLE_SDP_CLIENT
#include "app_sdp.h"
#endif
#endif

#include "gattc.h"
#include "ql_ble.h"
#include "app_comm.h"
#include "ble_api_5_x.h"
#include "param_config.h"
#include "app_task.h"
#include "quectel_demo_config.h"
#include "ql_wlan.h"
#include "ql_api_osi.h"
#include "wlan_ui_pub.h"
#include "ql_include.h"
#include "ql_app.h"

static ql_task_t ble_config_network_hdl = NULL;
static int ble_periphera_state = 0;
static uint8_t adv_actv_idx = -1;
static char ble_name[32] = {0};
struct adv_param g_adv_info = {0};
static uint16_t g_ucBleMtu = 23;
static char g_cApSsid[32] = {0};
static char g_cApPwd[64] = {0};
static mqtt_client_session _mqttc_ses = {0};
struct sub_msg_handlers sub_topic[2] = {0};
char mqtt_uart_buffer[128] = {0};
char receiver_buffer[2] = {0};
char uart_buffer_log[128] = {0};
extern MQTTConfig config;

#define QL_RSSI_AP_POWER(rssi) (((rssi + 100) > 100) ? 100 : (rssi + 100))

#define QUEC_ATT_DECL_PRIMARY_SERVICE_128 {0x00, 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define QUEC_ATT_DECL_CHARACTERISTIC_128 {0x03, 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define QUEC_ATT_DESC_CLIENT_CHAR_CFG_128 {0x02, 0x29, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

#define QUEC_WRITE_REQ_CHARACTERISTIC_128 {0x01, 0xFF, 0, 0, 0x34, 0x56, 0, 0, 0, 0, 0x28, 0x37, 0, 0, 0, 0}
#define QUEC_INDICATE_CHARACTERISTIC_128 {0x02, 0xFF, 0, 0, 0x34, 0x56, 0, 0, 0, 0, 0x28, 0x37, 0, 0, 0, 0}
#define QUEC_NOTIFY_CHARACTERISTIC_128 {0x03, 0xFF, 0, 0, 0x34, 0x56, 0, 0, 0, 0, 0x28, 0x37, 0, 0, 0, 0}

enum
{
    TEST_IDX_SVC,
    TEST_IDX_FF01_VAL_CHAR,
    TEST_IDX_FF01_VAL_VALUE,
    TEST_IDX_FF01_VAL_NTF_CFG,
    TEST_IDX_NB,
};
static ql_attm_desc_t quec_att_db_default[TEST_IDX_NB] =
    {
        //  Service Declaration
        [TEST_IDX_SVC] = {QUEC_ATT_DECL_PRIMARY_SERVICE_128, QL_PERM_SET(RD, ENABLE), 0, 0},

        //  Level Characteristic Declaration
        [TEST_IDX_FF01_VAL_CHAR] = {QUEC_ATT_DECL_CHARACTERISTIC_128, QL_PERM_SET(RD, ENABLE), 0, 0},

        //  Level Characteristic Value
        [TEST_IDX_FF01_VAL_VALUE] = {QUEC_WRITE_REQ_CHARACTERISTIC_128, QL_PERM_SET(WRITE_REQ, ENABLE) | QL_PERM_SET(NTF, ENABLE), QL_PERM_SET(RI, ENABLE) | QL_PERM_SET(UUID_LEN, UUID_16), 512},

        //  Level Characteristic - Client Characteristic Configuration Descriptor
        [TEST_IDX_FF01_VAL_NTF_CFG] = {QUEC_ATT_DESC_CLIENT_CHAR_CFG_128, QL_PERM_SET(RD, ENABLE) | QL_PERM_SET(WRITE_REQ, ENABLE), 0, 0},
};

static const uint8_t quec_svc_uuid_default[16] = {0xFF, 0xFF, 0, 0, 0x34, 0x56, 0, 0, 0, 0, 0x28, 0x37, 0, 0, 0, 0};
static void *demo_scan_sema = NULL;
void ql_ble_send_net_info(char *msg, unsigned int msg_len);

void ql_demo_sta_connect_ap(char *oob_ssid, char *connect_key)
{
    network_InitTypeDef_st wNetConfig;
    int len;
    os_memset(&wNetConfig, 0x0, sizeof(network_InitTypeDef_st));

    len = os_strlen(oob_ssid);
    if (32 < len)
    {
        os_printf("ssid name more than 32 Bytes\r\n");
        return;
    }

    os_strcpy((char *)wNetConfig.wifi_ssid, oob_ssid);
    os_strcpy((char *)wNetConfig.wifi_key, connect_key);

    wNetConfig.wifi_mode = QL_STATION;
    wNetConfig.dhcp_mode = DHCP_CLIENT;
    wNetConfig.wifi_retry_interval = 100;

    os_printf("ssid:%s key:%s\r\n", wNetConfig.wifi_ssid, wNetConfig.wifi_key);
    ql_wlan_start(&wNetConfig);
}

static void ql_demo_status_callback(ql_wlan_evt_type *ctxt)
{
    os_printf("[WLAN]%s:%d,event(%x)\r\n", __FUNCTION__, __LINE__, *ctxt);
    switch (*ctxt)
    {
    case QL_WLAN_EVT_STA_GOT_IP:
        ql_ble_send_net_info("+QSTASTAT:GOT_IP\r\n", strlen("+QSTASTAT:GOT_IP\r\n"));
        os_printf("+QSTASTAT:GOT_IP\r\n");
        break;
    case QL_WLAN_EVT_STA_CONNECTED:
        ql_ble_send_net_info("+QSTASTAT:WLAN_CONNECTED\r\n", strlen("+QSTASTAT:WLAN_CONNECTED\r\n"));
        os_printf("+QSTASTAT:WLAN_CONNECTED\r\n");
        break;
    default:
        break;
    }
}
void ql_ble_send_net_info(char *msg, unsigned int msg_len)
{
    int send = msg_len;
    char *p = msg;
    int mtu = g_ucBleMtu - 3;
    os_printf("%s:%d,mtu=%d + 3 , msg_len = %d\r\n", __FUNCTION__, __LINE__, mtu, msg_len);

    while (send > mtu)
    {
        if (0 != ql_ble_gatts_send_ntf_value(mtu, p, 0, TEST_IDX_FF01_VAL_VALUE))
        {
            os_printf("BLE SEND ERROR\r\n");
        }
        send = send - mtu;
        p = p + mtu;
    }

    if (0 != ql_ble_gatts_send_ntf_value(send, p, 0, TEST_IDX_FF01_VAL_VALUE))
    {
        os_printf("BLE SEND ERROR\r\n");
    }
}

static void ql_demo_ble_config_network_notice_cb(ble_notice_t notice, void *param)
{
    switch (notice)
    {
    case BLE_5_STACK_OK:
        os_printf("ble stack ok");
        break;
    case BLE_5_WRITE_EVENT:
    {
        write_req_t *w_req = (write_req_t *)param;
        os_printf("write_cb:conn_idx:%d, prf_id:%d, add_id:%d, len:%d, data[0]:%02x\r\n",
                  w_req->conn_idx, w_req->prf_id, w_req->att_idx, w_req->len, w_req->value[0]);
        os_printf("data: %.*s\r\n", w_req->len, (char *)w_req->value);
        if (NULL != strstr(w_req->value, "CONNECT"))
        {
            memset(g_cApSsid, 0, sizeof(g_cApSsid));
            memset(g_cApPwd, 0, sizeof(g_cApPwd));
            char *_ssid = strchr(w_req->value, '=');
            if (NULL != _ssid)
            {
                _ssid++;
                for (uint8_t i = 0; i < sizeof(g_cApSsid); i++)
                {
                    if (_ssid[i] != ',')
                        g_cApSsid[i] = _ssid[i];
                    else
                        break;
                }
            }
            char *pwd = strchr(_ssid, ',');
            if (NULL != pwd)
            {
                pwd++;
                for (uint8_t i = 0; i < sizeof(g_cApPwd); i++)
                {
                    if (pwd[i] != '#')
                        g_cApPwd[i] = pwd[i];
                    else
                        break;
                }
            }
            // snprintf(uart_buffer_log, sizeof(uart_buffer_log), "SSID = %s , PASSWORD = %s\r\n", g_cApSsid, g_cApPwd);
            // ql_uart_write(UART_TEST_PORT,uart_buffer_log,strlen(uart_buffer_log));
            // memset(uart_buffer_log,0,sizeof(uart_buffer_log));

            // connect ap
            ql_demo_sta_connect_ap(g_cApSsid, g_cApPwd);
            while (!ql_sta_ip_is_start())
            {
                ql_rtos_task_sleep_ms(1000);
                os_printf("Connecting\r\n");
                ql_uart_write(UART_TEST_PORT,
                              "Connecting...\r\n",
                              strlen("Connecting...\r\n"));
            }
            ql_uart_write(UART_TEST_PORT,
                          "Wi-Fi connection is established\r\n",
                          strlen("Wi-Fi connection is established\r\n"));
        }

        break;
    }
    case BLE_5_MTU_CHANGE:
    {
        mtu_change_t *m_ind = (mtu_change_t *)param;
        ql_uart_write(UART_TEST_PORT, "Successfully connected to BLE.\r\n", strlen("Successfully connected to BLE.\r\n"));
        g_ucBleMtu = m_ind->mtu_size;
        break;
    }
    case BLE_5_CONNECT_EVENT:
    {
        conn_ind_t *c_ind = (conn_ind_t *)param;
        os_printf("c_ind:conn_idx:%d, addr_type:%d, peer_addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                  c_ind->conn_idx, c_ind->peer_addr_type, c_ind->peer_addr[0], c_ind->peer_addr[1],
                  c_ind->peer_addr[2], c_ind->peer_addr[3], c_ind->peer_addr[4], c_ind->peer_addr[5]);
        break;
    }
    case BLE_5_DISCONNECT_EVENT:
    {
        discon_ind_t *d_ind = (discon_ind_t *)param;
        os_printf("d_ind:conn_idx:%d,reason:%d\r\n", d_ind->conn_idx, d_ind->reason);
        break;
    }
    default:
        break;
    }
}
static void ble_periphera_cmd_cb(ble_cmd_t cmd, ble_cmd_param_t *param)
{
    os_printf("cmd:%d idx:%d status:%d\r\n", cmd, param->cmd_idx, param->status);
}
static void ql_ble_demo_get_mac_address(char *ble_mac)
{
    char tmp_mac[6] = {0};
    if (ble_mac == NULL)
    {
        return;
    }
    ql_ble_address_get((char *)&tmp_mac[0]);
    memcpy(ble_mac, tmp_mac, 6);
}

static void ql_mqtt_demo_entry(void *arg)
{
    unsigned char ble_mac[6] = {0};
    struct adv_param *adv_info = &g_adv_info;
    int ret = 0;

    while (1)
    {
        while (kernel_state_get(TASK_BLE_APP) != APPM_READY)
        {
            ql_rtos_task_sleep_ms(200);
        }
        if (ble_periphera_state == 0)
        {
            ql_ble_set_notice_cb(ql_demo_ble_config_network_notice_cb);
            struct ql_ble_db_cfg ble_db_cfg;
            ble_db_cfg.att_db = quec_att_db_default;
            ble_db_cfg.att_db_nb = TEST_IDX_NB;
            ble_db_cfg.prf_task_id = 0;
            ble_db_cfg.start_hdl = 0;
            ble_db_cfg.svc_perm = QL_PERM_SET(SVC_UUID_LEN, UUID_16);
            memcpy(&(ble_db_cfg.uuid[0]), &quec_svc_uuid_default[0], 16);
            ql_ble_create_db(&ble_db_cfg);

            adv_info->interval_min = 160;
            adv_info->interval_max = 160;
            adv_info->channel_map = 7;
            adv_info->duration = 0;

            ql_ble_demo_get_mac_address((char *)ble_mac);
            memset(ble_name, 0, sizeof(ble_name));
            ret = snprintf(ble_name, sizeof(ble_name) - 1,
                           "FC41D_%02x:%02x:%02x:%02x:%02x:%02x",
                           ble_mac[0], ble_mac[1], ble_mac[2], ble_mac[3], ble_mac[4], ble_mac[5]);
            ql_uart_write(UART_TEST_PORT, "Your mac id: ", strlen("Your mac id: "));

            ql_uart_write(UART_TEST_PORT, ble_name, strlen(ble_name));
            ql_uart_write(UART_TEST_PORT, "\r\n", strlen("\r\n"));
            adv_info->advData[0] = 0x02;
            adv_info->advData[1] = 0x01;
            adv_info->advData[2] = 0x06;
            adv_info->advData[3] = ret + 1;
            adv_info->advData[4] = 0x09;
            memcpy(&adv_info->advData[5], ble_name, ret);
            adv_info->advDataLen = 5 + ret;
            adv_actv_idx = ql_ble_get_idle_actv_idx_handle();
            if (adv_actv_idx >= BLE_ACTIVITY_MAX)
            {
                return;
            }
            if (app_ble_actv_state_get(adv_actv_idx) == ACTV_IDLE)
            {
                ql_ble_adv_start(adv_actv_idx, adv_info, ble_periphera_cmd_cb);
                ql_uart_write(UART_TEST_PORT,
                              "BLE Adversitement started.\r\n",
                              strlen("BLE Adversitement started.\r\n"));
                ql_uart_write(UART_TEST_PORT,
                              "Please send the SSID and PASSWORD in the following format: CONNECT=SSID,PASSWORD#\r\n",
                              strlen("Please send the SSID and PASSWORD in the following format: CONNECT=SSID,PASSWORD#\r\n"));
            }
            else
            {
                ql_ble_start_advertising(adv_actv_idx, 0, ble_periphera_cmd_cb);
                ql_uart_write(UART_TEST_PORT,
                              "ql_ble_start_advertising\r\n",
                              strlen("ql_ble_start_advertising\r\n"));
            }
            ble_periphera_state = 1;
        }
        ql_rtos_task_sleep_ms(20);
        if (ql_sta_ip_is_start())
        {
            ql_rtos_task_sleep_ms(2000);
            ql_mqtt_demo_thread_creat();
            ql_rtos_task_delete(ble_config_network_hdl);
        }
    }
}

void fc41d_mqtt_demo(void)
{
    OSStatus err = kNoErr;
    err = ql_rtos_task_create(&ble_config_network_hdl,
                              8 * 1024,
                              THD_EXTENDED_APP_PRIORITY,
                              "ql_demo_mqtt_thread",
                              ql_mqtt_demo_entry,
                              0);
    if (err != kNoErr)
    {
        return;
    }
    return;
}

char *mqt_strdup(const char *s)
{
    char *res;
    size_t len;

    if (s == NULL)
        return NULL;

    len = os_strlen(s);
    res = os_malloc(len + 1);
    if (res)
        os_memcpy(res, s, len + 1);

    return res;
}
static void mqtt_sub_callback(mqtt_client_session *c, MessageData *msg_data)
{
    snprintf(uart_buffer_log, sizeof(uart_buffer_log),
             "Message arrived on topic %.*s: %.*s\r\n",
             msg_data->topicName->lenstring.len,
             msg_data->topicName->lenstring.data,
             msg_data->message->payloadlen,
             msg_data->message->payload);
    ql_uart_write(UART_TEST_PORT, uart_buffer_log, strlen(uart_buffer_log));
    return;
}

static void mqtt_default_sub_callback(mqtt_client_session *c, MessageData *msg_data)
{
    TMQTT_LOG("default Message arrived on topic %.*s: %.*s\n", msg_data->topicName->lenstring.len,
              msg_data->topicName->lenstring.data,
              msg_data->message->payloadlen, msg_data->message->payload);
    snprintf(uart_buffer_log, sizeof(uart_buffer_log),
             "Message arrived on topic %.*s: %.*s\n", msg_data->topicName->lenstring.len,
             msg_data->topicName->lenstring.data,
             msg_data->message->payloadlen, msg_data->message->payload);
    ql_uart_write(UART_TEST_PORT, uart_buffer_log, strlen(uart_buffer_log));
    return;
}

static void mqtt_connect_callback(mqtt_client_session *c)
{
    TMQTT_LOG("inter mqtt_connect_callback!\r\n");
}

static void mqtt_online_callback(mqtt_client_session *c)
{
    TMQTT_LOG("inter mqtt_online_callback!\r\n");
}

static void mqtt_offline_callback(mqtt_client_session *c, MQTT_EVNT_T ev)
{
    TMQTT_LOG("inter mqtt_offline_callback event %d\r\n", ev);
}

static void mqtt_notice_event_callback(mqtt_client_session *c, MQTT_EVNT_T ev)
{
    TMQTT_LOG("mqtt nitice event %d\r\n", ev);
}

static void mqtt_thread_pub(void *parameter)
{
    unsigned int pub_count = 0;
    char payload[60];
    char topic_mydesk[60];
    char message[60];
    char command[10] = {0};
    char uart_buffer[128] = {0};
    ql_uart_write(UART_TEST_PORT, "mqtt thread pub started.\r\n", strlen("mqtt thread pub started.\r\n"));
    snprintf(uart_buffer_log, sizeof(uart_buffer_log),
             "Please send 'pub,topic,message' to publish or 'sub,topic' to subscribe.\r\n");
    ql_uart_write(UART_TEST_PORT, uart_buffer_log, strlen(uart_buffer_log));
    while (1)
    {
        if (ql_uart_read(UART_TEST_PORT, receiver_buffer, 1) == 0)
        {
            strncat(mqtt_uart_buffer, receiver_buffer, 1);
        }
        ql_rtos_task_sleep_ms(10);
        if (strstr(mqtt_uart_buffer, "\r\n") != 0)
        {
            char temp_message[128];
            strncpy(temp_message, mqtt_uart_buffer, sizeof(temp_message) - 1);
            temp_message[sizeof(temp_message) - 1] = '\0';

            char *token = strtok(temp_message, ",");
            if (token != NULL)
            {
                strncpy(command, token, sizeof(command) - 1);
                command[sizeof(command) - 1] = '\0';

                if (strcmp(command, "pub") == 0)
                {
                    token = strtok(NULL, ",");
                    if (token != NULL)
                    {
                        strncpy(topic_mydesk, token, sizeof(topic_mydesk) - 1);
                        topic_mydesk[sizeof(topic_mydesk) - 1] = '\0';

                        token = strtok(NULL, "\r\n");
                        if (token != NULL)
                        {
                            strncpy(message, token, sizeof(message) - 1);
                            message[sizeof(message) - 1] = '\0';

                            snprintf(uart_buffer, sizeof(uart_buffer), "Publish Topic: %s\n", topic_mydesk);
                            ql_uart_write(UART_TEST_PORT, uart_buffer, strlen(uart_buffer));
                            snprintf(uart_buffer, sizeof(uart_buffer), "Publish Message: %s\n", message);
                            ql_uart_write(UART_TEST_PORT, uart_buffer, strlen(uart_buffer));
                            mqtt_client_publish(&_mqttc_ses, QOS1, topic_mydesk, message);
                        }
                        else
                        {
                            ql_uart_write(UART_TEST_PORT,
                                          "Invalid format. Missing message.\n",
                                          strlen("Invalid format. Missing message.\n"));
                        }
                    }
                    else
                    {
                        ql_uart_write(UART_TEST_PORT,
                                      "Invalid format. Missing topic.\n",
                                      strlen("Invalid format. Missing topic.\n"));
                    }
                }
                else if (strcmp(command, "sub") == 0)
                {
                    token = strtok(NULL, "\r\n");
                    if (token != NULL)
                    {
                        strncpy(topic_mydesk, token, sizeof(topic_mydesk) - 1);
                        topic_mydesk[sizeof(topic_mydesk) - 1] = '\0';

                        snprintf(uart_buffer, sizeof(uart_buffer), "Subscribe Topic: %s\n", topic_mydesk);
                        ql_uart_write(UART_TEST_PORT, uart_buffer, strlen(uart_buffer));
                        sub_topic[1].callback = mqtt_sub_callback;
                        sub_topic[1].qos = QOS1;
                        _mqttc_ses.sub.sub_topic_num = sizeof(sub_topic) / sizeof(struct sub_msg_handlers);
                        _mqttc_ses.defaultMessageHandler = mqtt_default_sub_callback;
                        mqtt_client_subscribe(&_mqttc_ses, topic_mydesk);
                    }
                    else
                    {
                        ql_uart_write(UART_TEST_PORT,
                                      "Invalid format. Missing topic for subscription.\n",
                                      strlen("Invalid format. Missing topic for subscription.\n"));
                    }
                }
                else
                {
                    ql_uart_write(UART_TEST_PORT,
                                  "Invalid command. Use 'pub' or 'sub'.\n",
                                  strlen("Invalid command. Use 'pub' or 'sub'.\n"));
                }
            }
            else
            {
                ql_uart_write(UART_TEST_PORT,
                              "Invalid format. Missing command.\n",
                              strlen("Invalid format. Missing command.\n"));
            }
            snprintf(uart_buffer, sizeof(uart_buffer),
                     "Please send 'pub,topic,message' to publish or 'sub,topic' to subscribe.\r\n");
            memset(mqtt_uart_buffer, 0, sizeof(mqtt_uart_buffer));
        }
        ql_rtos_task_sleep_ms(20);
    }

    ql_rtos_task_sleep_ms(1000);
    os_printf("IDLE\r\n");
}

static int mqtt_demo_start_connect(void)
{
    int ret;
    os_printf("MQTT DEMO START CONNECT\r\n");

    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

    if (mqtt_client_session_init(&_mqttc_ses) != MQTT_OK)
    {
        goto _exit;
    }

    ssl_mqtt_client_api_register(&_mqttc_ses.netport);

    if (mqtt_net_connect(&_mqttc_ses, config.url, config.port) != MQTT_OK)
    {
        goto _exit;
    }
    connectData.MQTTVersion = 3;
    connectData.clientID.cstring = config.clientID;
    connectData.username.cstring = config.username;
    connectData.password.cstring = config.password;
    // connectData.password.cstring = "";
    connectData.keepAliveInterval = 30000;
    connectData.cleansession = 1;

    _mqttc_ses.buf_size = MQTT_DEFUALT_BUF_SIZE;
    _mqttc_ses.buf = os_malloc(_mqttc_ses.buf_size);
    _mqttc_ses.readbuf_size = MQTT_DEFUALT_BUF_SIZE;
    _mqttc_ses.readbuf = os_malloc(_mqttc_ses.readbuf_size);

    if (!(_mqttc_ses.readbuf && _mqttc_ses.buf))
    {
        os_printf("no memory for MQTT client buffer!\n");
        goto _exit;
    }

    /* set event callback function */
    _mqttc_ses.connect_callback = mqtt_connect_callback;
    _mqttc_ses.online_callback = mqtt_online_callback;
    _mqttc_ses.offline_callback = mqtt_offline_callback;
    _mqttc_ses.mqtt_notice_cb = mqtt_notice_event_callback;

    /* set subscribe table and event callback */
    _mqttc_ses.sub.messageHandlers = sub_topic;
    if (sub_topic[0].topicFilter == NULL)
    {
        
        sub_topic[0].topicFilter = mqt_strdup(config.topicSub);
    }
    sub_topic[0].callback = mqtt_sub_callback;
    sub_topic[0].qos = QOS0;
    if (sub_topic[1].topicFilter == NULL)
    {
        sub_topic[1].topicFilter = mqt_strdup("/topic/empa/pub");
    }
    sub_topic[1].callback = mqtt_sub_callback;
    sub_topic[1].qos = QOS1;
    _mqttc_ses.sub.sub_topic_num = sizeof(sub_topic) / sizeof(struct sub_msg_handlers);
    _mqttc_ses.defaultMessageHandler = mqtt_default_sub_callback;
    ret = mqtt_client_connect(&_mqttc_ses, &connectData);
    if (MQTT_OK != ret)
    {
        os_printf("[MQTT]connect failed\r\n");
        ql_uart_write(UART_TEST_PORT, "MQTT Connect failed.\r\n", strlen("MQTT Connect failed.\r\n"));
        goto _exit;
    }
    os_printf("[MQTT]connect succeed\r\n");
    ql_uart_write(UART_TEST_PORT, "MQTT Connect succeed\r\n", strlen("MQTT Connect succeed\r\n"));

    return 0;
_exit:
    if (_mqttc_ses.readbuf)
    {
        os_free(_mqttc_ses.readbuf);
    }
    if (_mqttc_ses.buf)
    {
        os_free(_mqttc_ses.buf);
    }
    mqtt_client_session_deinit(&_mqttc_ses);
    return -1;
}

static void mqtt_test_start(void)
{
    if (mqtt_demo_start_connect() != 0)
    {
        return;
    }
    if (!_mqttc_ses.is_connected)
    {
        os_printf("Waiting for mqtt connection...\n");
        return;
    }

    ql_rtos_task_create(NULL,
                        3 * 1024,
                        THD_EXTENDED_APP_PRIORITY,
                        "MQTT_pub",
                        (beken_thread_function_t)mqtt_thread_pub,
                        NULL);
    return;
}

void mqtt_app_demo_init(void *arg)
{
    os_printf("MQTT APP DEMO INIT BASI\r\n");
    ql_rtos_task_sleep_ms(3000);
    // cli_register_commands(mqtt_cli_cmd, sizeof(mqtt_cli_cmd) / sizeof(struct cli_command));
    mqtt_core_handler_thread_init();
    tcp_mqtt_client_init();
    os_printf("[MQTT]%s:%d\r\n", __FUNCTION__, __LINE__);
    os_printf("!!!!!!!!!!!wait wifi connect \r\n");
    while (!ql_sta_ip_is_start())
    {
        ql_rtos_task_sleep_ms(1000);
        os_printf("connecting \r\n");
    }
    os_printf("MQTT TEST START ONCESI\r\n");
    mqtt_test_start();

    while (1)
    {
        ql_rtos_task_sleep_ms(1000);
    }
    // if( mqtt_thread_handle != NULL ) {
    // 	os_printf("TASK DELETING\r\n");
    // 	ql_rtos_task_delete(mqtt_thread_handle);
    // }
}

ql_task_t mqtt_thread_handle = NULL;

void ql_mqtt_demo_thread_creat(void)
{
    os_printf("mqtt demo thread creat GIRISI\r\n");
    int ret;
    ret = ql_rtos_task_create(&mqtt_thread_handle,
                              (unsigned short)1024 * 4,
                              THD_EXTENDED_APP_PRIORITY,
                              "mqtt_test",
                              mqtt_app_demo_init,
                              0);

    if (ret != kNoErr)
    {
        os_printf("Error: Failed to create mqtt test thread: %d\r\n", ret);
        goto init_err;
    }

    return;

init_err:
    if (mqtt_thread_handle != NULL)
    {
        ql_rtos_task_delete(mqtt_thread_handle);
    }
}

// #endif
