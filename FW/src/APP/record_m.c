/**
 * @file record.c
 * @brief H520B��Ŀ�ѻ���¼����ģ��
 * @version 1.0
 * @author joe
 * @date 2015��09��28��
 * @note ����ͨ�õļ�¼����ģ��ʵ�ֵ�
 * @copy
 *
 * �˴���Ϊ���ںϽܵ������޹�˾��Ŀ���룬�κ��˼���˾δ����ɲ��ø��ƴ�����������
 * ����˾�������Ŀ����˾����һ��׷��Ȩ����
 *
 * <h1><center>&copy; COPYRIGHT 2015 heroje</center></h1>
 */

#include "record_m.h"
#include "record_mod.h"
#include <string.h>
#include "assert.h"

#define RECORD_MAX_SIZE		(MAX_BARCODE_LEN+1)		//��ļ�¼��С

#define DELETE_FLAG			0xCF	//��¼��ɾ���ı��
unsigned char			rec_module_buffer[RECORD_MAX_SIZE];	//��¼ģ��Ĺ���buffer	

static unsigned int		rec_cnt;

static unsigned int     current_logical_index;
unsigned int             current_node_offset;
unsigned int g_rec_offset;


/**
* @brief  ĳһ����¼���ڳ�ʼ��ʱ��Ҫ����Ĳ���
* @note ����ü�¼���ļ�¼��Ҫ���޸ģ���ô��Ҫ����record2������ֻ��Ҫ����record1����
*/
typedef struct  
{
	unsigned char	record_index;				//ʵ��ʹ�õļ�¼������
	unsigned int	record_size;				//��¼�Ĵ�С
	unsigned int	record_cnt;					//��¼���������
} TRec_Init_Param;

//extern  TGOODS_SPEC_NAME_COLLECTION		spec_name;
//extern  TGOODS_SPEC_NAME_COLLECTION		*pSpec_Name_Collection;
/**
 * @brief ϵͳ�����м�¼ģ��ĳ�ʼ��
 * @return 0: OK   else: �������
 * @note: ����ֵ���ܺܺõĶ�λ������Ĵ�������λ�ã���������Ҫ���޸�
 */
int record_module_init(void)
{
	int ret;
	unsigned char flag;

	unsigned int		i;
	//unsigned char		*pRec;


	ret = record_init(1,sizeof(TBATCH_NODE),BATCH_LIST_MAX_CNT);	
	if (ret)
	{
			if (-3 == ret || -6 == ret || -4 == ret)	//��¼����û��Format
			{
				ret = record_format(1,sizeof(TBATCH_NODE),BATCH_LIST_MAX_CNT);
				if (ret)
				{
					//formatʧ�ܣ���¼���������û���㹻�Ŀռ���
					return ret;
				}
			}
			else
			{
				//��¼�Ĵ洢�����в��ɻָ��Ĵ���
				return ret;
			}
	}	

	rec_cnt = 0;
	for (i = 0;i < record_count_ext(1);i++)
	{
		ret = record_read(0x81,i,&flag,1);
		if (ret)
		{
			return ret;
		}

		if (flag == 0xff)
		{
			//�ü�¼��Ч
			rec_cnt++;
		}
	}

	if ((rec_cnt == 0)&&(record_count_ext(1) != 0))
	{
		record_delall(1);
	}

	current_logical_index = 0;

	return 0;
}

/**
 * @brief ��ָ����¼���ü�¼������ڣ��˺����в����ж�
 * @param[in] unsigned char rectype	 ��¼������
 * @param[in] int index ��¼������  ������������������Ӧ�ļ�¼�������Ѿ���ɾ���ļ�¼,  1 -- cnt
 * @return ���ؼ�¼�ĵ�ַ
 */
unsigned char *record_module_read(unsigned int index)
{
	unsigned char	*pBuf;
	//unsigned int	checkvalue;
	int		ret;
	int		re_read_cnt = 3;		//���һ�ζ�ȡʧ�ܣ���ô�ض�3��


	pBuf = rec_module_buffer;

	while(re_read_cnt--)
	{
		ret = record_read(1,record_count_ext(1)-index,pBuf,0);
		if(ret)
		{
			continue;
		}

		return pBuf;
	}

	return (unsigned char*)0;
}

//��startָ����ƫ�ƿ�ʼ������һ����ɾ���ļ�¼
//return -1:		û����������ɾ����¼��
//		else��  ���ر�ɾ��������
static int get_delete_node(unsigned int start)
{
	unsigned char flag;
	int	i;

	for (i = start; i <= record_count_ext(1);i++)
	{
		if (record_read(0x81,record_count_ext(1)-i,&flag,1))
		{
			return -1;
		}

		if (flag != 0xff)
		{
			return i;
		}
	}

	return -1;
}

//������¼���ĵ�һ����Ч��¼
static int get_first_valid_node(void)
{
	unsigned char flag;
	int	i;

	for (i = 1; i <= record_count_ext(1);i++)
	{
		if (record_read(0x81,record_count_ext(1)-i,&flag,1))
		{
			return -1;
		}

		if (flag == 0xff)
		{
			return i;
		}
	}

	return 0;
}


