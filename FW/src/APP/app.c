/**
* @file app.c
* @brief H520B �����������ݲɼ�����ĿAPP
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
#include "ucos_ii.h"
#include "app.h"
#include "hw_platform.h"
#include "usb_pwr.h"
#include <string.h>
#include <assert.h>
#include "JMemory.h"
#include "TimeBase.h"
#include "hw_config.h"
#include "basic_fun.h"
#include <stdio.h>
#include "record_m.h"
#include "Terminal_Para.h"
#include "usb_lib.h"
#include "PCUsart.h"

//������״̬���߳�������ģ���߳�֮��ͨѶ��IPC����
#define BARCODE_CASH_NUM	15			//����ȴ�����ģ���̷߳��͵����뻺����Ϊ15������
#define MAX_BARCODE_LEN		80			//����������󳤶�Ϊ80���ֽ�

//define the stack size of each task
#define STACK_SIZE_TASKEC			128	
#define STACK_SIZE_TASKSM			356
#define STACK_SIZE_TASKBT			128
#define STACK_SIZE_TASKINI			224

static OS_STK	thread_eventcapture_stk[STACK_SIZE_TASKEC];		//the stack of the Event_capture_thread
static OS_STK	thread_statemachine_stk[STACK_SIZE_TASKSM];		//the stack of the State_Machine_thread
static OS_STK	thread_bt_stk[STACK_SIZE_TASKBT];				//the stack of the BT_Daemon_thread
static OS_STK	*p_init_thread_stk;								//���̵߳�ջ��̬�������߳̽����Լ��ͷ�

static void *barcode_pool[BARCODE_CASH_NUM];	//�����ȡ���������ַ����ĵ�ַ����
static unsigned char barcode_cash[BARCODE_CASH_NUM][MAX_BARCODE_LEN+2];	//���һ���ֽڱ�ʾ���������Ƿ�����pool�д�������
static	unsigned char	lowpower_state;
static	unsigned char	lowpower_cnt;
static  TBATCH_NODE		batch_node;
/*
------------------------------------------------------------
|               barcode[MAX_BARCODE_LEN+1]              |flag|
------------------------------------------------------------
|               barcode[MAX_BARCODE_LEN+1]              |flag|
------------------------------------------------------------
|               barcode[MAX_BARCODE_LEN+1]              |flag|
------------------------------------------------------------
.........
------------------------------------------------------------
|               barcode[MAX_BARCODE_LEN+1]              |flag|
------------------------------------------------------------
*/
//��������ľ�̬�����������⶯̬�����ڴ�
static OS_EVENT	*pBarcode_Queue;			//barcode��Ϣ����


#define EVENT_CASH_NUM		8			//�����¼��Ļ�������
//�����¼�����߳�����״̬��֮��ͨѶ��IPC����
static void *event_pool[EVENT_CASH_NUM];		//�¼�����
OS_EVENT	*pEvent_Queue;			//�¼���Ϣ����

static OS_EVENT *pIOSem;				//IO�ź���
//

unsigned int	device_current_state;		//�豸��״̬��

unsigned int	keypress_timeout;


void u_disk_proc(void);
int lowpower_tip(void);
void system_err_tip(void);

extern void EnterLowPowerMode(void);
extern void ExitLowPowerMode(void);


/**
* @brief	ɨ������ɹ�����ʾ
*/
static inline void scan_barcode_ok_tip(void)
{
	hw_platform_led_ctrl(LED_YELLOW,1);
	hw_platform_beep_motor_ctrl(100,4000);
	OSTimeDlyHMSM(0,0,0,10);
	hw_platform_led_ctrl(LED_YELLOW,0);
}

/**
* @brief	��������������ľ�̬�����������ر���ĵ�ַ
* @param[in] unsigned char* barcode				��Ҫ���������
* @return   ����ĵ�ַ
* @note ����:ֻҪ�ҵ�һ����λ�þͷŽ�ȥ��ÿһ�е����һ���ֽ�Ϊ0��ʾ��λ���ǿյ�
*											     ���һ���ֽ�0x55��ʾ��λ���Ѿ�����������
*/
unsigned char * push_barcode_into_cash(unsigned char* barcode)
{
	unsigned int	i;
	for (i = 0; i < BARCODE_CASH_NUM;i++)
	{
		if (barcode_cash[i][MAX_BARCODE_LEN+1] == 0)
		{
			if (strlen((char const*)barcode) > MAX_BARCODE_LEN)
			{
				memcpy(barcode_cash[i],barcode,MAX_BARCODE_LEN);
				barcode_cash[i][MAX_BARCODE_LEN] = 0;
			}
			else
			{
				strcpy((char*)barcode_cash[i],(char const*)barcode);
			}
			barcode_cash[i][MAX_BARCODE_LEN+1] = 0x55;		//��ʾ�Ѿ�����������
			return (void*)barcode_cash[i];
		}
	}

	return (void*)0;
}


