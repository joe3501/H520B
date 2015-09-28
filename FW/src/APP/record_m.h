
#ifndef _RECORD_H_
#define _RECORD_H_

#define MAX_BARCODE_LEN			80

#define		RECORD_ZONE_NUM						1		//��¼������

#define TAG_BARCODE				0x01

#define  BATCH_LIST_MAX_CNT		45000				//�ѻ��嵥��¼�����ֵ

/**
* @brief	�����ѻ���¼�ڵ�
*/
typedef struct T_batch_node
{
	unsigned char		flag;
	unsigned char		barcode[MAX_BARCODE_LEN];				
}TBATCH_NODE;


#pragma pack()
				


#define ABSOLUTE_SEARCH		0		//��������
#define RELATIVE_SEARCH		1		//�������

#define SEARCH_MODE_FIRST	0		//������һ��
#define SEARCH_MODE_PREV	1		//����ǰһ��
#define SEARCH_MODE_NEXT	2		//������һ��



int record_module_init(void);
unsigned char *record_module_read(unsigned int index);
int record_add(unsigned char *precord);
int record_module_count(void);
int record_clear(void);
unsigned char *rec_searchby_tag(unsigned char *in_tag,int *index);
int record_module_replace(int index,unsigned char *p_new_record);
int delete_one_node(unsigned int index);
unsigned char* get_node(unsigned int mode,unsigned int offset);
int recover_record_by_logfile(void);
unsigned char* record_logical_read(unsigned int logical_index);

void record_m_test(void);

#endif
