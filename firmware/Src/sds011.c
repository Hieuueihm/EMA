#include "sds011.h"
uint8_t Sds011_Query[]={
0xAA, 0xB4, 0x04, 0x00, 0x00, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0xFF, 0xFF, 0x02 , 0xAB
};
uint8_t Sds011_SleepCommand[] = {
		0xAA,
		0xB4,
		0x06,
		0x01,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0xFF,
		0xFF,
		0x05,
		0xAB
};


uint8_t Sds011_WorkingMode[] = {
		0xAA,
		0xB4,
		0x06,
		0x01,
		0x01,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0xFF,
		0xFF,
		0x06,
		0xAB
};
static Status_e Send_Command(SDS011 *sds, const uint8_t * data_buffer){
    return sds->sds_uart.api.send_string_wtimeout(&sds->sds_uart,  (const char*)data_buffer, 1000) == OK ? OK : TIMEOUT;
}
static uint16_t SDS_GetPm2_5(SDS011* sds)
{
	return  sds->pm_2_5;
}
static uint16_t SDS_GetPm10(SDS011* sds)
{
	return  sds->pm_10;
}
static Status_e Set_WorkingMode(SDS011 *sds){
    return sds->sds_uart.api.send_string_wtimeout(&sds->sds_uart,  (const char*)Sds011_WorkingMode, 30) == OK ? OK:TIMEOUT;
    
}
static Status_e Set_SleepMode(SDS011 *sds){
    return sds->sds_uart.api.send_string_wtimeout(&sds->sds_uart,  (const char*)Sds011_SleepCommand, 30) == OK ? OK:TIMEOUT;
}



static Status_e Sds_QueryData(SDS011 *sds)
{
    Status_e stt = sds->sds_uart.api.send_string_wtimeout(&sds->sds_uart,  (const char*)Sds011_Query, 1000);
    if (stt != OK)
        return TIMEOUT;

    uint8_t buf[10];
    uint8_t byte;
    int len = 0;
    uint8_t checksum = 0;

    while (len < 10) 
    {
        while (sds->sds_uart.api.available(&sds->sds_uart) > 0)
        {
            sds->sds_uart.api.read_byte(&sds->sds_uart, &byte);

            switch (len)
            {
            case 0:
                if (byte != 0xAA) {
                    len = 0; 
                    continue;
                }
                break;

            case 1:
                if (byte != 0xC0) {
                    len = 0; 
                    continue;
                }
                break;
            }

            buf[len++] = byte;

            if (len == 10)
            {
                if (buf[9] != 0xAB) {
                    len = 0;
                    continue;
                }

                checksum = 0;
                for (int i = 2; i <= 7; i++)
                    checksum += buf[i];

                if (checksum != buf[8]) {
                    len = 0;
                    continue;
                }

                memcpy(sds->data_received, buf, 10);
                uint16_t pm25_raw = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
                uint16_t pm10_raw = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);

                sds->pm_2_5 = pm25_raw / 10;
                sds->pm_10  = pm10_raw / 10;
                return OK;
            }
        }
    }

    return TIMEOUT;
}



SDS011 SDS_Init(UART_Config cfg){
    SDS011 sds_ins;
    sds_ins.sds_uart = UART_Init(cfg);;
    sds_ins.api.send = Send_Command;
    sds_ins.api.get_pm25 = SDS_GetPm2_5;
    sds_ins.api.get_pm10= SDS_GetPm10;
    sds_ins.api.set_working_mode = Set_WorkingMode;
    sds_ins.api.set_sleep_mode = Set_SleepMode;
    sds_ins.api.query_data = Sds_QueryData;

    Set_WorkingMode(&sds_ins);
    // Sds_QueryData(&sds_ins);
    // HAL_UART_Receive(&sds_ins.sds_uart.huart, sds_ins.data_received, 10, 1000);

    return sds_ins;
}

