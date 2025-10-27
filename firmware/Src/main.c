#include "main.h"
#include "stm32f1xx_hal_rcc_ex.h"
#include "stm32f1xx_hal_spi.h"
#include "stm32f1xx_hal.h"
#include "lora_packet.h"
#include <stdlib.h>
#include "stm32f1xx_hal_rtc.h"
#include "stm32f1xx_hal_tim.h"
#include "stm32f1xx_hal_rtc_ex.h"

uint16_t seq16_random(void) {
    uint32_t tick = HAL_GetTick();   
    tick ^= (tick << 11);
    tick ^= (tick >> 7);
    tick ^= (tick << 3);

    return (uint16_t)(tick & 0xFFFF);
}
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

SDS011 sds;
UART_HandleTypeDef huart2;
DHT dht;
uint8_t data[50];
uint16_t size = 0;
extern UART_HandleTypeDef huart1;
uint8_t rxData[4] = {0};
volatile uint8_t rx_ready = 0;
float ppm = 0.0f;
uint8_t uvi = 0.0f;
uint8_t saved_gwid[6] = {0}; 
extern volatile uint8_t s_co_alarm;
extern volatile uint8_t awd_event;

#define DEVICE_ID 0x111

#define LORA_DIO0_PORT   GPIOB
#define LORA_DIO0_PIN    GPIO_PIN_11   
volatile uint8_t g_lora_rx_irq = 0;

TIM_HandleTypeDef htim3;
static volatile uint8_t buzzer_blink_enabled = 0;
static void MX_TIM3_Init(void);


static void LoRa_DIO0_EXTI_Init(void);
static void Buzzer_Init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();   

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_10;        
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP; 
    GPIO_InitStruct.Pull  = GPIO_NOPULL;       
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; 

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);

}
static volatile bool g_buzzer_muted = false;   

static inline void Buzzer_MuteOn(void)  { g_buzzer_muted = true; }
static inline void Buzzer_MuteOff(void) { g_buzzer_muted = false;              }


bool lora_send_one_cycle(uint8_t buzzer_state);

bool lora_send_data(uint8_t buzzer_state);

typedef enum {
    WAKE_BOOT = 0,
    WAKE_RTC,
    WAKE_AWD
} wake_reason_t;

#define LORA_RX_TIMEOUT_MS     10000U  
#define MAX_RETRY              10U      
#define SLEEP_TOTAL            1* 60
#define SLEEP_CHECK_INTERVAL   5000U    


#define LATITUDE1   21.028511f   // Hồ Hoàn Kiếm
#define LONGITUDE1  105.804817f

static Status_e read_all_now(void) {
    Status_e stt;
    stt = MQ7_GetPPM(&ppm);
    if(stt != OK) return stt;
    stt = GUVA_GetUVI(&uvi);
    if(stt != OK) return stt;
    stt = dht.api.read_data(&dht);
    if(stt != OK) return stt;
    stt = sds.api.query_data(&sds);
    return stt;
}
UART_Config cfg1 = {
            .port = UART1,       
            .baudrate = 115200  
};    
UART_Config cfg2 = {
            .port = UART2,       
            .baudrate = 9600  
    };
LoRa ins;
static uint16_t fill_sensor_payload(uint8_t *pl, uint16_t cap, uint8_t buzzer)
{
    if (cap < (4*4 + 2*2)) return 0; 

    uint16_t o = 0;
    write_f32_be(&pl[o], dht.temperature); o += 4;
    write_f32_be(&pl[o], dht.humidity);    o += 4;
    write_f32_be(&pl[o], ppm);             o += 4;
    pl[o]= uvi;            o += 1;
    write_u16_be(&pl[o], (uint16_t)sds.pm_2_5); o += 2;
    write_u16_be(&pl[o], (uint16_t)sds.pm_10);  o += 2;
    pl[o] = buzzer; o += 1;
    write_f32_be(&pl[o], LATITUDE1);  o += 4;
    write_f32_be(&pl[o], LONGITUDE1); o += 4;

    return o; 
}


