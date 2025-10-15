#ifndef __GUVA_H
#define __GUVA_H

#include "stm32f1xx_hal.h"
#include <math.h>
#include "common.h"
#include <stdint.h>

#define GUVA_VREF               3.3f        
#define GUVA_ADC_MAX            4095.0f      

#define GUVA_ADCx               ADC1
#define GUVA_GPIO_PORT          GPIOA
#define GUVA_GPIO_PIN           GPIO_PIN_2
#define GUVA_ADC_CHANNEL        ADC_CHANNEL_2

#define GUVA_V0                 0.00f
#define GUVA_SLOPE_MWCM2_PER_V  3.0f
#define GUVA_K_UVI_PER_mWcm2     10.0f 


void GUVA_Init(void);
Status_e GUVA_ReadVoltage(float *voltage);
Status_e GUVA_GetUV_mWcm2(float *uv_mwcm2);
Status_e GUVA_GetUVI(float *uvi);


#endif
