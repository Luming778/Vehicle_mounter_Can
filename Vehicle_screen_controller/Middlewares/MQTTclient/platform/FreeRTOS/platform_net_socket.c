/*
 * @Author: jiejie
 * @Github: https://github.com/jiejieTop
 * @Date: 2020-01-10 23:45:59
 * @LastEditTime: 2020-04-25 17:50:58
 * @Description: the code belongs to jiejie, please keep the author information and source code according to the license.
 */
#include "platform_net_socket.h"
#include "platform_esp8266_AT.h"
#include "string.h"
#include "stdio.h"
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
/*WiFi 配置宏*/
#define JAP_SSID			"1W18570"
#define JAP_passwoed	"@27B195v"

/**********************************************************************
 * 函数名称： platform_net_socket_connect	
 * 功能描述： 连接到服务器,一直连接直到成功才返回
 * 输入参数： host 服务器IP地址，port 服务器端口号，proto 协议类型
 * 输出参数： 无
 * 返 回 值： 0 成功
 ***********************************************************************/
int platform_net_socket_connect(const char *host, const char *port, int proto)
{
	char cmd[50]={0};
	
	volatile int ret;
	while(1)
	{
		/*先断开连接*/
		ret = AT_send_cmd("AT+CIPCLOSE",(int)strlen("AT+CIPCLOSE"),2000);
		if(ret)
		{
			printf("\r\nAT+CIPCLOSE faliue,ret = %d\r\n",ret);
		}
		/*1. 配置 WiFi 模式*/
		ret = AT_send_cmd("AT+CWMODE=3",(int)strlen("AT+CWMODE=3"),2000);
		if(ret)
		{
			printf("\r\nAT+CWMODE=3 faliue,ret = %d\r\n",ret);
		}
		/*2.1断开原有的连接*/
		ret = platform_net_socket_close(ret);
		if(ret)
		{
			printf("\r\nWIFI disconnet faliue,ret = %d\r\n",ret);
		}
		/*2.2连接路路由器*/
		ret = AT_send_cmd("AT+CWJAP=\"" JAP_SSID "\",\"" JAP_passwoed "\"",(int)strlen("AT+CWJAP=\"" JAP_SSID "\",\"" JAP_passwoed "\""),10000);
		if(ret)
		{
			printf("\r\nWIFI connect faliue,ret = %d\r\n",ret);
			continue;
		}	
		/*3. ESP8266 设备作为 TCP client 连接到上述服务器 例子:(AT+CIPSTART="TCP","192.168.3.116",8080) */
		if(proto == PLATFORM_NET_PROTO_TCP)
		{
			sprintf(cmd,"AT+CIPSTART=\"TCP\",\"%s\",%s",host,port);
		}
		else //proto == PLATFORM_NET_PROTO_UDP
		{
			sprintf(cmd,"AT+CIPSTART=\"UDP\",\"%s\",%s",host,port);
		}
		ret = AT_send_cmd(cmd,(int)strlen(cmd),10000);
		if(ret)
		{
			printf("\r\nTCP connect faliue,ret = %d\r\n",ret);
			continue;
		}
		if(!ret)
		{
			break;
		}
	}
  		
	/*连接成功*/
	return 0;
}

/**********************************************************************
 * 函数名称： platform_net_socket_recv_timeout	
 * 功能描述： 接收数据,超时时间timeout
 * 输入参数： fd 文件描述符， buf 接收数据缓冲区，len 接收数据长度，timeout 超时时间
 * 输出参数： 无
 * 返 回 值： len 接收数据长度
 ***********************************************************************/
int platform_net_socket_recv_timeout(int fd, unsigned char *buf, int len, int timeout)
{
	int i=0;
	int err;
	while(i<len)
	{
		err = AT_Recv_data((char*)&buf[i],timeout);
		if(err)
		{
			return err;
		}
		i++;
	}
	return len;
}
/**********************************************************************
 * 函数名称： platform_net_socket_write_timeout
 * 功能描述： 发送数据,超时时间timeout
 * 输入参数： fd 文件描述符， buf 发送数据缓冲区，len 发送数据长度，timeout 超时时间
 * 输出参数： 无
 * 返 回 值： len 发送数据长度
 ***********************************************************************/
int platform_net_socket_write_timeout(int fd, unsigned char *buf, int len, int timeout)
{
	volatile int ret;
	char cmd[20]={0};
	sprintf(cmd,"AT+CIPSEND=%d",len);
		/*1.发AT命令AT+CIPSEND=*/
		ret = AT_send_cmd(cmd,(int)strlen(cmd),timeout);
		if(ret)
		{
			printf("\r\nAT+CIPSEND faliue,ret = %d\r\n",ret);
			return ret;
		}
	/*2.发送数据*buf*/ /*3.收响应*/
	ret = AT_send_data((char*)buf,len,timeout);
	if(ret<0)
	{
		printf("\r\nSend data faliue,ret = %d\r\n",ret);
		return ret;
	}
	/*成功写*/
	return len;
}
/**********************************************************************
 * 函数名称： platform_net_socket_close	
 * 功能描述： 关闭连接
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 0 成功，其他值 失败
 ***********************************************************************/
int platform_net_socket_close(int fd)
{
  return 	AT_send_cmd("AT+CWQAP",(int)strlen("AT+CWQAP"),2000);
}
