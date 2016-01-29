/**
* @file keypad.c
* @brief ������������
* @version V0.0.1
* @author joe.zhou
* @date 2015��08��28��
* @note
* @copy
*
* �˴���Ϊ���ںϽܵ������޹�˾��Ŀ���룬�κ��˼���˾δ����ɲ��ø��ƴ�����������
* ����˾�������Ŀ����˾����һ��׷��Ȩ����
*
* <h1><center>&copy; COPYRIGHT 2015 heroje</center></h1>
*
*/
#include "stm32f10x_lib.h"
#include "ucos_ii.h"
#include <stdio.h>
#include "keypad.h"
#include "cpu.h"
#include "app.h"
#include "TimeBase.h"
#include "hw_platform.h"
#include "HJ5000_scanner.h"

static  unsigned char		current_press_key;
static  unsigned int		press_cnt;
static  unsigned int		release_cnt; 
static  unsigned char		keypad_state;

unsigned char	scan_key_trig;


extern	unsigned int	device_current_state;		//�豸��״̬��
extern	OS_EVENT		*pEvent_Queue;			//�¼���Ϣ����
extern unsigned int		keypress_timeout;
/**
* @brief   		Intialize the KeyBoard IO
* @param[in]   none
* @return      none
* @note   
*		������Ӳ����������ͼ��ʾ��
*		ScanKey -- PA0
*		EraseKey -- PB3
*/
static void Keypad_Initport(void)
{
	GPIO_InitTypeDef  gpio_init;
	EXTI_InitTypeDef EXTI_InitStructure;    
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

	//ScanKey  PA.0
	gpio_init.GPIO_Pin   = GPIO_Pin_0;
	gpio_init.GPIO_Speed = GPIO_Speed_10MHz;
	gpio_init.GPIO_Mode  = GPIO_Mode_IPU;
	GPIO_Init(GPIOA, &gpio_init);		

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

	//EraseKey PB3
	gpio_init.GPIO_Pin   = GPIO_Pin_3;
	GPIO_Init(GPIOB, &gpio_init);

	/* Connect EXTI Line0 to PA0 */
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
	/* Connect EXTI Line3 to PB3 */
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource3);

	/* Configure EXTI LineX to generate an interrupt on falling edge */
	EXTI_ClearITPendingBit(EXTI_Line0| EXTI_Line3);
	EXTI_InitStructure.EXTI_Line = EXTI_Line0 | EXTI_Line3;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);
	EXTI_GenerateSWInterrupt(EXTI_Line0 | EXTI_Line3);
	scan_key_trig = 0;
}

/**
* @brief   	Enable the keypad interrupt
* @return      none
*/
void Keypad_Int_Enable(void)
{
	NVIC_InitTypeDef	NVIC_InitStructure;
	EXTI_ClearITPendingBit(EXTI_Line0| EXTI_Line3);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);    // ��ռʽ���ȼ���

	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQChannel;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQChannel;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/**
* @brief   	Disable the keypad interrupt
* @return      none
*/
void Keypad_Int_Disable(void)
{
	NVIC_InitTypeDef	NVIC_InitStructure;
	EXTI_ClearITPendingBit(EXTI_Line0| EXTI_Line3);

	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQChannel;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
	NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQChannel;
	NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
	NVIC_Init(&NVIC_InitStructure);
}


/**
* @brief ��ʼ�����������Ҫʹ�õ��Ķ�ʱ��,20ms��ʱ�������Լ�ⰴ���İ���ʱ��
*/
void Keypad_Timer_Init(void)
{
	TIM_TimeBaseInitTypeDef						TIM_TimeBaseStructure;
	TIM_OCInitTypeDef							TIM_OCInitStructure;
	NVIC_InitTypeDef							NVIC_InitStructure;

	//��ʼ���ṹ�����
	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	TIM_OCStructInit(&TIM_OCInitStructure);

	/*������Ӧʱ�� */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);  


	//��ʱ���㹫ʽ��(��1+Prescaler��/72M ) *(Period+1) = ((1+39)/72M) * (72000/2-1+1) = 20ms
	/* Time Base configuration */
	TIM_TimeBaseStructure.TIM_Prescaler			= 39;      
	TIM_TimeBaseStructure.TIM_CounterMode		= TIM_CounterMode_Up; //���ϼ���
	TIM_TimeBaseStructure.TIM_Period			= (72000/2-1);      
	TIM_TimeBaseStructure.TIM_ClockDivision		= TIM_CKD_DIV1;
	//TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	/* Channel 1, 2, 3 and 4 Configuration in Timing mode */
	TIM_OCInitStructure.TIM_OCMode				= TIM_OCMode_Timing;
	TIM_OCInitStructure.TIM_Pulse				= 0x0;

	TIM_OC1Init(TIM3, &TIM_OCInitStructure);

	TIM_ClearFlag(TIM3,TIM_FLAG_Update);
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

	/* Enable the TIM2 Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel			= TIM3_IRQChannel;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	//NVIC_InitStructure.NVIC_IRQChannelSubPriority	= 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd		= ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/**
* @brief ʹ�ܰ��������Ҫʹ�õ��Ķ�ʱ��
*/
void Keypad_Timer_Enable(void)
{
	TIM_ClearFlag(TIM3,TIM_FLAG_Update);
	TIM_Cmd(TIM3, ENABLE);
}

