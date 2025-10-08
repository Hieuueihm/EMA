#ifndef __SDS011_H
#define __SDS011_H

#include "stm32f1xx_hal.h"
#include "uart.h"
#include "common.h"
extern uint8_t Sds011_WorkingMode[30];
extern uint8_t Sds011_SleepCommand[30];
extern uint8_t Sds011_Query[30];
typedef struct SDS011_t SDS011;
typedef struct SDS011_t {
    UART_Handler sds_uart;
    uint16_t pm_2_5;
    uint16_t pm_10;
    uint8_t data_received[10];
    struct {
        Status_e (*send)(SDS011 *sds, const uint8_t * data_buffer);
        uint16_t (*get_pm25)(SDS011 *sds);
        uint16_t (*get_pm10)(SDS011 *sds);
        Status_e (*set_working_mode)(SDS011 *sds);
        Status_e (*set_sleep_mode)(SDS011 *sds);
        Status_e (*query_data)(SDS011 *sds);
    } api;
};

SDS011 SDS_Init(UART_Config cfg);




#endif

