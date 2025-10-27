#include "mq7.h"
#include "stm32f1xx_hal_gpio.h"
uint8_t s_co_alarm = 0;
uint8_t awd_event = 0;
ADC_HandleTypeDef hadc1;
float R0;
float v_in = 5.0f;
#define VREF_V     3.3f
#define K_SCALE    0.66f
void MQ7_Init(void){

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    HAL_ADC_Init(&hadc1);
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_0;    
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADCEx_Calibration_Start(&hadc1);
    ADC_AnalogWDGConfTypeDef awd = {0};


    HAL_NVIC_SetPriority(ADC1_2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
    HAL_ADC_Start(&hadc1);





}
static Status_e Read_ADC(uint32_t *avg) {
    *avg = 0;
    for (int i = 0; i < 10; i++) {
        uint32_t t0 = HAL_GetTick();
        while(!__HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_EOC)) {
            if ((HAL_GetTick() - t0) > 100) return ERR;
        }
        *avg += HAL_ADC_GetValue(&hadc1);
        __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC);
        HAL_Delay(5);
    }
    *avg /= 10;
    return OK;
}

static Status_e MQ7_ReadVoltages(float *v_adc, float *v_node) {
    uint32_t adc_val = 0;
    Status_e stt = Read_ADC(&adc_val);
    if (stt != OK) return stt;

    float Vadc  = (adc_val * VREF_V) / 4095.0f; 
    float Vnode = Vadc / K_SCALE;              

    if (Vnode > v_in) Vnode = v_in;
    if (Vnode < 0.001f) Vnode = 0.001f;     
    if (Vadc  < 0.0f)  Vadc  = 0.0f;

    if (v_adc)  *v_adc  = Vadc;
    if (v_node) *v_node = Vnode;
    return OK;
}
static Status_e MQ7_ReadVoltage(float *voltage) {
    float Vadc=0, Vnode=0;
    Status_e stt = MQ7_ReadVoltages(&Vadc, &Vnode);
    if (stt != OK) return stt;
    *voltage = Vnode;               
    return OK;
}
static Status_e readRsRL(float *RsRL) {
    float Vnode = 0;
    Status_e stt = MQ7_ReadVoltage(&Vnode);   
    if (stt != OK) return stt;
    if (Vnode <= 0.0f) return ERR;            

    float r = (v_in - Vnode) / Vnode;

    if (r < 0.01f) r = 0.01f;
    if (r > 100.0f) r = 100.0f;

    *RsRL = r;
       
    return OK;
}
static Status_e readRs(float *Rs){
    float ratio = 0;
    Status_e stt = readRsRL(&ratio);
    if (stt != OK) return stt;
   
    float Rs_val = _LOAD_RES * ratio;

    if (Rs_val < 0.1f)   Rs_val = 0.1f;
    if (Rs_val > 1e6f)   Rs_val = 1e6f;

    *Rs = Rs_val;
    return OK;
}

Status_e MQ7_Calibrate(void){
    const int WARMUP_S = 3;                
    for (int i = 0; i < WARMUP_S; i++) {
        delay_ms(1000);
        uart_printf("warmup %d/%d\r\n", i+1, WARMUP_S);
    }

    const int SAMPLES = 40;                
    double acc = 0.0;
    int ok_cnt = 0;

    for (int i = 0; i < SAMPLES; i++) {
        float Rs = 0;
        if (readRs(&Rs) == OK) {
            acc += Rs;
            ok_cnt++;
        }
        uart_printf("calibration sampling %d/%d\r\n", i+1, SAMPLES);
        delay_ms(1000);
    }

    if (ok_cnt == 0) return ERR;

    float Rs_avg = (float)(acc / ok_cnt);
    R0 = Rs_avg / _CALIBRATION_CONSTANT;

    if (R0 < 0.1f)  R0 = 0.1f;
    if (R0 > 1e6f)  R0 = 1e6f;

    uart_printf("R0=%.3f (Rs_avg=%.3f, samples=%d)\r\n", R0, Rs_avg, ok_cnt);
    return OK;
}

