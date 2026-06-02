#ifndef __APP_UPDATE_H__
#define __APP_UPDATE_H__

#include "INT_CAN.h"
#include "usart.h"
#include "crc.h"
#include "AT24C02.h"
#include "w25q128.h"

#define BOOTLOADER_START_ADDR 0x08000000
#define BOOTLOADER_END_ADDR 0x08080000
#define STACK_ADDR 0x20000000

#define APP_UPDATE_USART_CMD "app_update"
#define APP_UPDATE_CAN_CMD "update"

/** AT24C02中校验密钥 */
#define CHECK_KEY_ADDR 0x11
#define CHECK_KEY 0x55A2

/** AT24C02中Bootloader更新标志 */
#define BOOTLOADER_UPDATE_ADDR 0x10
#define BOOTLOADER_UPDATE 0x01
#define BOOTLOADER_NO_UPDATE 0x02
#define BOOTLOADER_DEFAULT 0x03

/** w25Q128中应用数据写入内部flash的起始地址 */
#define APP_START_ADDR_UPDATE 0x08010000
/** w25Q128中应用数据起始地址 */
#define W25Q128_APP_ADDR 0x000000

typedef enum
{
    APP_UPDATE_STATE_IDLE = 0,
    APP_UPDATE_STATE_SEND,
    APP_UPDATE_STATE_RECEIVE,
    APP_UPDATE_STATE_CHECK,
    APP_UPDATE_STATE_WRITE,
    APP_UPDATE_STATE_JUMP,
}APP_UPDATE_STATE;



/**
 * @brief 初始化串口接收，can接收
 * @retval None
 */
void app_update_init(void);

/**
 * @brief 发送应用更新命令
 * @retval None
 */
void app_update_send_cmd(void);

/**
 * @brief 接收应用数据
 * @retval None
 */
void app_update_receive_app(void);

/**
 * @brief 检查应用数据
 * @retval None
 */
void app_update_check_app(void);

/**
 * @brief 校验通过后写入应用数据到w25Q128中
 * @retval None
 */
void app_update_write_app(void);

/**
 * @brief 更新模块工作函数,main循环调用
 * @retval None
 */
void app_update_work(void);

#endif /* __APP_UPDATE_H__ */
