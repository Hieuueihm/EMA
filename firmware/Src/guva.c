#include "guva.h"

#include "stm32f1xx_hal_adc.h"
#include "stm32f1xx_hal_gpio.h"
ADC_HandleTypeDef hadc2; 


void GUVA_Init(void){
    __HAL_RCC_GPIOB_CLK_ENABLE();
    RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;


    // PA2 analog
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
    s2.Channel      = ADC_CHANNEL_2;            
    s2.Rank         = ADC_REGULAR_RANK_1;
    s2.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc2, &s2);


    
}
static Status_e adc2_read_avg(uint32_t channel, uint16_t nsamples, uint32_t *avg)
{
    if (!avg || nsamples == 0) return ERR;
    *avg = 0;

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = channel;                   
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK) return ERR;

    for (uint16_t i = 0; i < nsamples; i++) {
        if (HAL_ADC_Start(&hadc2) != HAL_OK) return ERR;                       // Start
        if (HAL_ADC_PollForConversion(&hadc2, 100) != HAL_OK) return ERR;      // Poll
        *avg += HAL_ADC_GetValue(&hadc2);                                      // Read
        HAL_Delay(2);                                                          // nhỏ để ổn định
    }
    *avg /= nsamples;
    return OK;
}
Status_e GUVA_ReadVoltage(float *voltage)
{
    if (!voltage) return ERR;
    uint32_t avg = 0;
    Status_e st = adc2_read_avg(ADC_CHANNEL_2, 10, &avg);   
    if (st != OK) return st;
    *voltage = (avg * GUVA_VREF) / GUVA_ADC_MAX;
    return OK;
}

Status_e GUVA_GetUV_mWcm2(float *uv)
{
    if (!uv) return ERR;
    float v = 0.0f;
    Status_e st = GUVA_ReadVoltage(&v);
    if (st != OK) return st;

    float u = (v - GUVA_V0) * GUVA_SLOPE_MWCM2_PER_V; 
    if (u < 0.0f) u = 0.0f;
    *uv = u;
    return OK;
}

Status_e GUVA_GetUVI(float *uvi) {
    if (!uvi) return ERR;
    float uv_mw = 0.0f;
    Status_e st = GUVA_GetUV_mWcm2(&uv_mw);
    if (st != OK) return st;
    *uvi = GUVA_K_UVI_PER_mWcm2 * uv_mw;
    return OK;
}