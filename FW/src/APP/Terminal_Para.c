/**
 * @file  Terminal_Para.c
 * @brief T1Gen项目的终端参数管理模块
 * @version 1.0
 * @author joe
 * @date 2011年03月30日
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
//定义终端的系统参数
TTerminalPara		g_param;				//终端参数


/* Private function prototypes -----------------------------------------------*/
/* Private function  -----------------------------------------------*/

/**
* @brief	从保存终端系统参数的存储模块中读取参数保存到全局变量中
* @param[in]  none				
* @param[out] 存储终端参数的全局变量
* @return     unsigned char  0  :SUCCESS   else : 错误代码
* @note                  
*/
int ReadTerminalPara(void)
{
	unsigned char			*pDst;
//	unsigned char			i,j;
	unsigned long			checkvalue;
	unsigned int				rd;

	pDst				= (unsigned char *)&g_param;	//指向全局参数存储区	
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
	//计算校验值是否正确
	checkvalue = crc32(0,&g_param.struct_ver,sizeof(TTerminalPara) - 4);
	if (g_param.checkvalue != checkvalue)
	{
		//参数的校验值不对
		return 2;
	}

	// 检查参数是否正确
	if ((g_param.endtag[0] != 0x55)||(g_param.endtag[1] != 0xAA)||(g_param.struct_ver != 1))
	{
		//参数的结束标记不对
		return 3;
	}


	//检查其余参数是否正确
	//@todo....
#endif
	return 0;	
}

/**
* @brief	将用户变更后的终端系统参数保存到存储模块
* @param[in]   none				
* @param[in] 存储终端参数的全局变量
* @return     none
* @note   此函数中实现的是将参数保存到PSAM卡中                
*/
int SaveTerminalPara(void)
{
	unsigned char		*pSrc;
	unsigned int		wr;

	pSrc				= (unsigned char *)&g_param;

	// 重新计算校验        
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
* @brief 建立默认的测试参数并保存
*/
int DefaultTerminalPara(void)
{
	unsigned char		*pSrc;
	//unsigned char		i,tmp[2];

	pSrc				= (unsigned char *)&g_param;
	//先清空所有参数
	memset(pSrc,0,sizeof(TTerminalPara));
	//设置结束标记
	g_param.endtag[0]		= 0x55;
	g_param.endtag[1]		= 0xAA;
	//设置参数结构版本号
	g_param.struct_ver		= 0x01;

	
	//构造默认的终端参数
	g_param.beep_volume = 3;		
	//g_param.power_save_mode = 1;		//默认支持节能模式
	g_param.lower_power_timeout = 0;	//默认1分钟进入低功耗模式
	
	return SaveTerminalPara();
}

