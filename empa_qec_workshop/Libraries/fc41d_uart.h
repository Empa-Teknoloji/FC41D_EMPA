#include "ql_uart.h"
#include "ql_api_osi.h"
#include "mem_pub.h"
#include "stdint.h"
#include "uart_pub.h"

#define EMPA_OK 0
#define MAIN_UART 0

static void uart_rx_callback(int uport, void *param);
static void uart_tx_callback(int uport, void *param);
void fc41d_uart_init(void);
uint8_t fc41d_uart_write(const char *message);
uint8_t fc41d_read(unsigned char *buffer, uint32_t length);