/**
* @brief �رհ��������Ҫʹ�õ��Ķ�ʱ��
*/
void Keypad_Timer_Disable(void)
{
	TIM_Cmd(TIM3, DISABLE);
}

/**
* @brief ��ʼ������
*/
void Keypad_Init(void)
{
	Keypad_Initport();
	Keypad_Timer_Init();
	Keypad_Int_Enable();
}


#define KEYPAD_STATE_INIT					0
#define KEYPAD_STATE_AT_LEAST_CLICK			1
#define KEYPAD_STATE_FIRST_CLICK_RELEASE	2
#define KEYPAD_STATE_SECOND_CLICK			3
#define KEYPAD_STATE_LONG_PRESS				4

/**
 * @brief keypad 2��Key��Ӧ��IO�ⲿ�ж�ISR
 * @note  EXIT0��EXTI3���жϷ���������
*/
void Keypad_EXTI_ISRHandler(unsigned char	exti_line)
{
	//һ�����жϣ���ʾ��⵽�а��������£���ʱ��Ҫ�ȹر����а�����IO�жϣ�ֱ���˴��жϽ����ٿ���
	Keypad_Int_Disable();

	//�ٿ�����ʱ�������жϣ��жϰ����ǵ������ǳ���
	Keypad_Timer_Enable();

	if (exti_line == SCAN_KEY_EXTI_INT)
	{
		reset_resVar();
		SCANNER_TRIG_ON();
        hw_platform_start_led_blink(LED_GREEN,3);
		scan_key_trig = 1;
		current_press_key = SCAN_KEY;
		if(device_current_state ==  STATE_BT_Mode_WaitPair)
		{
			OSQPost(pEvent_Queue,(void*)EVENT_SCAN_KEY_SINGLE_CLICK);
		}
	}
	else if (exti_line == ERASE_KEY_EXTI_INT)
	{
		if (device_current_state == STATE_Memory_Mode)
		{
			reset_resVar();
			SCANNER_TRIG_ON();
			hw_platform_start_led_blink(LED_GREEN,3);
			scan_key_trig = 2;
		}
		current_press_key = ERASE_KEY;
	}
	press_cnt = 0;
	keypad_state = KEYPAD_STATE_INIT;
}


/**
 * @brief ��ȡkeypad IO������IO״̬
 * @param[in] unsigned char key		��Ҫ��ȡ�����ĸ�Key��Ӧ��IO
 * @return  0: low  else: high  
*/
static unsigned char Keypad_ReadIO(unsigned char key)
{
	unsigned char h1=0,h2=0;
	unsigned int	i;
reread:
	if (key == SCAN_KEY)
	{
		h1 = GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0);
	}
	else if (key == ERASE_KEY)
	{
		h1 = GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_3);
	}

	//hw_platform_trip_io();
	for(i=0;i < 10000;i++);	//Լ1ms(��72MƵ���£�ʵ��972us)
	//hw_platform_trip_io();

	if (key == SCAN_KEY)
	{
		h2 = GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0);
	}
	else if (key == ERASE_KEY)
	{
		h2 = GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_3);
	}

	if(h1 == h2)
	{
		return h1;
	}
	else
	{
		goto  reread;
	}
}

//��ס����2�������͵�ƽ�ж����ڼ���Ϊ������һ�ΰ��������¼��ķ�������20ms����Ϊ��������ֻҪ��ס����40ms����Ϊ���ٷ�������Ч�İ��������¼�
#define SINGLE_CLICK_TH			2		
#define LONG_PRESS_TH			300		//��ס����250�������͵�ƽ�ж����ڼ���Ϊ��һ�ΰ��������¼��ķ�������ס����5S����Ϊ��������
#define DOUBLE_CLICK_INTERVAL	4		//˫�����������ΰ���֮���ʱ�䲻����80ms����Ϊ��˫��
//����һ���ص�����ָ�룬�Թ��жϴ������ڻ�ȡ��������ֵʱpost������ģ��ʹ��ʱ�������ṩ��ͬ�ķ���
//typedef void (* post_key_method)(unsigned char key_value);

//static void post_key(unsigned char key)
//{
//	OSQPost(pEvent_Queue,(void*)key);
//}

