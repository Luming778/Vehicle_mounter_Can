#include "HAL_AT.h"
#include "usart.h"
#include "platform_mutex.h"
extern circle_buf	g_circle_buff;
extern SemaphoreHandle_t g_at_send_mutex_x; 			//HAL_AT_SENDcmd()函数互斥量

/**********************************************************************
 * 函数名称： HAL_AT_SENDcmd	
 * 功能描述： HAL层 发送AT命令（互斥操作）
 * 输入参数： cmd-要发送的AT命令
 *            len-命令长度
 *            timeout-超时时间
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void HAL_AT_SENDcmd(const unsigned char *cmd,int len,int timeout)
{
	xSemaphoreTake(g_at_send_mutex_x,portMAX_DELAY);
    uart3_write(cmd,len,timeout);    				//串口3发送AT命令
	xSemaphoreGive(g_at_send_mutex_x);
}

/**********************************************************************
 * 函数名称： HAL_AT_Recv	
 * 功能描述： HAL层 收AT命令回复（阻塞操作，等待esp8266传来数据，超时时间可设置）
 * 输入参数： c-要接收的字符
 *            timeout-超时时间
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void HAL_AT_Recv(char *c,unsigned int timeout)
{
	while(1)
	{
		if(0 == circle_buf_read(&g_circle_buff,(uint8_t*)c))/*读数据*/
		{
			return;
		}
		else
		{
			ulTaskNotifyTake(pdTRUE,timeout);//等待esp8266传来数据
		}
	}
}
