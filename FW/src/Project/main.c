/**
* @file main.c
* @brief 2.4GPOS��Ŀ������
*
* @version V0.0.1
* @author joe
* @date 2010��4��26��
* @note
*		none
*
* @copy
*
* �˴���Ϊ���ںϽܵ������޹�˾��Ŀ���룬�κ��˼���˾δ����ɲ��ø��ƴ�����������
* ����˾�������Ŀ����˾����һ��׷��Ȩ����
*
* <h1><center>&copy; COPYRIGHT 2015 heroje</center></h1>
*/

/* Private Includes ------------------------------------------------------------------*/
#include "stm32f10x_lib.h"
#include "ucos_ii.h"
#include <string.h>
#include <stdlib.h>
#include "app.h"
#include "hw_platform.h"
#include "data_uart.h"
#include "keypad.h"
#include "TimeBase.h"
#include "Terminal_para.h"
#include "record_m.h"
#include "stm32f10x_flash_config.h"
#include "scanner.h"
#include "usb_lib.h"
#include "PCUsart.h"
#include "JMemory.h"
#include "usb_pwr.h"
/* Private define ------------------------------------------------------------*/

// Cortex System Control register address
#define SCB_SysCtrl					((u32)0xE000ED10)
// SLEEPDEEP bit mask
#define SysCtrl_SLEEPDEEP_Set		((u32)0x00000004)



/* Global variables ---------------------------------------------------------*/
ErrorStatus			HSEStartUpStatus;							//Extern crystal OK Flag
unsigned int		pos_state;									//POS state flag
//unsigned char		factory_test_start_flag;


// 0x20000000-0x20000010 ���ֽ�Ϊ��������BootCode��Ĳ���������
__no_init unsigned int status_iap @ "VENDOR_RAM";
__no_init unsigned int nouse[3] @ "VENDOR_RAM";

/* Private functions ---------------------------------------------------------*/
static void Unconfigure_All(void);
static void GPIO_AllAinConfig(void);
void RCC_Configuration(void);

/* External variables -----------------------------------------------*/
extern	TTerminalPara			g_param;					//Terminal Param

/*******************************************************************************
* Function Name  : main
* Description    : Main program.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
int main(void)
{
	/* System Clocks Configuration **********************************************/
	RCC_Configuration(); 
//#ifdef RELEASE_VER
	/* NVIC Configuration *******************************************************/
//	NVIC_SetVectorTable(NVIC_VectTab_FLASH, IAP_SIZE);		//��Ҫ���ܵ� bootcode
//#else	
	/* NVIC Configuration *******************************************************/
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0);
//#endif
	
	// Clear SLEEPDEEP bit of Cortex System Control Register
	*(vu32 *) SCB_SysCtrl &= ~SysCtrl_SLEEPDEEP_Set;

	Unconfigure_All();
	

#ifdef DEBUG_VER
	// ���ݴ���(���Կ�)��ʼ��
	data_uart_init();

	printf("H520B startup...\r\n");
#endif

	//��ʼ��ʱ������
	TimeBase_Init();

	hw_platform_init();

	usb_device_init(USB_KEYBOARD);

	hw_platform_beep_ctrl(300,3000);
	hw_platform_beep_ctrl(300,2000);
	hw_platform_beep_ctrl(300,1500);
	hw_platform_beep_ctrl(300,4000);

	app_startup();
}

