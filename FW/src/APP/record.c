/**
 * @file record.c
 * @brief H520B��Ŀ��¼����ģ��
 * @version 1.0
 * @author joe
 * @date 2015��09��10��
 * @note ����SPI Flash ʵ�ֵ�FAT�ļ�ϵͳ�����¼
 *
 * �˴���Ϊ���ںϽܵ������޹�˾��Ŀ���룬�κ��˼���˾δ����ɲ��ø��ƴ�����������
 * ����˾�������Ŀ����˾����һ��׷��Ȩ����
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

unsigned char			rec_module_buffer[RECORD_MAX_SIZE];	//��¼ģ��Ĺ���buffer	


unsigned int current_node_offset;
unsigned int g_rec_offset;

static unsigned int	prev_node_offset;
static unsigned int	next_node_offset;


static unsigned int log_len;

FIL						file2,file3;
DIR						dir;							//�ļ���


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
* @brief ��ʼ����ϣ���ļ�
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
* @brief ��ʼ�����к���Ϣ�ļ�
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
 * @brief ϵͳ�����м�¼ģ��ĳ�ʼ��
 * @return 0: OK   else: �������
 * @note: ����ֵ���ܺܺõĶ�λ������Ĵ�������λ�ã���������Ҫ���޸�
 */
int record_module_init(void)
{
	unsigned int		j;	
	unsigned char		dir_str[35];
	const unsigned char	*p_hash_table_file[3];

	//f_mount(0, &fs);										// װ���ļ�ϵͳ

	if( f_opendir(&dir,batch_dirctory) != FR_OK )
	{
		//�򿪼�¼�ļ�ʧ�ܻ����޷�����SD��,����Ǹ��ļ��в����ڣ���ô�ʹ���һ���µ��ļ���
		if (f_mkdir(batch_dirctory) != FR_OK)
		{
			//�޷�����SD��
			return 1;	
		}

		if( f_opendir(&dir,batch_dirctory) != FR_OK )
		{
			//�մ����ɹ��˻��򲻿����Ǿ͹����ˣ��������������𣿣�����
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
 * @brief ���ѻ���¼���ü�¼������ڣ��˺����в����ж�
 * @param[in] int index ��¼������(���ϵļ�¼����������1)
 * @return ���ؼ�¼�ĵ�ַ
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
		//У��ֵ����
		f_close(&file1);
		return (unsigned char*)0;
	}

	f_close(&file1);
	return pBuf;
}

//����У��ֵ�����ȥ
static calc_check_value(unsigned char *precord)
{
	unsigned int		checkvalue;

	//����˷ݼ�¼��У��ֵ���ŵ���¼����ǰ��4���ֽ�
	checkvalue = crc32(0,precord + 4,sizeof(TBATCH_NODE) - 4);

	memcpy(precord,(unsigned char*)&checkvalue,4);
}
/**
* @brief ����һ���ѻ��ڵ㵽��¼�ļ�֮����¼�¼�ļ�������
* @param[in] FIL *file						��¼�ļ�ָ��,�Ѿ��򿪵��ļ�
* @param[in] unsigned char key_type			�ؼ�������
* @param[in] unsigned char	*p_node			Ҫ�����Ľڵ�ָ��	
* @param[in] unsigned int	header			�����׵�ַ
* @param[in] unsigned int	node_offset		Ҫ���ӽڵ��ƫ��
* @return 0:�ɹ�
*        else:ʧ��
* @note ֻ����Ʒ��Ϣ�ڵ���˫�������������ӽڵ�ʱ��Ҫά��˫����������ڵ㶼�ǵ�������
*/
static int  update_link_info_after_addNode(FIL *file,unsigned char key_type,unsigned char*p_node,unsigned int header,unsigned int node_offset)
{
	unsigned int	rec_size,next_node_offset,current_offset,tmp;
	unsigned char	buffer[RECORD_MAX_SIZE];		//���һ���ڵ�Ĵ�С������512����ô����Ҫ���Ӵ���ʱ�ռ�

	rec_size = sizeof(TBATCH_NODE);

	next_node_offset = header;
	while (next_node_offset)
	{
		current_offset = next_node_offset;
		f_lseek(file,(next_node_offset-1)*rec_size);		//�ļ�ָ�붨λ�������׼�¼��ƫ�ƴ���ע�������м�¼��ƫ���Ǹýڵ������ƫ��
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
	f_lseek(file,(current_offset-1)*rec_size);		//�ļ�ָ�����¶�λ���������һ���ڵ㴦
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


//��׼�����е��ļ������������Ϣ���浽��־�ļ�
//note : ����ÿһ�ֲ�����˵�����б��еĲ����ĺ��嶼����ͬ
//       op_type == OP_TYPE_ADD_NODE ʱ,		param1 ��ʾ��¼����, param2 ��ʾ�����ڵ��ƫ��  param3 ��ʾ�����ڵ�ĳ���  param4 ָ�������ڵ�����
//		 op_type == OP_TYPE_CLEAR_NODE ʱ,		param1 ��ʾ��¼����, param2 ������					param3 ������ param4 ������
//		 op_type == OP_TYPE_DEL_NODE ʱ,		param1 ��ʾ��¼����, param2 ��ʾҪɾ���ڵ��ƫ��	param3 ������ param4 ������
//		 op_type == OP_TYPE_REPLACE_NODE ʱ,	param1 ��ʾ��¼����, param2 ��ʾҪ�滻�ڵ��ƫ��	param3 ��ʾ�滻�ڵ�ĳ���  param4 ָ�����滻�ڵ�����
//		 op_type == OP_TYPE_WRITE_BIN_FILE ʱ,	param1 ��ʾ��¼����, param2 ��ʾ�ļ�����	param3 ������  param4 ������
static int save_log_file(void *data,unsigned int len)
{
	unsigned int tmp;
	unsigned int llen;
	unsigned char tmp_buffer[530];
	//���ò������浽��־�ļ���
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
	f_close(&file3);		//�ر���־�ļ�
	log_len = llen;
        return 0;
}


//�����־�ļ�������ӵ���־
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
* @brief ����һ���ѻ���¼
* @param[in] unsigned char *precord ��¼ָ��
* @return 0:�ɹ�
*        else:ʧ��
* @note �˺����Ѿ����Ӷϵ籣��
*/
int record_add(unsigned char *precord)
{
	unsigned char	dir_str[35];
	unsigned char	inf_file_str[35];
	unsigned int	rec_offset,i;
	unsigned int	node_size,tmp;
	unsigned long	hash_value[2];
	const unsigned char	*p_hash_table_file[3];
	unsigned int		link_end;		//�����β��ַ
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
	hash_value[0] = HashString(((TBATCH_NODE*)precord)->barcode,0);	//�������Ʒ�����hashֵ
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
		if ((f_read(&file1,(void*)&invalid_node_offset,4,&tmp) != FR_OK) ||(tmp != 4))		//��ȡ���������к���Ϣ�ļ��е�ĳһ���Ѿ���ɾ���ڵ��ƫ��
		{
			f_close(&file1);
			return -4;
		}
	}

	f_close(&file1);


	//�򿪱�����Ӧ��¼�Ľڵ��ļ�
	if (f_open(&file1,(const char*)dir_str,FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
	{
		return -2;
	}

	if (invalid_node_offset)
	{
		rec_offset = invalid_node_offset;		//������������кŽڵ㣬���Ҽ�¼�ļ��д����Ѿ���ɾ���Ľڵ㣬��ô�����Ľڵ�
	}
	else
	{
		rec_offset = file1.fsize/node_size;		//��ȡ��¼�ļ���ǰ�Ѿ�����ļ�¼��
		if (rec_offset < BATCH_LIST_MAX_CNT)
		{
			rec_offset += 1;	
		}
		else
		{
			//rec_offset = 1;		//�����¼���ﵽ������ֵ����ô�޷������¼�¼
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


	if (save_log_file((void*)log_data,12))		//״̬0��LOG�ļ���ֻ�����˲������͡���¼���͡�Ŀ���¼ƫ��
	{
		err_code = -10;
		goto err_handle;
	}

	//����˼�¼��Ҫ����Ĺؼ��ֵ�hashֵ
	i = 0;
	while (p_hash_table_file[i])
	{
		strcpy((char*)dir_str,batch_dirctory);
		strcat((char*)dir_str,(char const*)p_hash_table_file[i]);

		//����Ӧ��hash_table�ļ�
		if (f_open(&file2,(const char*)dir_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
		{
			err_code = -4;
			goto err_handle;
		}

		f_lseek(&file2,4*(hash_value[i]%HASH_TABLE_SIZE));		//��λ����hashֵ��hash���ж�Ӧ��ƫ�ƴ�

		//��ȡ��hashֵ��Ӧ�������β��ַ
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
		if(save_log_file((void*)log_data,8))		//״̬1����״̬2
		{
			err_code = -10;
			goto err_handle;
		}
		
		f_lseek(&file2,4*(hash_value[i]%HASH_TABLE_SIZE));		//��λ����hashֵ��hash���ж�Ӧ��ƫ�ƴ�

		//���¸������β�ڵ�
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
				//���������׽ڵ㻹��0����ô��Ҫ����������׽ڵ�
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

		f_close(&file2);	//hash���ļ����Թر���

		if (link_end)
		{
			//��������ǿյģ���ô��Ҫ�����������Ϣ(�������Ʒ��Ϣ�ڵ㣬����Ҫ���µ�ǰ�ڵ����Ϣ����Ϊ��Ʒ��Ϣ������˫������)
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

	f_lseek(&file1,(rec_offset-1)*node_size);		//���ļ�ָ���Ƶ��ļ�ĩβ
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

	//����ڵ���ļ����Թر���
	f_close(&file1);	

	if (invalid_node_offset)
	{
		if (f_open(&file1,(char const*)inf_file_str,FA_OPEN_EXISTING | FA_WRITE | FA_READ) != FR_OK)
		{
			return -2;
		}

		f_lseek(&file1,file1.fsize-4);
		f_truncate(&file1);		//����¼����Ϣ�ļ��е��Ѿ�ɾ���Ľڵ�ƫ�������
		f_close(&file1);
	}
	
	clear_log_file(0);
	return 0;

err_handle:

	f_close(&file1);
	return err_code;
}


/**
* @brief �õ��ѻ���¼������
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
		cnt -= (file1.fsize - 12)/4;		//��ȥ��Ч�Ľڵ㣬��������Ч�Ľڵ��ƫ�ƶ���¼����Ϣ�ļ��У�����ֱ�Ӽ�ȥ����Ϣ�ļ��м�¼����Ч�ڵ����������ʾ�ڽڵ��ļ��л����ڵĽڵ������
		f_close(&file1);
	}
	else
	{
		//������Ӧ�ò�����������������һ��������أ����꣬��ֻ����ɱ����ˣ�ֱ�ӽ���¼�ļ�ȫ����ʼ��
		f_close(&file1);
		record_clear();
		cnt = 0;
	}

	return cnt;
}

/**
* @brief ����ѻ���¼
* @return 0���ɹ�  -1��ʧ��
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

	f_lseek(&file1,0);		//�ļ�ָ���ƶ����ļ���ʼ

	//���ļ��ضϵ��ļ���ʼ
	if (f_truncate(&file1) != FR_OK)
	{
		f_close(&file1);
		return -2;
	}

	f_close(&file1);



	log_data[0] = OP_TYPE_CLEAR_NODE;
	log_data[1] = 0;
	log_len = 0;
	if (save_log_file((void*)log_data,8))		//״̬0��LOG�ļ���ֻ�����˲������͡���¼����
	{
		return -4;
	}

	i = 0;
	while (p_hash_table_file[i])
	{
		strcpy((char*)dir_str,batch_dirctory);
		strcat((char*)dir_str,(char const*)p_hash_table_file[i]);

		//����Ӧ��hash_table�ļ�
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

	clear_log_file(0);		//�����־
	return 0;
}

/**
* @brief ��������ؼ��ֲ�ѯ�ѻ���¼�����ؼ�¼�ڵ��ƫ��
* @param[in] unsigned char *barcode				Ҫƥ����ַ���
* @param[out] unsigned char *outbuf				�������ļ�¼������
* @return �������	=0:û���������ý��׼�¼  > 0:�������ļ�¼��ƫ��   < 0: ��������
* @note  
*/
static int rec_search(unsigned char *barcode,unsigned char *outbuf)
{
	unsigned int hash_value,link_end,tmp;
	unsigned char dir_str[35];
	unsigned char	buffer[RECORD_MAX_SIZE];		//���һ���ڵ�Ĵ�С������512����ô����Ҫ���Ӵ���ʱ�ռ�
	unsigned int	node_size;				//�ڵ��С

	node_size = sizeof(TBATCH_NODE);
	memset(outbuf,0,node_size);		//����������ݵĻ�����
	strcpy((char*)dir_str,batch_dirctory);


	//���Ҫ�����Ĺؼ�������������

	//step1: ����ؼ��ֵ�hashֵ
	hash_value = HashString(barcode,0);	

	//step2: ����hashֵ���ҵ���ùؼ��־�����ͬhashֵģֵ������β��ַ
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

	//���ݶ�ȡ�����������׵�ַ���ҵ���һ����ؼ�����ȫƥ��ļ�¼�������Ҫ����ȫƥ��ؼ��ֵ����м�¼ȫ������ȡ��������Ҫ����Ľӿ���ʵ�֡�

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
			f_lseek(&file1,(link_end-1)*node_size);		//�ļ�ָ��ָ��ýڵ����ʼλ��

			if ((f_read(&file1,buffer,node_size,&tmp) != FR_OK) || (node_size != tmp))
			{
				return -4;
			}

			if (strcmp((char*)barcode,(char const*)((TBATCH_NODE*)buffer)->barcode) == 0)
			{
				memcpy(outbuf,buffer,node_size);
				return link_end;		//�����������е�һ����ؼ���ƥ��Ľڵ�
			}

			link_end = ((TBATCH_NODE*)buffer)->by_barcode_prev;
		}
	}

	return 0;		//û����������ؼ���ƥ��ļ�¼
}

/**
* @brief ���������ѯ�ѻ���¼�����ؼ�¼ָ��
* @param[in] unsigned char *barcode				Ҫƥ����ַ���
* @param[out] int	*index						���ظü�¼������ֵ
* @return �������	0 û����������¼		else ��¼ָ��
* @note  ʵ�ֵ�ʱ���������ַ����Ƚ�
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
* @brief ��ȡ���кŽڵ�
* @param[in] unsigned int mode  0:��һ����Ч�ڵ�   1;ǰһ����Ч�ڵ�   2:��һ����Ч�ڵ�  3:ָ���ڵ�ƫ��
* @param[in] unsigned int offset  ֻ����mode = 3ʱ������
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
* @brief ɾ���ѻ���¼�ļ��е�ĳһ���ڵ�
* @param[in] unsigned int index	 ����ƫ�ƣ�������
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

	//step1:�Ƚ��ýڵ���Ϊ��Ч�ڵ�
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

	//step2:���¸ýڵ�ǰһ���ڵ�ͺ�һ���ڵ��������Ϣ
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

	//���Խ��ڵ��ļ��ر���
	f_close(&file1);

	//step3:��ɾ���Ľڵ��ƫ�Ƽ�¼����Ϣ�ļ���

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

	//���ɾ���Ľڵ���������׽ڵ㣬��ô����Ҫ���±���������׽ڵ���Ϣ
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

	//���ɾ���Ľڵ��������β�ڵ㣬��ô����Ҫ���±��������β�ڵ���Ϣ
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

	//��ȡ�����β��ַ
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

	clear_log_file(0);		//�����־

	return 0;
}

/**
* @brief ��ȡĳһ���ļ��Ĵ�С
* @param[in] const unsigned char *dir		�ļ���·��
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
* @brief ��ȡ��¼�ļ����е�ĳһ���ļ�
* @param[in] const unsigned char *dir		�ļ���·��
* @param[in] unsigned int	 offset			�ļ�ƫ��
* @param[in] unsigned int	 len			���ݳ���
* @param[in] unsigned char *pdata			����ָ��
* @return  < 0		��ȡ�ļ�ʧ��
*          >= 0		��ȡ�ɹ�,���ض�ȡ���ݵ�ʵ�ʳ���
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
* @brief ����¼�ļ����е�ĳһ���ļ�д������
* @param[in] const unsigned char *dir		�ļ���·��
* @param[in] unsigned int	 offset			�ļ�ƫ��
* @param[in] unsigned int	 len			���ݳ���
* @param[in] unsigned char *pdata			����ָ��
* @return  < 0		д���ļ�ʧ��
*          >= 0		д��ɹ�,����д�����ݵ�ʵ�ʳ���
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
* @brief ����¼���ļ����еļ�¼�ļ��Ƿ��������ߺϷ�
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
			//��¼�ļ�������
			return -2;
		}


		if (i==0)
		{
			if (file1.fsize != 4*HASH_TABLE_SIZE)
			{
				ret = -3;		//hash���ļ���С����
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
				ret = -5;		//��Ʒ��Ϣ��¼�ļ����ܱ��ڵ��С����
			}
		}

		f_close(&file1);

		if (ret)
		{
			return ret;
		}
		i++;
	}

	//���������˼�¼�ļ����м�¼�ļ��Ĵ����Լ�ÿ����¼�ļ��Ĵ�С�Ƿ���ȷ
	//����Ҫ����¼�ļ����hash�ļ�֮��Ĺ������Ƿ���ȷ���Լ���¼�ļ��ڲ�����Ľ����Ƿ���ȷ��
	//@todo.....

	return 0;
}


/**
***************************************************************************
*@brief	У�����ص������ļ��Ƿ���ȷ
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
*@brief �ж��Ƿ���Ӧ�������ļ��Ĵ��ڣ�����������ļ����ھ�ɾ��Ӧ�������ļ�
*@param[in] 
*@return 0 ɾ���ɹ�  else; ɾ��ʧ��
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

	//Ӧ�������ļ����ڣ��������ļ�ɾ��
	if (f_unlink("/update.bin") != FR_OK)
	{
		return -1;	//ɾ���ɵ���Դ�ļ�ʧ��
	}

	return 0;
}

/**
* @brief ]�ָ�hash����޸ģ������ָ���Ӧ�Ľڵ��ļ����޸�
* @param[in]
* @return 0:�ɹ�
*        -1:ʧ��
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

		//��HashTable�ļ�
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
* @brief ���Ӽ�¼�����Ļָ�
* @param[in] 
* @return 0:�ɹ�
*        -1:ʧ��
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
		//ֻ�Ǽ�¼��Barcode HashValue����Ӧ�����β�ڵ㣬��ôHashtable�ͽڵ��ļ����п����Ѿ��ı��ˣ����Ѿ��ı�Ļָ�Ϊԭ����״��
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
			//�ָ�Ϊԭ����״��
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
				//�����Ѿ�д���˽ڵ�����
				strcpy((char*)dir_str,batch_dirctory);
				strcat((char*)dir_str,batch_list_file);
				if (f_open(&file1,(char const*)dir_str,FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK)
				{
					return -4;
				}

				if (file1.fsize != rec_offset*sizeof(TBATCH_NODE))
				{
					//˵��ʵ����û��д��ڵ�
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
				return -2;			//��־���ݳ��ȴ���
			}
		}
	}
}



/**
* @brief �ָ����жϵ�������м�¼����
* @return 0���ɹ�  -1��ʧ��
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

		//����Ӧ��hash_table�ļ�
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
* @brief �ָ����жϵ�ɾ��ĳһ���ڵ�Ĳ���
* @return 0���ɹ�  -1��ʧ��
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

	//step2:���¸ýڵ�ǰһ���ڵ�ͺ�һ���ڵ��������Ϣ
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




	//���Խ��ڵ��ļ��ر���
	f_close(&file1);

	//step3:��ɾ���Ľڵ��ƫ�Ƽ�¼����Ϣ�ļ���
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
			goto check_next;		//˵����ɾ���Ľڵ�ƫ���Ѿ�������INF�ļ����ˣ�����Ҫ�ٱ���һ��
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

	//���ɾ���Ľڵ���������׽ڵ㣬��ô����Ҫ���±���������׽ڵ���Ϣ
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

	//���ɾ���Ľڵ��������β�ڵ㣬��ô����Ҫ���±��������β�ڵ���Ϣ
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
		//��ȡ�����β��ַ
		if (f_read(&file1,(void*)&link_end,4,&tmp) != FR_OK)
		{
			f_close(&file1);
			return -29;
		}

		if (link_end == index)
		{
			//����ýڵ�պ��Ƕ�Ӧ�����β�ڵ㣬��ô��Ҫ���¸������Ӧ��β�ڵ�
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
* @brief ��¼�����Ļָ�
* @param[in] unsigned char *precord ��¼ָ��
* @return 0:�ɹ�
*        -1:ʧ��
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
			return -2;		//��־���ݳ��ȴ���
		}

		rec_offset = *((unsigned int*)(log_data+8));

		if (log_data_len == 12)
		{
			//�����־����ֻ��12�ֽڣ���ô˵�������Ӽ�¼ʱ��û�ж�ԭ���ļ�¼�ļ����κεı䶯�����Բ���Ҫ�κεĴ���
			return 0;
		}

		//˵���ڼ�¼���ӹ����п����Ѿ��ı��˲��ּ�¼���ݣ���Ҫ��ȡ�ָ���ʩ
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
			return -2;	//��־���ݳ��ȴ���
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
			return -2;	//��־���ݳ��ȴ���
		}

	}

	return 0;
}


/**
***************************************************************************
*@brief �ж��Ƿ������־�ļ����������־�ļ���ͼ������־�ļ��ָ����жϵĲ������߻ع����жϵĲ���
*@param[in] 
*@return 0 �ɹ�  else; ʧ��
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
	
	logfile_len = file3.fsize;	//��־�ļ��ĳ���
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
		//	if (logitem_len == 0)	//����һ�������log�ļ������СΪ24�ֽڣ���������ȫ��Ϊ0��������дlog�ļ�����ʱ�ϵ���ɵ�
		//	{
		//		err_code = 0;		
		//	}
		//	break;
		//}

		if (logitem_len != logfile_len)
		{
			err_code = 0;		//�������������Ϊ����дLOG�ļ�����ϵ���ɵ�
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
		f_truncate(&file3);		//�����־�ļ�
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
		//����DEBUG���������ļ�
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
			//������ӡ��������16����������Ҫת��ΪASCII��ʽ
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
			//������ӡ��������ASCII��ʽ������Ҫת��
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