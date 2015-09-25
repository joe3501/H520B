
#ifndef _RECORD_H_
#define _RECORD_H_


//#define  T5_SD_DEBUG		//开启此宏，打开了利用T卡进行DEBUG的功能	
//#define REC_DEBUG



//文件系统采用绝对路径,由于文件系统关闭了支持长文件名的功能，所以文件名不能超过8个字节
#define batch_dirctory				"/batch"				//保存脱机记录的子文件夹

//定义文件名
#define barcode_hash_table_file			"/barhash.tbl"
#define batch_list_file					"/batch.lst"
#define batch_inf_file					"/batch.inf"

//#define zd_rec_file						"/ZD.rec"

#define log_file						"/log.tmp"		//日志文件 --- 断电保护，在一些文件操作之前会先将一些关键信息写入此文件


//定义对记录文件的操作类型
#define OP_TYPE_ADD_NODE			1		//增加一个节点
#define OP_TYPE_CLEAR_NODE			2		//清除所有节点
#define OP_TYPE_DEL_NODE			3		//删除一个节点
#define OP_TYPE_REPLACE_NODE		4		//替换节点
#define OP_TYPE_WRITE_BIN_FILE		5		//写透明二进制文件




#define TAG_BARCODE				0x01
#define TAG_INDEX				0x02

//定义每个保存记录的文件夹中hash表文件的大小（文件实际大小应该是hash表大小*4）
#define HASH_TABLE_SIZE		2048

#define RECORD_MAX_SIZE		110		//最长的记录大小


#pragma pack(1)
/**
* @brief	盘点信息记录明细
*/

/**
* @brief	脱机记录明细
*/
typedef struct  
{
	unsigned int			checkvalue;					// 4字节	0	B		此份结构的校验值 crc32        
	unsigned char			barcode[84];				// 84字节	4				
	unsigned int			flag;						// 4字节	88
	unsigned int			by_barcode_prev;			// 4字节	92
	unsigned int			by_barcode_next;			// 4字节    96
	unsigned int			by_index_prev;				// 4字节	100
	unsigned int			by_index_next;				// 4字节    104
}TBATCH_NODE;


/**
* @brief 节点修改时，需要构造的修改table元素的结构体
*/
typedef struct 
{
	unsigned char	keyspec_type;			//修改的关键字类型
	unsigned int	old_keyspec_hash;		//旧的关键字的hash值
	unsigned int	new_keyspec_hash;		//新的关键字的hash值
}TNODE_MODIFY_INFO;


/**
* @brief 保存log file文件时需要输入的保存信息数据结构
*/
typedef struct 
{
	unsigned int	op_type;			//操作类型
	unsigned int	rec_type;			//记录类型
	unsigned int	param1;
	unsigned int	param2;
	void			*param3;
	unsigned int	param4;
	void			*param5;
}TLOG_FILE_PARAM;

#pragma pack()


extern unsigned int g_rec_offset;		//返回record_add之后，新增记录的物理偏移

//定义各记录数的最大值
#define  BATCH_LIST_MAX_CNT		45000				//脱机清单记录的最大值


#define ABSOLUTE_SEARCH		0		//绝对搜索
#define RELATIVE_SEARCH		1		//相对搜索

#define SEARCH_MODE_FIRST	0		//搜索第一个
#define SEARCH_MODE_PREV	1		//搜索前一个
#define SEARCH_MODE_NEXT	2		//搜索后一个
				
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

