#include "lv_port.h"
#include "esp_lvgl_port.h"
#include "ft6336u_driver.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include <esp_lcd_panel_vendor.h>
#include "xl9555.h"
#include "lv_demos.h"
// #include "ft6336u_driver.h"

#define LCD_WIDTH 320
#define LCD_HEIGHT 240

static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;

static lv_display_t *lvgl_disp = NULL;

static lv_indev_t *lvgl_touch_indev = NULL;

// 显示硬件接口初始化
static void lv_disp_hard_init(void)
{
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = GPIO_NUM_1,  // DC引脚
        .wr_gpio_num = GPIO_NUM_41, // WR引脚
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {
            // 8个数据引脚
            GPIO_NUM_40,
            GPIO_NUM_38,
            GPIO_NUM_39,
            GPIO_NUM_48,
            GPIO_NUM_45,
            GPIO_NUM_21,
            GPIO_NUM_47,
            GPIO_NUM_14,
        },
        .bus_width = 8, // 传输的数据位宽，8位就填8
        /*max_transfer_bytes
        和屏幕大小有关，也就是一次最大能传输的字节数，
        我们直接写整个屏幕的像素大小，
        底层如果超过了最大值会帮我们自动设置为最大值
        */
        .max_transfer_bytes = LCD_HEIGHT * LCD_WIDTH,
        /*dma_burst_size
        突发传输字节，越大越能显著提高数据传输效率，
        减少总线占用次数和系统开销，这里最大可以设置为64
        */
        .dma_burst_size = 64,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = GPIO_NUM_2, // CS片选引脚
        /*pclk_hz 像素时钟
        素传输的频率，这里举个例子，配套的屏是320*240的，
        现在我们要求刷新速率为30帧，那像素时钟就是320*240*30 = 2304000，
        大概2M多，但实际上这个是最低要求，还需要考虑命令字节的传输，
        还有读写信号的切换等等，这些都是消耗，
        经过查询一些资料，这里我们设置成10M，如果屏幕会出现花屏现象，还需降低
        */
        .pclk_hz = 10 * 1000 * 1000,
        /*trans_queue_depth 后台传输深度
        当一帧数据未传完后，又有需要写入一帧数据，这时就会存入队列中，
        此值就是缓存要写入的数据，经尝试，修改后效果不明显，按默认10就行
        */
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,   // 8位命令
        .lcd_param_bits = 8, // 8位数据
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1, // dc高电平的时候传输的时数据
        },
        .flags = {
            .swap_color_bytes = 1, // 交换颜色字节
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    // 初始化液晶屏驱动芯片
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,              // LCD屏复位引脚接到了XL9555上，所以无法直接控制，这里给NC
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // 颜色传输顺序时RGB顺序
        .bits_per_pixel = 16,                       // 颜色采用16位，也是就是RGB565，R占5位，G占6位，B占5位
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &lcd_panel));
}

/* 触摸坐标获取回调函数
 *@param indev_drv 输入设备，暂时无用
 *@param data 需要设置的触摸坐标数据
 *@param NULL
 */
void IRAM_ATTR indev_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    int16_t x = 0, y = 0;
    int state = 0;
    /* 调用ft6336u驱动获取触摸坐标 */
    ft6336u_read(&x, &y, &state);
    /* 横屏变竖屏需要翻转X/Y坐标系 */
    data->point.x = LCD_WIDTH - 1 - y;
    data->point.y = x;
    data->state = state;
}

/* 触摸屏硬件接口初始化 */
static void lcd_touch_hard_init(void)
{
    ft6336u_init(GPIO_NUM_13, GPIO_NUM_12);
}

void lv_port_init(void)
{
    xl9555_pin_write(LCD_RST_IO, 0); // 调用xl9555接口，复位lcd屏
    vTaskDelay(pdMS_TO_TICKS(200));
    xl9555_pin_write(LCD_RST_IO, 1);
    lv_disp_hard_init();                          // 初始化8080接口
    lcd_touch_hard_init();                        //
    esp_lcd_panel_init(lcd_panel);                // 向屏写入初始化命令
    esp_lcd_panel_swap_xy(lcd_panel, true);       // 交换X/Y坐标系
    esp_lcd_panel_mirror(lcd_panel, false, true); // x方向不设置镜像.y方向设置镜像
    esp_lcd_panel_disp_on_off(lcd_panel, true);   // 打开显示
    // LVGL 任务
    lvgl_port_cfg_t lv_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lv_cfg.task_affinity = 1; // lvgl任务运行在哪个内核上
    lv_cfg.task_stack = 8192; // lvgl任务栈空间
    lvgl_port_init(&lv_cfg);  // 调用此函数后，lvgl任务将运行起来

    // 添加屏幕设备
    uint32_t buff_size = LCD_WIDTH * 100; // 显示缓存(100行)
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = lcd_panel,
        .buffer_size = buff_size,               // 显示缓存
        .double_buffer = 0,                     // 不启用双缓存
        .hres = LCD_WIDTH,                      // 屏宽
        .vres = LCD_HEIGHT,                     // 屏高
        .color_format = LV_COLOR_FORMAT_RGB565, // 颜色RGB565模式
        .rotation = {
            .swap_xy = true,   // 交换X/Y坐标轴
            .mirror_x = false, // X方向不设置镜像
            .mirror_y = true,  // Y方向设置镜像
        },
        .flags = {
            .buff_dma = true,     // 使用DMA传输
            .buff_spiram = false, // 不使用spiram
            .direct_mode = false, // 不使用直接显示（作用未知，一般不启用）
        }};
    lvgl_disp = lvgl_port_add_disp(&disp_cfg); // 往lvgl添加一个显示设备

    /* 添加触摸驱动 */
    lvgl_port_lock(0);
    lvgl_touch_indev = lv_indev_create();
    lv_indev_set_type(lvgl_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvgl_touch_indev, indev_read);
    lvgl_port_unlock();
}