static void lora_process_rx_once(void)
{
    if (!g_lora_rx_irq) return;      
    g_lora_rx_irq = 0;

    uint8_t rx[256];
    lora_header_t rxh;

    int rlen = ins.api.lora_receive_packet(&ins, rx, sizeof(rx));
    if (rlen < LORA_HEADER_LEN) {
        ins.api.lora_receive(&ins);
        return;
    }

    if (!lora_parse_header(rx, (uint16_t)rlen, &rxh)) {
        ins.api.lora_receive(&ins);
        return;
    }

    if (rxh.msg_type == MSG_NODE_CTR &&
        rxh.device_id == DEVICE_ID &&
        rxh.ack == 1)
    {
        Buzzer_MuteOn();
        uart_print("[DOWNLINK] CTRL: BUZZER MUTE\r\n");

        uint8_t tx[LORA_HEADER_LEN];
        lora_header_t ack = rxh;
        ack.msg_type = MSG_CTR_ACK;
        ack.ack      = 1;

        uint16_t n = lora_write_header_only(tx, sizeof(tx), &ack);
        if (n > 0) {
            ins.api.lora_send_packet(&ins, tx, n);
            uart_print("[UPLINK] MSG_CTR_ACK sent\r\n");
        } else {
            uart_print("[UPLINK] MSG_CTR_ACK build FAIL\r\n");
        }
    }

    ins.api.lora_receive(&ins);
}

RTC_HandleTypeDef hrtc;

volatile uint16_t rtc_sec_accum = 0;
volatile uint8_t  flag_15min = 0;
void MX_RTC_Init(void)
{
    __HAL_RCC_BKP_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    __HAL_RCC_LSE_CONFIG(RCC_LSE_ON);
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET);

    __HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSE);
    __HAL_RCC_RTC_ENABLE();

    hrtc.Instance = RTC;
    hrtc.Init.AsynchPrediv = 32767; 
    hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;

    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_EnableIRQ(RTC_IRQn);
    HAL_NVIC_SetPriority(RTC_IRQn, 2, 0);
}
void RTC_IRQHandler(void)
{
    HAL_RTCEx_RTCIRQHandler(&hrtc);     
}

void HAL_RTCEx_RTCEventCallback(RTC_HandleTypeDef *hrtc)
{
    if (++rtc_sec_accum >= SLEEP_TOTAL) {        
        rtc_sec_accum = 0;
        flag_15min = 1;                  
    }
}
static void Periph_StopForSleep(void)
{
    __HAL_RCC_USART1_CLK_DISABLE();
    __HAL_RCC_USART2_CLK_DISABLE();
    __HAL_RCC_GPIOB_CLK_DISABLE();
    __HAL_RCC_SPI1_CLK_DISABLE();
    __HAL_RCC_I2C1_CLK_DISABLE();
    __HAL_RCC_DMA1_CLK_DISABLE();
    __HAL_RCC_TIM3_CLK_DISABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    NVIC_ClearPendingIRQ(ADC1_2_IRQn);
    HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
}

static void Periph_RestoreAfterWake(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();


}
static wake_reason_t  Sleep_ADC_AWD_or_15min(void)
{

    Periph_StopForSleep();            
    HAL_SuspendTick();
    HAL_RTCEx_SetSecond_IT(&hrtc);               
    HAL_NVIC_SetPriority(RTC_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);
    wake_reason_t reason = WAKE_RTC;
    while(1){
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
        if (awd_event) { reason = WAKE_AWD; break; }
        if (flag_15min) { reason = WAKE_RTC; break; }   
     }
    HAL_RTCEx_DeactivateSecond(&hrtc); 

    HAL_ResumeTick();
    Periph_RestoreAfterWake();

    rtc_sec_accum = 0;
    if(flag_15min == 1){
            uart_print("Wake by RTC\r\n");
            flag_15min = 0;
     }
    if (awd_event) uart_print("Wake by CO AWD\r\n");
    MQ7_DebugADC();
    float ppm = 0;
MQ7_GetPPM(&ppm);
    uart_printf("current ppm %f\r\n", ppm);

    if(awd_event && !s_co_alarm){
        MQ7_ReCallPPM();
    }
    return reason;

  
}

