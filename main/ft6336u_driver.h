#ifndef _FT_6366U_H_
#define _FT_6366U_H_

#include "stdbool.h"
#include "driver/gpio.h"
#include "esp_err.h"


#define FT6336U_ADDR 0x38

esp_err_t ft6336u_init(gpio_num_t sda, gpio_num_t scl);
void ft6336u_read(int16_t *x, int16_t *y, int *state);
void ft6336u_int_info(bool flag);

#endif