#ifndef _HJ5000_SCANNER_H_
#define _HJ5000_SCANNER_H_
#include "stm32f10x_lib.h"

#define SCANNER_TRIG_HW		//Ó²¼þIO´¥·¢

#define SCANNER_TRIG_ON()	GPIO_ResetBits(GPIOA,GPIO_Pin_10);	
#define SCANNER_TRIG_OFF()	GPIO_SetBits(GPIOA,GPIO_Pin_10);	

void reset_resVar(void);
int HJ5000_RxISRHandler(unsigned char c);
void scanner_mod_init(void);
void scanner_mod_reset(void);
int scanner_get_barcode(unsigned char *barcode,unsigned int max_num,unsigned char *barcode_type,unsigned int *barcode_len);
#endif