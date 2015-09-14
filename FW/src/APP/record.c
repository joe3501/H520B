/**
 * @file record.c
 * @brief H520B项目记录管理模块
 * @version 1.0
 * @author joe
 * @date 2015年09月10日
 * @note 利用SPI Flash 实现的FAT文件系统保存记录
 *
 * 此代码为深圳合杰电子有限公司项目代码，任何人及公司未经许可不得复制传播，或用于
 * 本公司以外的项目。本司保留一切追究权利。
 *
 * <h1><center>&copy; COPYRIGHT 2015 heroje</center></h1>
 */

#include "record.h"
#include "crc32.h"
#include <string.h>
#include "TimeBase.h"
#include "ff.h"
#include "hash.h"
#include "JMemory.h"

unsigned char			rec_module_buffer[RECORD_MAX_SIZE];	//记录模块的公用buffer	


unsigned int current_node_offset;
unsigned int g_rec_offset;

static unsigned int	prev_node_offset;
static unsigned int	next_node_offset;


static unsigned int log_len;

FIL						file2,file3;
DIR						dir;							//文件夹


#ifdef T5_SD_DEBUG
FIL						debug_file;
unsigned int			debug_file_status;
#endif

extern FIL				file1;
extern FATFS			fs;


typedef struct  
{
	unsigned char					magic[4];
	unsigned int					xor;
	unsigned int					xor_data;
	unsigned int					length;
	unsigned char					OEMName[16];
	unsigned char					Version[16];
	unsigned char					Date[16];
}TPackHeader;

/**
* @brief 初始化哈希表文件
*/
static int init_hash_table_file(FIL* file)
{
	unsigned char	buffer[512];
	unsigned int	r_w_byte_cnt,i;

	f_lseek(file,0);
	memset(buffer,0,512);
	for (i = 0; i< ((HASH_TABLE_SIZE*4)/512);i++)
	{
		if (f_write(file,buffer,512,&r_w_byte_cnt) != FR_OK)
		{
			return -1;
		}

		if (r_w_byte_cnt != 512)
		{
			return -1;
		}
	}

	if (f_truncate(file) != FR_OK)
	{
		return -1;
	}

	f_sync(file);

	return FR_OK;
}

/**
* @brief 初始化序列号信息文件
*/
static int init_data_info_file(FIL* file)
{
	unsigned char	buffer[12];
	unsigned int	r_w_byte_cnt;

	f_lseek(file,0);
	memcpy(buffer,"info",4);
	memset(buffer+4,0,8);
	

	if (f_write(file,buffer,12,&r_w_byte_cnt) != FR_OK)
	{
		return -1;
	}

	if (r_w_byte_cnt != 12)
	{
		return -1;
	}

	if (f_truncate(file) != FR_OK)
	{
		return -1;
	}

	f_sync(file);

	return FR_OK;
}

/**
 * @brief 系统的所有记录模块的初始化
 * @return 0: OK   else: 错误代码
 * @note: 返回值不能很好的定位到具体的错误发生的位置，后续有需要再修改
 */
int record_module_init(void)
{
	unsigned int		j;	
	unsigned char		dir_str[35];
	const unsigned char	*p_hash_table_file[3];

	//f_mount(0, &fs);										// 装载文件系统

	if( f_opendir(&dir,batch_dirctory) != FR_OK )
	{
		//打开记录文件失败或者无法访问SD卡,如果是该文件夹不存在，那么就创建一个新的文件夹
		if (f_mkdir(batch_dirctory) != FR_OK)
		{
			//无法访问SD卡
			return 1;	
		}

		if( f_opendir(&dir,batch_dirctory) != FR_OK )
		{
			//刚创建成功了还打不开，那就诡异了（会出现这个错误吗？？？）
			return 1;
		}
	}

	p_hash_table_file[0] = barcode_hash_table_file;
	p_hash_table_file[1] = batch_inf_file;
	p_hash_table_file[2] = 0;

	j = 0;
	while (p_hash_table_file[j])
	{
		strcpy((char*)dir_str,batch_dirctory);
		strcat((char*)dir_str,(char const*)p_hash_table_file[j]);

		if (f_open(&file1,(const char*)dir_str,FA_OPEN_ALWAYS | FA_WRITE) != FR_OK)
		{
			return 2;
		}

		if (j == 0)
		{
			if (file1.fsize != HASH_TABLE_SIZE*4)
			{
				if (init_hash_table_file(&file1) != FR_OK)
				{
					return 3;
				}
			}
		}
		else if (j == 1)
		{
			if ((file1.fsize < 12)||(file1.fsize > 12 + 4*BATCH_LIST_MAX_CNT))
			{
				if (init_data_info_file(&file1) != FR_OK)
				{
					return 3;
				}
			}
		}

		f_sync(&file1);
		j++;
	}

	current_node_offset = 0;
	return 0;
}

/**
 * @brief 读脱机记录，该记录必须存在，此函数中不作判断
 * @param[in] int index 记录的索引(最老的记录的索引号是1)
 * @return 返回记录的地址
 */
unsigned char *record_module_read(unsigned int index)
{
	unsigned char	dir_str[35];
	unsigned int	node_size;
	unsigned char	*pBuf;
	unsigned int	checkvalue;

	if (index == 0)
	{
		return (unsigned char*)0;
	}
	
	pBuf = rec_module_buffer;

	strcpy((char*)dir_str,batch_dirctory);
	strcat((char*)dir_str,(char const*)batch_list_file);
	node_size = sizeof(TBATCH_NODE);


	if (f_open(&file1,(char const*)dir_str,FA_OPEN_EXISTING | FA_READ) != FR_OK)
	{
		return (unsigned char*)0;
	}

	if (file1.fsize < node_size*index)
	{
		f_close(&file1);
		return (unsigned char*)0;
	}

	f_lseek(&file1,node_size*(index-1));

	if (f_read(&file1,pBuf,node_size,&node_size) != FR_OK)
	{
		f_close(&file1);
		return (unsigned char*)0;
	}

	checkvalue = crc32(0,pBuf+4,node_size-4);
	if (memcmp((unsigned char*)&checkvalue,pBuf,4))
	{
		//校验值不对
		f_close(&file1);
		return (unsigned char*)0;
	}

	f_close(&file1);
	return pBuf;
}

