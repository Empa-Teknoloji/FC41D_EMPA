#include "ql_include.h"
#include "fc41d_uart.h"

#define ADC_CHANNEL_1 1
#define LOG_BUFFER_SIZE 100

static ql_adc_obj_s fc41d_adc_obj;
ql_task_t adc_demo_thread_handle = NULL;
char log_buffer[LOG_BUFFER_SIZE];

static void fc41d_adc_detect_callback(int new_mv, void *user_data)
{
	static int cnt = 0;
	if (cnt++ >= 100)
	{
		cnt = 0;
		snprintf(log_buffer, LOG_BUFFER_SIZE, "\r\nADC Channel%d voltage:%d mV", fc41d_adc_obj.channel, new_mv);
		fc41d_uart_write(log_buffer);
	}
}
void fc41d_adc_init(void)
{

	ql_adc_thread_init();
	ql_adc_channel_init(&fc41d_adc_obj, fc41d_adc_detect_callback, ADC_CHANNEL_1, NULL);
	snprintf(log_buffer, LOG_BUFFER_SIZE, "\r\nADC Channel%d initalized.", fc41d_adc_obj.channel);
	fc41d_uart_write(log_buffer);
}
void fc41d_adc_demo_thread(void *param)
{
	ql_adc_channel_start(&fc41d_adc_obj);
	snprintf(log_buffer, LOG_BUFFER_SIZE, "\r\nADC Channel%d started.", fc41d_adc_obj.channel);
	fc41d_uart_write(log_buffer);
	ql_rtos_task_delete(adc_demo_thread_handle);
}

void fc41d_adc_demo(void)
{
	int ret;
	ret = ql_rtos_task_create(&adc_demo_thread_handle,
							  (unsigned short)2048,
							  THD_EXTENDED_APP_PRIORITY,
							  "fc41d_adc_demo_thread",
							  fc41d_adc_demo_thread,
							  0);

	if (ret != kNoErr)
	{
		snprintf(log_buffer, LOG_BUFFER_SIZE, "\r\nError: Failed to create adc demo thread");
		fc41d_uart_write(log_buffer);
		goto init_err;
	}

	return;

init_err:
	if (adc_demo_thread_handle != NULL)
	{
		ql_rtos_task_delete(adc_demo_thread_handle);
	}
}
