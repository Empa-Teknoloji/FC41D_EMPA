#include "ql_include.h"

typedef enum
{
    QL_BLE_DEMO_START_SCAN,
    QL_BLE_DEMO_STOP_SCAN,
    QL_BLE_DEMO_CREATE_CONN,
    QL_BLE_DEMO_CONN_PEER_DEVICE,
    QL_BLE_DEMO_CENTRAL_EXCHANGE_MTU,
    QL_BLE_DEMO_CEN_GATT_NTFCFG,
    QL_BLE_DEMO_CEN_GATT_WRCMD,
    QL_BLE_DEMO_CEN_GATT_WRREQ,
} ql_ble_demo_msg_id;

typedef struct
{
    uint32_t msg_id;
} ql_ble_demo_msg;

#define QL_BLE_DEMO_QUEUE_MAX_NB 10

#define BIG_BUFFER_LEN 100
char big_buffer[BIG_BUFFER_LEN];
char write_buffer[BIG_BUFFER_LEN];
char read_buffer[BIG_BUFFER_LEN];
char recv_buf[2] = {0};
char temp_message[BIG_BUFFER_LEN];
static ql_task_t ble_periphera_hdl = NULL;
int ble_periphera_state = 0;
uint8_t adv_actv_idx = -1;
char ble_name[32] = {0};
struct adv_param g_adv_info = {0};

unsigned char ble_mac[6] = {0};
struct adv_param *adv_info = &g_adv_info;
int ret = 0;

#define QL_DEMO_ATT_DECL_PRIMARY_SERVICE_128 {0x00, 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define QL_DEMO_ATT_DECL_CHARACTERISTIC_128 {0x03, 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define QL_DEMO_ATT_DESC_CLIENT_CHAR_CFG_128 {0x02, 0x29, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define NOTIFY_CHARACTERISTIC_128 {0x15, 0xFF, 0, 0, 0x34, 0x56, 0, 0, 0, 0, 0x28, 0x37, 0, 0, 0, 0}
static const uint8_t ql_demo_svc_uuid[16] = {0x36, 0xF5, 0, 0, 0x34, 0x56, 0, 0, 0, 0, 0x28, 0x37, 0, 0, 0, 0};

enum
{
    QL_DEMO_IDX_SVC,
    QL_DEMO_IDX_FF03_VAL_CHAR,
    QL_DEMO_IDX_FF03_VAL_VALUE,
    QL_DEMO_IDX_FF03_VAL_NTF_CFG,
    QL_DEMO_IDX_NB,
};
ql_attm_desc_t ql_demo_att_db[QL_DEMO_IDX_NB] =
    {

        [QL_DEMO_IDX_SVC] = {QL_DEMO_ATT_DECL_PRIMARY_SERVICE_128, QL_PERM_SET(RD, ENABLE), 0, 0},

        [QL_DEMO_IDX_FF03_VAL_CHAR] = {QL_DEMO_ATT_DECL_CHARACTERISTIC_128, QL_PERM_SET(RD, ENABLE), 0, 0},

        [QL_DEMO_IDX_FF03_VAL_VALUE] = {NOTIFY_CHARACTERISTIC_128, QL_PERM_SET(WRITE_REQ, ENABLE) | QL_PERM_SET(RD, ENABLE), QL_PERM_SET(RI, ENABLE) | QL_PERM_SET(UUID_LEN, UUID_16), 512},

        [QL_DEMO_IDX_FF03_VAL_NTF_CFG] = {QL_DEMO_ATT_DESC_CLIENT_CHAR_CFG_128, QL_PERM_SET(RD, ENABLE) | QL_PERM_SET(WRITE_REQ, ENABLE), 0, 0},
};