static void Buzzer_BlinkStart(void)
{
    buzzer_blink_enabled = 1;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    HAL_TIM_Base_Start_IT(&htim3);
}

static void Buzzer_BlinkStop(void)
{
    buzzer_blink_enabled = 0;
    HAL_TIM_Base_Stop_IT(&htim3);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
}
int main(void)
{
   
    HAL_Init();
    MX_RTC_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    LoRa_DIO0_EXTI_Init();

    MX_TIM3_Init();

    delay_init();
    Buzzer_Init();


    uart1_init(115200);

   
    uart_print("program start\r\n");
    
    uart_print("sds init\r\n");

    sds = SDS_Init(cfg2);

    uart_print("dht init\r\n");

    dht = DHT_Init(GPIOA, GPIO_PIN_1);
    uart_print("mq7 init\r\n");

    MQ7_Init();
    MQ7_Calibrate();

    MQ7_AWD_SetByPPM(CO_ALARM_PPM_THRESHOLD);

    uart_print("guva init\r\n");
    GUVA_Init();


    ins = SX1278_Init();
    ins.api.lora_set_frequency(&ins, 433E6);
    ins.api.lora_set_spreading_factor(&ins, 12);
    ins.api.lora_set_bandwidth(&ins, 125E3);
    ins.api.lora_enable_crc(&ins);

    s_co_alarm = 0;
    awd_event = 0;

    uint32_t last_alert_ms= 0;
    bool flag_when_alert = false;
    bool ok = false;
    MQ7_ENABLE_WDG_ITR();
    wake_reason_t wake_reason = WAKE_BOOT;
    while (1)
    {
        if(wake_reason == WAKE_BOOT || wake_reason == WAKE_RTC){
            uart_print("\r\n=== SEND ===\r\n");
            ok = lora_send_data(0);
        }
       
        if(s_co_alarm){
            MQ7_DebugADC();
            Buzzer_BlinkStart();        
        
        }
        while(s_co_alarm) {
            if(!flag_when_alert){
                uart_print("send data alert C0 higher than thr\r\n");
                flag_when_alert = lora_send_data(1);
                    if(!flag_when_alert){
                        uart_print("All retries exhausted \r\n");
                        HAL_Delay(5000);
                        continue;
                    }
                    last_alert_ms = HAL_GetTick(); 
            }
          
            // send success
            float ppm = 0;
            MQ7_GetPPM(&ppm);
            if(ppm < CO_ALARM_PPM_THRESHOLD - 30){
                uart_print("send data when co < thr\r\n");
                bool flag = false;
                flag = lora_send_data(0);
                if(!flag){
                    uart_print("All retries exhausted\r\n");
                    HAL_Delay(5000);
                    continue;
                }else {
                    last_alert_ms = HAL_GetTick(); 
                    uart_print("co is lower than thr and data will be sent \r\n");      
                     __disable_irq();              
                     s_co_alarm = 0;
                     flag_when_alert = false;
                    __enable_irq();
                    MQ7_ENABLE_WDG_ITR();
                    Buzzer_BlinkStop();
                    Buzzer_MuteOff();

                    break;
                }
            }

            if ((int32_t)(HAL_GetTick() - last_alert_ms) >= 300000) { 
                uart_print("periodic alert resend (5 min)\r\n");
                bool ok_periodic = lora_send_data(1);
                if (!ok_periodic) {
                    uart_print("Periodic alert resend failed\r\n");
                } else {
                    uart_print("Periodic alert resend OK\r\n");
                }
                last_alert_ms = HAL_GetTick();  
            }    
            lora_process_rx_once();
        }

    

        uart_print("Send data OK → entering SLEEP\r\n");
        wake_reason = Sleep_ADC_AWD_or_15min();  
        HAL_Delay(1000);
        
   
          

        
    
    
    }
    /* USER CODE END 3 */
}

bool lora_send_data(uint8_t buzzer_state){
    bool ok = false;
    for (uint8_t attempt = 1; attempt <= MAX_RETRY; ++attempt) {
            uint16_t seq = seq16_random();
            uint32_t backoff_ms = 200 + (seq & 0x1FF); 
            HAL_Delay(backoff_ms);

            ok = lora_send_one_cycle(buzzer_state);
            if (ok) {
                uart_print("Send data OK\r\n");
                break;
            } else {
                char buf[48];
                sprintf(buf, "Attempt %u/%u failed\r\n", attempt, MAX_RETRY);
                uart_print(buf);
            }
        }
    return ok;
}

