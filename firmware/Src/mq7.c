#include "mq7.h"
ADC_HandleTypeDef hadc1;

float R0;
float v_in = 5.0f;
void MQ7_Init(void){

    __HAL_RCC_GPIOA_CLK_ENABLE();
__HAL_RCC_ADC1_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  HAL_ADC_Init(&hadc1);

  sConfig.Channel = ADC_CHANNEL_0;     // PA0
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);



}

static Status_e Read_ADC(uint32_t *sum)
{
  for (int i = 0; i < 10; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_StatusTypeDef ret = HAL_ADC_PollForConversion(&hadc1, 100);
    if (ret == HAL_OK) {
        *sum += HAL_ADC_GetValue(&hadc1);
    } else {
        return ERR;
    }
    HAL_Delay(5);
  }
  *sum /= 10;
  return OK;
}
static Status_e MQ7_ReadVoltage(float *voltage)
{
  uint32_t adc_val = 0;
  Status_e stt = Read_ADC(&adc_val);
  
  *voltage = (adc_val * 3.3) / 4095.0f;
  return stt;
}
static Status_e readRsRL(float * RsRL) {
	float voltage = 0;
  Status_e stt = MQ7_ReadVoltage(&voltage);

	*RsRL =(v_in - voltage) / voltage;
  return stt;
}
static Status_e readRs(float *Rs){
  float RsRL = 0;
  Status_e stt = readRsRL(&RsRL);
  *Rs = _LOAD_RES * RsRL;
  return stt;
}


Status_e MQ7_Calibrate(void){
  float Rs = 0;
  for (int i = 0; i <= CALIBRATION_SECONDS; i++) {
		HAL_Delay(1000);
    Status_e stt = readRs(&Rs); 
    if(stt == ERR) return ERR;
		R0 = Rs / _CALIBRATION_CONSTANT;
	}
  return OK;
}


Status_e MQ7_GetPPM(float *CO_ppm){

    float Rs = 0; 
    Status_e stt = readRs(&Rs);

    
    float ratio = Rs / R0;

    *CO_ppm = (float) _COEF_A0 * pow(ratio,_COEF_A1);
    return stt;


}