static float g_ppm_ema = 0.0f;
static uint8_t g_ppm_ema_init = 0;
Status_e MQ7_GetPPM(float *CO_ppm){
    if (!CO_ppm) return ERR;

    float Rs = 0;
    Status_e stt = readRs(&Rs);
    if (stt != OK) return stt;

    if (R0 <= 0.0f) return ERR; 

    float ratio = Rs / R0;

    if (ratio < 0.02f) ratio = 0.02f;
    if (ratio > 50.0f) ratio = 50.0f;

    float ppm = (float)_COEF_A0 * powf(ratio, _COEF_A1);

    if (ppm < 0.0f)   ppm = 0.0f;
    if (ppm > 50000.f) ppm = 50000.f; 

    const float alpha = 0.2f;
    if (!g_ppm_ema_init) {
        g_ppm_ema = ppm;
        g_ppm_ema_init = 1;
    } else {
        g_ppm_ema = g_ppm_ema + alpha * (ppm - g_ppm_ema);
    }

    *CO_ppm = g_ppm_ema;
    return OK;
}


bool MQ7_ReCallPPM(void){
    awd_event = 0;
    float ppm = 0;
    MQ7_GetPPM(&ppm);
    if(ppm >=CO_ALARM_PPM_THRESHOLD + 1.0f ){
        s_co_alarm = 1;
        uart_printf("current ppm:  %f \r\\n", ppm);
        uart_print("AWD confirmed: HIGH CO\r\n");
        return OK;

    }
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_AWD | ADC_FLAG_EOC);
    __HAL_ADC_ENABLE_IT(&hadc1, ADC_IT_AWD);
    
    return ERR;

}
static inline float ppm_to_Rs(float ppm) {
    return R0 * powf((ppm / _COEF_A0), (1.0f / _COEF_A1));
}

static inline float Rs_to_Vadc(float Rs) {
    if (Rs <= 0.0f) return 0.0f;
    float Vnode = v_in * (_LOAD_RES / (Rs + _LOAD_RES));
    float Vadc  = Vnode * K_SCALE;
    if (Vadc > VREF_V) Vadc = VREF_V;       
    if (Vadc < 0.0f)   Vadc = 0.0f;
    return Vadc;
}

static inline uint16_t V_to_cnt(float v) {
    if (v < 0.0f)   v = 0.0f;
    if (v > VREF_V) v = VREF_V;
    return (uint16_t)((v / VREF_V) * 4095.0f + 0.5f);
}

void MQ7_AWD_SetByPPM(float ppm_on)
{
    float  Rs_on  = ppm_to_Rs(ppm_on);
    float  Vadc_on = Rs_to_Vadc(Rs_on);
    uint16_t cnt_on = V_to_cnt(Vadc_on);

    uart_printf("cnt on %u (Vadc_on=%.3fV)\r\n", cnt_on, Vadc_on);

    ADC_AnalogWDGConfTypeDef awd = {0};
    awd.WatchdogMode  = ADC_ANALOGWATCHDOG_SINGLE_REG;
    awd.Channel       = ADC_CHANNEL_0; 
    awd.ITMode        = ENABLE;

    awd.LowThreshold  = 0;
    awd.HighThreshold = (cnt_on > 0) ? (cnt_on - 1) : 0;

    if (HAL_ADC_AnalogWDGConfig(&hadc1, &awd) != HAL_OK) {
        Error_Handler();
    }
    
   
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_AWD);
    __HAL_ADC_ENABLE_IT(&hadc1, ADC_IT_AWD);

    if ((hadc1.Instance->CR2 & ADC_CR2_ADON) == 0) HAL_ADC_Start(&hadc1);
    SET_BIT(hadc1.Instance->CR2, ADC_CR2_SWSTART);


    s_co_alarm = 0;

}


void ADC1_2_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);   
}
void MQ7_ENABLE_WDG_ITR(void){
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_AWD | ADC_FLAG_EOC);

    __HAL_ADC_ENABLE_IT(&hadc1, ADC_IT_AWD);

}

void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        awd_event = 1;                         
        __HAL_ADC_DISABLE_IT(&hadc1, ADC_IT_AWD); 
        __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_AWD | ADC_FLAG_EOC);
        
    }
}
void MQ7_DebugADC(void) {
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
        uint32_t adc_val = HAL_ADC_GetValue(&hadc1);
        float voltage = (adc_val * 3.3) / 4095.0f;
        uart_printf("ADC raw = %lu, Voltage = %.3f V\r\n", adc_val, voltage);
    } else {
        uart_printf("ADC read timeout\r\n");
    }
}