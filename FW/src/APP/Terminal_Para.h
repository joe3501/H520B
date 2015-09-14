/**
*  @file 	Terminal_Para.h
*  @brief  	�������ն���Ҫ������NVM����Ĳ��������ݽṹ
*  @note	
*/

#ifndef _TERMINAL_PARA_H_
#define _TERMINAL_PARA_H_


/**
*@brief ����洢��SPI FLASH�ڲ��ն˲����Ľṹ����
*@ note	
*/
#pragma pack(1)

typedef struct  {
	unsigned int			checkvalue;					//4�ֽ�		0	B	�˷ݽṹ��У��ֵ crc32			
	unsigned char			struct_ver;					//1�ֽ�		4	B	�����汾�ţ���ʶ�˽ṹ�İ汾
	unsigned char			ios_softkeypad_enable;		//1�ֽ�		5	B	�Ƿ�ʹ��IOSϵͳ�������
	unsigned char			motor_enable;				//1�ֽ�		6	B	�Ƿ�ʹ������
	unsigned char			beep_volume;				//1�ֽ�		7	B	����������
	unsigned char			power_save_mode;			//1�ֽ�		8	B	�Ƿ���Ҫ����͹���ģʽ
	unsigned char			lower_power_timeout;		//1�ֽ�		9	B	����͹���ģʽ�ĳ�ʱʱ��
	unsigned char			rfu[20];					//20�ֽ�	10	B	RFU
	unsigned char			endtag[2];					//0x55,0xAA  30      һ��32�ֽ�
} TTerminalPara;
#pragma pack()

extern TTerminalPara		g_param;				//�ն˲���

int ReadTerminalPara(void);
int SaveTerminalPara(void);
int DefaultTerminalPara(void);
#endif
