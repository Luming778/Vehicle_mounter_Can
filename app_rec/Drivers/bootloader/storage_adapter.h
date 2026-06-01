#ifndef __STORAGE_ADAPTER_H
#define __STORAGE_ADAPTER_H

#include <stdint.h>

/**
 * @brief 写入固件数据到外部 Flash
 */
void storage_adapter_write_flash(uint32_t addr, uint8_t *data, uint16_t len);

/**
 * @brief 读取固件数据从外部 Flash
 */
void storage_adapter_read_flash(uint32_t addr, uint8_t *data, uint16_t len);

/**
 * @brief 擦除 Flash 扇区
 */
void storage_adapter_erase_sector(uint8_t block, uint8_t sector);

/**
 * @brief 写入字节到 EEPROM
 */
void storage_adapter_write_eeprom(uint8_t byte_addr, uint8_t *data, uint16_t len);

#endif // __STORAGE_ADAPTER_H
