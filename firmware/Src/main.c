#include "main.h"
#include "stm32f1xx_hal_rcc_ex.h"
#include "stm32f1xx_hal_spi.h"
#include "stm32f1xx_hal.h"
#include "lora_packet.h"
#include <stdlib.h>

uint16_t seq16_random(void) {
    return (uint16_t)(rand() & 0xFFFF);
}
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
extern UART_HandleTypeDef huart1;
uint8_t rxData[4] = {0};
volatile uint8_t rx_ready = 0;
float ppm = 0.0f;
uint8_t uvi = 0.0f;
uint8_t gwid_zero[6] = {0,0,0,0,0,0};


#define DEVICE_ID 0x111





static uint8_t read_all_sensors(const char * buf){

    MQ7_GetPPM(&ppm);
    GUVA_GetUVI(&uvi);
    dht.api.read_data(&dht);
    sds.api.query_data(&sds);
    uint8_t len = 0;

    len += sprintf(buf, "Temp: %.1f C  Humidity: %.1f ppm %.1f uv %d pm2.5 %d pm10 %d \r\n ", dht.temperature, dht.humidity, ppm, uvi, sds.pm_2_5, sds.pm_10);
    return len;
}

static uint16_t fill_sensor_payload(uint8_t *pl, uint16_t cap)
{
    if (cap < (4*4 + 2*2)) return 0; 

    uint16_t o = 0;
    write_f32_be(&pl[o], dht.temperature); o += 4;
    write_f32_be(&pl[o], dht.humidity);    o += 4;
    write_f32_be(&pl[o], ppm);             o += 4;
    pl[o]= uvi;            o += 1;
    write_u16_be(&pl[o], (uint16_t)sds.pm_2_5); o += 2;
    write_u16_be(&pl[o], (uint16_t)sds.pm_10);  o += 2;
    return o; 
}

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


    LoRa ins;
    ins = SX1278_Init();
    ins.api.lora_set_frequency(&ins, 433E6);
    ins.api.lora_set_spreading_factor(&ins, 12);
    ins.api.lora_set_bandwidth(&ins, 125E3);
    ins.api.lora_enable_crc(&ins);



    uint8_t saved_gwid[6] = {0}; 

    while (1)
    {
        uint8_t tx[32];
        uint8_t rx[256];
        uint8_t payload[64];
        uint8_t pkt[128];
        uint16_t seq16 = seq16_random();


        uint16_t len = lora_pkt_build_hello(tx, sizeof(tx), DEVICE_ID, gwid_zero, seq16 , 0);
            if (len == 0) {
                uart_print("build HELLO failed\r\n");
                HAL_Delay(2000);
                continue;
            }

        ins.api.lora_send_packet(&ins, tx, len);


        uart_print("send hello ok \r\n");


        uint32_t t0 = HAL_GetTick();
        bool got_resp = false;
        lora_header_t hdr;    
        uart_print("wait resp \r\n");
        ins.api.lora_receive(&ins); 
        
        while ((HAL_GetTick() - t0) < 5000)
        {
            if (ins.api.lora_received(&ins)) {
                int rlen = ins.api.lora_receive_packet(&ins, rx, sizeof(rx)); 

                if (rlen >= LORA_HEADER_LEN) {

                    if (lora_parse_header(rx, (uint16_t)rlen, &hdr)) {
                        if (hdr.msg_type == MSG_HELLO_RESP &&
                            hdr.device_id == DEVICE_ID &&
                            hdr.seq16 == seq16 && hdr.ack)
                        {
                            got_resp = true;
                            uart_print("resp okela\r\n");
                            break;
                        }
                    }
                }
            } 
            HAL_Delay(10);  

        }

        uart_print("break point \r\n");
        if (got_resp) {
            memcpy(saved_gwid, hdr.gateway_id, 6);

            char logbuf[96];
            sprintf(logbuf,
                    "HELLO_RESP ok: dev=0x%04X seq=0x%04X ack=%u gw=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                    hdr.device_id, hdr.seq16, hdr.ack,
                    saved_gwid[0], saved_gwid[1], saved_gwid[2],
                    saved_gwid[3], saved_gwid[4], saved_gwid[5]);
            uart_print(logbuf);
            char dbg[100];
            read_all_sensors(dbg);

            uint16_t pl_len = fill_sensor_payload(payload, sizeof(payload));

            uint16_t seq_data = seq16_random(); // hoặc ++
            uint16_t pkt_len = build_data_packet(pkt, sizeof(pkt),
                                        DEVICE_ID, saved_gwid, seq_data,
                                        payload, pl_len);

            ins.api.lora_send_packet(&ins, pkt, pkt_len);
            uart_print("send data okela\r\n");

            uint32_t t0 = HAL_GetTick();
            bool got_resp = false;
            lora_header_t hdr;    
            uart_print("wait resp \r\n");
            ins.api.lora_receive(&ins); 
            
            while ((HAL_GetTick() - t0) < 5000)
            {
                if (ins.api.lora_received(&ins)) {
                    int rlen = ins.api.lora_receive_packet(&ins, rx, sizeof(rx)); 
                    if (rlen >= LORA_HEADER_LEN) {

                        if (lora_parse_header(rx, (uint16_t)rlen, &hdr)) {
                            if (hdr.msg_type == MSG_DATA_ACK &&
                                hdr.device_id == DEVICE_ID &&
                                hdr.seq16 == seq_data && hdr.ack)
                            {
                                got_resp = true;
                                uart_print("resp data okela\r\n");
                                break;
                            }
                        }
                    }
                } 
                HAL_Delay(10);  

            }
            if(got_resp){

                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
                HAL_Delay(2000);
                got_resp = true;
            } else {
                uart_print("DATA timeout, retry...\r\n");
                uint32_t backoff = 500 + (seq16 & 0x3FF); 
                HAL_Delay(backoff);
            }





        } else {
            uart_print("HELLO timeout, retry...\r\n");
            uint32_t backoff = 500 + (seq16 & 0x3FF);
            HAL_Delay(backoff);
        }

    
    
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

