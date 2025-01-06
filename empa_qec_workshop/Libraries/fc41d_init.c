#include "fc41d_init.h"
#include "kernel_task.h"
static char ble_name[32] = {0};
static void ql_ble_demo_get_mac_address(char *ble_mac)
{
    ql_ble_mac_address_task_create();
    ql_uart_log("KERNEL STATE SONU\r\n");

    char tmp_mac[6] = {0};
    if (ble_mac == NULL)
    {
        return;
    }
    ql_ble_address_get((char *)&tmp_mac[0]);
    snprintf(ble_name, sizeof(ble_name)-1, // 17 karakter + 1 null terminator
             "MAC:%02x:%02x:%02x:%02x:%02x:%02x",
             tmp_mac[0], tmp_mac[1], tmp_mac[2],
             tmp_mac[3], tmp_mac[4], tmp_mac[5]);
    
    // memcpy(ble_mac, tmp_mac, 6);
    ql_uart_log("%s\r\n", ble_mac);
}

void fc41d_init(void)
{
    ql_uart_log("\r\nFC41D_INIT\r\n");
    fc41d_uart_init();
    fc41d_uart_write("FC41D Initialized\r\n");
    // char ble_mac_address[32] = {0};
    // ql_ble_demo_get_mac_address(ble_mac_address);
    // ql_uart_log("%s\r\n", ble_mac_address);
    // fc41d_uart_write(ble_mac_address);
    // fc41d_uart_write("selam\r\n");

    // ql_uart_log("%s",ble_mac_address);
}

static void ql_ble_mac_address_entry(void *arg)
{
    unsigned char ble_mac[6] = {0};
    int ret = 0;
    while (1)
    {
        while (kernel_state_get(5) != 2)
        {
            ql_rtos_task_sleep_ms(200);
        }
        ql_ble_demo_get_mac_address((char *)ble_mac);
        memset(ble_name, 0, sizeof(ble_name));
        ret = snprintf(ble_name, sizeof(ble_name) - 1,
                       "FC41D_%02x:%02x:%02x:%02x:%02x:%02x",
                       ble_mac[0], ble_mac[1], ble_mac[2], ble_mac[3], ble_mac[4], ble_mac[5]);
        ql_uart_log("%s\r\n", ble_name);
        ql_rtos_task_sleep_ms(20);
        goto end;
    }
end:
    ql_rtos_task_delete(NULL);
}

static ql_task_t ble_config_network_hdl = NULL;

void ql_ble_mac_address_task_create(void)
{
    OSStatus err = kNoErr;
    err = ql_rtos_task_create(&ble_config_network_hdl,
                              8 * 1024,
                              5,
                              "ql_ble_mac_address_entry",
                              ql_ble_mac_address_entry,
                              0);

    if (err != kNoErr)
    {
        return;
    }

    return;
}
