#include "dht22.h"

static void Set_Pin_Mode(DHT *dht, PinMode Mode)
{

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = dht->pin;
    if(Mode == Input)
    {
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    }
    else
    {
      GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;    
   
    }
	HAL_GPIO_Init(dht->port, &GPIO_InitStruct);
}

static void Set_Pin_Level(DHT *dht, uint8_t Level)
{

	HAL_GPIO_WritePin(dht->port, dht->pin, Level);
}

static uint8_t Bus_Read(DHT *dht)
{

	return HAL_GPIO_ReadPin(dht->port, dht->pin);
}


static uint8_t DHT_Check_Response(DHT *dht)
{

	uint8_t Response = 0;


	Set_Pin_Mode(dht, Output);
	Set_Pin_Level(dht, 0);

	/* Delay waiting
	 * DHT22 = 1.2ms
	 */


	delay_us(1200);

	// /* Set Data Pin */
	// Set_Pin_Level(dht, 1);
	// delay_us(20);

	/* Set Data pin as Input */
	Set_Pin_Mode(dht, Input);
	/* Delay 40us */
	delay_us(40);

	if(!Bus_Read(dht))
	{


		delay_us(90);
		Response = (Bus_Read(dht)) ? 1 : -1;

        delay_us(50);
	}

	/* Wait for the pin to go reset */
	//   uint32_t count = 0;
    while (Bus_Read(dht)) {};
	// uart_print("go end");
	return Response;
}

static uint8_t DHT_Read(DHT *dht)
{
    
	uint8_t data = 0;
	for(uint8_t j = 0; j < 8; j++)
	{
		while(!Bus_Read(dht));
		delay_us(40);


		/* If the pin go reset */
		if(!Bus_Read(dht))
		{
			data &= ~(1 << (7 - j)); // 0 
		}else{
			data |= (1 << (7 - j));  // 1
		}
		while(Bus_Read(dht));


		
	}

	return data;
}




Status_e DHT_GetData(DHT *dht)
{

	if(DHT_Check_Response(dht))
	{
		dht->data_raw.Rh1 = DHT_Read(dht);
		dht->data_raw.Rh2 = DHT_Read(dht);
		dht->data_raw.Tp1 = DHT_Read(dht);
		dht->data_raw.Tp2 = DHT_Read(dht);
		dht->data_raw.Sum = DHT_Read(dht);


		if(dht->data_raw.Sum == ((dht->data_raw.Rh1 + dht->data_raw.Rh2 + dht->data_raw.Tp1 + dht->data_raw.Tp2) & 0xFF))
		{
			dht->temperature = ((dht->data_raw.Tp1 << 8) | dht->data_raw.Tp2) / 10.0;
			dht->humidity = ((dht->data_raw.Rh1 << 8) | dht->data_raw.Rh2) / 10.0;
		}
	} else return ERR;
    return OK;
}
static void GPIO_Clock_Enable(GPIO_TypeDef *port) {
    if(port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if(port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if(port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
}

static char * GPIO_Clock_get(GPIO_TypeDef *port) {
    if(port == GPIOA) return "A";
    else if(port == GPIOB) return "B";
    else if(port == GPIOC) return "C";
}
DHT DHT_Init(GPIO_TypeDef *port, uint16_t pin){
    GPIO_Clock_Enable(port);
    DHT ins;
    ins.port = port;
    ins.pin = pin;
    ins.api.read_data = DHT_GetData;

    return ins;
}