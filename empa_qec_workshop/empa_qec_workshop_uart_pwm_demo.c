// Includes
#include "ql_include.h"
#include "fc41d_uart.h"

// Defines
#define BIG_BUFFER_LEN 128
char buffer[BIG_BUFFER_LEN];

char recv_buf[2] = {0};

// Private Variables
UINT32 frequency = 0;
UINT32 duty = 0;
UINT16 state = 0;
char big_buffer[50];
extern ql_sem_t uart_rx_semaphore;
extern ql_sem_t uart_tx_semaphore;

typedef enum
{
    STATE_RX,
    STATE_UPDATE_PWM,
} States_Uart;

static void set_frequency(UINT32 freq)
{
    frequency = 26000000 / freq;
}

static void set_duty(UINT8 duty_cycle)
{
    duty = (duty_cycle * frequency) / 100;
}

void fc41d_pwm_config(UINT32 freq, UINT32 duty_cycle)
{
    UINT8 ret = 0;

    set_frequency(freq);
    set_duty(duty_cycle);
    ret = ql_pwmInit(PWM_TEST_CHANNEL, frequency, duty);
    if (ret == QL_PWM_SUCCESS)
    {
        snprintf(buffer, BIG_BUFFER_LEN, "\r\nPWM config success.");
        fc41d_uart_write(buffer);
    }
    else
    {
        snprintf(buffer, BIG_BUFFER_LEN, "\r\nPWM init fail.");
        fc41d_uart_write(buffer);
    }
    ql_pwmInit_level(PWM_TEST_CHANNEL, QL_PWM_INIT_LEVEL_HIGH);
}

void fc41d_pwm_start(void)
{
    ql_pwm_enable(PWM_TEST_CHANNEL);
    snprintf(buffer, BIG_BUFFER_LEN, "\r\nPWM started.");
    fc41d_uart_write(buffer);
}

static void pwm_update(UINT32 freq, UINT32 duty_cycle)
{
    ql_pwm_update_param(PWM_TEST_CHANNEL, freq, duty_cycle);
}

void fc41d_uart_pwm_demo_thread(void *param)
{
    int ret = 0;
    uint8_t *recv_buf;
    recv_buf = os_malloc(100 * sizeof(uint8_t));
    if (recv_buf == 0)
    {
        ql_uart_log("buf malloc failed\r\n");
        snprintf(buffer, BIG_BUFFER_LEN, "\r\nBuf malloc failed");
        fc41d_uart_write(buffer);
        return;
    }
    os_memset(recv_buf, 0, UART_TEST_LEN);
    snprintf(buffer, BIG_BUFFER_LEN, "\r\nEnter PWM Frequency,Duty: ");
    fc41d_uart_write(buffer);
    ql_rtos_semaphore_wait(uart_rx_semaphore, BEKEN_NEVER_TIMEOUT);

    while (1)
    {
        switch (state)
        {
        case STATE_RX:
            if (ql_uart_read(UART_TEST_PORT, recv_buf, 1) == 0)
            {
                strncat(big_buffer, recv_buf, 1);
            }
            ql_rtos_task_sleep_ms(10);
            if (strstr(big_buffer, "\r\n") != 0)
            {
                char temp_message[50];
                strncpy(temp_message, big_buffer, sizeof(temp_message) - 1);
                temp_message[sizeof(temp_message) - 1] = "\0";
                ql_uart_log("\n");
                ql_uart_log(temp_message);
                ql_uart_log("\n");
                char *token = strtok(temp_message, ",");
                if (token != NULL)
                {
                    ql_uart_log(token);
                    ql_uart_log("\n");
                    frequency = atoi(token);
                    if (frequency > 0)
                    {
                        token = strtok(NULL, "\r\n");
                        if (token != NULL)
                        {
                            ql_uart_log(token);
                            ql_uart_log("\n");
                            int duty_value = atoi(token);
                            if (duty_value > 0 && duty_value <= 100)
                            {
                                duty = duty_value;
                                state = STATE_UPDATE_PWM;
                                memset(big_buffer, 0, sizeof(big_buffer));
                                break;
                            }
                            else
                            {
                                duty_value = 0;
                                snprintf(buffer, BIG_BUFFER_LEN, "\r\nInvalid duty cycle. It must be between 0 and 100.");
                                fc41d_uart_write(buffer);
                                snprintf(buffer, BIG_BUFFER_LEN, "\r\nEnter PWM Frequency,Duty: ");
                                fc41d_uart_write(buffer);
                                memset(big_buffer, 0, sizeof(big_buffer));
                                break;
                            }
                        }
                        else
                        {
                            snprintf(buffer, BIG_BUFFER_LEN, "\r\nWrong message format. Empty duty value.");
                            fc41d_uart_write(buffer);
                            snprintf(buffer, BIG_BUFFER_LEN, "\r\nEnter PWM Frequency,Duty: ");
                            fc41d_uart_write(buffer);
                            memset(big_buffer, 0, sizeof(big_buffer));
                            break;
                        }
                    }
                    else
                    {
                        snprintf(buffer, BIG_BUFFER_LEN, "\r\nInvalid frequency. It must be greater than 0.");
                        fc41d_uart_write(buffer);
                        snprintf(buffer, BIG_BUFFER_LEN, "\r\nEnter PWM Frequency,Duty: ");
                        fc41d_uart_write(buffer);
                        memset(big_buffer, 0, sizeof(big_buffer));
                        break;
                    }
                }
                else
                {
                    snprintf(buffer, BIG_BUFFER_LEN, "\r\nWrong message format. Empty frequency value");
                    fc41d_uart_write(buffer);
                    snprintf(buffer, BIG_BUFFER_LEN, "\r\nEnter PWM Frequency,Duty: ");
                    fc41d_uart_write(buffer);
                    memset(big_buffer, 0, sizeof(big_buffer));
                    break;
                }
            }
            break;
        case STATE_UPDATE_PWM:
            snprintf(buffer, BIG_BUFFER_LEN, "\r\n%d Hz , %d %% duty PWM generated\r\n", frequency, duty);
            fc41d_uart_write(buffer);
            set_frequency(frequency);
            set_duty(duty);

            pwm_update(frequency, duty);
            state = STATE_RX;
            memset(big_buffer, 0, sizeof(big_buffer));
            break;
        }
    }
    os_printf(recv_buf);

uart_test_exit:
    if (uart_rx_semaphore)
        ql_rtos_semaphore_delete(uart_rx_semaphore);
    if (uart_tx_semaphore)
        ql_rtos_semaphore_delete(uart_tx_semaphore);

    os_free(recv_buf);

    while (1)
    {
        ql_rtos_task_sleep_ms(1000);
    }
}

ql_task_t fc41d_uart_pwm_demo_thread_handle = NULL;

void fc41d_uart_pwm_demo(void)
{
    int ret;
    ret = ql_rtos_task_create(&fc41d_uart_pwm_demo_thread_handle,
                              (unsigned short)4096,
                              THD_EXTENDED_APP_PRIORITY,
                              "fc41d_uart_pwm_demo_thread",
                              fc41d_uart_pwm_demo_thread,
                              0);

    if (ret != kNoErr)
    {
        snprintf(buffer, BIG_BUFFER_LEN, "\r\nError: Failed to create fc41d_uart_pwm_demo_thread");
        fc41d_uart_write(buffer);
        goto init_err;
    }
    return;

init_err:
    if (fc41d_uart_pwm_demo_thread_handle != NULL)
    {
        ql_rtos_task_delete(fc41d_uart_pwm_demo_thread_handle);
    }
}