#include "common.h"
UART_HandleTypeDef huart1;

void blink_led_test(uint32_t time_ms){
#ifdef __DEBUG_EN
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
      HAL_Delay(time_ms);
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
#endif
}


void uart1_init(uint32_t baudrate)
{
#ifdef __DEBUG_EN
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = baudrate;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if(HAL_UART_Init(&huart1) != HAL_OK)
    {
        while(1);
    }
#endif

}

void uart_print(const char *str)
{
#ifdef __DEBUG_EN
    HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 1000);
#endif
}

void uart_printf(const char *format, ...)
{
#ifdef __DEBUG_EN
    char buffer[128];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if(len > 0)
    {
        if(len > sizeof(buffer)) len = sizeof(buffer);
        HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, 1000);
    }
#endif
}


void uart_printf_tag(const char *TAG, const char *format, ...)
{
#ifdef __DEBUG_EN
    char buffer[1000];
    char msg[1000];  
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if(len > 0) {
        if(len > (int)sizeof(buffer)) len = sizeof(buffer);

        int prefix = snprintf(msg, sizeof(msg), "[%s] %s", TAG, buffer);
        if (prefix > (int)sizeof(msg)) prefix = sizeof(msg);

        HAL_UART_Transmit(&huart1, (uint8_t*)msg, prefix, 1000);
    }
#endif
}
