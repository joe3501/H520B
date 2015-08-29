#ifndef _APP_H_
#define _APP_H_

//��״̬�����壺
#define STATE_BT_Mode_Disconnect			1		//����ģʽδ����״̬
#define STATE_BT_Mode_Connect				2		//����ģʽ����״̬
#define STATE_BT_Mode_WaitPair				3		//����ģʽ�����״̬
#define STATE_Memory_Mode					4		//�ѻ��洢ģʽ
#define STATE_HID_Mode						5		//HID����ģʽ


//�¼��б�
//�����¼�
#define EVENT_SCAN_KEY_SINGLE_CLICK				1
#define EVENT_SCAN_KEY_LONG_PRESS				2
#define EVENT_ERASE_KEY_SINGLE_CLICK			3
#define EVENT_ERASE_KEY_LONG_PRESS				4
#define EVENT_RESET_KEY_PRESS					5

//USB�ߵĲ���γ�
#define EVENT_USB_CABLE_INSERT					6
#define EVENT_USB_CABLE_REMOVE					7

//��������״̬�ı仯
#define EVENT_BT_CONNECTED						8
#define EVENT_BT_DISCONNECTED					9

//�͵�ѹ
#define EVENT_LOW_POWER							10



#endif
