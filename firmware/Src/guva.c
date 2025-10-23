#include "guva.h"

#include "stm32f1xx_hal_adc.h"
#include "stm32f1xx_hal_gpio.h"
ADC_HandleTypeDef hadc2; 


void GUVA_Init(void){
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_ADC2_CLK_ENABLE();
    // RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;


    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_1;     //B1
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // ADC2 init
    hadc2.Instance                      = ADC2;
    hadc2.Init.ScanConvMode             = ADC_SCAN_DISABLE;
    hadc2.Init.ContinuousConvMode       = DISABLE;
    hadc2.Init.DiscontinuousConvMode    = DISABLE;
    hadc2.Init.ExternalTrigConv         = ADC_SOFTWARE_START;
    hadc2.Init.DataAlign                = ADC_DATAALIGN_RIGHT;
    hadc2.Init.NbrOfConversion          = 1;

    if (HAL_ADC_Init(&hadc2) != HAL_OK) return;
    ADC_ChannelConfTypeDef s2 = {0};
    s2.Channel      = ADC_CHANNEL_9;            
    s2.Rank         = ADC_REGULAR_RANK_1;
    s2.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc2, &s2);
    HAL_ADCEx_Calibration_Start(&hadc2);


    
}

Status_e GUVA_ReadVoltage(float *voltage)
{
    if (!voltage) return ERR;
    float sum = 0;
     for (int i = 0; i < SAMPLING_COUNT; i++) {
        if (HAL_ADC_Start(&hadc2) != HAL_OK) return ERR;
        if (HAL_ADC_PollForConversion(&hadc2, 100) != HAL_OK) return ERR;
        sum += HAL_ADC_GetValue(&hadc2);
        HAL_ADC_Stop(&hadc2);
        HAL_Delay(2);
    }

    uint32_t avg = sum / SAMPLING_COUNT;
    *voltage = (avg * GUVA_VREF) / GUVA_ADC_MAX; 

    return OK;

}



Status_e GUVA_GetUVI(uint8_t *uvi) {
    if (!uvi) return ERR;
    float voltage = 0.0f;
    if (GUVA_ReadVoltage(&voltage) != OK) return -1;

    float uvi_f = voltage;
    if (uvi_f < 0.0f) uvi_f = 0.0f;
    *uvi = (int)lroundf(uvi_f);
    return OK;
}