
#include "ql_app.h"

MQTTConfig config =
    {
        .clientID = "clientID",
        .username = "username",
        .password = "password",
        .url = "url",
        .port = 8883,
        .keepAliveInterval = 30000,
        .cleanSession = 1,
        .topicSub = "/topic/A1",
        .topicPub = "/topic/genel"};

void ql_demo_main()
{
   /* FC41D Init Section Start */
   // fc41d_init();
   /* FC41D Init Section End */

   /* FC41D UART PWM Demo Section Start */
   //  fc41d_pwm_config(1,50);
   //  fc41d_pwm_start();
   //  fc41d_uart_pwm_demo();
   /* FC41D UART PWM Demo Section End */

   /* FC41D ADC  Demo Section Start */
   //  fc41d_adc_init();
   //  fc41d_adc_demo();
   /* FC41D ADC  Demo Section End */

   /* FC41D BLE  Demo Section Start */
   // fc41d_ble_config();
   // fc41d_ble_demo();
   /* FC41D BLE  Demo Section End */

   /* FC41D MQTT  Demo Section Start */
   // fc41d_mqtt_demo();
   /* FC41D MQTT  Demo Section End */
}
