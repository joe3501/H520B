/**
 * @file hash.c
 * @brief hash���㷨�õ��Ľӿ�
 * @version 1.0
 * @author joe
 * @date 2011��12��21��
 * @copy
 *
 * �˴���Ϊ���ڽ������������޹�˾��Ŀ���룬�κ��˼���˾δ����ɲ��ø��ƴ�����������
 * ����˾�������Ŀ����˾����һ��׷��Ȩ����
 *
 * <h1><center>&copy; COPYRIGHT 2011 netcom</center></h1>
 */
#include "hash.h"
#include "crc32.h"
#include<string.h>

//static unsigned long cryptTable[0x500];			//5K���ڴ�ռ�

#if 0
void prepareCryptTable(void)
{ 
    unsigned long seed = 0x00100001, index1 = 0, index2 = 0, i;
	 unsigned long temp1, temp2;
 
    for( index1 = 0; index1 < 0x100; index1++ )
    { 
        for( index2 = index1, i = 0; i < 5; i++, index2 += 0x100 )
        { 
            seed = (seed * 125 + 3) % 0x2AAAAB;
            temp1 = (seed & 0xFFFF) << 0x10;
 
            seed = (seed * 125 + 3) % 0x2AAAAB;
            temp2 = (seed & 0xFFFF);
 
            cryptTable[index2] = ( temp1 | temp2 ); 
       } 
   } 
} 
#endif


/**
* @brief ���������ַ�����hashֵ
* @param[in] unsigned char *str				�����ַ���ָ��
* @param[in] unsigned long dwHashType		hash���� ȡֵ 0��1��2
*/
unsigned long HashString(unsigned char *str, unsigned long dwHashType)
{ 
#if 0
    unsigned char *key  = str;
	unsigned long seed1 = 0x7FED7FED;
	unsigned long seed2 = 0xEEEEEEEE;
    int ch;
 
    while( *key != 0 )
    { 
        ch = *key++;
 
        seed1 = cryptTable[(dwHashType << 8) + ch] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3; 
    }
    return seed1; 
#endif

	//ѡ��CRC��hash�㷨
	return crc32(dwHashType,str,strlen((char const*)str));
}