/**
* @brief	���Ѿ����ͳ�ȥ�Ļ����ַpull����
* @param[in] unsigned char* barcode_addr    һ���Ϸ��Ļ������ĵ�ַ
* @return   none
*/
void pull_barcode_from_cash(unsigned char* barcode_addr)
{
	assert((int)barcode_addr >= (int)barcode_cash[0]);
	assert((int)barcode_addr <= (int)barcode_cash[BARCODE_CASH_NUM-1]);
	assert(((int)barcode_addr - (int)barcode_cash[0])%(MAX_BARCODE_LEN+2) == 0);

	//memset(barcode_addr,0,MAX_BARCODE_LEN+2);
	barcode_addr[MAX_BARCODE_LEN+1] = 0;		//�ѱ�־�ָ�Ϊ0����
	return;
}



/**
* @brief	����Memoryģʽʱ����Ҫ���е�һЩ����
*/
static inline void enter_into_Memory_Mode(void)
{
#ifdef DEBUG_VER
	printf("enter into Memory Mode\r\n");
#endif
	g_param.last_state = 1;
	SaveTerminalPara();
}

/**
* @brief	�˳�Memoryģʽʱ����Ҫ���е�һЩ����
*/
static inline void exit_from_Memory_Mode(void)
{
#ifdef DEBUG_VER
	printf("exit from Memory Mode\r\n");
#endif
	//@todo...
}

/**
* @brief	����USB HIDģʽʱ����Ҫ���е�һЩ����
*/
static inline void enter_into_USB_HID_Mode(void)
{
#ifdef DEBUG_VER
	printf("enter into USB HID Mode\r\n");
#endif
	hw_platform_led_ctrl(LED_RED,1);
	//hw_platform_beep_ctrl(100,1045);
	//hw_platform_beep_ctrl(100,1171);
	//hw_platform_beep_ctrl(100,1316);
	//hw_platform_beep_ctrl(100,1393);
	//hw_platform_beep_ctrl(100,1563);
	//hw_platform_beep_ctrl(100,1755);
	//hw_platform_beep_ctrl(100,1971);

	hw_platform_beep_ctrl(100,1316);
	hw_platform_beep_ctrl(100,1316);
	hw_platform_beep_ctrl(100,1393);
	hw_platform_beep_ctrl(100,1563);
	hw_platform_beep_ctrl(100,1563);
	hw_platform_beep_ctrl(100,1393);
	hw_platform_beep_ctrl(100,1316);
	hw_platform_beep_ctrl(100,1171);
}

/**
* @brief	�˳�USB HIDģʽʱ����Ҫ���е�һЩ����
*/
static inline void exit_from_USB_HID_Mode(void)
{
#ifdef DEBUG_VER
	printf("exit from USB HID Mode\r\n");
#endif
	hw_platform_led_ctrl(LED_RED,0);
}

/**
* @brief	����BTģʽʱ����Ҫ���е�һЩ����
* @param[in] unsigned char	child_state		0: disconnect  1��connected  2��:waitpair
*/
static inline void enter_into_BT_Mode(unsigned char child_state)
{
#ifdef DEBUG_VER
	printf("enter into BT Mode:%d\r\n",child_state);
#endif
	if (child_state == 2)
	{
		WBTD_Reset();
		hw_platform_beep_ctrl(300,3000);
		hw_platform_start_led_blink(LED_BLUE,10);
		WBTD_set_autocon(0);
	}
	else if (child_state == 0)
	{
		hw_platform_start_led_blink(LED_BLUE,300);
	}
	else
	{
		hw_platform_led_ctrl(LED_BLUE,1);
	}
	g_param.last_state = 0;
	SaveTerminalPara();
}

