#ifndef __AT24C02_H__
#define __AT24C02_H__

#include "i2c.h"

#define AT24C02_ADDR_w 0xA0
#define AT24C02_ADDR_r 0xA1

/*
    * @brief Write one byte to AT24C02
    * @param mem_addr: Memory address to write to (0-255)
    * @param data: Data byte to write
*/
void AT24C02_write_one_byte(uint8_t mem_addr, uint8_t data);

/*
    * @brief Read one byte from AT24C02
    * @param mem_addr: Memory address to read from (0-255)
    * @return: Data byte read from the specified memory address
*/
uint8_t AT24C02_read_one_byte(uint8_t mem_addr);

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
void AT24C02_write_bytes(uint8_t mem_addr, uint8_t* data, uint16_t size);

/*  
    * @brief Read multiple bytes from AT24C02
    * @param mem_addr: Starting memory address to read from (0-255)
    * @param data: Pointer to the data buffer to store the read data
    * @param size: Number of bytes to read
*/
void AT24C02_read_bytes(uint8_t mem_addr, uint8_t* data, uint16_t size);


#endif /* __AT24C02_H__ */
