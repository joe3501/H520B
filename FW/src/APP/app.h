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
#define EVENT_SCAN_KEY_DOUBLE_CLICK				(EVENT_SCAN_KEY_SINGLE_CLICK+1)
#define EVENT_SCAN_KEY_LONG_PRESS				(EVENT_SCAN_KEY_DOUBLE_CLICK+1)
#define EVENT_ERASE_KEY_SINGLE_CLICK			(EVENT_SCAN_KEY_LONG_PRESS+1)
#define EVENT_ERASE_KEY_LONG_PRESS				(EVENT_ERASE_KEY_SINGLE_CLICK+1)
#define EVENT_RESET_KEY_PRESS					(EVENT_ERASE_KEY_LONG_PRESS+1)

//USB�ߵĲ���γ�
#define EVENT_USB_CABLE_INSERT					(EVENT_RESET_KEY_PRESS+1)
#define EVENT_USB_CABLE_REMOVE					(EVENT_USB_CABLE_INSERT+1)

//��������״̬�ı仯
#define EVENT_BT_CONNECTED						(EVENT_USB_CABLE_REMOVE+1)
#define EVENT_BT_DISCONNECTED					(EVENT_BT_CONNECTED+1)

//�͵�ѹ
#define EVENT_LOW_POWER							(EVENT_BT_DISCONNECTED+1)

//�����жϻ�ȡ��ɨ�����룬����ɨ����������Ӳ��IOֱ�������������жϽ��շ������룬����ɨ�������ȡ������Ҳ����һ���첽�¼�
#define EVENT_SCAN_GOT_BARCODE							(EVENT_LOW_POWER+1)
#define EVENT_ERASE_GOT_BARCODE							(EVENT_SCAN_GOT_BARCODE+1)

void app_startup(void);

#endif
