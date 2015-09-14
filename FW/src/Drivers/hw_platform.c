/**
* @file hw_platform.c
* @brief H520B 硬件相关接口的实现
* @version V0.0.1
* @author joe.zhou
* @date 2015年08月31日
* @note
* @copy
*
* 此代码为深圳合杰电子有限公司项目代码，任何人及公司未经许可不得复制传播，或用于
* 本公司以外的项目。本司保留一切追究权利。
*
* <h1><center>&copy; COPYRIGHT 2015 heroje</center></h1>
*
*/
#include "hw_platform.h"
#include "stm32f10x_lib.h"
#include "ucos_ii.h"
#include "TimeBase.h"

static unsigned int	charge_state_cnt;
static unsigned int	last_charge_detect_io_cnt;

unsigned int	charge_detect_io_cnt;

//USB_CHK		PA1	  

//LED_RED		PA4   
//LED_GREEN		PA5   
//LED_YELLOW	PA6   
//MOTOR			PA7   

//ADC_IN		PB0	  

//BEEP			PB5   
//CHARGE		PB6	  
//RFU_IO1		PB7
//RFU_IO2		PB8

//TRIG			PB12

/**
* @brief	初始化用于电池电压检测的ADC模块
* @param[in]  none
* @param[out] none
* @return     none
* @note                    
*/
static void ADC_Module_Init(void)
{
	ADC_InitTypeDef   adc_init;
	GPIO_InitTypeDef  gpio_init;


	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	//PB.0   ADC-IN
	gpio_init.GPIO_Pin  = GPIO_Pin_0;
	gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
	gpio_init.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOB, &gpio_init);

	adc_init.ADC_Mode               = ADC_Mode_Independent;		//
	adc_init.ADC_ScanConvMode       = DISABLE;
	adc_init.ADC_ContinuousConvMode = ENABLE;
	adc_init.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
	adc_init.ADC_DataAlign          = ADC_DataAlign_Right;
	adc_init.ADC_NbrOfChannel       = 1;
	ADC_Init(ADC1, &adc_init);

	ADC_RegularChannelConfig(ADC1, ADC_Channel_8, 1, ADC_SampleTime_13Cycles5);
	ADC_Cmd(ADC1, ENABLE);

	// 下面是ADC自动校准，开机后需执行一次，保证精度
	// Enable ADC1 reset calibaration register 
	ADC_ResetCalibration(ADC1);
	// Check the end of ADC1 reset calibration register
	while(ADC_GetResetCalibrationStatus(ADC1));

	// Start ADC1 calibaration
	ADC_StartCalibration(ADC1);
	// Check the end of ADC1 calibration
	while(ADC_GetCalibrationStatus(ADC1));
	// ADC自动校准结束---------------

	ADC_SoftwareStartConvCmd(ADC1, ENABLE);			//开始转换
}

/**
* @brief  Initialize the IO
* @return   none
*/
static void platform_misc_port_init(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;
	EXTI_InitTypeDef	EXTI_InitStructure;
	NVIC_InitTypeDef	NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

	//USB_CHK -- PA.1
	GPIO_InitStructure.GPIO_Pin				= GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode			= GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed			= GPIO_Speed_10MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//ChargeState detect -- PB.6
	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* Connect EXTI Line6 to PB.6 */
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOE, GPIO_PinSource0);

	EXTI_ClearITPendingBit(EXTI_Line6);
	EXTI_InitStructure.EXTI_Line = EXTI_Line6;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure); 
	EXTI_GenerateSWInterrupt(EXTI_Line6);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);      
	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQChannel;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 6;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	charge_detect_io_cnt = 0;
	last_charge_detect_io_cnt = 1;

	//LED-Red -- PA.4	LED-Green -- PA.5	LED-Yellow -- PA.6		Motor -- PA.7
	GPIO_InitStructure.GPIO_Pin	= GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6);
	GPIO_ResetBits(GPIOA, GPIO_Pin_7);

	//Beep -- PB.5	Trig -- PB.12
	GPIO_InitStructure.GPIO_Pin	= GPIO_Pin_5 | GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB,  GPIO_Pin_12);
	GPIO_ResetBits(GPIOA, GPIO_Pin_5);

	//RFU-IO1 -- PB.7	  RFU-IO2 -- PB.8		
	GPIO_InitStructure.GPIO_Pin	= GPIO_Pin_7 | GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
* @brief  Initialize the HW platform
* @return   none
*/
void hw_platform_init(void)
{
	platform_misc_port_init();
	ADC_Module_Init();
}


/**
* @brief	返回检测到的电压的级别
* @param[in]  none
* @param[out] none
* @return     0:电池电量低		1：还有电
* @note     暂定2级，当测定的电压低于3.15V时，表示系统的电压低了
*			电池的工作电压范围是: 3.0 -- 4.0
*/

#define  LOW_POWER_TH	1960		//	
unsigned int hw_platform_get_PowerClass(void)
{ 
	unsigned int  i,result = 0;
	unsigned short  temp[20];
	unsigned short	min,max;

	for(i = 0;i < 20;i++)
	{
		temp[i] = ADC_GetConversionValue(ADC1);		//得到AD转换的结果
		result += temp[i];
		if (i == 0)
		{
			min = temp[i];
			max = temp[i];
		}

		if (temp[i] < min)
		{
			min = temp[i];
		}

		if (temp[i] > max)
		{
			max = temp[i];
		}
	}

	//取20次值之后,去掉最小值和最大周，再取平均值，简单的平滑滤波
	result -= min;
	result -= max;
	result /= 18; 

	if (result > LOW_POWER_TH) 
	{
		return 1;
	}
	else
		return 0;
}

