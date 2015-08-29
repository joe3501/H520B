#include "ucos_ii.h"
#include "app.h"

//定义主状态机线程与蓝牙模块线程之间通讯的IPC对象
#define BARCODE_CASH_NUM	50			//定义等待蓝牙模块线程发送的条码缓冲区为50个条码
#define MAX_BARCODE_LEN		80			//定义条码最大长度为80个字节

static void *barcode_pool[BARCODE_CASH_NUM]	//保存获取到的条码字符串的地址数组
static unsigned char barcode_cash[BARCODE_CASH_NUM][MAX_BARCODE_LEN+2];	//最后一个字节表示此行数据是否被送入pool中待发送了
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

/**
* @brief	将条码推入条码的静态缓冲区，返回保存的地址
* @param[in] unsigned char* barcode    需要缓存的条码
* @return   缓存的地址
* @note 策略:只要找到一个空位置就放进去，每一列的最有一个字节为0表示该位置是空的
*											     最后一个字节0x55表示该位置已经缓存了条码
*/
unsigned char * push_barcode_into_cash(unsigned char* barcode)
{
	unsigned int	i;
	for (i = 0; i < BARCODE_CASH_NUM;i++)
	{
		if (barcode_cash[i][MAX_BARCODE_LEN+1] == 0)
		{
			if (strlen(barcode) > MAX_BARCODE_LEN)
			{
				memcpy(barcode_cash[i],barcode,MAX_BARCODE_LEN);
				barcode_cash[i][MAX_BARCODE_LEN] = 0;
			}
			else
			{
				strcpy(barcode_cash[i],barcode);
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
	unsigned int	i;
	ASSERT((int)barcode_addr >= (int)barcode_cash[0]);
	ASSERT((int)barcode_addr <= (int)barcode_cash[BARCODE_CASH_NUM-1]);
	ASSERT((int(barcode_addr) - (int)barcode_cash[0])%(MAX_BARCODE_LEN+2) == 0);

	//memset(barcode_addr,0,MAX_BARCODE_LEN+2);
	barcode_addr[MAX_BARCODE_LEN+1] = 0;		//把标志恢复为0即可
	return;
}



/**
* @brief	进入Momoery模式时，需要进行的一些设置
*/
static void enter_into_Memory_Mode(void)
{
	//@todo...
}

/**
* @brief	退出Momoery模式时，需要进行的一些设置
*/
static void exit_from_Memory_Mode(void)
{
	//@todo...
}

/**
* @brief	进入USB HID模式时，需要进行的一些设置
*/
static void enter_into_USB_HID_Mode(void)
{
	//@todo...
}

/**
* @brief	退出USB HID模式时，需要进行的一些设置
*/
static void exit_from_USB_HID_Mode(void)
{
	//@todo...
}

/**
* @brief	进入BT模式时，需要进行的一些设置
* @param[in] unsigned char	child_state		0: disconnect  1：connected  2：:waitpair
*/
static void enter_into_BT_Mode(unsigned char child_state)
{
	//@todo...
}

/**
* @brief	退出BT模式时，需要进行的一些设置
* @param[in] unsigned char	child_state		0: disconnect  1：connected  2：:waitpair
*/
static void exit_from_BT_Mode(unsigned char child_state)
{
	//@todo...
}

/**
* @brief	保存条码到Memory记录区
* @param[in] unsigned char	 *barcode	条码
* @return 0： 保存成功   else:保存失败
*/
static int barcode_record_add(unsigned char *barcode)
{
	//@todo...
}

/**
* @brief	删除保存条码的记录区中最后一条记录（最新的一条记录）
* @return 0： 删除成功   else:删除失败
*/
static int barcode_record_del_the_last(void)
{
	//@todo...
}

/**
* @brief	删除所有保存的条码
* @return 0： 删除成功   else:删除失败
*/
static int barcode_record_clear(void)
{
	//@todo...
}

/**
* @brief	返回保存的条码数
* @return	条码数量
*/
static unsigned int barcode_record_count(void)
{
	//@todo...
}

/**
* @brief	读取保存的条码
* @param[in]: unsigned char *buf		读取缓冲区
* @paran[in]: unsigned int	offset		偏移   1 --- [count]  1：表示最老的记录   [count]:最新的记录
* @return 0： 读取成功   else:读取失败
*/
static int barcode_record_read(unsigned char *buf,unsigned int offset)
{
	//@todo...
}


/**
* @brief	通过USB HID发送条码
*/
static void barcode_hid_send(unsigned char* barcode)
{
	//@todo...
}

/**
* @brief	应用的初始化
*/
void app_init(void)
{
	//创建一个消息队列，用于将事件捕获线程和蓝牙模块线程获取的异步事件通知给主状态机线程
	pEvent_Queue = OSQCreate(&event_pool,EVENT_CASH_NUM);
	ASSERT(pEvent_Queue != (OS_EVENT*)0);

	//创建一个消息队列，用于将主状态机线程获取的条码传送到蓝牙模块线程.
	pBarcode_Queue =OSQCreate(&barcode_pool,BARCODE_CASH_NUM);
	ASSERT(pBarcode_Queue != (OS_EVENT*)0);
	memset(barcode_cash,0,BARCODE_CASH_NUM*(MAX_BARCODE_LEN+2));

	//创建一个信号量，用于IO中断通知事件捕获线程，有外部IO产生，需要事件捕获线程开始采取捕获事件的动作
	pIOSem = OSSemCreate(0);
	ASSERT(pIOSem != (OS_EVENT*)0);
}

/**
* @brief	维护主状态机的线程
*/
void State_Machine_thread(void)
{
	unsigned int	i,cnt,event;
	unsigned char	err;
	unsigned char	barcode[MAX_BARCODE_LEN+1];
	unsigned char   codetype[20];
	unsigned int    codelen;
	int ret;
	unsigned char	*addr;
	unsigned int	current_state = STATE_BT_Mode_Disconnect;	//蓝牙模式未连接状态
	unsigned int	last_state;
	
	while(1)
	{
		event = (unsigned int)OSQPend(pEvent_Queue,0,&err);
		ASSERT(err == OS_ERR_NONE);
		if(current_state ==  STATE_BT_Mode_Disconnect)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
				scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);	//扫描条码
				//只是扫描到条码而已，什么都不做
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//切换到Memory Mode
				exit_from_BT_Mode(0);
				current_state = STATE_Memory_Mode;
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//切换到配对模式
				exit_from_BT_Mode(0);
				current_state = STATE_BT_Mode_WaitPair;
				enter_into_BT_Mode(2);
				break;
			case EVENT_RESET_KEY_PRESS:
				break;
			case EVENT_BT_CONNECTED:
				//切换到蓝牙连接模式
				exit_from_BT_Mode(0);
				current_state = STATE_BT_Mode_Connect;
				enter_into_BT_Mode(1);
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				//切换到USB HID模式
				exit_from_BT_Mode(0);
				last_state = STATE_BT_Mode_Disconnect;
				current_state = STATE_HID_Mode;
				enter_into_USB_HID_Mode();
				break;
			case EVENT_USB_CABLE_REMOVE:
				break;
			case EVENT_LOW_POWER:
				//@todo...
				break;
			default:
				break;
			}
		}
		else if(current_state ==  STATE_BT_Mode_Connect)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
				//扫描条码
				if (scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen) != 0)
				{
					break;
				}

				//扫描到条码了
				//将获取到的条码先push到cash缓存起来，然后Post到系统的
				//Queue，由蓝牙模块线程负责去发送到主机
repost:
				ret = OSQPost(pBarcode_Queue,(void*)push_barcode_into_cash(barcode));
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
						ASSERT(0);	//系统错误
					}
				}
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//切换到Memory mode
				exit_from_BT_Mode(1);
				current_state = STATE_Memory_Mode;
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//进入配对模式
				exit_from_BT_Mode(1);
				current_state = STATE_BT_Mode_WaitPair;
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
				current_state = STATE_BT_Mode_Disconnect;
				enter_into_BT_Mode(0);
				break;
			case EVENT_USB_CABLE_INSERT:
				//切换到USB HID模式
				exit_from_BT_Mode(1);
				last_state = STATE_BT_Mode_Disconnect;
				current_state = STATE_HID_Mode;
				enter_into_USB_HID_Mode();
				break;
			case EVENT_USB_CABLE_REMOVE:
				break;
			case EVENT_LOW_POWER:
				//@todo...
				break;
			default:
				break;
			}
		}
		else if(current_state ==  STATE_BT_Mode_WaitPair)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
				scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);	//扫描条码
				//只是扫描到条码而已，什么都不做
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//切换到Memory Mode
				exit_from_BT_Mode(2);
				current_state = STATE_Memory_Mode;
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
				current_state = STATE_BT_Mode_Connect;
				enter_into_BT_Mode(1);
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				//切换到USB HID模式
				exit_from_BT_Mode(2);
				last_state = STATE_BT_Mode_WaitPair;
				current_state = STATE_HID_Mode;
				enter_into_USB_HID_Mode();
				break;
			case EVENT_USB_CABLE_REMOVE:
				break;
			case EVENT_LOW_POWER:
				//@todo...
				break;
			default:
				break;
			}
		}
		else if(current_state ==  STATE_Memory_Mode)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
				//扫描条码
				if (scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen) != 0)
				{
					break;
				}

				//扫描到条码了
				//将获取到的条码保存到memory
				ret = barcode_record_add(barcode);
				if (ret)
				{
					//记录保存失败，给出提示给用户
					//@todo...

				}
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//切换至蓝牙模式
				exit_from_Memory_Mode();
				current_state = STATE_BT_Mode_Disconnect;
				enter_into_BT_Mode(0);
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				//@todo...
				//删除最后一笔资料
				ret = barcode_record_del_the_last();
				if (ret)
				{
					//提示用户，删除失败
					//@todo...
				}
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//删除所有的条码记录
				ret = barcode_record_clear();
				if (ret)
				{
					//提示用户，删除失败
					//@todo...
				}
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
				current_state = STATE_HID_Mode;
				enter_into_USB_HID_Mode();
				break;
			case EVENT_USB_CABLE_REMOVE:
				break;
			case EVENT_LOW_POWER:
				//@todo...
				break;
			default:
				break;
			}
		}
		else if(current_state ==  STATE_HID_Mode)
		{
			switch(event)
			{
			case EVENT_SCAN_KEY_SINGLE_CLICK:
				if(scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen)!=0)	//扫描条码
				{
					break;
				}

				//将扫描到的条码通过HID 接口发送出去
				barcode_hid_send(barcode);
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//将所有保存在Memory中的条码，全部上传到PC
				//@todo...
				cnt = barcode_record_count();
				for (i = 1; i <= cnt;i++)
				{
					if (barcode_record_read(barcode,i) == 0)
					{
						barcode_hid_send(barcode);
					}
					else
					{
						//提示用户读取失败一次
						//@todo...
					}
				}
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
				current_state = last_state;
				if (current_state == STATE_Memory_Mode)
				{
					enter_into_Memory_Mode();
				}
				else if (current_state == STATE_BT_Mode_WaitPair)
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
void Event_capture_thread(void)
{
	unsigned char err;

	while (1)
	{
		//判断电池电量低
		//@todo...
		//判断USB线的拔出
		//@todo...

		OSTimeDlyHMSM(0,0,0,100);	//100ms的频率运行此线程
	}
}



