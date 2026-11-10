#include <stdio.h>
#include "lv_port.h"
#include "xl9555.h"
#include "ft6336u_driver.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "lv_demos.h"
#include "gui_guider.h"
#include "gg_utils.h"
#include "custom.h"


#define TAG     "main"


#define TP_RST_IO            IO1_0
#define TP_INT_IO            IO1_1


#define XL9555_SDA  GPIO_NUM_10
#define XL9555_SCL  GPIO_NUM_11


gg_ui_t guider_ui;

/* XL9555中断回调函数
*@param io_num 获取变化的引脚电平
*@param level 电平
*@return NULL
*/
void xl9555_input_callback(uint16_t io_num,int level)
{
    /* 检测到TP_INT引脚有电平变化 */
    if(io_num==TP_INT_IO){
        ft6336u_int_info(!level);/* 调用ft6336u_int_info通知ft6336u驱动 */
    }
}

void app_main(void)
{
    xl9555_init(XL9555_SDA,XL9555_SCL,GPIO_NUM_17,xl9555_input_callback);
    xl9555_ioconfig((~(LCD_RST_IO | LCD_BL_IO|TP_RST_IO)&0xFFFF));
    ESP_LOGI(TAG, "XL9555 init succes!");
    lv_port_init();
    lvgl_port_lock(0);
    setup_ui(&guider_ui);
    custom_init(&guider_ui);
    lvgl_port_unlock();
}
