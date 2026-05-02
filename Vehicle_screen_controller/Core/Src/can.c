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
   HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
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

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
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

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
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
  filterConfig.FilterIdHigh = 0x0000;
  filterConfig.FilterIdLow = 0x0000;

  // 6. 配置掩码寄存器-FR2
  filterConfig.FilterMaskIdHigh = 0x0000;
  filterConfig.FilterMaskIdLow = 0x0000;

  // 7. 激活过滤器组
  filterConfig.FilterActivation = ENABLE;

  HAL_CAN_ConfigFilter(&hcan, &filterConfig);
}

// 发送报文
void CAN_SendMsg(uint16_t stdID, uint8_t * data, uint8_t len)
{
  // 1. 检测是否有空闲邮箱，等待直到有邮箱可用
  while ( HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0 )
  {
  }
  
  // 2. 配置数据帧信息
  CAN_TxHeaderTypeDef txHeader;
  txHeader.StdId = stdID;
  txHeader.IDE = CAN_ID_STD;  // 标准帧
  txHeader.RTR = CAN_RTR_DATA;  // 数据帧
  txHeader.DLC = len;

  // 3. 发送报文信息
  uint32_t txMailBox;
  HAL_CAN_AddTxMessage(&hcan, &txHeader, data, &txMailBox);

  // 4. 等待发送成功
  while (__HAL_CAN_GET_FLAG(&hcan, CAN_FLAG_TXOK0) == 0)
  {
  }
}

// 接收报文
void CAN_ReceiveMsg(RxMsg rxMsg[], uint8_t * msgCount)
{
  // 1. 检测FIFO0中收到的报文个数
  * msgCount = HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0);

  // 2. 遍历接收所有报文
  CAN_RxHeaderTypeDef rxHeader;
  for (uint8_t i = 0; i < *msgCount; i++)
  {
    HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &rxHeader, rxMsg[i].data);
    rxMsg[i].stdID = rxHeader.StdId;
    rxMsg[i].len = rxHeader.DLC;
  }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	  RxMsg rxMsg[4];     
    uint8_t msgCount = 0;
	  CAN_ReceiveMsg(rxMsg, &msgCount);
	
	  for(uint8_t i=0; i<msgCount; i++)
    {
        CanMsg_t msg;
        msg.stdID = rxMsg[i].stdID;
        msg.len   = rxMsg[i].len;
        memcpy(msg.data, rxMsg[i].data, 8);

        // 中断中安全发送到队列
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(can_rx_queue, &msg, &xHigherPriorityTaskWoken);
        xQueueSendFromISR(mqtt_can_rx_queue,&msg, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
/* USER CODE END 1 */

