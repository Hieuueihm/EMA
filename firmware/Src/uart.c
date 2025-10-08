#include "uart.h"

static USART_TypeDef *UART_GetInstance(UART_Port port)
{
    switch (port) {
        case UART1: return USART1;
        case UART2: return USART2;
        case UART3: return USART3;
        default: return NULL;
    }
}
static void UART_SendChar(UART_Handler *huart, char c)
{
    while (!(huart->huart.Instance->SR & USART_SR_TXE));
    huart->huart.Instance->DR = c;
    while (!(huart->huart.Instance->SR & USART_SR_TC));
}
static Status_e UART_SendChar_WTimeout(UART_Handler *huart, char c, uint32_t timeout_ms)
{
    uint32_t counter = 0;
    while (!(huart->huart.Instance->SR & USART_SR_TXE)){
        if(counter ++ > timeout_ms * 1000) return TIMEOUT;
        delay_us(1);
    }
    huart->huart.Instance->DR = c;
    counter = 0;
    while (!(huart->huart.Instance->SR & USART_SR_TC))
    {
        if (counter++ > timeout_ms * 1000)
            return TIMEOUT;
        delay_us(1);
    }
    return OK;
}
static void UART_SendString(UART_Handler *huart, const char *str)
{
    while (*str)
        UART_SendChar(huart, *str++);
}
static Status_e UART_SendString_WTimeout(UART_Handler *huart, const char *str, uint32_t timeout_ms)
{
        while (*str)
    {
        Status_e ret = UART_SendChar_WTimeout(huart, *str++, timeout_ms);
        if (ret != OK)
            return ret;
    }
    return OK;
}
static char UART_RecvChar(UART_Handler *huart)
{
    while (!(huart->huart.Instance->SR & USART_SR_RXNE));
    return (char)(huart->huart.Instance->DR & 0xFF);
}

static size_t UART_ReceiveString(UART_Handler *huart, uint8_t *buffer, size_t length)
{
    if (!buffer || length == 0) return 0;

    size_t count = 0;
    while (count < length)
    {
        buffer[count] = (uint8_t)UART_RecvChar(huart);
        count++;
    }
    return count;
}

static Status_e UART_Available(UART_Handler *huart)
{
    return (huart->huart.Instance->SR & USART_SR_RXNE) ? OK : ERR;
}
static Status_e UART_ReadByte(UART_Handler *huart,  uint8_t *byte){
     return HAL_UART_Receive(&huart->huart, byte, 1, 100);
}
UART_Handler UART_Init(UART_Config cfg) {


    switch(cfg.port)
{   
    case UART1:
        __HAL_RCC_USART1_CLK_ENABLE();   
        __HAL_RCC_GPIOA_CLK_ENABLE();

        {
            GPIO_InitTypeDef GPIO_InitStruct = {0};

            // TX (PA9)
            GPIO_InitStruct.Pin = GPIO_PIN_9;
            GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

            // RX (PA10)
            GPIO_InitStruct.Pin = GPIO_PIN_10;
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_NOPULL; // hoặc GPIO_PULLUP nếu muốn
            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
        }
        break;

    case UART2:
        __HAL_RCC_USART2_CLK_ENABLE();   
        __HAL_RCC_GPIOA_CLK_ENABLE();

        {
            GPIO_InitTypeDef GPIO_InitStruct = {0};

            // TX (PA2)
            GPIO_InitStruct.Pin = GPIO_PIN_2;
            GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

            // RX (PA3)
            GPIO_InitStruct.Pin = GPIO_PIN_3;
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        }
        break;

    case UART3:
        __HAL_RCC_USART3_CLK_ENABLE();   
        __HAL_RCC_GPIOB_CLK_ENABLE();

        {
            GPIO_InitTypeDef GPIO_InitStruct = {0};

            // TX (PB10)
            GPIO_InitStruct.Pin = GPIO_PIN_10;
            GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

            // RX (PB11)
            GPIO_InitStruct.Pin = GPIO_PIN_11;
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        }
        break;

    default:
        // Port không hợp lệ
        break;
}

    UART_Handler handle;
    handle.huart.Instance = UART_GetInstance(cfg.port);
    handle.huart.Init.BaudRate = cfg.baudrate;
    handle.huart.Init.WordLength = UART_WORDLENGTH_8B;
    handle.huart.Init.StopBits = UART_STOPBITS_1;
    handle.huart.Init.Parity = UART_PARITY_NONE;
    handle.huart.Init.Mode = UART_MODE_TX_RX;
    handle.huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    handle.huart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&handle.huart);

    handle.api.send_char   = UART_SendChar;
    handle.api.send_char_wtimeout = UART_SendChar_WTimeout;
    handle.api.send_string = UART_SendString;
    handle.api.send_string_wtimeout = UART_SendString_WTimeout;
    handle.api.recv_char   = UART_RecvChar;
    handle.api.available   = UART_Available;
    handle.api.recv_string = UART_ReceiveString;
    handle.api.read_byte = UART_ReadByte;
    return handle;
}