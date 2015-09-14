#ifndef _HW_PLATFORM_H_
#define _HW_PLATFORM_H_

#include "keypad.h"
#include "spi_flash.h"
#include "HJ5000_scanner.h"
#include "uart_drv.h"
#include "WBTDS01.h"
#include "data_uart.h"

#define		LED_RED			1
#define		LED_GREEN		2
#define		LED_YELLOW		3

typedef enum{
	BEEP_FREQ_1KHZ=1,
	BEEP_FREQ_2KHZ,
	BEEP_FREQ_3KHZ,
	BEEP_FREQ_4KHZ,
	BEEP_FREQ_5KHZ
}BEEP_FREQ;

extern unsigned int	charge_detect_io_cnt;

void hw_platform_init(void);
unsigned int hw_platform_get_PowerClass(void);
unsigned int hw_platform_ChargeState_Detect(void);
unsigned int hw_platform_USBcable_Insert_Detect(void);
void hw_platform_led_ctrl(unsigned int led,unsigned int ctrl);
void hw_platform_motor_ctrl(unsigned short delay);
void hw_platform_trig_ctrl(unsigned short delay);
void hw_platform_beep_ctrl(unsigned short delay,BEEP_FREQ	beep_freq);

void hw_platform_start_led_blink(unsigned int led,unsigned short delay);
void hw_platform_stop_led_blink(void);
#endif