/**
* @brief	�˳�BTģʽʱ����Ҫ���е�һЩ����
* @param[in] unsigned char	child_state		0: disconnect  1��connected  2��:waitpair
*/
static inline void exit_from_BT_Mode(unsigned char child_state)
{
#ifdef DEBUG_VER
	printf("exit from BT Mode:%d\r\n",child_state);
#endif
	if (child_state == 1)
	{
		WBTD_set_autocon(1);
		//delay_ms(1);
		hw_platform_beep_ctrl(300,3000);
		hw_platform_led_ctrl(LED_BLUE,0);
		WBTD_Reset();//�����Ͽ�����������������
		
	}
	else
	{
		hw_platform_stop_led_blink(LED_BLUE);
	}
}


/**
* @brief	ͨ��USB HID��������
*/
static void barcode_hid_send(unsigned char* barcode)
{
	unsigned int	i,code_len;
	unsigned char key_value_report[8];

    code_len = strlen((char const*)barcode);
	OSSchedLock();
	for (i = 0; i < code_len; i++)
	{
		ascii_to_keyreport(barcode[i],key_value_report);

		SendData_To_PC(key_value_report, 3);
		SendData_To_PC("\x00\x00\x00", 3);
	}

	memcpy(key_value_report,"\x00\x00\x28",3);	//����

	SendData_To_PC(key_value_report, 3);
	SendData_To_PC("\x00\x00\x00", 3);	//����
	OSSchedUnlock();
}

/**
* @brief	Ӧ�õĳ�ʼ��
*/
void app_init(void)
{
	//����һ����Ϣ���У����ڽ��¼������̺߳�����ģ���̻߳�ȡ���첽�¼�֪ͨ����״̬���߳�
	pEvent_Queue = OSQCreate((void**)&event_pool,EVENT_CASH_NUM);
	assert(pEvent_Queue != (OS_EVENT*)0);

	//����һ����Ϣ���У����ڽ���״̬���̻߳�ȡ�����봫�͵�����ģ���߳�.
	pBarcode_Queue =OSQCreate((void**)&barcode_pool,BARCODE_CASH_NUM);
	assert(pBarcode_Queue != (OS_EVENT*)0);
	memset(barcode_cash,0,BARCODE_CASH_NUM*(MAX_BARCODE_LEN+2));

	//����һ���ź���������IO�ж�֪ͨ�¼������̣߳����ⲿIO��������Ҫ�¼������߳̿�ʼ��ȡ�����¼��Ķ���
	pIOSem = OSSemCreate(0);
	assert(pIOSem != (OS_EVENT*)0);

	lowpower_state = 0;
	lowpower_cnt = 0;
}

/**
* @brief	ά����״̬�����߳�
*/
void State_Machine_thread(void *p)
{
	unsigned int	i,cnt,event;
	unsigned char	err;
	unsigned char	barcode[MAX_BARCODE_LEN+1];
	unsigned char   codetype[20];
	unsigned int    codelen;
	int				ret,index;
	unsigned int	last_state;
	unsigned char	*rec;

	Jfree(p_init_thread_stk);	//�˳���ʼ���߳�ʱ���ͷ��Լ�������ջ

	//hw_platform_led_blink_test();		//for test
	//lowpower_tip();					//for test
	//record_m_test();					//for test
	
	while(1)
	{
		event = (unsigned int)OSQPend(pEvent_Queue,25,&err);
		if (event == 0)
		{
			if ((g_param.lower_power_timeout)&&(device_current_state != STATE_HID_Mode))
			{
				keypress_timeout++;
				if (keypress_timeout == g_param.lower_power_timeout*4*60)
				{
					hw_platform_beep_ctrl(500,3000);
					EnterLowPowerMode();
					ExitLowPowerMode();
				}
			}
			continue;
		}
#ifdef DEBUG_VER
		printf("current state:%d\r\n",device_current_state);
		printf("event:%d\r\n",event);
#endif
		if(device_current_state ==  STATE_BT_Mode_Disconnect)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
			case EVENT_SCAN_KEY_DOUBLE_CLICK:
				ret = scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);	//ɨ������
				hw_platform_stop_led_blink(LED_GREEN);
				if (ret == 0)
				{
					scan_barcode_ok_tip();
				}

				if (lowpower_state)
				{
					lowpower_tip();
				}
				//ֻ��ɨ�赽������ѣ�ʲô������
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//�л���Memory Mode
				hw_platform_stop_led_blink(LED_GREEN);
				exit_from_BT_Mode(0);
				device_current_state = STATE_Memory_Mode;
				hw_platform_beep_ctrl(300,3000);
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//�л������ģʽ
				exit_from_BT_Mode(0);
				device_current_state = STATE_BT_Mode_WaitPair;
				enter_into_BT_Mode(2);
				break;
			case EVENT_RESET_KEY_PRESS:
				break;
			case EVENT_BT_CONNECTED:
				//�л�����������ģʽ
				exit_from_BT_Mode(0);
				device_current_state = STATE_BT_Mode_Connect;
				enter_into_BT_Mode(1);
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				//�л���USB HIDģʽ
				exit_from_BT_Mode(0);
				last_state = STATE_BT_Mode_Disconnect;
				device_current_state = STATE_HID_Mode;
				enter_into_USB_HID_Mode();
				break;
			case EVENT_USB_CABLE_REMOVE:
				break;
			case EVENT_LOW_POWER:
				lowpower_tip();
				break;
			default:
				break;
			}
		}
		else if(device_current_state ==  STATE_BT_Mode_Connect)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
				//ɨ������
				ret = scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);
				hw_platform_stop_led_blink(LED_GREEN);
				if (ret != 0)
				{
					break;
				}

				scan_barcode_ok_tip();
				if (lowpower_state)
				{
					lowpower_tip();
				}
				//ɨ�赽������
				//����ȡ����������push��cash����������Ȼ��Post��ϵͳ��
				//Queue��������ģ���̸߳���ȥ���͵�����
repost:
				ret = OSQPost(pBarcode_Queue,(void*)push_barcode_into_cash((unsigned char*)barcode));
				if(ret != OS_ERR_NONE)
				{
					if(ret == OS_ERR_Q_FULL || ret == OS_ERR_PEVENT_NULL)
					{
						//����������˻��߿��¼�ʱ����ô��Ҫ��ʱ����
						OSTimeDlyHMSM(0,0,0,100);
						goto repost;
					}
					else
					{
						assert(0);	//ϵͳ����
					}
				}
				break;
			case EVENT_SCAN_KEY_DOUBLE_CLICK:
				if (g_param.ios_softkeypad_enable)
				{
					WBTD_set_ioskeypad(1);
				}
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				hw_platform_stop_led_blink(LED_GREEN);
				//�л���Memory mode
				exit_from_BT_Mode(1);
				device_current_state = STATE_Memory_Mode;
				hw_platform_beep_ctrl(300,3000);
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				//WBTD_hid_send_test();
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//�������ģʽ
				exit_from_BT_Mode(1);
				device_current_state = STATE_BT_Mode_WaitPair;
				enter_into_BT_Mode(2);
				break;
			case EVENT_RESET_KEY_PRESS:
				//@todo...
				break;
			case EVENT_BT_CONNECTED:
				break;
			case EVENT_BT_DISCONNECTED:
				//�л��������Ͽ�״̬
				exit_from_BT_Mode(1);
				device_current_state = STATE_BT_Mode_Disconnect;
				enter_into_BT_Mode(0);
				break;
			case EVENT_USB_CABLE_INSERT:
				//�л���USB HIDģʽ
				exit_from_BT_Mode(1);
				last_state = STATE_BT_Mode_Disconnect;
				device_current_state = STATE_HID_Mode;
				enter_into_USB_HID_Mode();
				break;
			case EVENT_USB_CABLE_REMOVE:
				break;
			case EVENT_LOW_POWER:
				lowpower_tip();
				break;
			default:
				break;
			}
		}
		else if(device_current_state ==  STATE_BT_Mode_WaitPair)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
			case EVENT_SCAN_KEY_DOUBLE_CLICK:
				ret = scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);	//ɨ������
				hw_platform_stop_led_blink(LED_GREEN);
				if (ret == 0)
				{
					scan_barcode_ok_tip();
				}
				if (lowpower_state)
				{
					lowpower_tip();
				}
				//ֻ��ɨ�赽������ѣ�ʲô������
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				hw_platform_stop_led_blink(LED_GREEN);
				//�л���Memory Mode
				exit_from_BT_Mode(2);
				device_current_state = STATE_Memory_Mode;
				hw_platform_beep_ctrl(300,3000);
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//�Ѿ������ģʽ��ʲô������
				break;
			case EVENT_RESET_KEY_PRESS:
				//@todo...
				break;
			case EVENT_BT_CONNECTED:
				//�л�����������״̬
				exit_from_BT_Mode(2);
				device_current_state = STATE_BT_Mode_Connect;
				enter_into_BT_Mode(1);
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				//�л���USB HIDģʽ
				exit_from_BT_Mode(2);
				last_state = STATE_BT_Mode_WaitPair;
				device_current_state = STATE_HID_Mode;
				enter_into_USB_HID_Mode();
				break;
			case EVENT_USB_CABLE_REMOVE:
				break;
			case EVENT_LOW_POWER:
				lowpower_tip();
				break;
			default:
				break;
			}
		}
		else if(device_current_state ==  STATE_Memory_Mode)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
				//ɨ������
				ret = scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);
				hw_platform_stop_led_blink(LED_GREEN);
				if (ret != 0)
				{
					break;
				}

				scan_barcode_ok_tip();
				if (lowpower_state)
				{
					lowpower_tip();
				}
				//ɨ�赽������
				//����ȡ�������뱣�浽memory
				memset((void*)&batch_node,0,sizeof(TBATCH_NODE));
				strcpy((char*)batch_node.barcode,(char const*)barcode);
				ret = record_add((unsigned char*)&batch_node);
				if (ret)
				{
					//��¼����ʧ�ܣ�������ʾ���û�
					//@todo...

				}
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				hw_platform_stop_led_blink(LED_GREEN);
				//�л�������ģʽ
				exit_from_Memory_Mode();
				device_current_state = STATE_BT_Mode_Disconnect;
				enter_into_BT_Mode(0);
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				//ɾ��ɨ���������Ӧ�����һ������
				ret = scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);
				if (ret != 0)
				{
					break;
				}
				scan_barcode_ok_tip();
				if (lowpower_state)
				{
					lowpower_tip();
				}
				rec = rec_searchby_tag(barcode,&index);
				if (rec)
				{
					ret = delete_one_node(index);
					if (ret)
					{
						//��ʾ�û���ɾ��ʧ��
						//@todo...
					}
				}
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//ɾ�����е������¼
				ret = record_clear();
				if (ret)
				{
					//��ʾ�û���ɾ��ʧ��
					//@todo...
				}
				hw_platform_beep_ctrl(300,3000);
				break;
			case EVENT_RESET_KEY_PRESS:
				//@todo...
				break;
			case EVENT_BT_CONNECTED:
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				//�л���USB HIDģʽ
				exit_from_Memory_Mode();
				last_state = STATE_Memory_Mode;
				device_current_state = STATE_HID_Mode;
				enter_into_USB_HID_Mode();
				break;
			case EVENT_USB_CABLE_REMOVE:
				break;
			case EVENT_LOW_POWER:
				lowpower_tip();
				break;
			default:
				break;
			}
		}
		else if(device_current_state ==  STATE_HID_Mode)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
				ret = scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);
				hw_platform_stop_led_blink(LED_GREEN);
				if(ret !=0)	//ɨ������
				{
					break;
				}

				scan_barcode_ok_tip();
				//��ɨ�赽������ͨ��HID �ӿڷ��ͳ�ȥ
				barcode_hid_send(barcode);
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				hw_platform_stop_led_blink(LED_GREEN);
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//�����б�����Memory�е����룬ȫ���ϴ���PC
				hw_platform_beep_ctrl(300,3000);
				cnt = record_module_count();
				for (i = 0; i < cnt;i++)
				{
					rec = get_node((i==0)?0:2,0);
					if (rec)
					{
						barcode_hid_send(((TBATCH_NODE*)rec)->barcode);
					}
					else
					{
						//��ʾ�û���ȡʧ��һ��
						//@todo...
					}
				}
				hw_platform_beep_ctrl(300,3000);
				break;
			case EVENT_RESET_KEY_PRESS:
				//@todo...
				break;
			case EVENT_BT_CONNECTED:
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				break;
			case EVENT_USB_CABLE_REMOVE:
				//�л���֮ǰ��״̬
				exit_from_USB_HID_Mode();
				device_current_state = last_state;
				if (device_current_state == STATE_Memory_Mode)
				{
					enter_into_Memory_Mode();
				}
				else if (device_current_state == STATE_BT_Mode_WaitPair)
				{
					enter_into_BT_Mode(2);
				}
				else
				{
					enter_into_BT_Mode(0);
				}
				break;
			case EVENT_LOW_POWER:
				break;
			default:
				break;
			}
		}
	}
}

