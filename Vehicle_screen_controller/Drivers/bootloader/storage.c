#include "storage.h"
#include "storage_adapter.h"
#include "Int_w24c02.h"
#include "stdio.h"
void storage_write_firmware(uint32_t addr, uint8_t *data, uint16_t len)
{
    storage_adapter_write_flash(addr, data, len);
}

void storage_read_firmware(uint32_t addr, uint8_t *data, uint16_t len)
{
    storage_adapter_read_flash(addr, data, len);
}

void storage_erase_firmware_area(void)
{
    // 擦除 sector 1-30（120KB），足够存放 100KB 固件
    for (uint8_t i = 1; i <= 30; i++)
    {
        storage_adapter_erase_sector(0, i);
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
    
    /* 写前诊断：检测 EEPROM 是否在线 */
    if (HAL_I2C_IsDeviceReady(&hi2c1, W24C02_ADDR, 3, 5) != HAL_OK)
    {
        printf("boot flag: EEPROM not found on I2C bus!\r\n");
        return;
    }
    
    buf[0] = STORAGE_BOOT_UPDATE;
    buf[1] = (STORAGE_CHECK_KEY >> 8) & 0xFF;
    buf[2] = STORAGE_CHECK_KEY & 0xFF;
    
    storage_adapter_write_eeprom(STORAGE_BOOT_FLAG_ADDR, buf, 3);
    
    /* EEPROM 写周期 ≤5ms，延时 10ms 确保写入完成 */
    HAL_Delay(10);
    
    /* 回读校验，最多重试 3 次，失败时复位 I2C 总线 */
    for (retry = 0; retry < 3; retry++)
    {
        buf[0] = 0;
        buf[1] = 0;
        buf[2] = 0;
        
        if (HAL_I2C_Mem_Read(&hi2c1, W24C02_ADDR_R, STORAGE_BOOT_FLAG_ADDR,
                             I2C_MEMADD_SIZE_8BIT, buf, 3, 1000) == HAL_OK)
        {
            printf("boot flag: 0x%02X 0x%02X 0x%02X\r\n", buf[0], buf[1], buf[2]);
            return;
        }
        
        /* 读失败：复位 I2C 总线后重试 */
        HAL_I2C_DeInit(&hi2c1);
        MX_I2C1_Init();
        HAL_Delay(10);
    }
    
    printf("boot flag: EEPROM write/read failed after 3 retries\r\n");
}
