#ifndef __INT_CAN_H__
#define __INT_CAN_H__

#include "can.h"

typedef struct {
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t data[8];
} CAN_Res_t;

/* 环形缓冲区 */
#define CAN_RING_BUF_SIZE 256

typedef struct {
    CAN_Res_t buf[CAN_RING_BUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} CAN_RingBuf_t;

/*
   1.配置白名单过滤器 2.开启can 3.初始化环形缓冲区 4.激活中断通知
 * @brief Initialize the CAN interface
*/
void INT_CAN_Init(void);
/*
    * @brief Send data over CAN
    * @param data: Pointer to the data buffer to send
    * @param size: Number of bytes to send (must be <= 8)
*/
void INT_CAN_Send(uint32_t id, uint8_t* data, uint8_t size);

/*
    * @brief Receive data from CAN (从环形缓冲区读取)
    * @param data: Pointer to a CAN_Res_t structure to store the received data 数组
    * @param msg_count: Pointer to a variable to store the number of received messages
*/
void INT_CAN_Receive(CAN_Res_t* data, uint8_t *msg_count);

#endif /* __INT_CAN_H__ */