/**
* @brief	��ȡ�����첽�¼����߳�
* @note     ����һЩ�첽�¼��ļ�����жϷ��������post����������߳�post����
*			���磺�����¼����ڶ�ʱ���жϵķ������post����
*				  USB�ߵĲ��루ʵ������USB HID device��ö�ٳɹ�������USB���жϷ���������
*				  �������ӵ�״̬�仯��������ģ���ά���̸߳���
*				  ���߳�ֻ����һЩ�����Ե�״̬����¼�����ص����͡�USB���Ƿ񱻰γ�
*/
void Event_capture_thread(void *p)
{
#ifdef DEBUG_VER
	printf("Enter into Event_capture_thread!\r\n");
#endif
	while (1)
	{
		if (device_current_state == STATE_HID_Mode)
		{
			lowpower_state = 0;
			lowpower_cnt = 0;
			//�ж�USB�ߵİγ�
			if (bDeviceState == UNCONNECTED)
			{
#ifdef DEBUG_VER
				printf("usb cable remove detected!\r\n");
#endif
				OSQPost(pEvent_Queue,(void*)EVENT_USB_CABLE_REMOVE);
			}

			if (hw_platform_ChargeState_Detect())
			{
				//������
				hw_platform_led_ctrl(LED_RED,0);
			}

			OSTimeDlyHMSM(0,0,0,50);	//50ms��Ƶ�����д��߳�
		}
		else
		{
			//�жϵ�ص�����
			if (hw_platform_get_PowerClass() == 0)
			{
				lowpower_cnt++;
				if (lowpower_cnt>10)
				{
#ifdef DEBUG_VER
					printf("low power detected!\r\n");
#endif
					if (lowpower_state == 0)
					{
						OSQPost(pEvent_Queue,(void*)EVENT_LOW_POWER);
						lowpower_state = 1;
					}
					
				}
			}
			else
			{
				lowpower_cnt = 0;
			}

			if (bDeviceState == CONFIGURED)
			{
#ifdef DEBUG_VER
				printf("USB HID Enum OK detected!\r\n");
#endif
				OSQPost(pEvent_Queue,(void*)EVENT_USB_CABLE_INSERT);
			}
			OSTimeDlyHMSM(0,0,0,50);	//50ms��Ƶ�����д��߳�
		}
	}
}