bool lora_send_one_cycle(uint8_t buzzer_state){
    uint8_t rx[256];
    uint8_t payload[64];
    uint8_t pkt[128];
    uint8_t tx[32];
    
    uint16_t seq_hello = seq16_random();
    uint16_t len = lora_pkt_build_hello(tx, sizeof(tx), DEVICE_ID, ZERO_GWID, seq_hello, 0);
    if (len == 0) {
        uart_print("build HELLO failed\r\n");
        return false;
    }
    ins.api.lora_send_packet(&ins, tx, len);
    uart_print("send HELLO\r\n");
    bool got_resp = false;
    lora_header_t hdr;
    ins.api.lora_receive(&ins);
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < LORA_RX_TIMEOUT_MS) {
        if (ins.api.lora_received(&ins)) {
            int rlen = ins.api.lora_receive_packet(&ins, rx, sizeof(rx));
            if (rlen >= LORA_HEADER_LEN && lora_parse_header(rx, (uint16_t)rlen, &hdr)) {
                if (hdr.msg_type == MSG_HELLO_RESP &&
                    hdr.device_id == DEVICE_ID &&
                    hdr.seq16 == seq_hello && hdr.ack) {
                    memcpy(saved_gwid, hdr.gateway_id, 6);
                    got_resp = true;
                    break;
                }
            }
        }
        HAL_Delay(10);
    }
    if (!got_resp) {
        uart_print("HELLO timeout\r\n");
        return false;
    }
    Status_e stt;
    stt = read_all_now();
    if(stt != OK){
        uart_print("read data failed\r\n");
        return false;
    }
    uint16_t pl_len = fill_sensor_payload(payload, sizeof(payload), buzzer_state);
    if (pl_len == 0) {
        uart_print("payload too small\r\n");
        return false;
    }

    uint16_t seq_data = seq16_random();
    uint16_t pkt_len = build_data_packet(pkt, sizeof(pkt),
                                         DEVICE_ID, saved_gwid, seq_data,
                                         payload, pl_len);
    if (pkt_len == 0) {
        uart_print("build DATA failed\r\n");
        return false;
    }

    ins.api.lora_send_packet(&ins, pkt, pkt_len);
    uart_print("send DATA\r\n");

    // 6) wait DATA_ACK
    got_resp = false;
    ins.api.lora_receive(&ins);
    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < LORA_RX_TIMEOUT_MS) {
        if (ins.api.lora_received(&ins)) {
            int rlen = ins.api.lora_receive_packet(&ins, rx, sizeof(rx));
            if (rlen >= LORA_HEADER_LEN && lora_parse_header(rx, (uint16_t)rlen, &hdr)) {
                if (hdr.msg_type == MSG_DATA_ACK &&
                    hdr.device_id == DEVICE_ID &&
                    hdr.seq16 == seq_data && hdr.ack) {
                    got_resp = true;
                    break;
                }
            }
        }
        HAL_Delay(10);
    }

    if (!got_resp) {
        uart_print("DATA timeout\r\n");
        return false;
    }

    uart_print("DATA ack ok\r\n");
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); 
    return true;



}
void TIM3_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim3);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) {
        if (buzzer_blink_enabled && !g_buzzer_muted) {
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_10); 
        } else {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
        }
    }
}
void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(LORA_DIO0_PIN);
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == LORA_DIO0_PIN) {
        g_lora_rx_irq = 1;  
    }
}
 void LoRa_DIO0_EXTI_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE(); 

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = LORA_DIO0_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LORA_DIO0_PORT, &GPIO_InitStruct);

    uint32_t irq = EXTI15_10_IRQn;
    HAL_NVIC_SetPriority(irq, 4, 0);
    HAL_NVIC_EnableIRQ(irq);
}

static void MX_TIM3_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 7200 - 1;      
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 10000 - 1;        
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(TIM3_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
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



   
}


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
