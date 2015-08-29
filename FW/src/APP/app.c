#include "ucos_ii.h"
#include "app.h"

//������״̬���߳�������ģ���߳�֮��ͨѶ��IPC����
#define BARCODE_CASH_NUM	50			//����ȴ�����ģ���̷߳��͵����뻺����Ϊ50������
#define MAX_BARCODE_LEN		80			//����������󳤶�Ϊ80���ֽ�

static void *barcode_pool[BARCODE_CASH_NUM]	//�����ȡ���������ַ����ĵ�ַ����
static unsigned char barcode_cash[BARCODE_CASH_NUM][MAX_BARCODE_LEN+2];	//���һ���ֽڱ�ʾ���������Ƿ�����pool�д�������
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

/**
* @brief	��������������ľ�̬�����������ر���ĵ�ַ
* @param[in] unsigned char* barcode    ��Ҫ���������
* @return   ����ĵ�ַ
* @note ����:ֻҪ�ҵ�һ����λ�þͷŽ�ȥ��ÿһ�е�����һ���ֽ�Ϊ0��ʾ��λ���ǿյ�
*											     ���һ���ֽ�0x55��ʾ��λ���Ѿ�����������
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
	unsigned int	i;
	ASSERT((int)barcode_addr >= (int)barcode_cash[0]);
	ASSERT((int)barcode_addr <= (int)barcode_cash[BARCODE_CASH_NUM-1]);
	ASSERT((int(barcode_addr) - (int)barcode_cash[0])%(MAX_BARCODE_LEN+2) == 0);

	//memset(barcode_addr,0,MAX_BARCODE_LEN+2);
	barcode_addr[MAX_BARCODE_LEN+1] = 0;		//�ѱ�־�ָ�Ϊ0����
	return;
}



/**
* @brief	����Momoeryģʽʱ����Ҫ���е�һЩ����
*/
static void enter_into_Memory_Mode(void)
{
	//@todo...
}

/**
* @brief	�˳�Momoeryģʽʱ����Ҫ���е�һЩ����
*/
static void exit_from_Memory_Mode(void)
{
	//@todo...
}

/**
* @brief	����USB HIDģʽʱ����Ҫ���е�һЩ����
*/
static void enter_into_USB_HID_Mode(void)
{
	//@todo...
}

/**
* @brief	�˳�USB HIDģʽʱ����Ҫ���е�һЩ����
*/
static void exit_from_USB_HID_Mode(void)
{
	//@todo...
}

/**
* @brief	����BTģʽʱ����Ҫ���е�һЩ����
* @param[in] unsigned char	child_state		0: disconnect  1��connected  2��:waitpair
*/
static void enter_into_BT_Mode(unsigned char child_state)
{
	//@todo...
}

/**
* @brief	�˳�BTģʽʱ����Ҫ���е�һЩ����
* @param[in] unsigned char	child_state		0: disconnect  1��connected  2��:waitpair
*/
static void exit_from_BT_Mode(unsigned char child_state)
{
	//@todo...
}

/**
* @brief	�������뵽Memory��¼��
* @param[in] unsigned char	 *barcode	����
* @return 0�� ����ɹ�   else:����ʧ��
*/
static int barcode_record_add(unsigned char *barcode)
{
	//@todo...
}

/**
* @brief	ɾ����������ļ�¼�������һ����¼�����µ�һ����¼��
* @return 0�� ɾ���ɹ�   else:ɾ��ʧ��
*/
static int barcode_record_del_the_last(void)
{
	//@todo...
}

/**
* @brief	ɾ�����б��������
* @return 0�� ɾ���ɹ�   else:ɾ��ʧ��
*/
static int barcode_record_clear(void)
{
	//@todo...
}

/**
* @brief	���ر����������
* @return	��������
*/
static unsigned int barcode_record_count(void)
{
	//@todo...
}

/**
* @brief	��ȡ���������
* @param[in]: unsigned char *buf		��ȡ������
* @paran[in]: unsigned int	offset		ƫ��   1 --- [count]  1����ʾ���ϵļ�¼   [count]:���µļ�¼
* @return 0�� ��ȡ�ɹ�   else:��ȡʧ��
*/
static int barcode_record_read(unsigned char *buf,unsigned int offset)
{
	//@todo...
}


/**
* @brief	ͨ��USB HID��������
*/
static void barcode_hid_send(unsigned char* barcode)
{
	//@todo...
}

/**
* @brief	Ӧ�õĳ�ʼ��
*/
void app_init(void)
{
	//����һ����Ϣ���У����ڽ��¼������̺߳�����ģ���̻߳�ȡ���첽�¼�֪ͨ����״̬���߳�
	pEvent_Queue = OSQCreate(&event_pool,EVENT_CASH_NUM);
	ASSERT(pEvent_Queue != (OS_EVENT*)0);

	//����һ����Ϣ���У����ڽ���״̬���̻߳�ȡ�����봫�͵�����ģ���߳�.
	pBarcode_Queue =OSQCreate(&barcode_pool,BARCODE_CASH_NUM);
	ASSERT(pBarcode_Queue != (OS_EVENT*)0);
	memset(barcode_cash,0,BARCODE_CASH_NUM*(MAX_BARCODE_LEN+2));

	//����һ���ź���������IO�ж�֪ͨ�¼������̣߳����ⲿIO��������Ҫ�¼������߳̿�ʼ��ȡ�����¼��Ķ���
	pIOSem = OSSemCreate(0);
	ASSERT(pIOSem != (OS_EVENT*)0);
}

