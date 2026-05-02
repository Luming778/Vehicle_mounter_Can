/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f1xx_hal.h"
#include "can.h"
#include "usart.h"
#include "string.h"
#include "OLED.h"
#include "queue.h"
#include "platform_esp8266_AT.h"
#include "mqtt_config.h"
#include "mqtt_log.h"
#include "mqttclient.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
void oled_task(void * pvParameters );
void can_task(void * pvParameters );
void MQTT_Task(void*parm);
void voice_task(void * pvParameters );
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
TaskHandle_t  oled_handler;
TaskHandle_t  can_handler;

TaskHandle_t  voice_handler;
TaskHandle_t AT_pars_handle;//AT指令分析任务句柄

QueueHandle_t can_rx_queue;
QueueHandle_t mqtt_can_rx_queue;//服务器接收can消息
QueueHandle_t voice_rx_queue; 
extern uint8_t g_uart3_rx_char;   //串口3接收数据变量
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationIdleHook(void);

/* USER CODE BEGIN 2 */
//static char g_task_infor[200];
//void vApplicationIdleHook(void)
//{
//	vTaskList(g_task_infor);
//		for(int i = 0 ;i<16;i++)
//	{
//		printf("-");
//	}
//	printf("\n\r\n\r");
//	printf("%s\n\r",g_task_infor);
//}
/* USER CODE END 2 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
	AT_Init();//AT命令相关程序初始化
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  BaseType_t ret;
	 // 进入临界区
  taskENTER_CRITICAL();
	ret = xTaskCreate(oled_task,"oled_task",200,NULL,15,&oled_handler);
  if(ret != pdPASS)
  {
     printf("oled_task create failed!\r\n");
  }
	ret = xTaskCreate(can_task,"can_task",128,NULL,13,&can_handler);
  if(ret != pdPASS)
  {
     printf("can_task create failed!\r\n");
  }
  ret = xTaskCreate(AT_Parse,"AT_parse",68,NULL,15,&AT_pars_handle);
  if(ret != pdPASS)
  {
     printf("AT_parse create failed!\r\n");
  }
	ret = xTaskCreate(MQTT_Task,"MQTT_Task",200,NULL,15,NULL);
  if(ret != pdPASS)
  {
     printf("MQTT_Task create failed!\r\n");
  }
	xTaskCreate(voice_task,"voice_task",128,NULL,14,&voice_handler);

	can_rx_queue = xQueueCreate(5, sizeof(CanMsg_t));
  mqtt_can_rx_queue = xQueueCreate(5, sizeof(CanMsg_t)); //在can接收中断队列
	voice_rx_queue = xQueueCreate(5, sizeof(uint8_t));
   // 退出临界区
  taskEXIT_CRITICAL();
	vTaskDelete(NULL);
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void oled_task(void * pvParameters )  //can_rx
{
	CanMsg_t msg;
	OLED_Printf(0,0, OLED_8X16,"车灯:");
	OLED_Printf(0,16,OLED_8X16,"车窗:");
	OLED_Printf(0,32,OLED_8X16,"天窗:");
	OLED_Printf(0,48,OLED_8X16,"车门:");	
	OLED_Update();
	
	while(1)
	{
		while(xQueueReceive(can_rx_queue, &msg, portMAX_DELAY) != pdPASS);
		if(msg.stdID==0x666)
		{
			if(msg.data[2]==0x01)
			{
				OLED_ClearArea(40,0,32,16);
				OLED_Printf(40,0,OLED_8X16,"开启");
				OLED_UpdateArea(40,0,32,16);
			}
			else if(msg.data[2]==0x02)
			{
				OLED_ClearArea(40,0,32,16);
				OLED_Printf(40,0,OLED_8X16,"关闭");
				OLED_UpdateArea(40,0,32,16);
			}
			else if(msg.data[2]==0x03)
			{
				OLED_ClearArea(40,16,32,16);
				OLED_Printf(40,16,OLED_8X16,"开启");
				OLED_UpdateArea(40,16,32,16);
			}
			else if(msg.data[2]==0x04)
			{
				OLED_ClearArea(40,16,32,16);
				OLED_Printf(40,16,OLED_8X16,"关闭");
				OLED_UpdateArea(40,16,32,16);
			}
			else if(msg.data[2]==0x05)
			{
				OLED_ClearArea(40,32,32,16);
				OLED_Printf(40,32,OLED_8X16,"开启");
				OLED_UpdateArea(40,32,32,16);
			}
			else if(msg.data[2]==0x06)
			{
				OLED_ClearArea(40,32,32,16);
				OLED_Printf(40,32,OLED_8X16,"关闭");
				OLED_UpdateArea(40,32,32,16);
			}
		}
		else if(msg.stdID==0x555)
		{
			if(msg.data[2]==0x07)
			{
				OLED_ClearArea(40,48,32,16);
				OLED_Printf(40,48,OLED_8X16,"开启");
				OLED_UpdateArea(40,48,32,16);
			}
			else if(msg.data[2]==0x08)
			{
				OLED_ClearArea(40,48,32,16);
				OLED_Printf(40,48,OLED_8X16,"关闭");
				OLED_UpdateArea(40,48,32,16);
		  }
		}
		//vTaskDelay(1000);
	}
}


void can_task(void * pvParameters )
{
  printf("send test ...\n");
	// 发送报文
//	uint32_t stdID = 0x77f;
//	uint16_t stdID2 =0x768;
//	uint8_t * data = "abdc";
//  uint8_t * data2 = "111";
//	// 定义发送数据缓冲区，在基本信息的基础上拼接一个数字
//	uint8_t buffer[10] ={0} ;
//	uint32_t i = 10;
	while(1)
	{	
    // 拼接要发送的报文信息
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
		//CAN_SendMsg(stdID, data, strlen((char *)data));
		//CAN_SendMsg(stdID2, data2, strlen((char *)data2));
		//printf("send ok...\r\n");
		// 每隔1s发送一次报文
		  vTaskDelay(1000);
	}
}

void voice_task(void * pvParameters ) //can_tx
{
	uint16_t stdID = 0x77f;
	uint16_t stdID2 = 0X7bf;
	uint8_t pdata[3]={0xaa,0x11,0};
	uint8_t data=0;
	while(1)
	{	
		while((xQueueReceive( voice_rx_queue,&data ,portMAX_DELAY) != pdPASS));
		pdata[2]=data;
		if(pdata[2]==0x01|pdata[2]==0x02|pdata[2]==0x03|pdata[2]==0x04|pdata[2]==0x05|pdata[2]==0x06)
		{
			CAN_SendMsg(stdID, pdata, strlen((char *)pdata));
		}
		else if(pdata[2]==0x07|pdata[2]==0x08)
		{
			CAN_SendMsg(stdID2, pdata, strlen((char *)pdata));
		}
			//vTaskDelay(1000);
	}
}

static void MQTTSmart_handler(void* client, message_data_t* msg)
{
	uint16_t stdID = 0x77f;
	uint16_t stdID2 = 0X7bf;
	uint8_t pdata[3]={0xaa,0x11,0};
	(void) client;
	MQTT_LOG_I("-----------------------------------------------------------------------------------");
	MQTT_LOG_I("%s:%d %s()...\ntopic: %s\nmessage:%s", __FILE__, __LINE__, __FUNCTION__, msg->topic_name, (char*)msg->message->payload);
	MQTT_LOG_I("-----------------------------------------------------------------------------------");
	/*消息处理函数 eg.{led_on}*/
	if(strstr((char*)msg->message->payload,"打开车门"))
	{
		pdata[2]=0x07;
		CAN_SendMsg(stdID2, pdata, strlen((char *)pdata));
	}
	else if(strstr((char*)msg->message->payload,"关闭车门"))
	{
		pdata[2]=0x08;
		CAN_SendMsg(stdID2, pdata, strlen((char *)pdata));
	}
	else if(strstr((char*)msg->message->payload,"打开车灯"))
	{
		pdata[2]=0x01;
		CAN_SendMsg(stdID, pdata, strlen((char *)pdata));
	}
	else if(strstr((char*)msg->message->payload,"关闭车灯"))
	{
		pdata[2]=0x02;
		CAN_SendMsg(stdID, pdata, strlen((char *)pdata));
	}
	else if(strstr((char*)msg->message->payload,"打开车窗"))
	{
		pdata[2]=0x03;
		CAN_SendMsg(stdID, pdata, strlen((char *)pdata));
	}
	else if(strstr((char*)msg->message->payload,"关闭车窗"))
	{
		pdata[2]=0x04;
		CAN_SendMsg(stdID, pdata, strlen((char *)pdata));
	}
	else if(strstr((char*)msg->message->payload,"打开天窗"))
	{
		pdata[2]=0x05;
		CAN_SendMsg(stdID, pdata, strlen((char *)pdata));
	}
	else if(strstr((char*)msg->message->payload,"关闭天窗"))
	{
		pdata[2]=0x06;
		CAN_SendMsg(stdID, pdata, strlen((char *)pdata));
	}
}