/**
 * @brief ����һ����¼
 * @param[in] unsigned char *precord ��¼ָ��
 * @return 0:�ɹ�
 *        -1:��¼���Ѵ�����
 * @note 
 */
int record_add(unsigned char *precord)
{
	int			ret;
	int			valid_offset = -1;

	if(record_count_ext(1) >= BATCH_LIST_MAX_CNT)
	{

		if (rec_cnt >= BATCH_LIST_MAX_CNT)
		{
			return -1;
		}
		else
		{
			//�ҵ�һ���Ѿ���ɾ����λ��
			valid_offset = get_delete_node(1);
			if (valid_offset == -1)
			{
				return -1;
			}
		}
	}


	precord[0] = 0xff;		//��֤��¼�ĵ�һ���ֽ�(��־�ֽ�)��0xff;
	if (valid_offset != -1)
	{
		//��ʾ�¼�¼��Ҫ�滻ԭ����ɾ���ļ�¼
		g_rec_offset = valid_offset;
		ret = record_replace(1, record_count_ext(1)-valid_offset,precord);
	}
	else
	{
		g_rec_offset = record_count_ext(1)+1;
		ret = record_write(1,precord,0);
	}
	
	if(ret)
	{
		return ret;
	}

	rec_cnt++;
	current_node_offset = g_rec_offset;
	return 0;
}



/**
* @brief ��ָ����¼������дһ����¼
* @param[in] unsigned char rectype ��¼����
* @param[in] int					��¼����������������
* @return 0:�ɹ�
*        -1:ʧ��
* @note 
*/
int record_module_replace(int index,unsigned char *precord)
{
	precord[0] = 0xff;
	return record_replace(1,record_count_ext(1)-index,precord);
}

/**
 * @brief �õ���¼������
 * @return 0...LOGIC_RECORD_BLOCK_SIZE
 */
int record_module_count(void)
{
	return rec_cnt;
}

/**
 * @brief ������м�¼
 * @return none
 */
int record_clear(void)
{
	record_delall(1);
	rec_cnt = 0;  
    return 0;
}

/**
* @brief ����ĳһ���ؼ��ֲ�ѯ��¼
* @param[in] unsigned char *in_tag			Ҫƥ��Ĺؼ����ַ���
* @param[in] int *index						�����߼�����
* @return �������	0:û���������ý��׼�¼  else:�������ļ�¼�ĵ�ַ
* @note  ʵ�ֵ�ʱ���������ַ����Ƚ�,�����µļ�¼��ʼ����
*/
unsigned char *rec_searchby_tag(unsigned char *in_tag, int *index)
{
	unsigned char					*pBuf;
	unsigned int					i;
	int								count,ret;
	unsigned char					flag;

	i = 0;

	count = record_count_ext(1);
	if (count <= 0)
	{
		return (unsigned char*)0;
	}

	//������ֻ��һ��������һ�����Ƚϣ���֪������¼̫���ʱ��᲻��̫���ˣ�����
	//˳Ѱ���ҷ�������Ҫ�����������һ����¼�����ĵ�ʱ���Ƿ��ܹ����ܣ�������У���ֻ��ͨ��һЩ�ռ任ȡʱ��ķ������Ż�
	//�����㷨�ˡ�
	do
	{
		ret = record_read(0x81,i,&flag,1);
		if (ret)
		{
			return (unsigned char*)0;
		}

		if (flag == 0xff)
		{
			pBuf = record_module_read(count-i);

			ret = strcmp((const char*)in_tag,(const char*)((TBATCH_NODE*)pBuf)->barcode);

			if (ret == 0) 
			{
				*index = i;
				return pBuf;
			}
		}
		
		i ++;
	}while(i<count);

	return (unsigned char*)0;		//û����������ؼ���ƥ��ļ�¼
}


//��¼���߼�ƫ�ƶ�Ӧ������ƫ��
int rec_LA_to_PA(unsigned int logical_index)
{
	int ret,i,cnt = 0;
	unsigned char flag;
	unsigned int  num;

	if ((logical_index ==0)||(logical_index > rec_cnt))
	{
		return 0;
	}

	if (logical_index == 1)
	{
		current_logical_index = 1;
		current_node_offset = get_first_valid_node();
		if(logical_index == 1)
		{
			return current_node_offset;
		}
	}

	if (logical_index == current_logical_index)
	{
		return current_node_offset;
	}
	else if (logical_index > current_logical_index)
	{
		num = logical_index - current_logical_index;
		for (i = current_node_offset+1; i<=record_count_ext(1);i++)
		{
			ret = record_read(0x81,record_count_ext(1)-i,&flag,1);
			if (ret)
			{
				return -1;
			}

			if (flag == 0xff)
			{
				cnt++;

				if (cnt == num)
				{
					current_logical_index = logical_index;
					current_node_offset = i;		
					return i;
				}
			}
		}
	}
	else
	{
		num = current_logical_index - logical_index;
		for (i = current_node_offset-1; i > 0;i--)
		{
			ret = record_read(0x81,record_count_ext(1)-i,&flag,1);
			if (ret)
			{
				return -1;
			}

			if (flag == 0xff)
			{
				cnt++;

				if (cnt == num)
				{
					current_logical_index = logical_index;
					current_node_offset = i;		
					return i;
				}
			}
		}

	}
	

	return 0;
}

