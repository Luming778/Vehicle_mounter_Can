/*
 * @Author: jiejie
 * @Github: https://github.com/jiejieTop
 * @Date: 2019-12-15 13:39:00
 * @LastEditTime: 2020-04-27 23:46:58
 * @Description: the code belongs to jiejie, please keep the author information and source code according to the license.
 */
#ifndef _PLATFORM_ESP8266_AT_H_
#define _PLATFORM_ESP8266_AT_H_

#include <stdint.h>

/*AT回复宏*/
#define AT_OK			0
#define AT_ERR			-1
#define AT_TIMEOUT		-2
#define AT_RESP_LEN		128
#define AT_SEND_TIMEOUT	1000

/*AT命令类型*/
enum AT_Typte {
	Recevie_Data = 1,
	Send_Data,
	Send_Data_Busy
};

/*AT命令初始化*/
void AT_Init(void);

/*AT数据发送函数*/
int AT_send_data(char* data ,int data_len,int timeout);

/*从Processr_recv_data()函数环形buff读出数据*/
int AT_Recv_data(char *c,unsigned int timeout);

/* AT命令发送函数*/
int AT_send_cmd(char* cmd ,int cmd_len,int timeout);

/*AT解析任务*/
void AT_Parse(void*parmas);

#endif /* _PLATFORM_ESP8266_AT_H_ */
