#ifndef __W25Q128_H
#define __W25Q128_H

#include "spi.h"
#include "stm32f1xx_hal.h"

#define W25Q_CMD_READ_DATA       0x03
#define W25Q_CMD_FAST_READ       0x0B
#define W25Q_CMD_PAGE_PROGRAM    0x02
#define W25Q_CMD_SECTOR_ERASE    0x20
#define W25Q_CMD_BLOCK_ERASE     0xD8
#define W25Q_CMD_CHIP_ERASE      0xC7
#define W25Q_CMD_READ_STATUS     0x05
#define W25Q_CMD_WRITE_ENABLE    0x06
#define W25Q_CMD_WRITE_DISABLE   0x04
#define W25Q_CMD_JEDEC_ID        0x9F

#define W25Q_PAGE_SIZE           256
#define W25Q_SECTOR_SIZE         4096

/**
 * W25Q128 句柄结构体
 * - hspi: 指向 HAL SPI 句柄
 * - cs_port: 片选 GPIO 端口
 * - cs_pin: 片选 GPIO 引脚
 */
typedef struct {
	SPI_HandleTypeDef *hspi;
	GPIO_TypeDef *cs_port;
	uint16_t cs_pin;
} W25Q128_HandleTypeDef;

/**
 * @brief 初始化 W25Q128 句柄并拉高 CS
 * @param h: 指向 W25Q128 句柄结构体
 * @param hspi: 指向已配置好的 SPI 句柄
 * @param cs_port: CS 所在的 GPIO 端口
 * @param cs_pin: CS 引脚
 * @return HAL status
 */
HAL_StatusTypeDef W25Q128_Init(W25Q128_HandleTypeDef *h, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);

/**
 * @brief 读取 JEDEC ID（三字节）
 * @param h: 设备句柄
 * @param id3: 输出 3 字节 ID，长度至少为 3
 * @return HAL status
 */
HAL_StatusTypeDef W25Q128_ReadJEDECID(W25Q128_HandleTypeDef *h, uint8_t *id3);

/**
 * @brief 读取状态寄存器 1
 * @param h: 设备句柄
 * @param status: 输出状态字节
 * @return HAL status
 */
HAL_StatusTypeDef W25Q128_ReadStatus(W25Q128_HandleTypeDef *h, uint8_t *status);

/**
 * @brief 发送写使能命令（WREN）
 * @param h: 设备句柄
 * @return HAL status
 */
HAL_StatusTypeDef W25Q128_WriteEnable(W25Q128_HandleTypeDef *h);

/**
 * @brief 从指定地址读取数据（字节流）
 * @param h: 设备句柄
 * @param addr: 24 位地址
 * @param buf: 接收缓冲区
 * @param len: 要读取的字节长度
 * @return HAL status
 */
HAL_StatusTypeDef W25Q128_ReadData(W25Q128_HandleTypeDef *h, uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief 页编程（单页内最多 256 字节）
 * @param h: 设备句柄
 * @param addr: 起始地址（建议不跨页）
 * @param buf: 待写入数据缓冲区
 * @param len: 写入字节数（<= 256）
 * @return HAL status
 */
HAL_StatusTypeDef W25Q128_PageProgram(W25Q128_HandleTypeDef *h, uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief 擦除 4KB 扇区
 * @param h: 设备句柄
 * @param addr: 扇区内任意地址（低 12 位被忽略）
 * @return HAL status
 */
HAL_StatusTypeDef W25Q128_SectorErase4K(W25Q128_HandleTypeDef *h, uint32_t addr);

/**
 * @brief 擦除 64KB 块
 * @param h: 设备句柄
 * @param addr: 块内任意地址（低 16 位被忽略）
 * @return HAL status
 */
HAL_StatusTypeDef W25Q128_BlockErase64K(W25Q128_HandleTypeDef *h, uint32_t addr);

/**
 * @brief 芯片擦除（可能耗时较长）
 * @param h: 设备句柄
 * @return HAL status
 */
HAL_StatusTypeDef W25Q128_ChipErase(W25Q128_HandleTypeDef *h);

/**
 * @brief 等待设备就绪（BUSY 清零）
 * @param h: 设备句柄
 * @param timeout_ms: 超时时间（毫秒）
 * @return HAL_OK 或 HAL_TIMEOUT/HAL_ERROR
 */
HAL_StatusTypeDef W25Q128_WaitUntilReady(W25Q128_HandleTypeDef *h, uint32_t timeout_ms);

#endif /* __W25Q128_H */
