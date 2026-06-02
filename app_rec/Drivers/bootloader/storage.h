#ifndef __STORAGE_H
#define __STORAGE_H

#include <stdint.h>

#define STORAGE_PAGE_SIZE        256
#define STORAGE_META_ADDR        0x000000
#define STORAGE_FW_ADDR          0x001000
#define STORAGE_BOOT_FLAG_ADDR   0x10
#define STORAGE_BOOT_UPDATE      0x01
#define STORAGE_CHECK_KEY        0x5A6B

/**
 * @brief 写入固件数据到 Flash
 */
void storage_write_firmware(uint32_t addr, uint8_t *data, uint16_t len);

/**
 * @brief 读取固件数据从 Flash
 */
void storage_read_firmware(uint32_t addr, uint8_t *data, uint16_t len);

/**
 * @brief 擦除固件区域（扇区 1-10）
 */
void storage_erase_firmware_area(void);

/**
 * @brief 写入元数据（固件地址 + 固件长度）
 */
void storage_write_metadata(uint32_t app_addr, uint32_t app_len);

/**
 * @brief 设置升级标志（写入 EEPROM）
 */
void storage_set_boot_flag(void);

#endif // __STORAGE_H
