#ifndef __GUVA_H
#define __GUVA_H

#include "stm32f1xx_hal.h"
#include <math.h>
#include "common.h"
#include <stdint.h>
#include "stm32f1xx_hal_adc_ex.h"

#define GUVA_VREF               3.3f      
#define GUVA_ADC_MAX            4096.0f      

#define GUVA_ADCx               ADC1
#define GUVA_GPIO_PORT          GPIOA
#define GUVA_GPIO_PIN           GPIO_PIN_2
#define GUVA_ADC_CHANNEL        ADC_CHANNEL_2

#define SAMPLING_COUNT 1000


void GUVA_Init(void);
Status_e GUVA_ReadVoltage(float *voltage);
Status_e GUVA_GetUVI(uint8_t *uvi);


#endif