/**
* @brief	检测充电状态
* @param[in]  none
* @param[out] none
* @return     1:  充电完成    0: 正在充电
* @note  正在充电时，充电检测IO输出脉冲，充满后输出低电平 
*        需要注意的是：此函数只有在得知充电电源已经插入的前提下，再去检测才有意义。因为没有插入充电电源时，检测IO也是低电平的。
*		利用IO的边沿中断来完成充电状态的检测，如果一直在进中断，那说明正在充电，否则表示充电完成
*/
unsigned int  hw_platform_ChargeState_Detect(void)
{
	if (charge_detect_io_cnt != last_charge_detect_io_cnt)
	{
		charge_state_cnt = 50000;
		last_charge_detect_io_cnt = charge_detect_io_cnt;
		return 0;
	}
	else
	{
		if (charge_state_cnt--)
		{
			return 0;
		}
	}
	return 1;
}

/**
* @brief	检测USB是否插入
* @param[in]  none
* @param[out] none
* @return     1:  插入    0: 没有插入
* @note  如果需要在中断里面去实现，那么还需要在初始化时给此检测IO分配外部中断
*		 如果在任务级查询，那么可以之际调用此函数来检测是否插入充电电源                 
*/
unsigned int hw_platform_USBcable_Insert_Detect(void)
{
	unsigned int i;
	if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1))
	{
		for (i=0;i < 2000;i++);		//延时一小段时间，防止是因为抖动造成的
		if(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_1))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
		return 0;
}

/**
* @brief	LED控制接口
* @param[in]  unsigned int led	 LED_RED or  LED_GREEN  or LED_YELLOW
* @param[in]  unsigned int ctrl  0: close   1:open
* @return   none                 
*/
void hw_platform_led_ctrl(unsigned int led,unsigned int ctrl)
{
	if (led == LED_RED)
	{
		if (ctrl)
		{
			GPIO_ResetBits(GPIOA, GPIO_Pin_4);
		}
		else
		{
			GPIO_SetBits(GPIOA, GPIO_Pin_4);
		}
	}
	else if (led == LED_GREEN)
	{
		if (ctrl)
		{
			GPIO_ResetBits(GPIOA, GPIO_Pin_5);
		}
		else
		{
			GPIO_SetBits(GPIOA, GPIO_Pin_5);
		}
	}
	else if (led == LED_YELLOW)
	{
		if (ctrl)
		{
			GPIO_ResetBits(GPIOA, GPIO_Pin_6);
		}
		else
		{
			GPIO_SetBits(GPIOA, GPIO_Pin_6);
		}
	}
}


/**
* @brief	Motor控制接口
* @param[in]  unsigned int delay  延时一段时间,单位ms
* @return   none                 
*/
void hw_platform_motor_ctrl(unsigned short delay)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_7);
	if (delay < 5)
	{
		Delay(delay*2000);
	}
	else
	{
		OSTimeDlyHMSM(0,0,0,delay);
	}
	GPIO_ResetBits(GPIOA, GPIO_Pin_7);
}

/**
* @brief	Trig控制接口
* @param[in]  unsigned int delay  延时一段时间,单位ms
* @return   none                 
*/
void hw_platform_trig_ctrl(unsigned short delay)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_12);
	if (delay < 5)
	{
		Delay(delay*2000);
	}
	else
	{
		OSTimeDlyHMSM(0,0,0,delay);
	}
	GPIO_SetBits(GPIOB, GPIO_Pin_12);
}

/**
* @brief	蜂鸣器简单的控制接口
* @param[in]  unsigned int delay	延时一段时间,单位50ms
* @param[in]  BEEP_FREQ	beep_freq	蜂鸣器的发声频率
* @return   none 
*/
void hw_platform_beep_ctrl(unsigned short delay,BEEP_FREQ	beep_freq)
{
	int i;
	for (i = 0; i < delay*50*(unsigned int)beep_freq;i++)
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_5);
		Delay(1000/(unsigned int)beep_freq);
		GPIO_SetBits(GPIOB, GPIO_Pin_5);
		Delay(1000/(unsigned int)beep_freq);
	}
}

static unsigned char	current_blink_led;
static unsigned char	current_led_state;
static unsigned short   current_blink_frq;
static unsigned int   current_timer_cnt;
/**
 * @brief 利用时基模块提供的定时器接口实现LED闪烁
*/
static void led_blink_timer_hook(void)
{
	current_timer_cnt++;
	if (current_timer_cnt == current_blink_frq*2*10)
	{
		current_led_state ^= 0x01;
		hw_platform_led_ctrl(current_blink_led,current_led_state);
		current_timer_cnt = 0;
	}
}

/**
 * @brief 开始LED闪烁指示
 * @param[in] unsigned int		led		
 * @param[in] unsigned short	delay	闪烁的时间间隔，也就是闪烁频率,单位10ms
 * @note 注意此接口只能让一个LED在闪烁，不能实现几个LED同时闪烁
*/
void hw_platform_start_led_blink(unsigned int led,unsigned short delay)
{
	current_timer_cnt = 0;
	current_blink_led = led;
	current_blink_frq = delay;
	current_led_state = 1;
	hw_platform_led_ctrl(current_blink_led,current_led_state);
	start_timer(led_blink_timer_hook);
}

/**
 * @brief 关闭正在闪烁的LED
*/
void hw_platform_stop_led_blink(void)
{
	stop_timer();
	hw_platform_led_ctrl(current_blink_led,0);
}