void MQTT_Task(void*parm)
{
    int res;
    mqtt_client_t *client = NULL;
    
    printf("\nwelcome to mqttclient test...\r\n");
		HAL_UART_Receive_DMA(&huart3,&g_uart3_rx_char, 1); //开dma中断
	
    mqtt_log_init();

    client = mqtt_lease();
	mqtt_set_port(client, "1883");  //修改服务器端口
	
    mqtt_set_host(client, "10.60.14.76");  //修改服务器IP
    mqtt_set_client_id(client, random_string(10));
    mqtt_set_user_name(client, random_string(10));
    mqtt_set_password(client, random_string(10));
    mqtt_set_clean_session(client, 1);
		
		
    res = mqtt_connect(client);
		if(MQTT_SUCCESS_ERROR != res)
		{
			printf("MQTT Broker connect failed\r\n");
		}
		
    res =mqtt_subscribe(client, "MQTTSmart", QOS0, MQTTSmart_handler);
    if(MQTT_SUCCESS_ERROR != res)
		{
			printf("MQTT Broker subscribe topic1 failed\r\n");
		}
		
    CanMsg_t msg;
	mqtt_message_t mqtt_msg;
	mqtt_msg.qos = 0;
   	
		while(1)
		{
			while(xQueueReceive(mqtt_can_rx_queue, &msg, portMAX_DELAY) != pdPASS);
			if(msg.stdID==0x666)
			{
				if(msg.data[2]==0x01)
				{
					mqtt_msg.payload = "车灯：开启\r\n";
					mqtt_msg.payloadlen = strlen(mqtt_msg.payload);
					mqtt_publish(client, "mcu_test", &mqtt_msg);
				}
				else if(msg.data[2]==0x02)
				{
					mqtt_msg.payload = "车灯：关闭\r\n";
					mqtt_msg.payloadlen = strlen(mqtt_msg.payload);
					mqtt_publish(client, "mcu_test", &mqtt_msg);
				}
				else if(msg.data[2]==0x03)
				{
					mqtt_msg.payload = "车窗：开启\r\n";
					mqtt_msg.payloadlen = strlen(mqtt_msg.payload);
					mqtt_publish(client, "mcu_test", &mqtt_msg);
				}
				else if(msg.data[2]==0x04)
				{
					mqtt_msg.payload = "车窗：关闭\r\n";
					mqtt_msg.payloadlen = strlen(mqtt_msg.payload);
					mqtt_publish(client, "mcu_test", &mqtt_msg);
				}
				else if(msg.data[2]==0x05)
				{
					mqtt_msg.payload = "天窗：开启\r\n";
					mqtt_msg.payloadlen = strlen(mqtt_msg.payload);
					mqtt_publish(client, "mcu_test", &mqtt_msg);
				}
				else if(msg.data[2]==0x06)
				{
					mqtt_msg.payload = "天窗：关闭\r\n";
					mqtt_msg.payloadlen = strlen(mqtt_msg.payload);
					mqtt_publish(client, "mcu_test", &mqtt_msg);
				}
			}
			else if(msg.stdID==0x555)
			{
				if(msg.data[2]==0x07)
				{
					mqtt_msg.payload = "车门：开启\r\n";
					mqtt_msg.payloadlen = strlen(mqtt_msg.payload);
					mqtt_publish(client, "mcu_test", &mqtt_msg);
				}
				else if(msg.data[2]==0x08)
				{
					mqtt_msg.payload = "车门：关闭\r\n";
					mqtt_msg.payloadlen = strlen(mqtt_msg.payload);
					mqtt_publish(client, "mcu_test", &mqtt_msg);
				}
			}
//			mqtt_publish(client, "mcu_test", &msg);
			// vTaskDelay(3000);
		}
}
/* USER CODE END Application */

