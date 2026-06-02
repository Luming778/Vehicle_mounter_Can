#include "AT24C02.h"

/*
    * @brief Write one byte to AT24C02
    * @param mem_addr: Memory address to write to (0-255)
    * @param data: Data byte to write
*/
void AT24C02_write_one_byte(uint8_t mem_addr, uint8_t data)
{
    HAL_I2C_Mem_Write(&hi2c1, AT24C02_ADDR_w, mem_addr, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
}

/*
    * @brief Read one byte from AT24C02
    * @param mem_addr: Memory address to read from (0-255)
    * @return: Data byte read from the specified memory address
*/
uint8_t AT24C02_read_one_byte(uint8_t mem_addr)
{
    uint8_t data;
    HAL_I2C_Mem_Read(&hi2c1, AT24C02_ADDR_r, mem_addr, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    return data;
}

/*  写多个字节时，只能按页写入，每页最多16字节，且不能跨页写入
    0x00-0x0F为第一页，0x10-0x1F为第二页，以此类推
     例如：要写入20字节数据到地址0x00开始的内存位置，必须分两次写入：
     第一次写入16字节到地址0x00（第一页）
     第二次写入剩余4字节到地址0x10（第二页）
    * @brief Write multiple bytes to AT24C02
    * @param mem_addr: Starting memory address to write to (0-255)
    * @param data: Pointer to the data buffer to write
    * @param size: Number of bytes to write
*/
void AT24C02_write_bytes(uint8_t mem_addr, uint8_t* data, uint16_t size)
{
    if (size == 0) return;

    const uint8_t page_size = 16; /* AT24C02 page size */
    uint16_t remaining = size;
    uint16_t addr = mem_addr; /* use wider type to avoid overflow during arithmetic */
    uint8_t *p = data;

    while (remaining > 0) {
        uint8_t page_offset = addr % page_size;
        uint8_t space_in_page = page_size - page_offset;
        uint16_t chunk = (remaining < space_in_page) ? remaining : space_in_page;

        if (HAL_I2C_Mem_Write(&hi2c1, AT24C02_ADDR_w, (uint16_t)addr, I2C_MEMADD_SIZE_8BIT, p, chunk, HAL_MAX_DELAY) != HAL_OK) {
            return; /* write failed - bail out */
        }

        /* Wait for internal write cycle to complete by polling device readiness.
           This avoids blind delays and is more efficient. */
        const uint32_t trials = 100; /* max trials */
        const uint32_t timeout_ms = 5; /* per-trial timeout (ms) */
        uint32_t tries = 0;
        while (HAL_I2C_IsDeviceReady(&hi2c1, AT24C02_ADDR_w, 1, timeout_ms) != HAL_OK) {
            tries++;
            if (tries >= trials) {
                return; /* device not responding in time */
            }
        }

        remaining -= chunk;
        addr = (addr + chunk) & 0xFF; /* wrap within 0-255 if needed */
        p += chunk;
    }
}

/*
    * @brief Read multiple bytes from AT24C02
    * @param mem_addr: Starting memory address to read from (0-255)
    * @param data: Pointer to the data buffer to store the read data
    * @param size: Number of bytes to read
*/
void AT24C02_read_bytes(uint8_t mem_addr, uint8_t* data, uint16_t size)
{
    HAL_I2C_Mem_Read(&hi2c1, AT24C02_ADDR_r, mem_addr, I2C_MEMADD_SIZE_8BIT, data, size, HAL_MAX_DELAY);
}
