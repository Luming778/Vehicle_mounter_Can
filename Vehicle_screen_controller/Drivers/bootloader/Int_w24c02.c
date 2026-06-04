#include "Int_w24c02.h"

/**
 * @brief 读取一个字节
 *
 * @param byte_addr
 * @return uint8_t
 */
uint8_t Int_w24c02_read_byte(uint8_t byte_addr)
{
    uint8_t data = 0;
    if (HAL_I2C_Mem_Read(&hi2c1, W24C02_ADDR_R, byte_addr, I2C_MEMADD_SIZE_8BIT, &data, 1, 100) != HAL_OK)
    {
        printf("EEPROM read byte failed at addr 0x%02X\r\n", byte_addr);
    }
    return data;
}

/**
 * @brief 写入一个字节
 *
 * @param byte_addr
 * @param data
 */
void Int_w24c02_write_byte(uint8_t byte_addr, uint8_t data)
{
    if (HAL_I2C_Mem_Write(&hi2c1, W24C02_ADDR, byte_addr, I2C_MEMADD_SIZE_8BIT, &data, 1, 100) != HAL_OK)
    {
        printf("EEPROM write byte failed at addr 0x%02X\r\n", byte_addr);
    }
}

/**
 * @brief 读取多个字节
 *
 */
void Int_w24c02_read_bytes(uint8_t byte_addr, uint8_t *data, uint16_t len)
{

    // 1. 流程介绍: 启动信号  发送写从设备地址  发送字节地址 启动信号  发送读从设备地址  读取数据 ACK 读取数据 ACK... 读取最后一个数据 NACK 停止信号
    // (1)句柄编号 I2C2 (2) 设备地址 芯片固定值 (3) 字节地址 数据写入的位置 (4) 地址长度 8位 (5) 数据存放地址 (6) 数据长度 (7) 超时时间
    if (HAL_I2C_Mem_Read(&hi2c1, W24C02_ADDR_R, byte_addr, I2C_MEMADD_SIZE_8BIT, data, len, 1000) != HAL_OK)
    {
        printf("EEPROM read failed at addr 0x%02X, len %d\r\n", byte_addr, len);
    }
}

/**
 * @brief 写入多个字节
 * EEPROM中  一次只能写入一页 (16字节)   0x00 -> 0x10  从0x05开始写 => 也一样只能写到0x10
 * @param byte_addr
 * @param data
 * @param len
 */
void Int_w24c02_write_bytes(uint8_t byte_addr, uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;

    // 1.0 健壮性判断  地址值 不能超过EEPROM的地址值
    if (byte_addr + len > 255)
    {
        printf("写入的地址值超过EEPROM的地址值\n");
        return;
    }

    // 1.1 判断当前一页的剩余空间
    uint8_t page_remain_len = 16 - byte_addr % 16;

    if (len <= page_remain_len)
    {
        // 可以一次写完
        status = HAL_I2C_Mem_Write(&hi2c1, W24C02_ADDR, byte_addr,
                                   I2C_MEMADD_SIZE_8BIT, data, len, 1000);
        if (status != HAL_OK)
        {
            printf("EEPROM write failed at addr 0x%02X, len %d, status %d\r\n",
                   byte_addr, len, status);
        }
    }
    else
    {
        // 下次写入的起始地址
        uint8_t start_page_addr = byte_addr;
        // 已写入的累计字节数
        uint16_t written = 0;
        // 不能一次写完
        while (len > page_remain_len)
        {
            // 将当前页剩余的空间 写满
            status = HAL_I2C_Mem_Write(&hi2c1, W24C02_ADDR, start_page_addr,
                                       I2C_MEMADD_SIZE_8BIT, data + written,
                                       page_remain_len, 1000);
            if (status != HAL_OK)
            {
                printf("EEPROM write failed at addr 0x%02X, len %d, status %d\r\n",
                       start_page_addr, page_remain_len, status);
                return;
            }

            written += page_remain_len;
            start_page_addr += page_remain_len;
            // 数据还剩下的长度
            len -= page_remain_len;
            // 当前页剩余的空间
            page_remain_len = 16;
            // EEPROM每次写入之后需要等待5ms以上
            HAL_Delay(10);
        }
        // 最后一页写入
        if (len != 0)
        {
            status = HAL_I2C_Mem_Write(&hi2c1, W24C02_ADDR, start_page_addr,
                                       I2C_MEMADD_SIZE_8BIT, data + written,
                                       len, 1000);
            if (status != HAL_OK)
            {
                printf("EEPROM write failed at addr 0x%02X, len %d, status %d\r\n",
                       start_page_addr, len, status);
            }
        }
    }
}