/*
 * @brief ����ģ��ά���߳�
 * @note  ���߳���Ҫ�����������
 *        1 : �������ģ���Ƿ��з�������״̬�仯��ָʾ�źŻ����������⵽�ˣ������¼���Ϣ����
 *        2 ������Ƿ���������Ҫͨ������ģ�鷢�ͣ�����оͷ��ͳ�ȥ
*/
void BT_Daemon_thread(void *p)
{
	int ret;
        unsigned int len;
	unsigned char	err;
	unsigned char	*pbarcode;

	ret = WBTD_init();
	if (ret)
	{
		WBTD_Reset();
		ret = WBTD_init();
		assert(ret == 0);
	}

#ifdef DEBUG_VER
	printf("WBTD init Success!\r\n");
#endif

	while (1)
	{
		ret = WBTD_got_notify_type();
		if ((ret == 1) || (ret == 2))
		{
#ifdef DEBUG_VER
			printf("WBTD got notify = %s!\r\n",(ret==1)?"Connected":"Disconnect");
#endif
			OSQPost(pEvent_Queue,(void*)((ret == 1)?EVENT_BT_CONNECTED:EVENT_BT_DISCONNECTED));
		}

		pbarcode = (unsigned char*)OSQPend(pBarcode_Queue,20,&err);
		if (pbarcode)
		{
#ifdef DEBUG_VER
			printf("WBTD got data(%s) to send!\r\n",pbarcode);
#endif
			if (WBTD_hid_send(pbarcode,strlen((char const*)pbarcode),&len))
			{
				//����ʧ��Ӧ����ô����ʲô��������ô????!!!!
				//@todo...
#ifdef DEBUG_VER
				printf("WBTD send data Fail!\r\n");
#endif
			}

			pull_barcode_from_cash(pbarcode);
			//OSTimeDlyHMSM(0,0,0,50);

#ifdef DEBUG_VER
			printf("WBTD send data Success!\r\n");
#endif
		}
	}
}


