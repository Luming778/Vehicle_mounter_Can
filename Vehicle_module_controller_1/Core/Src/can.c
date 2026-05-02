/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
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
#include "can.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 36;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_2TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_3TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = ENABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
// 配置过滤器
void CAN_FilterConfig(void)
{
  // 定义结构体变量
  CAN_FilterTypeDef filterConfig;

  // 1. 关联FIFO0
  filterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;

  // 2. 过滤器组编号 0
  filterConfig.FilterBank = 0;

  // 3. 工作模式：屏蔽位模式
  filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;

  // 4. 位宽32位
  filterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

  // 5. 配置ID寄存器-FR1
  filterConfig.FilterIdHigh = (0x7BF <<5)&0xFFE0;
  filterConfig.FilterIdLow = 0x0000;

  // 6. 配置掩码寄存器-FR2
  filterConfig.FilterMaskIdHigh = 0xFFE0;
  filterConfig.FilterMaskIdLow = 0x0000;

  // 7. 激活过滤器组
  filterConfig.FilterActivation = ENABLE;

  HAL_CAN_ConfigFilter(&hcan, &filterConfig);
}

// 发送报文
void CAN_SendMsg(TxMsg *txMsg)
{
  int timeout = 0xffffff; // 设置超时时间为1000ms
  // 1. 检测是否有空闲邮箱，等待直到有邮箱可用
  while ( HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0 && timeout--)
  {
    // 可以添加一些处理逻辑，例如超时处理

  }
  if (timeout == 0)
  {
    // 超时处理，例如返回错误码或打印日志
    printf("CAN Send Timeout!\r\n");
    return;
  }
  // 3. 发送报文信息

  HAL_CAN_AddTxMessage(&hcan, &txMsg->txHeader, txMsg->data, &txMsg->txMailBox);


  // 4. 等待发送成功
  timeout = 0xffffff;
  while (__HAL_CAN_GET_FLAG(&hcan, CAN_FLAG_TXOK0) == 0 && timeout--)
  {
  }
  if (timeout == 0)
  {
    // 超时处理，例如返回错误码或打印日志
    printf("CAN Send Timeout!\r\n");
    return;
  }
}

// 接收报文
void CAN_ReceiveMsg(RxMsg *rxMsg)
{
	
  /*从FIFO0接收报文，将数据存储到rxMsg结构体中，同时更新rxHeader结构体*/
  HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &rxMsg->rxHeader, rxMsg->data);
}

//是否有报文
uint8_t CAN_IsMsgPending(void)
{
  /*检查FIFO0中是否有待接收的报文，如果有返回1，否则返回0*/
  return (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) > 0) ? 1 : 0;
}
/* USER CODE END 1 */

