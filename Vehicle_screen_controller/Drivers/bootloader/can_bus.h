#ifndef __CAN_BUS_H
#define __CAN_BUS_H

#include <stdint.h>

/**
 * @brief 初始化 CAN 模块
 */
void can_bus_init(void);

/**
 * @brief 发送 CAN 消息
 */
void can_bus_send(uint16_t id, uint8_t *data, uint8_t len);

#endif // __CAN_BUS_H
