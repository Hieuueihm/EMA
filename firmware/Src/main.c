#include "main.h"
#include "stm32f1xx_hal_rcc_ex.h"
#include "stm32f1xx_hal_spi.h"
#include "stm32f1xx_hal.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
// SDS011 sds;

SDS011 sds;
UART_HandleTypeDef huart2;
DHT dht;

uint8_t data[50];
uint16_t size = 0;
// uint8_t data[50];
// uint16_t size = 0;
UART_Handler huart;
extern UART_HandleTypeDef huart1;
uint8_t rxData[4] = {0};
volatile uint8_t rx_ready = 0;
int main(void)
{
   
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    delay_init();

    uart1_init(115200);

    UART_Config cfg1 = {
            .port = UART1,       
            .baudrate = 115200  
    };    
    UART_Config cfg2 = {
            .port = UART2,       
            .baudrate = 9600  
    };    
    uart_print("program start\r\n");
    
    uart_print("sds init\r\n");

    sds = SDS_Init(cfg2);

    uart_print("dht init\r\n");

    dht = DHT_Init(GPIOA, GPIO_PIN_1);
    uart_print("mq7 init\r\n");

    MQ7_Init();
    MQ7_Calibrate();
    uart_print("guva init\r\n");

    GUVA_Init();
    char msg[64];
    float ppm = 0.0f;


    LoRa ins;
    ins = SX1278_Init();
    ins.api.lora_set_frequency(&ins, 433E6);
    ins.api.lora_set_spreading_factor(&ins, 12);
    ins.api.lora_set_bandwidth(&ins, 125E3);
    ins.api.lora_enable_crc(&ins);

    float uv_mw = 0.0f;
   
    while (1)
    {

    char buf[100];

    MQ7_GetPPM(&ppm);
    GUVA_GetUVI(&uv_mw);
    dht.api.read_data(&dht);
    sds.api.query_data(&sds);
    uint8_t len = 0;

    len += sprintf(buf, "Temp: %.1f C  Humidity: %.1f ppm %.1f uv %.1f pm2.5 %d pm10 %d \r\n ", dht.temperature, dht.humidity, ppm, uv_mw, sds.pm_2_5, sds.pm_10);

    // // In dạng text đơn giản
    // if (len < 0) len = 0;
    // if (len > (int)sizeof(buf)) len = sizeof(buf);
    uart_printf("%d \r\n", len);

    // len += snprintf(buf, "abc", 3);
    // uart_printf("looo");

    


    ins.api.lora_send_packet(&ins, (uint8_t *)buf, len);
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_Delay(2000);
    
    
    
    }
    /* USER CODE END 3 */
}




void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

    /*Configure GPIO pin : PC13 */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);



    //  GPIO_InitStruct = {0};


    /* SPI1 Master pins: PA5=SCK, PA7=MOSI, PA6=MISO */
    //  GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    // GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    // HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // GPIO_InitStruct.Pin = GPIO_PIN_6;
    // GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    // GPIO_InitStruct.Pull = GPIO_NOPULL;
    // HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // // CS: PA4
    // GPIO_InitStruct.Pin = GPIO_PIN_4;
    // GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    // HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

    // SCK = PA5

   
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