/**
* @brief	ά����״̬�����߳�
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
	unsigned int	current_state = STATE_BT_Mode_Disconnect;	//����ģʽδ����״̬
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
				scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);	//ɨ������
				//ֻ��ɨ�赽������ѣ�ʲô������
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//�л���Memory Mode
				exit_from_BT_Mode(0);
				current_state = STATE_Memory_Mode;
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//�л������ģʽ
				exit_from_BT_Mode(0);
				current_state = STATE_BT_Mode_WaitPair;
				enter_into_BT_Mode(2);
				break;
			case EVENT_RESET_KEY_PRESS:
				break;
			case EVENT_BT_CONNECTED:
				//�л�����������ģʽ
				exit_from_BT_Mode(0);
				current_state = STATE_BT_Mode_Connect;
				enter_into_BT_Mode(1);
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				//�л���USB HIDģʽ
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
				//ɨ������
				if (scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen) != 0)
				{
					break;
				}

				//ɨ�赽������
				//����ȡ����������push��cash����������Ȼ��Post��ϵͳ��
				//Queue��������ģ���̸߳���ȥ���͵�����
repost:
				ret = OSQPost(pBarcode_Queue,(void*)push_barcode_into_cash(barcode));
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
						ASSERT(0);	//ϵͳ����
					}
				}
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//�л���Memory mode
				exit_from_BT_Mode(1);
				current_state = STATE_Memory_Mode;
				enter_into_Memory_Mode();
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//�������ģʽ
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
				//�л��������Ͽ�״̬
				exit_from_BT_Mode(1);
				current_state = STATE_BT_Mode_Disconnect;
				enter_into_BT_Mode(0);
				break;
			case EVENT_USB_CABLE_INSERT:
				//�л���USB HIDģʽ
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
				scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen);	//ɨ������
				//ֻ��ɨ�赽������ѣ�ʲô������
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//�л���Memory Mode
				exit_from_BT_Mode(2);
				current_state = STATE_Memory_Mode;
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
				current_state = STATE_BT_Mode_Connect;
				enter_into_BT_Mode(1);
				break;
			case EVENT_BT_DISCONNECTED:
				break;
			case EVENT_USB_CABLE_INSERT:
				//�л���USB HIDģʽ
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
				//ɨ������
				if (scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen) != 0)
				{
					break;
				}

				//ɨ�赽������
				//����ȡ�������뱣�浽memory
				ret = barcode_record_add(barcode);
				if (ret)
				{
					//��¼����ʧ�ܣ�������ʾ���û�
					//@todo...

				}
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				//�л�������ģʽ
				exit_from_Memory_Mode();
				current_state = STATE_BT_Mode_Disconnect;
				enter_into_BT_Mode(0);
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				//@todo...
				//ɾ�����һ������
				ret = barcode_record_del_the_last();
				if (ret)
				{
					//��ʾ�û���ɾ��ʧ��
					//@todo...
				}
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//ɾ�����е������¼
				ret = barcode_record_clear();
				if (ret)
				{
					//��ʾ�û���ɾ��ʧ��
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
				//�л���USB HIDģʽ
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
				if(scanner_get_barcode(barcode,MAX_BARCODE_LEN,codetype,&codelen)!=0)	//ɨ������
				{
					break;
				}

				//��ɨ�赽������ͨ��HID �ӿڷ��ͳ�ȥ
				barcode_hid_send(barcode);
				break;
			case EVENT_SCAN_KEY_LONG_PRESS:
				break;
			case EVENT_ERASE_KEY_SINGLE_CLICK:
				break;
			case EVENT_ERASE_KEY_LONG_PRESS:
				//�����б�����Memory�е����룬ȫ���ϴ���PC
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
						//��ʾ�û���ȡʧ��һ��
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
				//�л���֮ǰ��״̬
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
* @brief	��ȡ�����첽�¼����߳�
* @note     ����һЩ�첽�¼��ļ�����жϷ��������post����������߳�post����
*			���磺�����¼����ڶ�ʱ���жϵķ������post����
*				  USB�ߵĲ��루ʵ������USB HID device��ö�ٳɹ�������USB���жϷ���������
*				  �������ӵ�״̬�仯��������ģ���ά���̸߳���
*				  ���߳�ֻ����һЩ�����Ե�״̬����¼�����ص����͡�USB���Ƿ񱻰γ�
*/
void Event_capture_thread(void)
{
	unsigned char err;

	while (1)
	{
		//�жϵ�ص�����
		//@todo...
		//�ж�USB�ߵİγ�
		//@todo...

		OSTimeDlyHMSM(0,0,0,100);	//100ms��Ƶ�����д��߳�
	}
}