/**
 * @brief ��ʱ���жϴ������
 * @note �����жϰ�������20ms��ʱ���жϴ��������ɵ�
*/
void Keypad_Timer_ISRHandler(void)
{
	if (Keypad_ReadIO(current_press_key) == 0)
	{
		if (keypad_state == KEYPAD_STATE_INIT)
		{
			press_cnt++;
			if (press_cnt == SINGLE_CLICK_TH)
			{
				keypress_timeout = 0;
				keypad_state = KEYPAD_STATE_AT_LEAST_CLICK;
				//if (current_press_key == SCAN_KEY)
				//{
				//	hw_platform_start_led_blink(LED_GREEN,3);
				//	if (device_current_state == STATE_HID_Mode)
				//	{
				//		OSQPost(pEvent_Queue,(void*)EVENT_SCAN_KEY_SINGLE_CLICK);
				//		//keypad_state = KEYPAD_STATE_INIT;
				//		Keypad_Timer_Disable();
				//		Keypad_Int_Enable();
				//	}
				//}
			}
		}
		else if (keypad_state == KEYPAD_STATE_AT_LEAST_CLICK)
		{
			press_cnt++;
			if (press_cnt == LONG_PRESS_TH)
			{
				if (current_press_key == SCAN_KEY)
				{
					OSQPost(pEvent_Queue,(void*)EVENT_SCAN_KEY_LONG_PRESS);
				}
				else if (current_press_key == ERASE_KEY)
				{
					OSQPost(pEvent_Queue,(void*)EVENT_ERASE_KEY_LONG_PRESS);
				}
				
				//keypad_state = KEYPAD_STATE_INIT;
				//Keypad_Timer_Disable();
				//Keypad_Int_Enable();
			}
		}
		else if (keypad_state == KEYPAD_STATE_FIRST_CLICK_RELEASE)
		{
			keypad_state = KEYPAD_STATE_SECOND_CLICK;
			press_cnt = 0;
		}
		else if (keypad_state == KEYPAD_STATE_SECOND_CLICK)
		{
			press_cnt++;
			if (press_cnt == LONG_PRESS_TH)
			{
				OSQPost(pEvent_Queue,(void*)EVENT_SCAN_KEY_LONG_PRESS);
				//Keypad_Timer_Disable();
				//Keypad_Int_Enable();
			}
		}
	}
	else
	{
		if((current_press_key == SCAN_KEY)||((current_press_key == ERASE_KEY)&&(device_current_state == STATE_Memory_Mode)))
		{
			SCANNER_TRIG_OFF();
			scan_key_trig = 0;
			hw_platform_stop_led_blink(LED_GREEN);
		}

		if (keypad_state == KEYPAD_STATE_INIT)
		{
			//keypad_state = KEYPAD_STATE_INIT;
			Keypad_Timer_Disable();
			Keypad_Int_Enable();
		}
		else if (keypad_state == KEYPAD_STATE_AT_LEAST_CLICK)
		{
			if (current_press_key == SCAN_KEY)
			{
				//OSQPost(pEvent_Queue,(void*)EVENT_SCAN_KEY_SINGLE_CLICK);
				if (device_current_state == STATE_BT_Mode_Connect)
				{
					//ֻ������������״̬�£�����Ҫ���SCAN����˫����Ϊ
					keypad_state = 	KEYPAD_STATE_FIRST_CLICK_RELEASE;
					release_cnt = 0;
				}
				else
				{
					//����״̬��û�б�Ҫ���SCAN����˫����Ϊ
				//	hw_platform_start_led_blink(LED_GREEN,3);
				//	OSQPost(pEvent_Queue,(void*)EVENT_SCAN_KEY_SINGLE_CLICK);
					Keypad_Timer_Disable();
					Keypad_Int_Enable();
				}
				
			}
			else if (current_press_key == ERASE_KEY)
			{
				OSQPost(pEvent_Queue,(void*)EVENT_ERASE_KEY_SINGLE_CLICK);
				//keypad_state = KEYPAD_STATE_INIT;
				Keypad_Timer_Disable();
				Keypad_Int_Enable();
			}
		}
		else if (keypad_state == KEYPAD_STATE_FIRST_CLICK_RELEASE)
		{
			release_cnt++;
			if (release_cnt == DOUBLE_CLICK_INTERVAL)
			{
				//��������˫�����ʱ����û���ٴΰ��°�����������ȷ�ϵ����¼��ķ���
				//hw_platform_start_led_blink(LED_GREEN,3);
				//OSQPost(pEvent_Queue,(void*)EVENT_SCAN_KEY_SINGLE_CLICK);
				Keypad_Timer_Disable();
				Keypad_Int_Enable();
			}
		}
		else if (keypad_state == KEYPAD_STATE_SECOND_CLICK)
		{
			OSQPost(pEvent_Queue,(void*)EVENT_SCAN_KEY_DOUBLE_CLICK);
			//keypad_state = KEYPAD_STATE_INIT;
			Keypad_Timer_Disable();
			Keypad_Int_Enable();
			hw_platform_stop_led_blink(LED_GREEN);
		}
	}
}