/*******************************************************************************
* Function Name  : RCC_Configuration
* Description    : Configures the different system clocks.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void RCC_Configuration(void)
{   
	vu32 i=0;

	/* RCC system reset(for debug purpose) */
	RCC_DeInit();

	/* Enable HSE							*/
	RCC_HSEConfig(RCC_HSE_ON);
	// ����Ҫ����ʱ�����ܼ���ĳЩ�Ƚϲ�ľ��壬�Ա�˳������	
	for(i=0; i<200000; i++);

	/* Wait till HSE is ready			*/
	HSEStartUpStatus = RCC_WaitForHSEStartUp();

	if(HSEStartUpStatus == SUCCESS)
	{
		/* Enable Prefetch Buffer		*/
		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);

		/* Flash 2 wait state			*/
		FLASH_SetLatency(FLASH_Latency_2);

		/* HCLK = SYSCLK					*/
		RCC_HCLKConfig(RCC_SYSCLK_Div1); 

		/* PCLK2 = HCLK					*/
		RCC_PCLK2Config(RCC_HCLK_Div1); 

		/* PCLK1 = HCLK/2					*/
		RCC_PCLK1Config(RCC_HCLK_Div2);

		/* PLLCLK = 12MHz * 6 = 72 MHz	*/
		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_6);

		/* PLLCLK = 8MHz * 9 = 72 MHz	*/
		//RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);

		/* Enable PLL						*/
		RCC_PLLCmd(ENABLE);

		/* Wait till PLL is ready		*/
		while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
		{
		}

		/* Select PLL as system clock source */
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

		/* Wait till PLL is used as system clock source */
		while(RCC_GetSYSCLKSource() != 0x08)
		{
		}
	}
}

/*******************************************************************************
* Function Name  : Unconfigure_All
* Description    : set all the RCC data to the default values 
*                  configure all the GPIO as input
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static void Unconfigure_All(void)
{
	//RCC_DeInit();

	/* RCC configuration */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ALL, DISABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_ALL, DISABLE);

	GPIO_AllAinConfig();
}


/*******************************************************************************
* Function Name  : GPIO_AllAinConfig
* Description    : Configure all GPIO port pins in Analog Input mode 
*                  (floating input trigger OFF)
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static void GPIO_AllAinConfig(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Configure all GPIO port pins in Analog Input mode (floating input trigger OFF) */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	//GPIO_Init(GPIOD, &GPIO_InitStructure);
	//GPIO_Init(GPIOE, &GPIO_InitStructure);
}

/************************************************
* Function Name  : EnterLowPowerMode()
************************************************/
void EnterLowPowerMode(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
#if(BT_MODULE == USE_BT816)
	BT816_hid_disconnect();
	BT816_enter_sleep();
#endif
	stop_real_timer();
	hw_platform_led_ctrl(LED_RED,0);
	hw_platform_led_ctrl(LED_BLUE,0);
	hw_platform_led_ctrl(LED_GREEN,0);
	hw_platform_led_ctrl(LED_YELLOW,0);

	//�ȹر����������ʱ��
	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_ALL, DISABLE);
	//RCC_APB1PeriphClockCmd(RCC_APB1Periph_ALL, DISABLE);
	//RCC_AHBPeriphClockCmd(RCC_AHBPeriph_ALL, DISABLE);

	//������Ҫ����PWRģ���ʱ��
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // Enable PWR clock

	// enable Debug in Stop mode
	//DBGMCU->CR |= DBGMCU_CR_DBG_STOP;
	
	//����͹���ģʽ
	EXTI_ClearFlag(0xffff);
	PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFI);
}

/************************************************
* Function Name  : ExitLowPowerMode()
************************************************/
void ExitLowPowerMode(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	//��������ʱ��
	RCC_Configuration();

#if(BT_MODULE == USE_BT816)
	BT816_wakeup();
#endif
	start_real_timer();
	scanner_mod_init();
}


/*******************************************************************************
* Function Name  : assert_failed
* Description    : Reports the name of the source file and the source line number
*                  where the assert_param error has occurred.
* Input          : - file: pointer to the source file name
*                  - line: assert_param error line source number
* Output         : None
* Return         : None
*******************************************************************************/

//void assert_failed(u8* file, u32 line)
//{ 
/* User can add his own implementation to report the file name and line number,
ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

/* Infinite loop */
//while (1)
//{
//}
//}
/******************* (C) COPYRIGHT 2008 STMicroelectronics *****END OF FILE****/

