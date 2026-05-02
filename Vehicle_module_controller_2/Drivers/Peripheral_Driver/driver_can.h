#ifndef __DRIVER_CAN_H
#define __DRIVER_CAN_H

#include "stm32f1xx_hal.h"

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

void CAN_FilterConfig(void);
void CAN_SendMsg(TxMsg *txMsg);
void CAN_ReceiveMsg(RxMsg *rxMsg);
uint8_t CAN_IsMsgPending(void);

#endif
