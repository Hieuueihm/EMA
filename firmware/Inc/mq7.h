#ifndef __MQ7_H
#define __MQ7_H


#include "common.h"
#include "stm32f1xx_hal_adc.h"
#include "math.h"
#define _CALIBRATION_CONSTANT 5.0
#define _LOAD_RES 10.0
#define _COEF_A0 100.0
#define _COEF_A1 -1.513
#define CALIBRATION_SECONDS 15
void MQ7_Init(void);
Status_e MQ7_GetPPM(float *CO_ppm);
Status_e MQ7_Calibrate(void);

#endif