#ifndef _QUECTEL_DEMO_MAIN_H_
#define _QUECTEL_DEMO_MAIN_H_

#include "include.h"
#include "rtos_pub.h"
#include "error.h"
#include "uart_pub.h"
#include "drv_model_pub.h"
#include "quectel_demo_config.h"

#include "ql_uart.h"
#include "ql_timer.h"
#include "ql_i2c1_eeprom.h"
#include "ql_watchdog.h"
#include "ql_spi.h"
#include "ql_spi_flash.h"
#include "ql_pwm.h"
#include "ql_gpio.h"
#include "ql_flash.h"
#include "ql_adc.h"
#include "ql_ble.h"
#include "ql_api_osi.h"
#include "ql_lowpower.h"
#include "wizchip_port.h"
#include "ql_ftp_client_c.h"

typedef struct {
    char *clientID;
    char *username;
    char *password;
    char *url;
    int port;
    int keepAliveInterval;
    int cleanSession;
    char *topicSub;
    char *topicPub;
} MQTTConfig;

extern void ql_demo_main();
#endif