static void fc41d_ble_demo_notice_cb(ble_notice_t notice, void *param)
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
        ql_uart_write(UART_TEST_PORT,"WRITE EVENt\r\n",strlen("WRITE EVENt\r\n"));
        snprintf(write_buffer, sizeof(write_buffer), "Written data: %.*s\r\n", w_req->len, (char *)w_req->value);
        ql_uart_write(UART_TEST_PORT,write_buffer,strlen(write_buffer));
        break;
    }

    case BLE_5_READ_EVENT:
    {
        read_req_t *r_req = (read_req_t *)param;
        os_printf("read_cb:conn_idx:%d, prf_id:%d, add_id:%d\r\n",
                  r_req->conn_idx, r_req->prf_id, r_req->att_idx);
        int message_length = strlen(temp_message);
        memcpy(r_req->value, temp_message, message_length);
        r_req->length = message_length;
        ql_uart_write(UART_TEST_PORT,"Write a new value or read a new value\r\n",strlen("Write a new value or read a new value\r\n"));
        break;
    }

    case BLE_5_REPORT_ADV:
    {
        recv_adv_t *r_ind = (recv_adv_t *)param;
        int8_t rssi = r_ind->rssi;
        os_printf("r_ind:actv_idx:%d, adv_addr:%02x:%02x:%02x:%02x:%02x:%02x,rssi:%d\r\n",
                  r_ind->actv_idx, r_ind->adv_addr[0], r_ind->adv_addr[1], r_ind->adv_addr[2],
                  r_ind->adv_addr[3], r_ind->adv_addr[4], r_ind->adv_addr[5], rssi);
        break;
    }

    case BLE_5_MTU_CHANGE:
    {
        mtu_change_t *m_ind = (mtu_change_t *)param;
        os_printf("m_ind:conn_idx:%d, mtu_size:%d\r\n", m_ind->conn_idx, m_ind->mtu_size);
        ql_uart_write(UART_TEST_PORT,"BLE Connected\r\n",strlen("BLE Connected\r\n"));
        break;
    }

    case BLE_5_CONNECT_EVENT:
    {
        conn_ind_t *c_ind = (conn_ind_t *)param;
        os_printf("BLE_5_CONNECT_EVENT c_ind:conn_idx:%d, addr_type:%d, peer_addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                  c_ind->conn_idx, c_ind->peer_addr_type, c_ind->peer_addr[0], c_ind->peer_addr[1],
                  c_ind->peer_addr[2], c_ind->peer_addr[3], c_ind->peer_addr[4], c_ind->peer_addr[5]);

        break;
    }

    case BLE_5_DISCONNECT_EVENT:
    {
        discon_ind_t *d_ind = (discon_ind_t *)param;
        os_printf("BLE_5_DISCONNECT_EVENT, conn_idx:%d, reason:%d \r\n", d_ind->conn_idx, d_ind->reason);

        break;
    }

    case BLE_5_ATT_INFO_REQ:
    {
        att_info_req_t *a_ind = (att_info_req_t *)param;
        os_printf("a_ind:conn_idx:%d\r\n", a_ind->conn_idx);
        a_ind->length = 128;
        a_ind->status = ERR_SUCCESS;
        break;
    }

    case BLE_5_CREATE_DB:
    {
        create_db_t *cd_ind = (create_db_t *)param;
        os_printf("cd_ind:prf_id:%d, status:%d\r\n", cd_ind->prf_id, cd_ind->status);
        break;
    }

    case BLE_5_INIT_CONNECT_EVENT:
    {
        conn_ind_t *c_ind = (conn_ind_t *)param;
        os_printf("BLE_5_INIT_CONNECT_EVENT:conn_idx:%d, addr_type:%d, peer_addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                  c_ind->conn_idx, c_ind->peer_addr_type, c_ind->peer_addr[0], c_ind->peer_addr[1],
                  c_ind->peer_addr[2], c_ind->peer_addr[3], c_ind->peer_addr[4], c_ind->peer_addr[5]);
        break;
    }

    case BLE_5_INIT_DISCONNECT_EVENT:
    {
        discon_ind_t *d_ind = (discon_ind_t *)param;
        os_printf("BLE_5_INIT_DISCONNECT_EVENT, conn_idx:%d, reason:%d\r\n", d_ind->conn_idx, d_ind->reason);
        break;
    }

    case BLE_5_SDP_REGISTER_FAILED:
        os_printf("BLE_5_SDP_REGISTER_FAILED\r\n");
        break;

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