/*
 * @brief����U��ģʽ
*/
void u_disk_proc(void)
{
	//g_mass_storage_device_type = MASSTORAGE_DEVICE_TYPE_SPI_FLASH;
	//usb_device_init(USB_MASSSTORAGE);

	OSSchedLock();

	while(hw_platform_USBcable_Insert_Detect() == 1)
	{	
		if(bDeviceState != CONFIGURED)
		{
			break;
		}

		delay_ms(1);
	}

	OSSchedUnlock();
}

/*
 * @brief �͵�����ʾ,��Ƴ���5S����˸
 * @return 0:��ʾ�ڼ�û��USB�ߵĲ���		1:��ʾ�ڼ���USB�ߵĲ���
*/
int lowpower_tip(void)
{
	int i;
	OSSchedLock();
	hw_platform_start_led_blink(LED_RED,5);
	for (i = 0; i<20;i++)
	{
		hw_platform_beep_ctrl(50,1000);
		if (hw_platform_USBcable_Insert_Detect())
		{
			hw_platform_stop_led_blink(LED_RED);
			OSSchedUnlock();
			return 1;
		}
	}
	hw_platform_stop_led_blink(LED_RED);
	OSSchedUnlock();
	return 0;
}

/*
 * @brief ϵͳ�������ʾ
*/
void system_err_tip(void)
{
	while(1)
	{
		//@todo...
	}
}


// Cortex System Control register address
#define SCB_SysCtrl					((u32)0xE000ED10)
// SLEEPDEEP bit mask
#define SysCtrl_SLEEPDEEP_Set		((u32)0x00000004)

/*
 * @brief ��ʼ���߳�
 */
void app_init_thread(void *p)
{
	int ret;
#ifdef DEBUG_VER
	printf("app init thread startup...\r\n");
#endif

	OS_CPU_SysTickInit();

	app_init();

	Keypad_Init();

	ret = record_module_init();
	if (ret != 0)
	{
		system_err_tip();
	}

	if (recover_record_by_logfile())
	{
		system_err_tip();
	}

	if (ReadTerminalPara())
	{
		if (DefaultTerminalPara())
		{
			system_err_tip();
		}
	}

	if (g_param.last_state == 1)
	{
		device_current_state = STATE_Memory_Mode;	//�ѻ�״̬
		enter_into_Memory_Mode();
	}
	else
	{
		device_current_state = STATE_BT_Mode_Disconnect;	//����ģʽδ����״̬
		enter_into_BT_Mode(0);
	}

	scanner_mod_init();

	usb_device_init(USB_KEYBOARD);

	OSTaskCreateExt(State_Machine_thread,
		(void *)0,
		&thread_statemachine_stk[STACK_SIZE_TASKSM-1],
		8,
		8,
		&thread_statemachine_stk[0],
		STACK_SIZE_TASKSM,
		(void *)0,
		(INT16U)(OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR));

	OSTaskCreateExt(Event_capture_thread,
		(void *)0,
		&thread_eventcapture_stk[STACK_SIZE_TASKEC-1],
		7,
		7,
		&thread_eventcapture_stk[0],
		STACK_SIZE_TASKEC,
		(void *)0,
		(INT16U)(OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR));

	OSTaskCreateExt(BT_Daemon_thread,
		(void *)0,
		&thread_bt_stk[STACK_SIZE_TASKBT-1],
		6,
		6,
		&thread_bt_stk[0],
		STACK_SIZE_TASKBT,
		(void *)0,
		(INT16U)(OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR));

	OSTimeDlyHMSM(0,0,0,10);
	OSTaskDel(OS_PRIO_SELF);
}

/*
 * @brief ����Ӧ��
 */
void app_startup(void)
{
	memset((void*)thread_eventcapture_stk, 0xAA, sizeof(thread_eventcapture_stk));
	memset((void*)thread_statemachine_stk, 0xBB, sizeof(thread_statemachine_stk));
	memset((void*)thread_bt_stk, 0xCC, sizeof(thread_bt_stk));

	OSInit();

	OSDebugInit();

	p_init_thread_stk = (OS_STK*)Jmalloc(STACK_SIZE_TASKINI*sizeof(OS_STK));
	assert(p_init_thread_stk != 0);

	OSTaskCreateExt(app_init_thread,
		(void *)0,
		&p_init_thread_stk[STACK_SIZE_TASKINI-1],
		5,
		5,
		&p_init_thread_stk[0],
		STACK_SIZE_TASKINI,
		(void *)0,
		(INT16U)(OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR));

	OSStart();
}

