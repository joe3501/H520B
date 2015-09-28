/**
* @file app.c
* @brief H520B 蓝牙条码数据采集器项目APP
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

//定义主状态机线程与蓝牙模块线程之间通讯的IPC对象
#define BARCODE_CASH_NUM	15			//定义等待蓝牙模块线程发送的条码缓冲区为15个条码
#define MAX_BARCODE_LEN		80			//定义条码最大长度为80个字节

//define the stack size of each task
#define STACK_SIZE_TASKEC			128	
#define STACK_SIZE_TASKSM			356
#define STACK_SIZE_TASKBT			128
#define STACK_SIZE_TASKINI			224

static OS_STK	thread_eventcapture_stk[STACK_SIZE_TASKEC];		//the stack of the Event_capture_thread
static OS_STK	thread_statemachine_stk[STACK_SIZE_TASKSM];		//the stack of the State_Machine_thread
static OS_STK	thread_bt_stk[STACK_SIZE_TASKBT];				//the stack of the BT_Daemon_thread
static OS_STK	*p_init_thread_stk;								//此线程的栈动态创建，线程结束自己释放

static void *barcode_pool[BARCODE_CASH_NUM];	//保存获取到的条码字符串的地址数组
static unsigned char barcode_cash[BARCODE_CASH_NUM][MAX_BARCODE_LEN+2];	//最后一个字节表示此行数据是否被送入pool中待发送了
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
//定义条码的静态缓冲区，以免动态分配内存
static OS_EVENT	*pBarcode_Queue;			//barcode消息队列


#define EVENT_CASH_NUM		8			//定义事件的缓存数量
//定义事件监测线程与主状态机之间通讯的IPC对象
static void *event_pool[EVENT_CASH_NUM];		//事件缓存
OS_EVENT	*pEvent_Queue;			//事件消息队列

static OS_EVENT *pIOSem;				//IO信号量
//

unsigned int	device_current_state;		//设备主状态机

unsigned int	keypress_timeout;


void u_disk_proc(void);
int lowpower_tip(void);
void system_err_tip(void);

extern void EnterLowPowerMode(void);
extern void ExitLowPowerMode(void);


/**
* @brief	扫描条码成功的提示
*/
static inline void scan_barcode_ok_tip(void)
{
	hw_platform_led_ctrl(LED_YELLOW,1);
	hw_platform_beep_motor_ctrl(100,4000);
	OSTimeDlyHMSM(0,0,0,10);
	hw_platform_led_ctrl(LED_YELLOW,0);
}

/**
* @brief	将条码推入条码的静态缓冲区，返回保存的地址
* @param[in] unsigned char* barcode				需要缓存的条码
* @return   缓存的地址
* @note 策略:只要找到一个空位置就放进去，每一列的最后一个字节为0表示该位置是空的
*											     最后一个字节0x55表示该位置已经缓存了条码
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
			barcode_cash[i][MAX_BARCODE_LEN+1] = 0x55;		//表示已经缓存数据了
			return (void*)barcode_cash[i];
		}
	}

	return (void*)0;
}


/**
* @brief	将已经发送出去的缓存地址pull出来
* @param[in] unsigned char* barcode_addr    一个合法的缓存区的地址
* @return   none
*/
void pull_barcode_from_cash(unsigned char* barcode_addr)
{
	assert((int)barcode_addr >= (int)barcode_cash[0]);
	assert((int)barcode_addr <= (int)barcode_cash[BARCODE_CASH_NUM-1]);
	assert(((int)barcode_addr - (int)barcode_cash[0])%(MAX_BARCODE_LEN+2) == 0);

	//memset(barcode_addr,0,MAX_BARCODE_LEN+2);
	barcode_addr[MAX_BARCODE_LEN+1] = 0;		//把标志恢复为0即可
	return;
}



/**
* @brief	进入Memory模式时，需要进行的一些设置
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
* @brief	退出Memory模式时，需要进行的一些设置
*/
static inline void exit_from_Memory_Mode(void)
{
#ifdef DEBUG_VER
	printf("exit from Memory Mode\r\n");
#endif
	//@todo...
}

/**
* @brief	进入USB HID模式时，需要进行的一些设置
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
* @brief	退出USB HID模式时，需要进行的一些设置
*/
static inline void exit_from_USB_HID_Mode(void)
{
#ifdef DEBUG_VER
	printf("exit from USB HID Mode\r\n");
#endif
	hw_platform_led_ctrl(LED_RED,0);
}

