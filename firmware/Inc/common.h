#ifndef __COMMON_H
#define __COMMON_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include "stm32f103xb.h"
#include "define.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_uart.h"
#include "stm32f1xx_hal_adc.h"

#include <stdarg.h>
#include <stdio.h>
#include "string.h"

void blink_led_test(uint32_t time_ms);
void uart1_init(uint32_t baudrate);
void uart_print(const char *str);
void uart_printf(const char *format, ...);
 



#endif