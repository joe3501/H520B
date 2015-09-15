/**
* @file hw_platform.c
* @brief H520B Ӳ����ؽӿڵ�ʵ��
* @version V0.0.1
* @author joe.zhou
* @date 2015��08��31��
* @note
* @copy
*
* �˴���Ϊ���ںϽܵ������޹�˾��Ŀ���룬�κ��˼���˾δ����ɲ��ø��ƴ�����������
* ����˾�������Ŀ����˾����һ��׷��Ȩ����
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

  
//LED_RED		PA4   
//LED_GREEN		PA5   
//LED_YELLOW	PA6   
//MOTOR			PA7   


//BEEP			PB5   

/**
* @brief	��ʼ�����ڵ�ص�ѹ����ADCģ��
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

	// ������ADC�Զ�У׼����������ִ��һ�Σ���֤����
	// Enable ADC1 reset calibaration register 
	ADC_ResetCalibration(ADC1);
	// Check the end of ADC1 reset calibration register
	while(ADC_GetResetCalibrationStatus(ADC1));

	// Start ADC1 calibaration
	ADC_StartCalibration(ADC1);
	// Check the end of ADC1 calibration
	while(ADC_GetCalibrationStatus(ADC1));
	// ADC�Զ�У׼����---------------

	ADC_SoftwareStartConvCmd(ADC1, ENABLE);			//��ʼת��
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

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);


	//LED-Red -- PF.6	LED-Green -- PF.7	LED-Yellow -- PF.8		Motor -- PF.10  Beep -- PF.11
	GPIO_InitStructure.GPIO_Pin	= GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOF, &GPIO_InitStructure);
	GPIO_SetBits(GPIOF, GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_11);
	GPIO_ResetBits(GPIOF, GPIO_Pin_10);
}

/**
* @brief  Initialize the HW platform
* @return   none
*/
void hw_platform_init(void)
{
	platform_misc_port_init();
	//ADC_Module_Init();
}


/**
* @brief	���ؼ�⵽�ĵ�ѹ�ļ���
* @param[in]  none
* @param[out] none
* @return     0:��ص�����		1�����е�
* @note     �ݶ�2�������ⶨ�ĵ�ѹ����3.15Vʱ����ʾϵͳ�ĵ�ѹ����
*			��صĹ�����ѹ��Χ��: 3.0 -- 4.0
*/

#define  LOW_POWER_TH	1960		//	
unsigned int hw_platform_get_PowerClass(void)
{ 
		return 1;
}

/**
* @brief	�����״̬
* @param[in]  none
* @param[out] none
* @return     1:  ������    0: ���ڳ��
* @note  ���ڳ��ʱ�������IO������壬����������͵�ƽ 
*        ��Ҫע����ǣ��˺���ֻ���ڵ�֪����Դ�Ѿ������ǰ���£���ȥ���������塣��Ϊû�в������Դʱ�����IOҲ�ǵ͵�ƽ�ġ�
*		����IO�ı����ж�����ɳ��״̬�ļ�⣬���һֱ�ڽ��жϣ���˵�����ڳ�磬�����ʾ������
*/
unsigned int  hw_platform_ChargeState_Detect(void)
{
	return 0;
}

/**
* @brief	���USB�Ƿ����
* @param[in]  none
* @param[out] none
* @return     1:  ����    0: û�в���
* @note  �����Ҫ���ж�����ȥʵ�֣���ô����Ҫ�ڳ�ʼ��ʱ���˼��IO�����ⲿ�ж�
*		 ��������񼶲�ѯ����ô����֮�ʵ��ô˺���������Ƿ�������Դ                 
*/
unsigned int hw_platform_USBcable_Insert_Detect(void)
{
		return 0;
}

/**
* @brief	LED���ƽӿ�
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
			GPIO_ResetBits(GPIOF, GPIO_Pin_6);
		}
		else
		{
			GPIO_SetBits(GPIOF, GPIO_Pin_6);
		}
	}
	else if (led == LED_GREEN)
	{
		if (ctrl)
		{
			GPIO_ResetBits(GPIOF, GPIO_Pin_7);
		}
		else
		{
			GPIO_SetBits(GPIOF, GPIO_Pin_7);
		}
	}
	else if (led == LED_YELLOW)
	{
		if (ctrl)
		{
			GPIO_ResetBits(GPIOF, GPIO_Pin_8);
		}
		else
		{
			GPIO_SetBits(GPIOF, GPIO_Pin_8);
		}
	}
}


/**
* @brief	Motor���ƽӿ�
* @param[in]  unsigned int delay  ��ʱһ��ʱ��,��λms
* @return   none                 
*/
void hw_platform_motor_ctrl(unsigned short delay)
{
	GPIO_SetBits(GPIOF, GPIO_Pin_10);
	if (delay < 5)
	{
		Delay(delay*2000);
	}
	else
	{
		OSTimeDlyHMSM(0,0,0,delay);
	}
	GPIO_ResetBits(GPIOF, GPIO_Pin_10);
}

/**
* @brief	Trig���ƽӿ�
* @param[in]  unsigned int delay  ��ʱһ��ʱ��,��λms
* @return   none                 
*/
void hw_platform_trig_ctrl(unsigned short delay)
{

}

/**
* @brief	�������򵥵Ŀ��ƽӿ�
* @param[in]  unsigned int delay	��ʱһ��ʱ��,��λms,����ʱ��
* @param[in]  unsigned int	beep_freq	�������ķ���Ƶ�ʣ�����
* @return   none 
*/
void hw_platform_beep_ctrl(unsigned short delay,unsigned int beep_freq)
{
	int i;
	for (i = 0; i < (delay*beep_freq)/1000;i++)
	{
		GPIO_SetBits(GPIOF, GPIO_Pin_11);
		Delay(1000000/beep_freq);
		GPIO_ResetBits(GPIOF, GPIO_Pin_11);
		Delay(1000000/beep_freq);
	}
}

static unsigned char	current_blink_led;
static unsigned char	current_led_state;
static unsigned short   current_blink_frq;
static unsigned int   current_timer_cnt;
/**
 * @brief ����ʱ��ģ���ṩ�Ķ�ʱ���ӿ�ʵ��LED��˸
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
 * @brief ��ʼLED��˸ָʾ
 * @param[in] unsigned int		led		
 * @param[in] unsigned short	delay	��˸��ʱ������Ҳ������˸Ƶ��,��λ10ms
 * @note ע��˽ӿ�ֻ����һ��LED����˸������ʵ�ּ���LEDͬʱ��˸
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
 * @brief �ر�������˸��LED
*/
void hw_platform_stop_led_blink(void)
{
	stop_timer();
	hw_platform_led_ctrl(current_blink_led,0);
}

