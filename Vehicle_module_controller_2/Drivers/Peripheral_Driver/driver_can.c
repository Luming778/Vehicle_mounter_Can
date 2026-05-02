#include "can.h"
#include "driver_can.h"
#include "stdio.h"
/*只取标准ID为0x77F的报文，其他ID为0的报文*/
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
  filterConfig.FilterIdHigh = (0x77F<<5)&0xFFE0;
  filterConfig.FilterIdLow = 0x0000;

  // 6. 配置掩码寄存器-FR2
  filterConfig.FilterMaskIdHigh = 0xFFE0; // 只匹配ID的高11位，低5位不匹配
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
