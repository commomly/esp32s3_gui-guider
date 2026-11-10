

#include "ft6336u_driver.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "ft6336u";

/* 中断 */
static volatile bool ft6336u_int_flag = false;

static i2c_master_bus_handle_t ft6336u_i2c_master = NULL;
static i2c_master_dev_handle_t ft6336u_i2c_device = NULL;

/* 判断触摸芯片连接是否成功 */
bool touch_detected;

/*ft6336u初始化
 *@param cfg 配置
 *@retrun err
 */

esp_err_t ft6336u_init(gpio_num_t sda, gpio_num_t scl)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1, /* 自动选择i2c控制器 */
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT, /* 时钟源设置为默认时钟源 */
        .glitch_ignore_cnt = 7,            /* 过滤毛刺周期，典型值为7 */
    };

    /* 初始化i2c总线，返回i2c总线句柄 */
    i2c_new_master_bus(&bus_config, &ft6336u_i2c_master);

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, /* i2c地址，7位 */
        .device_address = FT6336U_ADDR,        /* ft6336u的地址时0x38 */
        .scl_speed_hz = 400 * 1000,            /* i2c通信时钟速率，400kHz */
    };

    /* 在i2c总线上添加一个设备 */
    i2c_master_bus_add_device(ft6336u_i2c_master, &dev_config, &ft6336u_i2c_device);

    uint8_t read_data_buf[1];
    uint8_t write_data_buf[2];
    write_data_buf[0] = 0xA3;
    esp_err_t ret;

    /* 读取芯片出场ID：0xA3 ，成功说明芯片存在并挂载成功 */
    ret = i2c_master_transmit_receive(ft6336u_i2c_device, &write_data_buf[0], 1, read_data_buf, 1, 200);
    if (ret == ESP_OK)
    {
        touch_detected = true;
    }
    ESP_LOGI(TAG, "\tChip ID:0x%02x", read_data_buf[0]);

    write_data_buf[0] = 0xA4;
    write_data_buf[1] = 0x00;
    /* 向0xA4地址写入0值，启动中断轮询模式 */
    ret = i2c_master_transmit(ft6336u_i2c_device, &write_data_buf[0], 2, 500);
    return touch_detected ? ESP_OK : ESP_FAIL;
}

/*读取坐标值
 *@param x x坐标
 *@param y y坐标
 *@param state 松手状态 0：松手 1：按下
 *@return NULL
 */
void ft6336u_read(int16_t *x, int16_t *y, int *state)
{
    static int16_t last_x = 0; /* 12位坐标数据 */
    static int16_t last_y = 0; /* 12位坐标数据 */
    uint8_t reg_addr[1];
    uint8_t data_coord[5]; /* 坐标 */
    /* 如果没有检测到INT引脚有下降沿，直接返回，不调用i2c函数节省操作时间 */
    if (!ft6336u_int_flag)
    {
        *x = last_x;
        *y = last_y;
        *state = 0;
        return;
    }

    /* 读取触点个数，坐标 */
    reg_addr[0] = 0x02;
    i2c_master_transmit_receive(ft6336u_i2c_device, &reg_addr[0], 1, &data_coord[0], 5, 500);
    /* 触点个数不等于一，不进行处理 */
    if ((data_coord[0] & 0x0f )!= 1)
    {
        *x = last_x;
        *y = last_y;
        *state = 0;
        return;
    }

    /* 计算触点x坐标 */
    int16_t current_x = ((data_coord[1] & 0x0f) << 8) | (data_coord[2] & 0xFF);
    /* 计算触点y坐标 */
    int16_t current_y = ((data_coord[3] & 0x0f) << 8) | (data_coord[4] & 0xFF);
    if (last_x != current_x || current_y != last_y)
    {
        last_x = current_x;
        last_y = current_y;
    };
    *x = last_x;
    *y = last_y;
    /* 显示按键按下 */
    *state = 1;
}

/*INT中断引脚通知
 *@param flag 是否检测到中断 true：是 ；false：否
 *@return NULL
 */

void ft6336u_int_info(bool flag){
    ft6336u_int_flag=flag;
}