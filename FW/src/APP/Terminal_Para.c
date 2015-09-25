/**
 * @file  Terminal_Para.c
 * @brief T1Gen��Ŀ���ն˲�������ģ��
 * @version 1.0
 * @author joe
 * @date 2011��03��30��
 * @note
*/  
/* Private include -----------------------------------------------------------*/
#include "Terminal_Para.h"
#include <string.h>
#include "crc32.h"
#include "ff.h"

extern FATFS fs;
FIL	 file1;

#define PARAM_FILE	"/device.cfg"


/* private Variable -----------------------------------------------*/
/* external Variable -----------------------------------------------*/

/* Global Variable -----------------------------------------------*/
//�����ն˵�ϵͳ����
TTerminalPara		g_param;				//�ն˲���


/* Private function prototypes -----------------------------------------------*/
/* Private function  -----------------------------------------------*/

/**
* @brief	�ӱ����ն�ϵͳ�����Ĵ洢ģ���ж�ȡ�������浽ȫ�ֱ�����
* @param[in]  none				
* @param[out] �洢�ն˲�����ȫ�ֱ���
* @return     unsigned char  0  :SUCCESS   else : �������
* @note                  
*/
int ReadTerminalPara(void)
{
	unsigned char			*pDst;
//	unsigned char			i,j;
	unsigned long			checkvalue;
	unsigned int				rd;

	pDst				= (unsigned char *)&g_param;	//ָ��ȫ�ֲ����洢��	
	if( f_open(&file1, PARAM_FILE, FA_OPEN_EXISTING | FA_READ ) != FR_OK )
	{
		return -1;
	}

	if(f_lseek(&file1,0) != FR_OK)
	{
		f_close(&file1);
		return -1;
	}
	
	if (f_read(&file1,(void*)pDst,sizeof(TTerminalPara),&rd) != FR_OK)
	{
		f_close(&file1);
		return -1;
	}

#if 1
	//����У��ֵ�Ƿ���ȷ
	checkvalue = crc32(0,&g_param.struct_ver,sizeof(TTerminalPara) - 4);
	if (g_param.checkvalue != checkvalue)
	{
		//������У��ֵ����
		return 2;
	}

	// �������Ƿ���ȷ
	if ((g_param.endtag[0] != 0x55)||(g_param.endtag[1] != 0xAA)||(g_param.struct_ver != 1))
	{
		//�����Ľ�����ǲ���
		return 3;
	}


	//�����������Ƿ���ȷ
	//@todo....
#endif
	return 0;	
}

/**
* @brief	���û��������ն�ϵͳ�������浽�洢ģ��
* @param[in]   none				
* @param[in] �洢�ն˲�����ȫ�ֱ���
* @return     none
* @note   �˺�����ʵ�ֵ��ǽ��������浽PSAM����                
*/
int SaveTerminalPara(void)
{
	unsigned char		*pSrc;
	unsigned int		wr;

	pSrc				= (unsigned char *)&g_param;

	// ���¼���У��        
	g_param.checkvalue = crc32(0,&g_param.struct_ver, sizeof(TTerminalPara)-4);

	if( f_open(&file1, PARAM_FILE, FA_OPEN_ALWAYS | FA_WRITE ) != FR_OK )
	{
		return -1;
	}

	if(f_lseek(&file1,0) != FR_OK)
	{
		f_close(&file1);
		return -1;
	}

	if (f_write(&file1,(void*)pSrc,sizeof(TTerminalPara),&wr) != FR_OK)
	{
		f_close(&file1);
		return -1;
	}

	if (wr != sizeof(TTerminalPara))
	{
		f_close(&file1);
		return -1;
	}
	 
	f_close(&file1);
	return 0;
}


/**
* @brief ����Ĭ�ϵĲ��Բ���������
*/
int DefaultTerminalPara(void)
{
	unsigned char		*pSrc;
	//unsigned char		i,tmp[2];

	pSrc				= (unsigned char *)&g_param;
	//��������в���
	memset(pSrc,0,sizeof(TTerminalPara));
	//���ý������
	g_param.endtag[0]		= 0x55;
	g_param.endtag[1]		= 0xAA;
	//���ò����ṹ�汾��
	g_param.struct_ver		= 0x01;

	
	//����Ĭ�ϵ��ն˲���
	g_param.beep_volume = 3;		
	//g_param.power_save_mode = 1;		//Ĭ��֧�ֽ���ģʽ
	g_param.lower_power_timeout = 0;	//Ĭ��1���ӽ���͹���ģʽ
	
	return SaveTerminalPara();
}

