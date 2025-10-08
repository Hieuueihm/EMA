#ifndef __DHT22_H
#define __DHT22_H

#include "common.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_tim.h"
#include "delay.h"
#include "stm32f1xx_hal_rcc.h"
typedef enum
{
  Input,
  Output
} PinMode;

typedef struct {
	uint8_t		Rh1;
	uint8_t		Rh2;
	uint8_t		Tp1;
	uint8_t		Tp2;
	uint16_t	Sum;
}DHT_Raw_t;

typedef struct DHT_t DHT;
struct DHT_t {
    GPIO_TypeDef	*port;
	uint16_t		pin;
	DHT_Raw_t		data_raw;
	float 			temperature;
	float 			humidity;
    struct {
        Status_e (*read_data)(DHT *dht);
    } api;
};


DHT DHT_Init(GPIO_TypeDef *DataPort, uint16_t DataPin);
#endif