//计算校验值并填回去
static calc_check_value(unsigned char *precord)
{
	unsigned int		checkvalue;

	//计算此份记录的校验值，放到记录的最前面4个字节
	checkvalue = crc32(0,precord + 4,sizeof(TBATCH_NODE) - 4);

	memcpy(precord,(unsigned char*)&checkvalue,4);
}
/**
* @brief 增加一个脱机节点到记录文件之后更新记录文件的链表
* @param[in] FIL *file						记录文件指针,已经打开的文件
* @param[in] unsigned char key_type			关键字类型
* @param[in] unsigned char	*p_node			要新增的节点指针	
* @param[in] unsigned int	header			链表首地址
* @param[in] unsigned int	node_offset		要增加节点的偏移
* @return 0:成功
*        else:失败
* @note 只有商品信息节点是双向链表，所以增加节点时需要维护双向链表，其余节点都是单向链表
*/
static int  update_link_info_after_addNode(FIL *file,unsigned char key_type,unsigned char*p_node,unsigned int header,unsigned int node_offset)
{
	unsigned int	rec_size,next_node_offset,current_offset,tmp;
	unsigned char	buffer[RECORD_MAX_SIZE];		//如果一个节点的大小超过了512，那么还需要增加此临时空间

	rec_size = sizeof(TBATCH_NODE);

	next_node_offset = header;
	while (next_node_offset)
	{
		current_offset = next_node_offset;
		f_lseek(file,(next_node_offset-1)*rec_size);		//文件指针定位到链表首记录的偏移处，注意链表中记录的偏移是该节点结束的偏移
		if (f_read(file,buffer,rec_size,&tmp) != FR_OK)
		{
			return -2;
		}

		if (key_type == 1)
		{
			next_node_offset = ((TBATCH_NODE*)buffer)->by_barcode_next;
			if (next_node_offset == 0)
			{
				((TBATCH_NODE*)buffer)->by_barcode_next = node_offset;
				((TBATCH_NODE*)p_node)->by_barcode_prev = current_offset;
				((TBATCH_NODE*)p_node)->by_barcode_next = 0;
			}
		}
		else
		{
			next_node_offset = ((TBATCH_NODE*)buffer)->by_index_next;
			if (next_node_offset == 0)
			{
				((TBATCH_NODE*)buffer)->by_index_next = node_offset;
				((TBATCH_NODE*)p_node)->by_index_prev = current_offset;
				((TBATCH_NODE*)p_node)->by_index_next = 0;
			}
		}
	}

	calc_check_value(buffer);
	f_lseek(file,(current_offset-1)*rec_size);		//文件指针重新定位到链表最后一个节点处
	if (f_write(file,buffer,rec_size,&tmp) != FR_OK)
	{
		return -3;
	}

	if (tmp != rec_size)
	{
		return -3;
	}

	f_sync(file);
	
	return 0;
}


