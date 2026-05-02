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

/* USER CODE END Includes */

extern CAN_HandleTypeDef hcan;

/* USER CODE BEGIN Private defines */
typedef struct
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t data[8];
} RxMsg;

typedef struct
{
    CAN_TxHeaderTypeDef txHeader; 
    uint32_t txMailBox; // 返回使用的邮箱编号
    uint8_t data[8];    // 发送数据
} TxMsg;

/* USER CODE END Private defines */

void MX_CAN_Init(void);

/* USER CODE BEGIN Prototypes */
// 配置过滤器
void CAN_FilterConfig(void);
// 发送报文
void CAN_SendMsg(TxMsg *txMsg);
// 接收报文
void CAN_ReceiveMsg(RxMsg *rxMsg);
//是否有报文
uint8_t CAN_IsMsgPending(void);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */

