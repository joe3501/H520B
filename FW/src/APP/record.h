
#ifndef _RECORD_H_
#define _RECORD_H_


//#define  T5_SD_DEBUG		//�����˺꣬��������T������DEBUG�Ĺ���	
//#define REC_DEBUG



//�ļ�ϵͳ���þ���·��,�����ļ�ϵͳ�ر���֧�ֳ��ļ����Ĺ��ܣ������ļ������ܳ���8���ֽ�
#define batch_dirctory				"/batch"				//�����ѻ���¼�����ļ���

//�����ļ���
#define barcode_hash_table_file			"/barhash.tbl"
#define batch_list_file					"/batch.lst"
#define batch_inf_file					"/batch.inf"

//#define zd_rec_file						"/ZD.rec"

#define log_file						"/log.tmp"		//��־�ļ� --- �ϵ籣������һЩ�ļ�����֮ǰ���Ƚ�һЩ�ؼ���Ϣд����ļ�


//����Լ�¼�ļ��Ĳ�������
#define OP_TYPE_ADD_NODE			1		//����һ���ڵ�
#define OP_TYPE_CLEAR_NODE			2		//������нڵ�
#define OP_TYPE_DEL_NODE			3		//ɾ��һ���ڵ�
#define OP_TYPE_REPLACE_NODE		4		//�滻�ڵ�
#define OP_TYPE_WRITE_BIN_FILE		5		//д͸���������ļ�




#define TAG_BARCODE				0x01
#define TAG_INDEX				0x02

//����ÿ�������¼���ļ�����hash���ļ��Ĵ�С���ļ�ʵ�ʴ�СӦ����hash���С*4��
#define HASH_TABLE_SIZE		2048

#define RECORD_MAX_SIZE		110		//��ļ�¼��С


#pragma pack(1)
/**
* @brief	�̵���Ϣ��¼��ϸ
*/

/**
* @brief	�ѻ���¼��ϸ
*/
typedef struct  
{
	unsigned int			checkvalue;					// 4�ֽ�	0	B		�˷ݽṹ��У��ֵ crc32        
	unsigned char			barcode[84];				// 84�ֽ�	4				
	unsigned int			flag;						// 4�ֽ�	88
	unsigned int			by_barcode_prev;			// 4�ֽ�	92
	unsigned int			by_barcode_next;			// 4�ֽ�    96
	unsigned int			by_index_prev;				// 4�ֽ�	100
	unsigned int			by_index_next;				// 4�ֽ�    104
}TBATCH_NODE;


/**
* @brief �ڵ��޸�ʱ����Ҫ������޸�tableԪ�صĽṹ��
*/
typedef struct 
{
	unsigned char	keyspec_type;			//�޸ĵĹؼ�������
	unsigned int	old_keyspec_hash;		//�ɵĹؼ��ֵ�hashֵ
	unsigned int	new_keyspec_hash;		//�µĹؼ��ֵ�hashֵ
}TNODE_MODIFY_INFO;


/**
* @brief ����log file�ļ�ʱ��Ҫ����ı�����Ϣ���ݽṹ
*/
typedef struct 
{
	unsigned int	op_type;			//��������
	unsigned int	rec_type;			//��¼����
	unsigned int	param1;
	unsigned int	param2;
	void			*param3;
	unsigned int	param4;
	void			*param5;
}TLOG_FILE_PARAM;

#pragma pack()


extern unsigned int g_rec_offset;		//����record_add֮��������¼������ƫ��

//�������¼�������ֵ
#define  BATCH_LIST_MAX_CNT		45000				//�ѻ��嵥��¼�����ֵ


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
unsigned char *rec_searchby_tag(unsigned char *barcode,int *index);

int delete_one_node(unsigned int index);
unsigned char* get_node(unsigned int mode,unsigned int offset);

int get_file_size(const unsigned char *dir);
int read_rec_file(const unsigned char *dir,unsigned int offset,unsigned int len,unsigned char *pdata);
int write_rec_file(const unsigned char *dir,unsigned int offset,unsigned int len,unsigned char *pdata);
int check_record_dir(void);
int check_updatefile(void);
int del_update_bin(void);
int recover_record_by_logfile(void);
int clear_log_file(int mode);

#ifdef T5_SD_DEBUG 
void debug_out(unsigned char* out,unsigned int len,unsigned char format);
void delete_debug_file(void);
void close_debug_file(void);

extern unsigned int debug_file_status;
#endif

#endif

