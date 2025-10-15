#include "sx1278.h"

static SPI_HandleTypeDef hspi1;

static void SPI_transmit(uint8_t txData[], size_t len){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    delay_ms(1);
    HAL_SPI_Transmit(&hspi1, txData, len, 100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

static void LoRa_Write_Reg(uint8_t reg, uint8_t val){
    uint8_t out[2] = { 0x80 | reg, val };
   //  uart_printf_tag(SPI_TAG, "out[0] = %02X, out[1] = %02X\r\n", out[0], out[1]);
    SPI_transmit(out, sizeof(out));
}
static uint8_t LoRa_Read_Reg(uint8_t reg){
    uint8_t out[2] = { (uint8_t)reg & 0x7F, 0x00 };
    uint8_t in[2] = {0};
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); 
    HAL_SPI_TransmitReceive(&hspi1, out, in, 2, 100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);  

    return in[1];
}

static void LoRa_Reset(void){
   
    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, 0);
    delay_ms(1);
    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, 1);
    delay_ms(10);

}
static void LoRa_Explicit_Header_Mode(LoRa *lr){
    lr->implicit = 0;
    lr->api.lora_write_reg(REG_MODEM_LORA_1, lr->api.lora_read_reg(REG_MODEM_LORA_1) & 0xfe);
}
static void Lora_Implicit_Header_Mode(LoRa *lr, uint32_t size)
{
   lr->implicit = 1;
   lr->api.lora_write_reg(REG_MODEM_LORA_1, lr->api.lora_read_reg(REG_MODEM_LORA_1) | 0x01);
   lr->api.lora_write_reg(REG_PAYLOAD_LENGTH, size);
}
static void LoRa_Idle(LoRa *lr)
{
   lr->api.lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}
static void LoRa_Sleep(LoRa *lr)
{ 
   lr->api.lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

static void LoRa_Receive(LoRa *lr)
{
   lr->api.lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

static void LoRa_Set_TX_Power(LoRa *lr, uint8_t level)
{
   if (level < 2) level = 2;
   else if (level > 17) level = 17;
   lr->api.lora_write_reg(REG_PA_CONFIG, PA_BOOST | (level - 2));
}
static void LoRa_Set_Frequency(LoRa *lr, long frequency)
{
   lr->freq = frequency;

   uint64_t frf = ((uint64_t)frequency << 19) / 32000000;

   lr->api.lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
   lr->api.lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
   lr->api.lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
}
static void LoRa_Set_Spreading_Factor(LoRa *lr, uint8_t sf)
{
   if (sf < 6) sf = 6;
   else if (sf > 12) sf = 12;

   if (sf == 6) {
      lr->api.lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc5);
      lr->api.lora_write_reg(REG_DETECTION_THRESHOLD, 0x0c);
   } else {
      lr->api.lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc3);
      lr->api.lora_write_reg(REG_DETECTION_THRESHOLD, 0x0a);
   }

   lr->api.lora_write_reg(REG_MODEM_LORA_2, (lr->api.lora_read_reg(REG_MODEM_LORA_2) & 0x0f) | ((sf << 4) & 0xf0));
}

static void LoRa_Set_Bandwidth(LoRa *lr, long sbw)
{
   int bw;

   if (sbw <= 7.8E3) bw = 0;
   else if (sbw <= 10.4E3) bw = 1;
   else if (sbw <= 15.6E3) bw = 2;
   else if (sbw <= 20.8E3) bw = 3;
   else if (sbw <= 31.25E3) bw = 4;
   else if (sbw <= 41.7E3) bw = 5;
   else if (sbw <= 62.5E3) bw = 6;
   else if (sbw <= 125E3) bw = 7;
   else if (sbw <= 250E3) bw = 8;
   else bw = 9;
   lr->api.lora_write_reg(REG_MODEM_LORA_1, (lr->api.lora_read_reg(REG_MODEM_LORA_1) & 0x0f) | (bw << 4));
   // lr->api.lora_write_reg(REG_MODEM_LORA_1, 0x72);

}