/**
* @brief	进入BT模式时，需要进行的一些设置
* @param[in] unsigned char	child_state		0: disconnect  1：connected  2：:waitpair
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
* @brief	退出BT模式时，需要进行的一些设置
* @param[in] unsigned char	child_state		0: disconnect  1：connected  2：:waitpair
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
		WBTD_Reset();//主动断开与蓝牙主机的连接
		
	}
	else
	{
		hw_platform_stop_led_blink(LED_BLUE);
	}
}


/**
* @brief	通过USB HID发送条码
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

	memcpy(key_value_report,"\x00\x00\x28",3);	//换行

	SendData_To_PC(key_value_report, 3);
	SendData_To_PC("\x00\x00\x00", 3);	//弹起
	OSSchedUnlock();
}

/**
* @brief	应用的初始化
*/
void app_init(void)
{
	//创建一个消息队列，用于将事件捕获线程和蓝牙模块线程获取的异步事件通知给主状态机线程
	pEvent_Queue = OSQCreate((void**)&event_pool,EVENT_CASH_NUM);
	assert(pEvent_Queue != (OS_EVENT*)0);

	//创建一个消息队列，用于将主状态机线程获取的条码传送到蓝牙模块线程.
	pBarcode_Queue =OSQCreate((void**)&barcode_pool,BARCODE_CASH_NUM);
	assert(pBarcode_Queue != (OS_EVENT*)0);
	memset(barcode_cash,0,BARCODE_CASH_NUM*(MAX_BARCODE_LEN+2));

	//创建一个信号量，用于IO中断通知事件捕获线程，有外部IO产生，需要事件捕获线程开始采取捕获事件的动作
	pIOSem = OSSemCreate(0);
	assert(pIOSem != (OS_EVENT*)0);

	lowpower_state = 0;
	lowpower_cnt = 0;
}

