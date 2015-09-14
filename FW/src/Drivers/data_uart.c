/**
 * @file data_uart.c
 * @brief 
 *			����1��������ϵͳ�ṩ���ⲿ�Ĵ��ڣ������׶ο������ڵ���
 * @version V0.0.1
 * @author zhongyh
 * @date 2009��12��28��
 * @note
 *
 * @copy
 *
 * �˴���Ϊ���ںϽܵ������޹�˾��Ŀ���룬�κ��˼���˾δ����ɲ��ø��ƴ�����������
 * ����˾�������Ŀ����˾����һ��׷��Ȩ����
 *
 * <h1><center>&copy; COPYRIGHT 2009 heroje</center></h1>
 */
/* Private include -----------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "data_uart.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/**
* @brief ��ʼ�����ݴ���
*/
void data_uart_init(void)
{
	USART_InitTypeDef						USART_InitStructure;
	GPIO_InitTypeDef						GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
	

//#ifndef BOOTCODE_EXIST		//���û����������	
	/* Configure USART1 Tx (PA.09) as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin				= GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode			= GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed			= GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* Configure USART1 Rx (PA.10) as input floating */
	GPIO_InitStructure.GPIO_Pin				= GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode			= GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* ���ô��ڲ���								*/
	USART_InitStructure.USART_BaudRate		= 115200;
	USART_InitStructure.USART_WordLength	= USART_WordLength_8b;
	USART_InitStructure.USART_StopBits		= USART_StopBits_1;
	USART_InitStructure.USART_Parity		= USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode			= USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_Cmd(USART1, ENABLE);	
}

/**
 * @brief	����һ���ֽ�
 */
void data_uart_sendbyte(unsigned char data)
{
	USART_SendData(USART1, (unsigned short)data);
}

/**
 * @brief	����һ���ֽ�
 */
unsigned char uart_rec_byte(void)
{
	int	i = 0;
	while((USART_GetFlagStatus(USART1,USART_FLAG_RXNE)== RESET)&&(i<400000))
	{
		i++;
	}
	if (i == 400000) 
	{
		return 0x55;
	}
	return  USART_ReceiveData(USART1) & 0xFF;              /* Read one byte from the receive data register         */
}

/**
* @brief ʵ�ִ˺�����������ϵͳ����printf,�������ʱ��ʽ���������Ϣ
*/
int fputc(int ch, FILE *f)
{
	//ENABLE_DATA_UART();
	/* Write a character to the USART */
	
	USART_SendData(USART1, (u8) ch);
	
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
	{
	}
	//	DISABLE_DATA_UART;
	return ch;        
}
