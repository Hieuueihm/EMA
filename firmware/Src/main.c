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
    // delay_init();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    delay_init();

    // huart1.Instance = USART1;
    // huart1.Init.BaudRate = 115200;          // tốc độ truyền
    // huart1.Init.WordLength = UART_WORDLENGTH_8B;
    // huart1.Init.StopBits = UART_STOPBITS_1;
    // huart1.Init.Parity = UART_PARITY_NONE;
    // huart1.Init.Mode = UART_MODE_TX_RX;     // cho phép truyền và nhận
    // huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    // huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    // if(HAL_UART_Init(&huart1) != HAL_OK)
    // {
    //     // Nếu init lỗi
    //     Error_Handler();
    // }


    uart1_init(115200);


    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    UART_Config cfg1 = {
            .port = UART1,       
            .baudrate = 115200  
    };    
    UART_Config cfg2 = {
            .port = UART2,       
            .baudrate = 9600  
    };    
    // huart = UART_Init(cfg1);
    // huart.api.send_string(&huart, "test\r\n");
    uart_print("test11\n");
    

    // sds = SDS_Init(cfg2);
    // __HAL_RCC_USART2_CLK_ENABLE();   
    //     __HAL_RCC_GPIOA_CLK_ENABLE();

    //     {
    //         GPIO_InitTypeDef GPIO_InitStruct = {0};

    //         // TX (PA2)
    //         GPIO_InitStruct.Pin = GPIO_PIN_2;
    //         GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    //         GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    //         HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    //         // RX (PA3)
    //         GPIO_InitStruct.Pin = GPIO_PIN_3;
    //         GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    //         GPIO_InitStruct.Pull = GPIO_NOPULL;
    //         HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    //     }
    // 	MX_USART2_UART_Init();
        
    //     	sdsInit(&sds, &huart2);
    // sds = SDS_Init(cfg2);



  


   
    // dht = DHT_Init(GPIOA, GPIO_PIN_4);




    // huart.api.send_string_wtimeout(&huart, "test1\r\n". 1000);
    char data1[64];
            // sds.api.query_data(&sds);

    // size += sprintf(data1 + size, "%d %d  \n ", sds.pm_2_5, sds.pm_10);
    // for(int i = 0; i < 10; i++) // 10 byte data_receive
    // {
    //     size += sprintf(data1 + size, "%02X ", sds.data_received[i]);
    // }
    // sprintf(data1 + size, "\n\r"); 
    // HAL_UART_Transmit(&huart.huart, (uint8_t*)data1, size, 1000);
	// HAL_Delay(1000);


    // Gửi dữ liệu


    // dht.api.read_data(&dht);

    // size += sprintf(data1 + size, "Temp: %.1f C  Humidity: %.1f %%\n", dht.temperature, dht.humidity);
    //     HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, 1);

    // // Gửi tất cả dữ liệu qua UART
    // HAL_UART_Transmit(&huart1, (uint8_t*)data1, size, 1000);

    // uart_printf((uint8_t*)data1);
    // Delay trước khi gửi tiếp
    // MQ7_Init();
    // MQ7_Calibrate();

    char msg[64];
    float ppm = 0;
    
//   HAL_SPI_Receive_IT(&hspi1, rxData, sizeof(rxData));


    LoRa ins;
    ins = SX1278_Init();
    while (1)
    {
            ins = SX1278_Init();

        

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

