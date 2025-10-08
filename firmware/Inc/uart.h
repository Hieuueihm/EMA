#ifndef __UART_H
#define __UART_H


#include <stdint.h>
#include "stm32f1xx_hal_gpio.h"
#include "stm32f103xb.h"
#include "stm32f1xx_hal_dma.h"
#include "stm32f1xx_hal_uart.h"
#include "common.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_rcc.h"
#include "delay.h"
#include "string.h"
typedef enum {
    UART1 = 1,
    UART2,
    UART3
} UART_Port;
typedef struct {
    UART_Port port;
    uint32_t baudrate;
} UART_Config;
typedef struct UART_Handler_t UART_Handler;

typedef struct {
    void (*send_char)(UART_Handler *huart, char c);
    Status_e (*send_char_wtimeout)(UART_Handler *huart, char c, uint32_t timeout_ms);
    void (*send_string)(UART_Handler *huart, const char *str);
    Status_e (*send_string_wtimeout)(UART_Handler *huart, const char *str, uint32_t timeout_ms);
    char (*recv_char)(UART_Handler *huart);
    size_t (*recv_string)(UART_Handler *huart, uint8_t *buffer, size_t length);
    Status_e  (*available)(UART_Handler *huart);
    Status_e (*read_byte)(UART_Handler *huart, uint8_t *byte);

} UART_Api;

struct UART_Handler_t {
    UART_HandleTypeDef huart;
    UART_Api api;
};


UART_Handler UART_Init(UART_Config cfg);

#endif