//��¼������ƫ�ƶ�Ӧ���߼�ƫ��
int rec_PA_to_LA(unsigned int physical_index)
{
	int ret,i;//,cnt = 0;
	unsigned char flag;
//	unsigned int num;


	if ((physical_index == 0)||(physical_index > record_count_ext(1)))
	{
		return 0;
	}

	if (physical_index == current_node_offset)
	{
		return current_logical_index;
	}
	else if (physical_index > current_node_offset)
	{
		for (i = current_node_offset+1; i <= physical_index;i++)
		{
			ret = record_read(0x81,record_count_ext(1)-i,&flag,1);
			if (ret)
			{
				return 0;
			}

			if (flag == 0xff)
			{
				current_logical_index++;
			}
		}
	}
	else
	{
		for (i = current_node_offset-1; i >= physical_index;i--)
		{
			ret = record_read(0x81,record_count_ext(1)-i,&flag,1);
			if (ret)
			{
				return 0;
			}

			if (flag == 0xff)
			{
				current_logical_index--;
			}
		}
	}
	current_node_offset = physical_index;	
	return current_logical_index;
}


//���һ����¼
//index  ����ƫ��
int delete_one_node(unsigned int index)
{
	int ret;
//	unsigned char rec_index;
//	int k;

	ret = record_modify(1,record_count_ext(1)-index,0xcf);
	if (ret)
	{
		return ret;
	}

	rec_cnt--;
	return 0;
}


/**
* @brief �߼���ȡ��¼
* @param[in] unsigned int mode  0:��һ����Ч�ڵ�   1;ǰһ����Ч�ڵ�   2:��һ����Ч�ڵ�  3:ָ���ڵ�ƫ��
* @param[in] unsigned int offset  ֻ����mode = 3ʱ�����ã�����ƫ��
*/
unsigned char* get_node(unsigned int mode,unsigned int offset)
{
	unsigned int index;
	//unsigned char flag;

	if (mode == 0)
	{
		index = rec_LA_to_PA(1);
		if (index == 0)
		{
			return (unsigned char *)0;
		}

		return record_module_read(index);
	}
	else if (mode == 1)
	{
		index = rec_LA_to_PA(current_logical_index-1);
		if (index == 0)
		{
			return (unsigned char *)0;
		}

		return record_module_read(index);
	}
	else if (mode == 2)
	{
		index = rec_LA_to_PA(current_logical_index+1);
		if (index == 0)
		{
			return (unsigned char *)0;
		}

		return record_module_read(index);
	}
	else if (mode == 3)
	{
		index = rec_PA_to_LA(offset);
		if (index == 0)
		{
			return (unsigned char *)0;
		}

		return record_module_read(offset);
	}

	return (unsigned char *)0;
}

int recover_record_by_logfile(void)
{
	return 0;
}


unsigned char* record_logical_read(unsigned int logical_index)
{
	int	index;

	index = rec_LA_to_PA(logical_index);
	if (index == 0)
	{
		return (unsigned char*)0;
	}

	return record_module_read(index);
}

void record_m_test(void)
{
	int i,ret;
	unsigned char *pRec;
	TBATCH_NODE		node;
#if 0      
        ret = record_clear();
        assert(ret == 0);
        
	//test record_add() interface
	for (i = 0; i < BATCH_LIST_MAX_CNT;i++)
	{
		sprintf(node.barcode,"test%d",i);
		hw_platform_trip_io();
		ret = record_add((unsigned char*)&node);
		assert(ret == 0);
	}
	

	//test record_module_read() interface
	for (i = 0; i < BATCH_LIST_MAX_CNT;i++)
	{
		hw_platform_trip_io();
		pRec = record_module_read(i+1);
		assert(pRec != 0);
		sprintf(node.barcode,"test%d",i);

		ret = strcmp(((TBATCH_NODE*)pRec)->barcode,node.barcode);
		assert(ret == 0);
	}

	//test rec_searchby_tag()  speed performance			//45000����¼ʱ��������ɵ�һ����¼��ʱ13.45s
	//�������㷨������ʱ�������Եģ����������ͻ�������Ҫ�Ľ��������㷨
	hw_platform_trip_io();
	sprintf(node.barcode,"test%d",0);
	pRec = rec_searchby_tag(node.barcode,&i);
	assert(pRec != 0);
	hw_platform_trip_io();

#endif

	for (i = 0; i < BATCH_LIST_MAX_CNT;i++)
	{
		ret = delete_one_node(i+1);
		assert(ret == 0);
		i++;
	}

	for (i = 0; i < record_module_count();i++)
	{
		pRec = get_node((i==0)?0:2,0);
		assert(pRec != 0);
		sprintf(node.barcode,"test%d",2*i+1);

		ret = strcmp(((TBATCH_NODE*)pRec)->barcode,node.barcode);
		assert(ret == 0);
	}
	
}