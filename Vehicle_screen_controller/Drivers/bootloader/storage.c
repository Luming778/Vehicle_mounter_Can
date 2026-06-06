#include "storage.h"
#include "storage_adapter.h"
#include "Int_w24c02.h"
#include "stdio.h"

/* W25Q32: sector = 4KB (16 pages × 256 bytes), sector 0 保留给元数据 */
#define STORAGE_FW_MAX_SECTOR   128    /* 128 × 4KB = 512KB，预留余量 */

void storage_write_firmware(uint32_t addr, uint8_t *data, uint16_t len)
{
    storage_adapter_write_flash(addr, data, len);
}

void storage_read_firmware(uint32_t addr, uint8_t *data, uint16_t len)
{
    storage_adapter_read_flash(addr, data, len);
}

/**
 * @brief  擦除固件存储区域（sector 1 ~ STORAGE_FW_MAX_SECTOR）
 * @note   必须在固件写入前调用，确保所有目标页为 0xFF
 *         使用固定扇区数，不依赖 total_rec_len（调用时该值为 0）
 */
void storage_erase_firmware_area(void)
{
    for (uint16_t i = 1; i <= STORAGE_FW_MAX_SECTOR; i++)
    {
        storage_adapter_erase_sector(0, (uint8_t)i);
    }
}

void storage_write_metadata(uint32_t app_addr, uint32_t app_len)
{
    storage_adapter_erase_sector(0, 0);

    uint8_t meta[8] = {0};
    meta[0] = (app_addr & 0xFF);
    meta[1] = ((app_addr >> 8) & 0xFF);
    meta[2] = ((app_addr >> 16) & 0xFF);
    meta[3] = ((app_addr >> 24) & 0xFF);
    meta[4] = (app_len & 0xFF);
    meta[5] = ((app_len >> 8) & 0xFF);
    meta[6] = ((app_len >> 16) & 0xFF);
    meta[7] = ((app_len >> 24) & 0xFF);

    storage_adapter_write_flash(STORAGE_META_ADDR, meta, 8);
}

void storage_set_boot_flag(void)
{
    uint8_t buf[3] = {0};
    uint8_t retry;
    
    buf[0] = STORAGE_BOOT_UPDATE;
    buf[1] = (STORAGE_CHECK_KEY >> 8) & 0xFF;
    buf[2] = STORAGE_CHECK_KEY & 0xFF;
    
    storage_adapter_write_eeprom(STORAGE_BOOT_FLAG_ADDR, buf, 3);
    
}
