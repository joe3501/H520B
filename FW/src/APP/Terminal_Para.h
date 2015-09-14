/**
*  @file 	Terminal_Para.h
*  @brief  	定义了终端需要保存在NVM区域的参数的数据结构
*  @note	
*/

#ifndef _TERMINAL_PARA_H_
#define _TERMINAL_PARA_H_


/**
*@brief 定义存储在SPI FLASH内部终端参数的结构类型
*@ note	
*/
#pragma pack(1)

typedef struct  {
	unsigned int			checkvalue;					//4字节		0	B	此份结构的校验值 crc32			
	unsigned char			struct_ver;					//1字节		4	B	参数版本号，标识此结构的版本
	unsigned char			ios_softkeypad_enable;		//1字节		5	B	是否使能IOS系统的软键盘
	unsigned char			motor_enable;				//1字节		6	B	是否使能振动器
	unsigned char			beep_volume;				//1字节		7	B	蜂鸣器音量
	unsigned char			power_save_mode;			//1字节		8	B	是否需要进入低功耗模式
	unsigned char			lower_power_timeout;		//1字节		9	B	进入低功耗模式的超时时间
	unsigned char			rfu[20];					//20字节	10	B	RFU
	unsigned char			endtag[2];					//0x55,0xAA  30      一共32字节
} TTerminalPara;
#pragma pack()

extern TTerminalPara		g_param;				//终端参数

int ReadTerminalPara(void);
int SaveTerminalPara(void);
int DefaultTerminalPara(void);
#endif