static void LoRa_Set_Coding_Rate(LoRa *lr, uint8_t denominator)
{
   if (denominator < 5) denominator = 5;
   else if (denominator > 8) denominator = 8;

   int cr = denominator - 4;
   lr->api.lora_write_reg(REG_MODEM_LORA_1, (lr->api.lora_read_reg(REG_MODEM_LORA_1) & 0xf1) | (cr << 1));
}
static void LoRa_Set_Preamble_Length(LoRa *lr, uint8_t length)
{
   lr->api.lora_write_reg(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
   lr->api.lora_write_reg(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}
static void LoRa_Set_Sync_Word(LoRa *lr, uint32_t sw)
{
   lr->api.lora_write_reg(REG_SYNC_WORD, sw);
}

static void LoRa_Enable_Crc(LoRa *lr)
{
   lr->api.lora_write_reg(REG_MODEM_LORA_2, lr->api.lora_read_reg(REG_MODEM_LORA_2) | 0x04);
      // lr->api.lora_write_reg(REG_MODEM_LORA_2, 0xC4);

}
static void Lora_Disable_Crc(LoRa *lr)
{
   lr->api.lora_write_reg(REG_MODEM_LORA_2, lr->api.lora_read_reg(REG_MODEM_LORA_2) & 0xfb);
}

static void LoRa_Init(LoRa *lr){
   uart_printf_tag(LORA_TAG, "lora init start\r\n");
   lr->api.lora_reset();
   lr->api.lora_sleep(lr);
   lr->api.lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0);
   lr->api.lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0);
   lr->api.lora_write_reg(REG_LNA, lr->api.lora_read_reg(REG_LNA) | 0x03);
   lr->api.lora_write_reg(REG_MODEM_LORA_3, 0x04);
   lr->api.lora_set_tx_power(lr, 17);

   lr->api.lora_idle(lr);

   uart_printf_tag(LORA_TAG, "lora init done\r\n");


}
static void LoRa_Send_Packet(LoRa *lr, uint8_t *buf, uint32_t size){
   lr->api.lora_idle(lr);
   lr->api.lora_write_reg(REG_FIFO_ADDR_PTR, 0);

   for(int i=0; i<size; i++) 
      lr->api.lora_write_reg(REG_FIFO, *buf++);
   
   lr->api.lora_write_reg(REG_PAYLOAD_LENGTH, size);
   
   /*
    * Start transmission and wait for conclusion.
    */
   lr->api.lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
   while((lr->api.lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0)
   delay_ms(2);

   lr->api.lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
}

static uint32_t LoRa_Receive_Packet(LoRa *lr, uint8_t *buf, uint32_t size){
    uint32_t len = 0;

   /*
    * Check interrupts.
    */
   int irq = lr->api.lora_read_reg(REG_IRQ_FLAGS);
   lr->api.lora_write_reg(REG_IRQ_FLAGS, irq);
   if((irq & IRQ_RX_DONE_MASK) == 0) return 0;
   if(irq & IRQ_PAYLOAD_CRC_ERROR_MASK) return 0;

   /*
    * Find packet size.
    */
   if (lr->implicit) len = lr->api.lora_read_reg(REG_PAYLOAD_LENGTH);
   else len = lr->api.lora_read_reg(REG_RX_NB_BYTES);

   /*
    * Transfer data from radio.
    */
   lr->api.lora_idle(lr);   
   lr->api.lora_write_reg(REG_FIFO_ADDR_PTR, lr->api.lora_read_reg(REG_FIFO_RX_CURRENT_ADDR));
   if(len > size) len = size;
   for(int i=0; i<len; i++) 
      *buf++ = lr->api.lora_read_reg(REG_FIFO);

   return len;
}
static Status_e LoRa_Received(LoRa *lr)
{
   if(lr->api.lora_read_reg(REG_IRQ_FLAGS) & IRQ_RX_DONE_MASK) return OK;
   return ERR;
}
static uint32_t Lora_Packet_Rssi(LoRa *lr)
{
   return (lr->api.lora_read_reg(REG_PKT_RSSI_VALUE) - (lr->freq < 868E6 ? 164 : 157));
}

static float LoRa_Packet_Snr(LoRa *lr)
{
   return ((int8_t)lr->api.lora_read_reg(REG_PKT_SNR_VALUE)) * 0.25;
}
static void LoRa_Dump_Register(LoRa *lr){
   int i;
   uart_printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
   for(i=0; i<0x40; i++) {
      uart_printf("%02X ", lr->api.lora_read_reg(i));
      if((i & 0x0f) == 0x0f) uart_printf("\n");
   }
   uart_printf("\n");
}
LoRa SX1278_Init(void){

   __HAL_RCC_SPI1_CLK_ENABLE();
   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();


    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* SPI1 Master pins: PA5=SCK, PA7=MOSI, PA6=MISO */
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // CS: PA4
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = LORA_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LORA_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, GPIO_PIN_SET);


    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;

    HAL_SPI_Init(&hspi1);

    LoRa ins;
    ins.implicit = 1;
    ins.api.lora_write_reg = LoRa_Write_Reg;
    ins.api.lora_read_reg = LoRa_Read_Reg;
    ins.api.lora_reset = LoRa_Reset;
    ins.api.lora_explicit_header_mode = LoRa_Explicit_Header_Mode;
    ins.api.lora_implicit_header_mode = Lora_Implicit_Header_Mode;
    ins.api.lora_idle = LoRa_Idle;
    ins.api.lora_sleep = LoRa_Sleep;
    ins.api.lora_receive = LoRa_Receive;
    ins.api.lora_set_tx_power = LoRa_Set_TX_Power;
    ins.api.lora_set_frequency = LoRa_Set_Frequency;
    ins.api.lora_set_spreading_factor = LoRa_Set_Spreading_Factor;
    ins.api.lora_set_bandwidth = LoRa_Set_Bandwidth;
    ins.api.lora_set_coding_rate = LoRa_Set_Coding_Rate;
    ins.api.lora_set_preamble_length = LoRa_Set_Preamble_Length;
    ins.api.lora_set_sync_word = LoRa_Set_Sync_Word;
    ins.api.lora_enable_crc = LoRa_Enable_Crc;
    ins.api.lora_disable_crc = Lora_Disable_Crc;
    ins.api.lora_init = LoRa_Init;
    ins.api.lora_send_packet = LoRa_Send_Packet;
    ins.api.lora_receive_packet = LoRa_Receive_Packet;
    ins.api.lora_receive = LoRa_Receive;
    ins.api.lora_packet_rssi = Lora_Packet_Rssi;
    ins.api.lora_dump_registers = LoRa_Dump_Register;


    ins.api.lora_init(&ins);


    return ins;
     
}