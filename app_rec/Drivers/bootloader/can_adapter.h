#ifndef __CAN_ADAPTER_H
#define __CAN_ADAPTER_H

#include "can.h"
#include <stdint.h>

/**
 * @brief 初始化 CAN 过滤器并启动 CAN（FIFO1 模式）
 */
void can_adapter_init(void);

/**
 * @brief 发送 CAN 消息
 * @param id 标准帧 ID
 * @param data 数据指针
 * @param len 数据长度（最大 8）
 */
void can_adapter_send(uint16_t id, uint8_t *data, uint8_t len);

#endif // __CAN_ADAPTER_H