//将准备进行的文件操作的相关信息保存到日志文件
//note : 对于每一种操作来说参数列表中的参数的含义都不相同
//       op_type == OP_TYPE_ADD_NODE 时,		param1 表示记录类型, param2 表示新增节点的偏移  param3 表示新增节点的长度  param4 指向新增节点数据
//		 op_type == OP_TYPE_CLEAR_NODE 时,		param1 表示记录类型, param2 无意义					param3 无意义 param4 无意义
//		 op_type == OP_TYPE_DEL_NODE 时,		param1 表示记录类型, param2 表示要删除节点的偏移	param3 无意义 param4 无意义
//		 op_type == OP_TYPE_REPLACE_NODE 时,	param1 表示记录类型, param2 表示要替换节点的偏移	param3 表示替换节点的长度  param4 指向新替换节点数据
//		 op_type == OP_TYPE_WRITE_BIN_FILE 时,	param1 表示记录类型, param2 表示文件类型	param3 无意义  param4 无意义
static int save_log_file(void *data,unsigned int len)
{
	unsigned int tmp;
	unsigned int llen;
	unsigned char tmp_buffer[530];
	//将该操作保存到日志文件中
	if (f_open(&file3,log_file,FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
	{
		return -1;
	}

	if (log_len == 0)
	{
		llen = len+4;
	}
	else
	{
		llen = log_len+len;
	}
	
	f_lseek(&file3,file3.fsize-log_len);

	if (log_len)
	{
		if (f_read(&file3,(void*)&tmp_buffer,log_len,&tmp) != FR_OK)
		{
			f_close(&file3);
			return -1;
		}

		if (tmp != log_len)
		{		
			f_close(&file3);
			return -1;
		}
	}
	memcpy((void*)tmp_buffer,(void*)&llen,4);
	if (log_len)
	{
		memcpy((void*)(tmp_buffer+log_len),data,len);
	}
	else
	{
		memcpy((void*)(tmp_buffer+4),data,len);
	}
	
	f_lseek(&file3,file3.fsize-log_len);
	if(f_write(&file3,tmp_buffer,llen,&tmp) != FR_OK)
	{
		f_close(&file3);
		return -1;
	}

	if (tmp != llen)
	{
		f_close(&file3);
		return -1;
	}
	f_close(&file3);		//关闭日志文件
	log_len = llen;
        return 0;
}


//清除日志文件最新添加的日志
int clear_log_file(int mode)
{
	if (f_open(&file3,log_file,FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
	{
		return -1;
	}

	if (mode)
	{
		f_lseek(&file3,0);
	}
	else
	{
		if (file3.fsize >= log_len)
		{
			f_lseek(&file3,file3.fsize-log_len);
		}
	}
	f_truncate(&file3);

	f_close(&file3);
	return 0;
}

/**
* @brief 增加一条脱机记录
* @param[in] unsigned char *precord 记录指针
* @return 0:成功
*        else:失败
* @note 此函数已经增加断电保护
*/
int record_add(unsigned char *precord)
{
	unsigned char	dir_str[35];
	unsigned char	inf_file_str[35];
	unsigned int	rec_offset,i;
	unsigned int	node_size,tmp;
	unsigned long	hash_value[2];
	const unsigned char	*p_hash_table_file[3];
	unsigned int		link_end;		//链表的尾地址
	//const unsigned char	*target_dir;
	//unsigned int		target_max_cnt;
	unsigned int		invalid_node_offset = 0;
	unsigned int		temp;
	int					err_code = 0;
	unsigned int		log_data[3];

	log_len = 0;

	//target_dir = batch_dirctory;
	strcpy((char*)dir_str,batch_dirctory);
	strcat((char*)dir_str,(char const*)batch_list_file);
	node_size = sizeof(TBATCH_NODE);
	hash_value[0] = HashString(((TBATCH_NODE*)precord)->barcode,0);	//计算该商品条码的hash值
	hash_value[1] = 2;
	p_hash_table_file[0] = barcode_hash_table_file;
	p_hash_table_file[1] = batch_inf_file;
	p_hash_table_file[2] = 0;
	//target_max_cnt = BATCH_LIST_MAX_CNT;		

	strcpy((char*)inf_file_str,batch_dirctory);
	strcat((char*)inf_file_str,(char const*)batch_inf_file);

	if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_READ) != FR_OK)
	{
		return -2;
	}

	if (file1.fsize > 12)
	{
		f_lseek(&file1,file1.fsize-4);
		if ((f_read(&file1,(void*)&invalid_node_offset,4,&tmp) != FR_OK) ||(tmp != 4))		//获取保存在序列号信息文件中的某一个已经被删除节点的偏移
		{
			f_close(&file1);
			return -4;
		}
	}

	f_close(&file1);


	//打开保存相应记录的节点文件
	if (f_open(&file1,(const char*)dir_str,FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
	{
		return -2;
	}

	if (invalid_node_offset)
	{
		rec_offset = invalid_node_offset;		//如果是新增序列号节点，并且记录文件中存在已经被删除的节点，那么新增的节点
	}
	else
	{
		rec_offset = file1.fsize/node_size;		//获取记录文件当前已经保存的记录数
		if (rec_offset < BATCH_LIST_MAX_CNT)
		{
			rec_offset += 1;	
		}
		else
		{
			//rec_offset = 1;		//如果记录数达到了上限值，那么无法增加新记录
			f_close(&file1);
			return -20;
		}
	}

	g_rec_offset = rec_offset;
#ifdef REC_DEBUG
	debug_out("\x0d\x0a",2,1);
	debug_out("Add Node:",strlen("Add Node:"),1);
	debug_out((unsigned char*)&rec_offset,4,0);
#endif

	log_data[0] = OP_TYPE_ADD_NODE;
	log_data[1] = 0;
	log_data[2] = rec_offset;


	if (save_log_file((void*)log_data,12))		//状态0，LOG文件中只保存了操作类型、记录类型、目标记录偏移
	{
		err_code = -10;
		goto err_handle;
	}

	//计算此记录需要计算的关键字的hash值
	i = 0;
	while (p_hash_table_file[i])
	{
		strcpy((char*)dir_str,batch_dirctory);
		strcat((char*)dir_str,(char const*)p_hash_table_file[i]);

		//打开相应的hash_table文件
		if (f_open(&file2,(const char*)dir_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
		{
			err_code = -4;
			goto err_handle;
		}

		f_lseek(&file2,4*(hash_value[i]%HASH_TABLE_SIZE));		//定位到该hash值在hash表中对应的偏移处

		//读取此hash值对应的链表的尾地址
		if (f_read(&file2,(void*)&link_end,4,&tmp) != FR_OK)
		{
			err_code = -5;
			goto err_handle;
		}
#ifdef REC_DEBUG
		debug_out(" LinkEnd=",strlen(" LinkEnd="),1);
		debug_out((unsigned char*)&link_end,4,0);
#endif
		log_data[0] = hash_value[i];
		log_data[1] = link_end;
		if(save_log_file((void*)log_data,8))		//状态1或者状态2
		{
			err_code = -10;
			goto err_handle;
		}
		
		f_lseek(&file2,4*(hash_value[i]%HASH_TABLE_SIZE));		//定位到该hash值在hash表中对应的偏移处

		//更新该链表的尾节点
		if (f_write(&file2,(void*)&rec_offset,4,&tmp) != FR_OK)
		{
			err_code = -6;
			goto err_handle;
		}

		if (tmp != 4)
		{
			err_code = -6;
			goto err_handle;
		}

		f_sync(&file2);

		if (i == 1)
		{
			f_lseek(&file2,4);
			if (f_read(&file2,(void*)&temp,4,&tmp) != FR_OK)
			{
				err_code = -7;
				goto err_handle;
			}

			if (temp == 0)
			{
				//如果链表的首节点还是0，那么需要更新链表的首节点
				f_lseek(&file2,4);
				if (f_write(&file2,(void*)&rec_offset,4,&tmp) != FR_OK)
				{
					err_code = -8;
					goto err_handle;
				}

				if (tmp != 4)
				{
					err_code = -8;
					goto err_handle;
				}
			}
		}

		f_close(&file2);	//hash表文件可以关闭了

		if (link_end)
		{
			//如果链表不是空的，那么需要更新链表的信息(如果是商品信息节点，还需要更新当前节点的信息，因为商品信息链表是双向链表)
			if (update_link_info_after_addNode(&file1,(i+1),precord,link_end,rec_offset))
			{
				err_code = -9;
				goto err_handle;
			}
		}

		i++;
	}

	calc_check_value(precord);

	if (save_log_file((void*)precord,node_size))
	{
		err_code = -8;
		goto err_handle;
	}

	f_lseek(&file1,(rec_offset-1)*node_size);		//将文件指针移到文件末尾
	if (f_write(&file1,precord,node_size,&tmp) != FR_OK)
	{
		err_code = -3;	 
		goto err_handle;
	}

	if (tmp != node_size)
	{
		err_code = -3;	 
		goto err_handle;
	}

	//保存节点的文件可以关闭了
	f_close(&file1);	

	if (invalid_node_offset)
	{
		if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_WRITE | FA_READ) != FR_OK)
		{
			return -2;
		}

		f_lseek(&file1,file1.fsize-4);
		f_truncate(&file1);		//将记录在信息文件中的已经删除的节点偏移清掉。
		f_close(&file1);
	}
	
	clear_log_file(0);
	return 0;

err_handle:

	f_close(&file1);
	return err_code;
}


/**
* @brief 得到脱机记录总条数
* @return 0...LOGIC_RECORD_BLOCK_SIZE
*/
int record_module_count(void)
{
	unsigned char dir_str[35];
	unsigned char inf_file_str[35];
	int		cnt;

	strcpy((char*)dir_str,batch_dirctory);
	strcat((char*)dir_str,(char const*)batch_list_file);

	if (f_open(&file1,(const char*)dir_str,FA_OPEN_ALWAYS | FA_READ) != FR_OK)
	{
		return -1;
	}

	cnt = file1.fsize / sizeof(TBATCH_NODE);
	f_close(&file1);

	strcpy((char*)inf_file_str,batch_dirctory);
	strcat((char*)inf_file_str,(char const*)batch_inf_file);

	if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_READ) != FR_OK)
	{
		return -1;
	}

	if (cnt >= (file1.fsize - 12)/4)
	{
		cnt -= (file1.fsize - 12)/4;		//减去无效的节点，理论上无效的节点的偏移都记录在信息文件中，所以直接减去在信息文件中记录的无效节点个数，即表示在节点文件中还存在的节点个数。
		f_close(&file1);
	}
	else
	{
		//理论上应该不会出现这种情况！万一真出现了呢？尼玛，那只能来杀手锏了，直接将记录文件全部初始化
		f_close(&file1);
		record_clear();
		cnt = 0;
	}

	return cnt;
}

/**
* @brief 清除脱机记录
* @return 0：成功  -1：失败
*/
int record_clear(void)
{
	unsigned char	dir_str[35];
	const unsigned char	*p_hash_table_file[3];
	int i;
	unsigned int		log_data[2];

	strcpy((char*)dir_str,batch_dirctory);
	strcat((char*)dir_str,(char const*)batch_list_file);
	p_hash_table_file[0] = barcode_hash_table_file;
	p_hash_table_file[1] = batch_inf_file;
	p_hash_table_file[2] = 0;


	if (f_open(&file1,(const char*)dir_str,FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
	{
		return -1;
	}

	f_lseek(&file1,0);		//文件指针移动到文件开始

	//将文件截断到文件开始
	if (f_truncate(&file1) != FR_OK)
	{
		f_close(&file1);
		return -2;
	}

	f_close(&file1);



	log_data[0] = OP_TYPE_CLEAR_NODE;
	log_data[1] = 0;
	log_len = 0;
	if (save_log_file((void*)log_data,8))		//状态0，LOG文件中只保存了操作类型、记录类型
	{
		return -4;
	}

	i = 0;
	while (p_hash_table_file[i])
	{
		strcpy((char*)dir_str,batch_dirctory);
		strcat((char*)dir_str,(char const*)p_hash_table_file[i]);

		//打开相应的hash_table文件
		if (f_open(&file2,(const char*)dir_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
		{
			return -3;
		}

		if (i == 0)
		{
			init_hash_table_file(&file2);	
		}
		else if(i == 1)
		{
			init_data_info_file(&file2);	
		}
		f_close(&file2);
        i++;
	}

	clear_log_file(0);		//清除日志
	return 0;
}

/**
* @brief 根据条码关键字查询脱机记录，返回记录节点的偏移
* @param[in] unsigned char *barcode				要匹配的字符串
* @param[out] unsigned char *outbuf				搜索到的记录缓冲区
* @return 搜索结果	=0:没有搜索到该交易记录  > 0:搜索到的记录的偏移   < 0: 错误类型
* @note  
*/
static int rec_search(unsigned char *barcode,unsigned char *outbuf)
{
	unsigned int hash_value,link_end,tmp;
	unsigned char dir_str[35];
	unsigned char	buffer[RECORD_MAX_SIZE];		//如果一个节点的大小超过了512，那么还需要增加此临时空间
	unsigned int	node_size;				//节点大小

	node_size = sizeof(TBATCH_NODE);
	memset(outbuf,0,node_size);		//清掉返回数据的缓冲区
	strcpy((char*)dir_str,batch_dirctory);


	//如果要搜索的关键字类型是条码

	//step1: 计算关键字的hash值
	hash_value = HashString(barcode,0);	

	//step2: 根据hash值查找到与该关键字具有相同hash值模值的链表尾地址
	strcat((char*)dir_str,(char const*)barcode_hash_table_file);		
	if (f_open(&file1,(char const*)dir_str,FA_OPEN_ALWAYS | FA_READ) != FR_OK)
	{
		return -1;
	}

	f_lseek(&file1,4*(hash_value%HASH_TABLE_SIZE));

	if (f_read(&file1,(void*)&link_end,4,&tmp) != FR_OK)
	{
		return -2;
	}	

	f_close(&file1);

	//根据读取出来的链表首地址查找到第一个与关键字完全匹配的记录，如果需要将完全匹配关键字的所有记录全部都读取出来，需要另外的接口来实现。

	if (link_end)
	{	
		strcpy((char*)dir_str,batch_dirctory);
		strcat((char*)dir_str,(char const*)batch_list_file);

		if (f_open(&file1,(char const*)dir_str,FA_OPEN_ALWAYS | FA_READ) != FR_OK)
		{
			return -3;
		}

		while (link_end)
		{
			f_lseek(&file1,(link_end-1)*node_size);		//文件指针指向该节点的起始位置

			if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK) || (node_size != tmp))
			{
				return -4;
			}

			if (strcmp((char*)barcode,(char const*)((TBATCH_NODE*)buffer)->barcode) == 0)
			{
				memcpy(outbuf,buffer,node_size);
				return link_end;		//搜索到链表中第一个与关键字匹配的节点
			}

			link_end = ((TBATCH_NODE*)buffer)->by_barcode_prev;
		}
	}

	return 0;		//没有搜索到与关键字匹配的记录
}

/**
* @brief 根据条码查询脱机记录，返回记录指针
* @param[in] unsigned char *barcode				要匹配的字符串
* @param[out] int	*index						返回该记录的索引值
* @return 搜索结果	0 没有搜索到记录		else 记录指针
* @note  实现的时候必须进行字符串比较
*/
unsigned char *rec_searchby_tag(unsigned char *barcode, int *index)
{
	unsigned char *pBuf;

	pBuf = rec_module_buffer;
	*index = rec_search(barcode,pBuf);
	if (*index > 0)
	{
		return pBuf;
	}

	return (unsigned char *)0;
}
/**
* @brief 读取序列号节点
* @param[in] unsigned int mode  0:第一个有效节点   1;前一个有效节点   2:后一个有效节点  3:指定节点偏移
* @param[in] unsigned int offset  只有在mode = 3时才有用
*/
unsigned char* get_node(unsigned int mode,unsigned int offset)
{
	unsigned int	node_offset,tmp;
	unsigned char	*pRec = 0;
	unsigned char   inf_file_str[35];
	if (mode == 0)
	{
		current_node_offset = 0;
		
		strcpy((char*)inf_file_str,batch_dirctory);
		strcat((char*)inf_file_str,(char const*)batch_inf_file);

		if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_READ) != FR_OK)
		{
			return (unsigned char*)0;
		}

		f_lseek(&file1,8);
		if ((f_read(&file1,(void*)&node_offset,4,&tmp) != FR_OK)||(tmp != 4))
		{
			f_close(&file1);
			return (unsigned char*)0;
		}

		f_close(&file1);
	}
	else if (1 == mode)
	{
		node_offset = next_node_offset;
	}
	else if (2 == mode)
	{
		node_offset = prev_node_offset;
	}
	else if(3 == mode)
	{
		node_offset = offset;
	}
	else
	{
		return (unsigned char*)0;
	}

	if (node_offset)
	{
		current_node_offset = node_offset;

		pRec = record_module_read(current_node_offset);
		prev_node_offset = ((TBATCH_NODE*)pRec)->by_index_prev;
		next_node_offset = ((TBATCH_NODE*)pRec)->by_index_next;
	}

	return pRec;
}
/**
* @brief 删除脱机记录文件中的某一个节点
* @param[in] unsigned int index	 物理偏移（索引）
*/
int delete_one_node(unsigned int index)
{
	unsigned char	buffer[RECORD_MAX_SIZE];
	unsigned int	tmp,temp;
	unsigned short	prev,next;
	unsigned int	barcode_hash_prev,barcode_hash_next;
	unsigned int	hash_value;
	unsigned char	inf_file_str[35];
	unsigned int	node_size;
	unsigned int	log_data[8];

	//step1:先将该节点标记为无效节点
	strcpy((char*)inf_file_str,batch_dirctory);
	strcat((char*)inf_file_str,(char const*)batch_list_file);
	node_size = sizeof(TBATCH_NODE);

	if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
	{
		return -2;
	}

	f_lseek(&file1,(index-1)*node_size);
	if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
	{
		f_close(&file1);
		return -3;
	}


	((TBATCH_NODE*)buffer)->flag = 0x55;

	prev = ((TBATCH_NODE*)buffer)->by_index_prev;
	next = ((TBATCH_NODE*)buffer)->by_index_next;

	barcode_hash_prev = ((TBATCH_NODE*)buffer)->by_barcode_prev;
	barcode_hash_next = ((TBATCH_NODE*)buffer)->by_barcode_next;
	hash_value = HashString(((TBATCH_NODE*)buffer)->barcode,0);

	f_lseek(&file1,(index-1)*node_size);
	if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
	{
		f_close(&file1);
		return -4;
	}

	if (tmp != node_size)
	{
		f_close(&file1);
		return -5;
	}

	f_sync(&file1);

	log_data[0] = OP_TYPE_DEL_NODE;
	log_data[1] = 0;
	log_data[2] = index;
	log_data[3] = prev;
	log_data[4] = next;
	log_data[5] = barcode_hash_prev;
	log_data[6] = barcode_hash_next;
	log_data[7] = hash_value;
	log_len = 0;
	if (save_log_file((void*)log_data,32))
	{
		return -6;
	}

	//step2:更新该节点前一个节点和后一个节点的链表信息
	if(prev)
	{
		f_lseek(&file1,(prev-1)*node_size);
		if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
		{
			f_close(&file1);
			return -7;
		}


		((TBATCH_NODE*)buffer)->by_index_next = next;

		calc_check_value(buffer);

		f_lseek(&file1,(prev-1)*node_size);
		if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -8;
		}

		if (tmp != node_size)
		{
			f_close(&file1);
			return -9;
		}

		f_sync(&file1);
	}

	if (next)
	{
		f_lseek(&file1,(next-1)*node_size);
		if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
		{
			f_close(&file1);
			return -10;
		}


		((TBATCH_NODE*)buffer)->by_index_prev = prev;

		calc_check_value(buffer);
		f_lseek(&file1,(next-1)*node_size);
		if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -11;
		}

		if (tmp != node_size)
		{
			f_close(&file1);
			return -12;
		}

		f_sync(&file1);
	}

	if(barcode_hash_prev)
	{
		f_lseek(&file1,(barcode_hash_prev-1)*node_size);
		if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
		{
			f_close(&file1);
			return -7;
		}


		((TBATCH_NODE*)buffer)->by_barcode_next = barcode_hash_next;

		calc_check_value(buffer);

		f_lseek(&file1,(barcode_hash_prev-1)*node_size);
		if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -8;
		}

		if (tmp != node_size)
		{
			f_close(&file1);
			return -9;
		}

		f_sync(&file1);
	}

	if (barcode_hash_next)
	{
		f_lseek(&file1,(barcode_hash_next-1)*node_size);
		if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
		{
			f_close(&file1);
			return -10;
		}


		((TBATCH_NODE*)buffer)->by_barcode_prev = barcode_hash_prev;

		calc_check_value(buffer);
		f_lseek(&file1,(barcode_hash_next-1)*node_size);
		if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -11;
		}

		if (tmp != node_size)
		{
			f_close(&file1);
			return -12;
		}

		f_sync(&file1);
	}

	//可以将节点文件关闭了
	f_close(&file1);

	//step3:将删除的节点的偏移记录在信息文件中

	strcpy((char*)inf_file_str,batch_dirctory);
	strcat((char*)inf_file_str,(char const*)batch_inf_file);

	if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
	{
		return -19;
	}

	f_lseek(&file1,file1.fsize);
	if (f_write(&file1,(void*)&index,4,&tmp) != FR_OK)
	{
		f_close(&file1);
		return -20;
	}

	if (tmp != 4)
	{
		f_close(&file1);
		return -21;
	}

	f_sync(&file1);

	//如果删除的节点是链表的首节点，那么还需要更新保存的链表首节点信息
	f_lseek(&file1,4);
	if ((f_read(&file1,(void*)&temp,4,&tmp) != FR_OK)||(tmp != 4))
	{
		f_close(&file1);
		return -22;
	}

	if (temp == index)
	{
		f_lseek(&file1,4);
		temp = next;
		if (f_write(&file1,(void*)&temp,4,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -23;
		}

		if (tmp != 4)
		{
			f_close(&file1);
			return -24;
		}

		f_sync(&file1);
	}

	//如果删除的节点是链表的尾节点，那么还需要更新保存的链表尾节点信息
	f_lseek(&file1,8);
	if ((f_read(&file1,(void*)&temp,4,&tmp) != FR_OK)||(tmp != 4))
	{
		f_close(&file1);
		return -25;
	}

	if (temp == index)
	{
		f_lseek(&file1,8);
		temp = prev;
		if (f_write(&file1,(void*)&temp,4,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -26;
		}

		if (tmp != 4)
		{
			f_close(&file1);
			return -27;
		}
	}

	f_close(&file1);

	strcpy((char*)inf_file_str,batch_dirctory);
	strcat((char*)inf_file_str,(char const*)barcode_hash_table_file);

	if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
	{
		return -28;
	}

	f_lseek(&file1,4*(hash_value%HASH_TABLE_SIZE));

	//获取链表的尾地址
	if (f_read(&file1,(void*)&temp,4,&tmp) != FR_OK)
	{
		f_close(&file1);
		return -29;
	}

	if (temp == index)
	{
		f_lseek(&file1,4*(hash_value%HASH_TABLE_SIZE));

		if (f_write(&file1,(void*)barcode_hash_prev,4,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -30;
		}
	}

	f_close(&file1);

	clear_log_file(0);		//清除日志

	return 0;
}

/**
* @brief 获取某一个文件的大小
* @param[in] const unsigned char *dir		文件的路径
*/
int get_file_size(const unsigned char *dir)
{
	int size;
	if (f_open(&file1,(char const*)dir,FA_OPEN_EXISTING | FA_READ) != FR_OK)
	{
		return 0;
	}

	size = file1.fsize;

	f_close(&file1);

	return size;
}

/**
* @brief 读取记录文件夹中的某一个文件
* @param[in] const unsigned char *dir		文件的路径
* @param[in] unsigned int	 offset			文件偏移
* @param[in] unsigned int	 len			数据长度
* @param[in] unsigned char *pdata			数据指针
* @return  < 0		读取文件失败
*          >= 0		读取成功,返回读取数据的实际长度
*/
int read_rec_file(const unsigned char *dir,unsigned int offset,unsigned int len,unsigned char *pdata)
{
	unsigned int	tmp;

	if (f_open(&file1,(char const*)dir,FA_OPEN_EXISTING | FA_READ) != FR_OK)
	{
		return -1;
	}

	if(file1.fsize < offset)
	{
		f_close(&file1);
		return -2;
	}

	f_lseek(&file1,offset);
	if (f_read(&file1,pdata,len,&tmp) != FR_OK)
	{
		f_close(&file1);
		return -3;
	}

	f_close(&file1);
	return tmp;
}

/**
* @brief 往记录文件夹中的某一个文件写入数据
* @param[in] const unsigned char *dir		文件的路径
* @param[in] unsigned int	 offset			文件偏移
* @param[in] unsigned int	 len			数据长度
* @param[in] unsigned char *pdata			数据指针
* @return  < 0		写入文件失败
*          >= 0		写入成功,返回写入数据的实际长度
*/
int write_rec_file(const unsigned char *dir,unsigned int offset,unsigned int len,unsigned char *pdata)
{
	unsigned int	tmp;

	if (f_open(&file1,(char const*)dir,FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
	{
		return -1;
	}

	if(file1.fsize < offset)
	{
		f_close(&file1);
		return -2;
	}

	f_lseek(&file1,offset);
	if (f_write(&file1,pdata,len,&tmp) != FR_OK)
	{
		f_close(&file1);
		return -3;
	}

	if (tmp != len)
	{
		f_close(&file1);
		return -3;
	}

	f_close(&file1);
	return tmp;
}

/**
* @brief 检查记录的文件夹中的记录文件是否完整或者合法
*/
int check_record_dir(void)
{
	const unsigned char	*p_rec_file[4];
	const unsigned char *p_rec_dir;
	unsigned char dir_str[35];
	unsigned int	i;
	int				ret = 0;

	p_rec_dir = batch_dirctory;

	p_rec_file[0] = barcode_hash_table_file;
	p_rec_file[1] = batch_inf_file;
	p_rec_file[2] = batch_list_file;
	p_rec_file[3] = 0;


	i = 0;
	while(p_rec_file[i])
	{
		strcpy((char*)dir_str,(char const*)p_rec_dir);
		strcat((char*)dir_str,(char const*)p_rec_file[i]);

		if (f_open(&file1,(char const*)dir_str,FA_OPEN_EXISTING | FA_READ) != FR_OK)
		{
			//记录文件不存在
			return -2;
		}


		if (i==0)
		{
			if (file1.fsize != 4*HASH_TABLE_SIZE)
			{
				ret = -3;		//hash表文件大小错误
			}
		}
		else if(i == 1)
		{
			if (file1.fsize < 12 || file1.fsize > 12 + 4*BATCH_LIST_MAX_CNT)
			{
				ret = -4;
			}
		}
		else if(i == 2)
		{
			if ((file1.fsize % sizeof(TBATCH_NODE)) != 0)
			{
				ret = -5;		//商品信息记录文件不能被节点大小整除
			}
		}

		f_close(&file1);

		if (ret)
		{
			return ret;
		}
		i++;
	}

	//上面检查完了记录文件夹中记录文件的存在以及每个记录文件的大小是否正确
	//还需要检查记录文件与各hash文件之间的关联性是否正确，以及记录文件内部链表的建立是否正确。
	//@todo.....

	return 0;
}


/**
***************************************************************************
*@brief	校验下载的升级文件是否正确
*@param[in] 
*@return 
*@warning
*@see	
*@note 
***************************************************************************
*/
int check_updatefile(void)
{
	UINT							rd;
	int								i;
	int								j;
	unsigned int					code_sector,code_xor,xor;

	if( f_open(&file1, "/update.bin", FA_OPEN_EXISTING | FA_READ ) != FR_OK )
	{
		return -1;
	}

	if( f_read(&file1, rec_module_buffer, 512, &rd) != FR_OK )
	{
		f_close(&file1);
		return -1;
	}

	if( rec_module_buffer[0] != 'J' || rec_module_buffer[1] != 'B' || rec_module_buffer[2] != 'L' )
	{
		f_close(&file1);
		return -1;
	}

	code_sector						= ((TPackHeader*)rec_module_buffer)->length;
	code_xor						= ((TPackHeader*)rec_module_buffer)->xor_data;

	code_sector						/= 512;
	xor								= 0;
	for(i=0; i<code_sector; i++)
	{
		if( f_read(&file1, rec_module_buffer, 512, &rd) != FR_OK )
		{
			f_close(&file1);
			return -1;
		}

		for(j=0; j<128; j++)
		{
			xor						^= *((unsigned int*)&rec_module_buffer[j*4]);
		}
	}

	f_close(&file1);

	if(xor != code_xor)
	{
		return -1;
	}

	return 0;
}


/**
***************************************************************************
*@brief 判断是否有应用升级文件的存在，如果有升级文件存在就删除应用升级文件
*@param[in] 
*@return 0 删除成功  else; 删除失败
*@warning
*@see	
*@note 
***************************************************************************
*/
int del_update_bin(void)
{
	if( f_open(&file1, "/update.bin", FA_OPEN_EXISTING | FA_READ ) != FR_OK )
	{
		return 0;
	}

	f_close(&file1);

	//应用升级文件存在，将升级文件删除
	if (f_unlink("/update.bin") != FR_OK)
	{
		return -1;	//删除旧的资源文件失败
	}

	return 0;
}

/**
* @brief ]恢复hash表的修改，包括恢复相应的节点文件的修改
* @param[in]
* @return 0:成功
*        -1:失败
*/
static int recover_hash_table_modify(unsigned char hash_type,unsigned int hash_value,unsigned int linkend)
{
		unsigned char	dir_str[35],node_size;
		unsigned int	tmp;
		unsigned char   buffer[RECORD_MAX_SIZE];

		if (hash_type == 0)
		{
			strcpy((char*)dir_str,batch_dirctory);
			strcat((char*)dir_str,(char const*)barcode_hash_table_file);
		}
		else if (hash_type == 1)
		{
			strcpy((char*)dir_str,batch_dirctory);
			strcat((char*)dir_str,(char const*)batch_inf_file);
		}

		//打开HashTable文件
		if (f_open(&file2,(char const*)dir_str,FA_OPEN_EXISTING | FA_WRITE | FA_READ) != FR_OK)
		{
			return -3;
		}

		f_lseek(&file2,4*(hash_value%HASH_TABLE_SIZE));

		if (f_write(&file2,(void*)&linkend,4,&tmp) != FR_OK)
		{
			f_close(&file2);
			return -4;
		}

		if (tmp != 4)
		{
			f_close(&file2);
			return -4;
		}

		f_close(&file2);

		if(linkend)
		{
			strcpy((char*)dir_str,batch_dirctory);
			strcat((char*)dir_str,batch_list_file);
			node_size = sizeof(TBATCH_NODE);
			

			if (f_open(&file1,(char const*)dir_str,FA_OPEN_EXISTING | FA_WRITE | FA_READ) != FR_OK)
			{
				return -3;
			}

			if(file1.fsize == 0)
			{
				f_close(&file1);
				return 0;
			}

			f_lseek(&file1,(linkend-1)*node_size);

			if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
			{
				f_close(&file1);
				return -4;
			}


			if (hash_type == 0)
			{
				if (((TBATCH_NODE*)buffer)->by_barcode_next)
				{
					((TBATCH_NODE*)buffer)->by_barcode_next = 0;
				}
			}
			else if (hash_type == 1)
			{
				if (((TBATCH_NODE*)buffer)->by_index_next)
				{
					((TBATCH_NODE*)buffer)->by_index_next = 0;
				}
			}
			

			calc_check_value(buffer);

			f_lseek(&file1,(linkend-1)*node_size);
			if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
			{
				f_close(&file1);
				return -5;
			}

			if (tmp != node_size)
			{
				f_close(&file1);
				return -5;
			}

			f_close(&file1);
		}

		return 0;
}



/**
* @brief 增加记录操作的恢复
* @param[in] 
* @return 0:成功
*        -1:失败
*/
static int record_add_recover(unsigned int rec_offset,unsigned char *log_data,unsigned int log_data_len)
{
	unsigned char	dir_str[35];
	unsigned int	tmp;
	unsigned int	hash_value,linkend;
	if (log_data_len < 8)
	{
		return -1;
	}

	if (log_data_len == 8)
	{
		//只是记录了Barcode HashValue和相应链表的尾节点，那么Hashtable和节点文件都有可能已经改变了，将已经改变的恢复为原来的状况
		hash_value = *((unsigned int*)(log_data)); 
		linkend = *((unsigned int*)(log_data+4));
		if (recover_hash_table_modify(0,hash_value,linkend))
		{
			return -2;
		}
		return 0;
	}
	else
	{
		if (log_data_len == 16)
		{
			//恢复为原来的状况
			hash_value = *((unsigned int*)(log_data)); 
			linkend = *((unsigned int*)(log_data+4));
			if (recover_hash_table_modify(0,hash_value,linkend))
			{
				return -2;
			}

			hash_value = *((unsigned int*)(log_data+8)); 
			linkend = *((unsigned int*)(log_data+12));
			if (recover_hash_table_modify(1,hash_value,linkend))
			{
				return -3;
			}

			return 0;
		}
		else
		{
			if (log_data_len == 16+sizeof(TBATCH_NODE))
			{
				//可能已经写入了节点数据
				strcpy((char*)dir_str,batch_dirctory);
				strcat((char*)dir_str,batch_list_file);
				if (f_open(&file1,(char const*)dir_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
				{
					return -4;
				}

				if (file1.fsize != rec_offset*sizeof(TBATCH_NODE))
				{
					//说明实际上没有写入节点
					f_lseek(&file1,file1.fsize);
					if (f_write(&file1,(void*)(log_data+16),sizeof(TBATCH_NODE),&tmp) != FR_OK)
					{
						f_close(&file1);
						return -5;
					}

					if (tmp != sizeof(TBATCH_NODE))
					{	
						f_close(&file1);
						return -5;
					}
				}
				f_close(&file1);
				return 0;
			}
			else
			{	
				return -2;			//日志数据长度错误
			}
		}
	}
}



/**
* @brief 恢复被中断的清除所有记录操作
* @return 0：成功  -1：失败
*/
int record_clear_recover(void)
{
	unsigned char dir_str[35];
	const unsigned char	*p_hash_table_file[3];
	int i;


	p_hash_table_file[0] = barcode_hash_table_file;
	p_hash_table_file[1] = batch_inf_file;
	p_hash_table_file[2] = 0;


	i = 0;
	while (p_hash_table_file[i])
	{
		strcpy((char*)dir_str,batch_dirctory);
		strcat((char*)dir_str,(char const*)p_hash_table_file[i]);

		//打开相应的hash_table文件
		if (f_open(&file2,(const char*)dir_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
		{
			return -3;
		}
		if (i == 1)
		{
			init_data_info_file(&file2);	
		}
		else
		{
			init_hash_table_file(&file2);	
		}
		

		f_close(&file2);
		i++;
	}
	return 0;
}

/**
* @brief 恢复被中断的删除某一个节点的操作
* @return 0：成功  -1：失败
*/
static int del_one_node_recover(unsigned int index,unsigned int logical_prev,unsigned int logical_next,unsigned int barcode_hash_prev,unsigned int barcode_hash_next,unsigned int hash_value)
{
	unsigned char	buffer[RECORD_MAX_SIZE];
	unsigned int	tmp,link_end;
	unsigned char	inf_file_str[35];
	unsigned int	node_size;

	strcpy((char*)inf_file_str,batch_dirctory);
	strcat((char*)inf_file_str,batch_list_file);
	node_size = sizeof(TBATCH_NODE);

	//step2:更新该节点前一个节点和后一个节点的链表信息
	if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
	{
		return -2;
	}

	if(logical_prev)
	{
		f_lseek(&file1,(logical_prev-1)*node_size);
		if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
		{
			f_close(&file1);
			return -4;
		}

	
		((TBATCH_NODE*)buffer)->by_index_next = logical_next;

		calc_check_value(buffer);

		f_lseek(&file1,(logical_prev-1)*node_size);
		if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -5;
		}

		if (tmp != node_size)
		{
			f_close(&file1);
			return -5;
		}
	}

	if (logical_next)
	{
		f_lseek(&file1,(logical_next-1)*node_size);
		if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
		{
			f_close(&file1);
			return -6;
		}

	
		((TBATCH_NODE*)buffer)->by_index_prev = logical_prev;
		calc_check_value(buffer);
		f_lseek(&file1,(logical_next-1)*node_size);
		if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -7;
		}

		if (tmp != node_size)
		{
			f_close(&file1);
			return -7;
		}
	}

		if(barcode_hash_prev)
		{
			f_lseek(&file1,(barcode_hash_prev-1)*node_size);
			if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
			{
				f_close(&file1);
				return -13;
			}



			((TBATCH_NODE*)buffer)->by_barcode_next = barcode_hash_next;
			calc_check_value(buffer);

			f_lseek(&file1,(barcode_hash_prev-1)*node_size);
			if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
			{
				f_close(&file1);
				return -14;
			}

			if (tmp != node_size)
			{
				f_close(&file1);
				return -15;
			}

			f_sync(&file1);
		}


		if (barcode_hash_next)
		{
			f_lseek(&file1,(barcode_hash_next-1)*node_size);
			if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK)||(tmp != node_size))
			{
				f_close(&file1);
				return -16;
			}


			((TBATCH_NODE*)buffer)->by_barcode_prev = barcode_hash_prev;
			calc_check_value(buffer);
			f_lseek(&file1,(barcode_hash_next-1)*node_size);
			if (f_write(&file1,buffer,node_size,&tmp) != FR_OK)
			{
				f_close(&file1);
				return -17;
			}

			if (tmp != node_size)
			{
				f_close(&file1);
				return -18;
			}

			f_sync(&file1);	
		}




	//可以将节点文件关闭了
	f_close(&file1);

	//step3:将删除的节点的偏移记录在信息文件中
	strcpy((char*)inf_file_str,batch_dirctory);
	strcat((char*)inf_file_str,batch_inf_file);

	if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
	{
		return -8;
	}

	if (file1.fsize > 12)
	{
		f_lseek(&file1,file1.fsize-4);
		if (f_read(&file1,(void*)&link_end,4,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -9;
		}

		if (link_end == index)
		{
			goto check_next;		//说明该删除的节点偏移已经保存在INF文件中了，不需要再保存一次
		}
	}

	f_lseek(&file1,file1.fsize);
	if (f_write(&file1,(void*)&index,4,&tmp) != FR_OK)
	{
		f_close(&file1);
		return -9;
	}

	if (tmp != 4)
	{
		f_close(&file1);
		return -9;
	}

	f_sync(&file1);

check_next:

	//如果删除的节点是链表的首节点，那么还需要更新保存的链表首节点信息
	f_lseek(&file1,4);
	if ((f_read(&file1,(void*)&link_end,4,&tmp) != FR_OK)||(tmp != 4))
	{
		f_close(&file1);
		return -10;
	}

	if (link_end == index)
	{
		f_lseek(&file1,4);
		link_end = logical_next;
		if (f_write(&file1,(void*)&link_end,4,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -11;
		}

		if (tmp != 4)
		{
			f_close(&file1);
			return -11;
		}

		f_sync(&file1);
	}

	//如果删除的节点是链表的尾节点，那么还需要更新保存的链表尾节点信息
	f_lseek(&file1,8);
	if ((f_read(&file1,(void*)&link_end,4,&tmp) != FR_OK)||(tmp != 4))
	{
		f_close(&file1);
		return -10;
	}

	if (link_end == index)
	{
		f_lseek(&file1,8);
		link_end = logical_prev;
		if (f_write(&file1,(void*)&link_end,4,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -11;
		}

		if (tmp != 4)
		{
			f_close(&file1);
			return -11;
		}
	}

	f_close(&file1);

	strcpy((char*)inf_file_str,batch_dirctory);
	strcat((char*)inf_file_str,barcode_hash_table_file);

	if (hash_value)
	{
		if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
		{
			return -28;
		}

		f_lseek(&file1,4*(hash_value%HASH_TABLE_SIZE));
		//获取链表的尾地址
		if (f_read(&file1,(void*)&link_end,4,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -29;
		}

		if (link_end == index)
		{
			//如果该节点刚好是对应链表的尾节点，那么需要更新该链表对应的尾节点
			f_lseek(&file1,8*(hash_value%HASH_TABLE_SIZE));

			if (f_write(&file1,(void*)&barcode_hash_prev,4,&tmp) != FR_OK)
			{
				f_close(&file1);
				return -30;
			}
		}

		f_close(&file1);

	}

	return 0;
}

/**
* @brief 记录操作的恢复
* @param[in] unsigned char *precord 记录指针
* @return 0:成功
*        -1:失败
*/
static int record_recover(unsigned char *log_data,unsigned int log_data_len)
{
	unsigned int	op_type,rec_offset,prev,next,saved_linkend;
	unsigned int	barcode_hash_prev,barcode_hash_next;

	op_type = *((unsigned int*)log_data);

	if (OP_TYPE_ADD_NODE == op_type)
	{
		if (log_data_len < 12)
		{
			return -2;		//日志数据长度错误
		}

		rec_offset = *((unsigned int*)(log_data+8));

		if (log_data_len == 12)
		{
			//如果日志数据只有12字节，那么说明在增加记录时还没有对原来的记录文件做任何的变动，所以不需要任何的处理
			return 0;
		}

		//说明在记录增加过程中可能已经改变了部分记录数据，需要采取恢复措施
		if (record_add_recover(rec_offset,log_data+12,log_data_len-12))
		{
			return -3;
		}
	}
	else if (OP_TYPE_CLEAR_NODE == op_type)
	{
		if ((log_data_len == 8)||(log_data_len == 12))
		{
			if (record_clear_recover())
			{
				return -3;
			}
		}
		else
		{
			return -2;	//日志数据长度错误
		}
	}
	else if (OP_TYPE_DEL_NODE == op_type)
	{
		if ((log_data_len == 32)||(log_data_len == 36))
		{
			//rec_type = *((unsigned int*)(log_data+4));
			rec_offset = *((unsigned int*)(log_data+8));
			prev = *((unsigned int*)(log_data+12));
			next = *((unsigned int*)(log_data+16));
			barcode_hash_prev = *((unsigned int*)(log_data+20));
			barcode_hash_next = *((unsigned int*)(log_data+24));
			saved_linkend = *((unsigned int*)(log_data+28));
			if (del_one_node_recover(rec_offset,prev,next,barcode_hash_prev,barcode_hash_next,saved_linkend))
			{
				return -3;
			}
		}
		else
		{
			return -2;	//日志数据长度错误
		}

	}

	return 0;
}


/**
***************************************************************************
*@brief 判断是否存在日志文件，如果有日志文件试图根据日志文件恢复被中断的操作或者回滚被中断的操作
*@param[in] 
*@return 0 成功  else; 失败
***************************************************************************
*/
int recover_record_by_logfile(void)
{
	unsigned int	logfile_len,offset,logitem_len;
	unsigned int				tmp;
	int				err_code = 0;
	unsigned char	log_bufffer[256];

	if (f_open(&file3,log_file,FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
	{
		return -1;
	}
	
	logfile_len = file3.fsize;	//日志文件的长度
	offset = 0;
	while(logfile_len)
	{
		f_lseek(&file3,offset);
		if (f_read(&file3,(void*)&logitem_len,4,&tmp) != FR_OK)
		{
			err_code = -2;
			break;
		}

		if (tmp != 4)
		{
			err_code = -2;
			break;
		}

		//if ((logitem_len <= 4)||(logitem_len > 256))
		//{
		//	err_code = -3;
		//	if (logitem_len == 0)	//碰到一种情况，log文件本身大小为24字节，但是内容全部为0，怀疑是写log文件自身时断电造成的
		//	{
		//		err_code = 0;		
		//	}
		//	break;
		//}

		if (logitem_len != logfile_len)
		{
			err_code = 0;		//出现这种情况认为是在写LOG文件本身断电造成的
			break;
		}

		if (f_read(&file3,(void*)log_bufffer,logitem_len-4,&tmp) != FR_OK)
		{
			err_code = -4;
			break;
		}

		if (tmp != logitem_len-4)
		{
			err_code = -4;
			break;
		}

		if (record_recover(log_bufffer,logitem_len-4))
		{
			err_code = -5;
			break;
		}

		offset += logitem_len;
		logfile_len -= logitem_len;
	}

	if (err_code == 0)
	{
		f_lseek(&file3,0);
		f_truncate(&file3);		//清除日志文件
	}

	f_close(&file3);
	return err_code;
}




#ifdef T5_SD_DEBUG

void debug_out(unsigned char* out,unsigned int len,unsigned char format)
{
	unsigned int write_num;
	unsigned char *pBuf;
	unsigned char data;

	if (0 == debug_file_status)
	{
		//创建DEBUG结果的输出文件
		if (f_open(&debug_file,"Debug.out",FA_OPEN_ALWAYS | FA_WRITE) != FR_OK)
		{
			return;
		}

		f_lseek(&debug_file,debug_file.fsize);
		debug_file_status = 1;
	}


	if (1 == debug_file_status)
	{
		if (format == 0)
		{
			//表明打印的数据是16进制数，需要转换为ASCII格式
			pBuf = (unsigned char*)Jmalloc(len*3);
			if (pBuf == NULL)
			{
				return;
			}

			for (write_num = 0;write_num < len;write_num++)
			{
				data = out[write_num];
				pBuf[write_num*3] = HexToAscii(data >> 4);
				pBuf[write_num*3+1] = HexToAscii(out[write_num]&0x0f);
				pBuf[write_num*3+2] = ' ';
			}

			if (f_write(&debug_file,pBuf,len*3,&write_num) != FR_OK)
			{
				Jfree(pBuf);
				f_close(&debug_file);
				debug_file_status = 0;
			}

			Jfree(pBuf);
		}
		else
		{
			//表明打印的数据是ASCII格式，不需要转换
			if (f_write(&debug_file,out,len,&write_num) != FR_OK)
			{
				f_close(&debug_file);
				debug_file_status = 0;
			}
		}

		f_sync(&debug_file);
	}
}


void delete_debug_file(void)
{
	debug_file_status = 0;
	if (f_open(&debug_file,"Debug.out",FA_OPEN_EXISTING | FA_WRITE) != FR_OK)
	{
		return;
	}
	
	f_lseek(&debug_file,0);
	f_truncate(&debug_file);

	f_close(&debug_file);
}

void close_debug_file(void)
{
	if (debug_file_status == 1)
	{
		f_close(&debug_file);
	}
}

#endif

#if 0
int check_serial_list_file(void)
{
#if 0
	unsigned char buf1[200],buf2[200];
	unsigned int tmp;

	if (f_open(&file3,"/T5_DB/serial/serial.lst",FA_OPEN_EXISTING | FA_READ) != FR_OK)
	{
		return -1;
	}

	if (file3.fsize >= 2*sizeof(SERIAL_DATA_NODE))
	{
		f_lseek(&file3,file3.fsize - 2*sizeof(SERIAL_DATA_NODE));
		if (f_read(&file3,(void*)buf1,sizeof(SERIAL_DATA_NODE),&tmp) != FR_OK)
		{
			f_close(&file3);
			return -1;
		}

		if (tmp != sizeof(SERIAL_DATA_NODE))
		{
			f_close(&file3);
			return -1;
		}

		if (f_read(&file3,(void*)buf2,sizeof(SERIAL_DATA_NODE),&tmp) != FR_OK)
		{
			f_close(&file3);
			return -1;
		}

		if (tmp != sizeof(SERIAL_DATA_NODE))
		{
			f_close(&file3);
			return -1;
		}

		if (memcmp(buf1,buf2,sizeof(SERIAL_DATA_NODE)) == 0)
		{
			f_close(&file3);
			return 1;
		}
	}

		f_close(&file3);

#endif

	unsigned int index;
	unsigned char *ptmp;

	if (f_open(&file3,"/T5_DB/serial/serial.lst",FA_OPEN_EXISTING | FA_READ) != FR_OK)
	{
		return -1;
	}

	index = file3.fsize/sizeof(SERIAL_DATA_NODE);
	f_close(&file3);

	ptmp = record_module_read(REC_TYPE_SERIAL_LIST,index);
	if (ptmp == 0)
	{

		return 1;
	}

	return 0;
}
#endif