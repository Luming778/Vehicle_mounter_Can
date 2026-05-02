/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.h
  * @brief   This file contains all the function prototypes for
  *          the can.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CAN_H__
#define __CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "queue.h"
#include <string.h>
/* USER CODE END Includes */

extern CAN_HandleTypeDef hcan;

/* USER CODE BEGIN Private defines */
typedef struct
{
    uint16_t stdID;
    uint8_t data[8];
    uint8_t len;
} RxMsg; //接收can数据

typedef struct {
    uint16_t stdID;
    uint8_t len;
    uint8_t data[8];
} CanMsg_t; //发送给oled

extern QueueHandle_t can_rx_queue;
extern QueueHandle_t mqtt_can_rx_queue;
/* USER CODE END Private defines */

void MX_CAN_Init(void);

/* USER CODE BEGIN Prototypes */
// 配置过滤器
void CAN_FilterConfig(void);

// 发送报文
void CAN_SendMsg(uint16_t stdID, uint8_t * data, uint8_t len);

// 接收报文
void CAN_ReceiveMsg(RxMsg rxMsg[], uint8_t * msgCount);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */

