#include "fc41d_uart.h"

ql_sem_t uart_rx_semaphore = NULL;
ql_sem_t uart_tx_semaphore = NULL;

static void uart_rx_callback(int uport, void *param)
{
    if (uart_rx_semaphore)
    {
        ql_rtos_semaphore_release(uart_rx_semaphore);
    }
}

static void uart_tx_callback(int uport, void *param)
{
    if (uart_tx_semaphore)
        ql_rtos_semaphore_release(uart_tx_semaphore);
}

void fc41d_uart_init(void)
{

    int ret = 0;
    uint8_t *receive_buffer;
    ql_uart_config_s uart_config;
    receive_buffer = os_malloc(100 * sizeof(uint8_t));
    if (receive_buffer == 0)
    {
        ql_uart_log("malloc error!");
        goto uart_error;
    }
    os_memset(receive_buffer, 0, 100);

    uart_config.baudrate = QL_UART_BAUD_115200;
    uart_config.data_bit = QL_UART_DATABIT_8;
    uart_config.parity_bit = QL_UART_PARITY_NONE;
    uart_config.stop_bit = QL_UART_STOP_1;
    uart_config.flow_ctrl = QL_FC_NONE;

    ql_uart_set_dcbconfig(MAIN_UART, &uart_config);

    ret = ql_uart_open(MAIN_UART);

    ret = ql_rtos_semaphore_create(&uart_tx_semaphore, 1);
    if (ret != EMPA_OK)
    {
        ql_uart_log("rtos_init_semaphore err:%d\r\n", ret);
        goto uart_error;
    }

    ret = ql_rtos_semaphore_create(&uart_rx_semaphore, 1);
    if (ret != EMPA_OK)
    {
        ql_uart_log("rtos_init_semaphore err:%d\r\n", ret);
        goto uart_error;
    }

    ret = ql_uart_set_tx_cb(UART_TEST_PORT, uart_tx_callback);
    if (ret != EMPA_OK)
    {
        ql_uart_log("ql_uart_set_tx_cb failed\r\n");
        goto uart_error;
    }

    ret = ql_uart_set_rx_cb(UART_TEST_PORT, uart_rx_callback);
    if (ret != EMPA_OK)
    {
        ql_uart_log("ql_uart_set_rx_cb failed\r\n");
        goto uart_error;
    }
    return;
    
    uart_error:
    if (uart_rx_semaphore)
        ql_rtos_semaphore_delete(uart_rx_semaphore);
    if (uart_tx_semaphore)
        ql_rtos_semaphore_delete(uart_tx_semaphore);
    os_free(receive_buffer);
    ql_uart_log("An error occurred, please reset the module.");
    return;
}

uint8_t fc41d_uart_write(const char *message)
{
    uint8_t ret = 0;
    if (message == NULL)
    {
        ql_uart_log("Message is empty");
        return 0;
    }

    ret = ql_uart_write(MAIN_UART, message, strlen(message));
    if(ret != EMPA_OK)
    {
        ql_uart_log("Uart write failed");
        return 1;
    }
}

uint8_t fc41d_read(unsigned char *buffer, uint32_t length)
{
    if (buffer == NULL || length == 0)
        return 0xFF;

    // UART'tan veri okuma
    uint8_t result = ql_uart_read(MAIN_UART, buffer, length);
    if (result == 0)
        return 0U;

    else
        return 0xFF;
}