/**
* @brief	维护主状态机的线程
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

	Jfree(p_init_thread_stk);	//退出初始化线程时，释放自己的任务栈

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
				ret = scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);	//扫描条码
				hw_platform_stop_led_blink(LED_GREEN);
				if (ret == 0)
				{
					scan_barcode_ok_tip();
				}

				if (lowpower_state)
				{
					lowpower_tip();
				}
				//只是扫描到条码而已，什么都不做
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//切换到Memory Mode
				hw_platform_stop_led_blink(LED_GREEN);
				exit_from_BT_Mode(0);
				device_current_state = STATE_Memory_Mode;
				hw_platform_beep_ctrl(300,3000);
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//切换到配对模式
				exit_from_BT_Mode(0);
				device_current_state = STATE_BT_Mode_WaitPair;
				enter_into_BT_Mode(2);
				break;
			case EVENT_RESET_KEY_PRESS:
				break;
			case EVENT_BT_CONNECTED:
				//切换到蓝牙连接模式
				exit_from_BT_Mode(0);
				device_current_state = STATE_BT_Mode_Connect;
				enter_into_BT_Mode(1);
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				//切换到USB HID模式
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
				//扫描条码
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
				//扫描到条码了
				//将获取到的条码先push到cash缓存起来，然后Post到系统的
				//Queue，由蓝牙模块线程负责去发送到主机
repost:
				ret = OSQPost(pBarcode_Queue,(void*)push_barcode_into_cash((unsigned char*)barcode));
				if(ret != OS_ERR_NONE)
				{
					if(ret == OS_ERR_Q_FULL || ret == OS_ERR_PEVENT_NULL)
					{
						//如果队列满了或者空事件时，那么需要延时重试
						OSTimeDlyHMSM(0,0,0,100);
						goto repost;
					}
					else
					{
						assert(0);	//系统错误
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
				//切换到Memory mode
				exit_from_BT_Mode(1);
				device_current_state = STATE_Memory_Mode;
				hw_platform_beep_ctrl(300,3000);
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				//WBTD_hid_send_test();
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//进入配对模式
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
				//切换到蓝牙断开状态
				exit_from_BT_Mode(1);
				device_current_state = STATE_BT_Mode_Disconnect;
				enter_into_BT_Mode(0);
				break;
			case EVENT_USB_CABLE_INSERT:
				//切换到USB HID模式
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
				ret = scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);	//扫描条码
				hw_platform_stop_led_blink(LED_GREEN);
				if (ret == 0)
				{
					scan_barcode_ok_tip();
				}
				if (lowpower_state)
				{
					lowpower_tip();
				}
				//只是扫描到条码而已，什么都不做
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				hw_platform_stop_led_blink(LED_GREEN);
				//切换到Memory Mode
				exit_from_BT_Mode(2);
				device_current_state = STATE_Memory_Mode;
				hw_platform_beep_ctrl(300,3000);
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//已经是配对模式，什么都不做
				break;
			case EVENT_RESET_KEY_PRESS:
				//@todo...
				break;
			case EVENT_BT_CONNECTED:
				//切换到蓝牙连接状态
				exit_from_BT_Mode(2);
				device_current_state = STATE_BT_Mode_Connect;
				enter_into_BT_Mode(1);
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				//切换到USB HID模式
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
				//扫描条码
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
				//扫描到条码了
				//将获取到的条码保存到memory
				memset((void*)&batch_node,0,sizeof(TBATCH_NODE));
				strcpy((char*)batch_node.barcode,(char const*)barcode);
				ret = record_add((unsigned char*)&batch_node);
				if (ret)
				{
					//记录保存失败，给出提示给用户
					//@todo...

				}
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				hw_platform_stop_led_blink(LED_GREEN);
				//切换至蓝牙模式
				exit_from_Memory_Mode();
				device_current_state = STATE_BT_Mode_Disconnect;
				enter_into_BT_Mode(0);
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				//删除扫到的条码对应的最后一笔资料
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
						//提示用户，删除失败
						//@todo...
					}
				}
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//删除所有的条码记录
				ret = record_clear();
				if (ret)
				{
					//提示用户，删除失败
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
				//切换到USB HID模式
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
				if(ret !=0)	//扫描条码
				{
					break;
				}

				scan_barcode_ok_tip();
				//将扫描到的条码通过HID 接口发送出去
				barcode_hid_send(barcode);
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				hw_platform_stop_led_blink(LED_GREEN);
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//将所有保存在Memory中的条码，全部上传到PC
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
						//提示用户读取失败一次
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
				//切换到之前的状态
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
* @brief	获取部分异步事件的线程
* @note     另外一些异步事件的检测在中断服务程序中post或者另外的线程post出来
*			比如：按键事件会在定时器中断的服务程序post出来
*				  USB线的插入（实际上是USB HID device被枚举成功）会在USB的中断服务程序给出
*				  蓝牙连接的状态变化会在蓝牙模块的维护线程给出
*				  此线程只负责一些周期性的状态检查事件，电池电量低、USB线是否被拔出
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
			//判断USB线的拔出
			if (bDeviceState == UNCONNECTED)
			{
#ifdef DEBUG_VER
				printf("usb cable remove detected!\r\n");
#endif
				OSQPost(pEvent_Queue,(void*)EVENT_USB_CABLE_REMOVE);
			}

			if (hw_platform_ChargeState_Detect())
			{
				//充电完成
				hw_platform_led_ctrl(LED_RED,0);
			}

			OSTimeDlyHMSM(0,0,0,50);	//50ms的频率运行此线程
		}
		else
		{
			//判断电池电量低
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
			OSTimeDlyHMSM(0,0,0,50);	//50ms的频率运行此线程
		}
	}
}


/*
 * @brief 蓝牙模块维护线程
 * @note  此线程需要完成两个任务：
 *        1 : 监测蓝牙模块是否有返回连接状态变化的指示信号回来，如果监测到了，发出事件消息出来
 *        2 ：检测是否有数据需要通过蓝牙模块发送，如果有就发送出去
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
				//发送失败应该怎么处理，什么都不做了么????!!!!
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
 * @brief进入U盘模式
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
 * @brief 低电量提示,红灯持续5S的闪烁
 * @return 0:提示期间没有USB线的插入		1:提示期间有USB线的插入
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
 * @brief 系统错误的提示
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
 * @brief 初始化线程
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
		device_current_state = STATE_Memory_Mode;	//脱机状态
		enter_into_Memory_Mode();
	}
	else
	{
		device_current_state = STATE_BT_Mode_Disconnect;	//蓝牙模式未连接状态
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
 * @brief 启动应用
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