void fc41d_ble_config()
{
    while (kernel_state_get(TASK_BLE_APP) != APPM_READY)
    {
        ql_rtos_task_sleep_ms(200);
    }
    
    ql_ble_set_notice_cb(fc41d_ble_demo_notice_cb);
    struct ql_ble_db_cfg ble_db_cfg;
    ble_db_cfg.att_db = ql_demo_att_db;
    ble_db_cfg.att_db_nb = QL_DEMO_IDX_NB;
    ble_db_cfg.prf_task_id = 0;
    ble_db_cfg.start_hdl = 0;
    ble_db_cfg.svc_perm = QL_PERM_SET(SVC_UUID_LEN, UUID_16);
    memcpy(&(ble_db_cfg.uuid[0]), &ql_demo_svc_uuid[0], 16);
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
    adv_info->advData[0] = 0x02;
    adv_info->advData[1] = 0x01;
    adv_info->advData[2] = 0x06;
    adv_info->advData[3] = ret + 1;
    adv_info->advData[4] = 0x09;
    memcpy(&adv_info->advData[5], ble_name, ret);
    adv_info->advDataLen = 5 + ret;
    char buf_log[128] = {0};
    snprintf(buf_log,sizeof(buf_log),"BLE Config finished \r\nBLE Name: %s\r\n",ble_name);
    ql_uart_write(UART_TEST_PORT,buf_log,strlen(buf_log));
}

static void fc41d_ble_demo_thread(void *arg)
{
    while (1)
    {

        if (ble_periphera_state == 0)
        {
            adv_actv_idx = ql_ble_get_idle_actv_idx_handle();
            if (adv_actv_idx >= BLE_ACTIVITY_MAX)
            {
                return;
            }
            if (app_ble_actv_state_get(adv_actv_idx) == ACTV_IDLE)
            {
                ql_ble_adv_start(adv_actv_idx, adv_info, ble_periphera_cmd_cb);
                ql_uart_write(UART_TEST_PORT,"BLE Advertisement started\r\n",strlen("BLE Advertisement started\r\n"));
            }
            else
            {
                ql_ble_start_advertising(adv_actv_idx, 0, ble_periphera_cmd_cb);
            }
            ble_periphera_state = 1;
        }
        if (ble_periphera_state)
        {
            if (ql_uart_read(UART_TEST_PORT, recv_buf, 1) == 0)
            {
                strncat(big_buffer, recv_buf, 1);
            }

            if (strstr(big_buffer, "\r\n") != NULL)
            {

                strncpy(temp_message, big_buffer, strstr(big_buffer, "\r\n") - big_buffer);
                temp_message[strstr(big_buffer, "\r\n") - big_buffer] = '\0'; 

                snprintf(big_buffer, sizeof(big_buffer), "The value to be read: %s\r\n", temp_message);
                ql_uart_write(UART_TEST_PORT,big_buffer,strlen(big_buffer));
                memset(big_buffer, 0, sizeof(big_buffer));
            }

            ql_rtos_task_sleep_ms(10);
        }
        ql_rtos_task_sleep_ms(20);
    }
    ql_rtos_task_delete(NULL);
}

void fc41d_ble_demo(void)
{
    QlOSStatus err = 0;
    err = ql_rtos_task_create(&ble_periphera_hdl,
                              8 * 1024,
                              THD_EXTENDED_APP_PRIORITY,
                              "fc41d_ble_demo",
                              fc41d_ble_demo_thread,
                              0);

    if (err != 0)
    {
        return;
    }

    return;
}