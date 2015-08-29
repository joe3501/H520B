#ifndef _APP_H_
#define _APP_H_

//主状态机定义：
#define STATE_BT_Mode_Disconnect			1		//蓝牙模式未连接状态
#define STATE_BT_Mode_Connect				2		//蓝牙模式连接状态
#define STATE_BT_Mode_WaitPair				3		//蓝牙模式待配对状态
#define STATE_Memory_Mode					4		//脱机存储模式
#define STATE_HID_Mode						5		//HID键盘模式


//事件列表：
//按键事件
#define EVENT_SCAN_KEY_SINGLE_CLICK				1
#define EVENT_SCAN_KEY_LONG_PRESS				2
#define EVENT_ERASE_KEY_SINGLE_CLICK			3
#define EVENT_ERASE_KEY_LONG_PRESS				4
#define EVENT_RESET_KEY_PRESS					5

//USB线的插入拔出
#define EVENT_USB_CABLE_INSERT					6
#define EVENT_USB_CABLE_REMOVE					7

//蓝牙连接状态的变化
#define EVENT_BT_CONNECTED						8
#define EVENT_BT_DISCONNECTED					9

//低电压
#define EVENT_LOW_POWER							10



#endif
