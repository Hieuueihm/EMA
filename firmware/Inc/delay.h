#ifndef __DELAY_H__
#define __DELAY_H__

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include "stm32f1xx_hal_tim.h"
#include "stm32f1xx_hal_rcc.h"
void delay_init(void);
void delay_us(uint16_t us);
void delay_ms(uint32_t ms);

#endif
