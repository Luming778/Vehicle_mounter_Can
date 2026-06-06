#include "Int_w25q32.h"
#include "stm32f1xx_hal.h"

#define W25Q32_BUSY_TIMEOUT_MS  5000

/**
 * @brief ����Ƭѡ
 *
 */
void Int_w25q32_start(void)
{
    HAL_GPIO_WritePin(W25Q128_CS_GPIO_Port, W25Q128_CS_Pin, GPIO_PIN_RESET);
}

/**
 * @brief ����Ƭѡ
 *
 */
void Int_w25q32_stop(void)
{
    HAL_GPIO_WritePin(W25Q128_CS_GPIO_Port, W25Q128_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief д��һ���ֽ�
 *
 * @param data
 */
void Int_w25q32_write_byte(uint8_t data)
{
    HAL_SPI_Transmit(&hspi2, &data, 1, 100);
}

/**
 * @brief ��ȡһ���ֽ�
 *
 * @return uint8_t
 */
uint8_t Int_w25q32_read_byte(void)
{
    uint8_t data;
    // uint8_t dummy = 0xff;
    HAL_SPI_Receive(&hspi2, &data, 1, 100);
    return data;
}

/**
 * @brief ��ȡоƬID
 *
 * @param mf_id
 * @param device_id
 */
void Int_w25q32_read_id(uint8_t *mf_id, uint16_t *device_id)
{
    // 1. ����Ƭѡ
    Int_w25q32_start();

    // 2. ���Ͷ�ȡIDָ��
    Int_w25q32_write_byte(W25Q32_READ_ID);

    // 3. ��ȡmf_id
    *mf_id = Int_w25q32_read_byte();
    uint8_t high = Int_w25q32_read_byte();
    uint8_t low = Int_w25q32_read_byte();
    *device_id = high << 8 | low;

    // 4. ����Ƭѡ
    Int_w25q32_stop();
}

// ��̬���� �ȴ�оƬæ״̬
static uint8_t Int_w25q32_wait_busy(void)
{
    Int_w25q32_start();

    uint32_t tick_start = HAL_GetTick();
    while (1)
    {
        Int_w25q32_write_byte(W25Q32_READ_STATUS_REG);
        uint8_t status = Int_w25q32_read_byte();
        if ((status & 0x01) == 0)
        {
            Int_w25q32_stop();
            return 0;
        }
        if ((HAL_GetTick() - tick_start) > W25Q32_BUSY_TIMEOUT_MS)
        {
            Int_w25q32_stop();
            return 1;
        }
    }
}

/**
 * @brief ��ȡ����
 *
 * addr: һ����22λ  0x000000 -> 0x3F  F  F  FF  һ�β���4096�ֽ�  һ��д����256�ֽ�
 */
// void Int_w25q32_read_data(uint32_t addr, uint8_t *data, uint16_t len);
void Int_w25q32_read_data(uint8_t block, uint8_t sector, uint8_t page, uint8_t addr, uint8_t *data, uint16_t len)
{
    Int_w25q32_start();

    Int_w25q32_write_byte(W25Q32_READ_DATA);
    uint32_t addr_24 = block << 16 | sector << 12 | page << 8 | addr;
    Int_w25q32_write_byte(addr_24 >> 16);
    Int_w25q32_write_byte(addr_24 >> 8);
    Int_w25q32_write_byte(addr_24);

    HAL_SPI_Receive(&hspi2, data, len, 100);

    Int_w25q32_stop();
}

/**
 * @brief ��ȡ���� 32λ��ַ
 *
 * @param addr
 * @param data
 * @param len
 */
void Int_w25q32_read_data_with_32addr(uint32_t addr, uint8_t *data, uint16_t len)
{
    Int_w25q32_start();

    Int_w25q32_write_byte(W25Q32_READ_DATA);

    Int_w25q32_write_byte((addr >> 16) & 0xff);
    Int_w25q32_write_byte((addr >> 8) & 0xff);
    Int_w25q32_write_byte(addr & 0xff);

    HAL_SPI_Receive(&hspi2, data, len, 100);

    Int_w25q32_stop();
}

static void Int_w25q32_write_enable(void)
{
    // 1. �ȴ�æ״̬
    Int_w25q32_wait_busy();
    // 2. ����Ƭѡ
    Int_w25q32_start();
    // 3. ����дʹ������
    Int_w25q32_write_byte(W25Q32_WRITE_ENABLE);
    // 4. ����Ƭѡ
    Int_w25q32_stop();
}

/**
 * @brief д������
 * �����ַ������1ҳ�ķ�Χ
 */
void Int_w25q32_write_data(uint8_t block, uint8_t sector, uint8_t page, uint8_t addr, uint8_t *data, uint16_t len)
{
    // 1.  дʹ��
    Int_w25q32_write_enable();

    // 2. ����Ƭѡ
    Int_w25q32_start();
    uint32_t addr_24 = block << 16 | sector << 12 | page << 8 | addr;
    Int_w25q32_write_byte(W25Q32_WRITE_DATA);
    Int_w25q32_write_byte(addr_24 >> 16);
    Int_w25q32_write_byte(addr_24 >> 8);
    Int_w25q32_write_byte(addr_24);
    // 3. д������
    for (uint16_t i = 0; i < len; i++)
    {
        Int_w25q32_write_byte(data[i]);
    }
    // 4. ����Ƭѡ
    Int_w25q32_stop();
    // 5. ���ȴ�ҳ��������
    Int_w25q32_wait_busy();
}

/**
 * @brief д������
 * �����ַ������1ҳ�ķ�Χ
 */
void Int_w25q32_write_data_with_32addr(uint32_t addr, uint8_t *data, uint16_t len)
{
    // 1.  дʹ��
    Int_w25q32_write_enable();

    // 2. ����Ƭѡ
    Int_w25q32_start();

    Int_w25q32_write_byte(W25Q32_WRITE_DATA);
    Int_w25q32_write_byte((addr >> 16) & 0xff);
    Int_w25q32_write_byte((addr >> 8) & 0xff);
    Int_w25q32_write_byte(addr & 0xff);
    // 3. д������
    for (uint16_t i = 0; i < len; i++)
    {
        Int_w25q32_write_byte(data[i]);
    }
    // 4. ����Ƭѡ
    Int_w25q32_stop();
    // 5. ���ȴ�ҳ��������
    Int_w25q32_wait_busy();
}

/**
 * @brief ����1������
 *
 */
void Int_w25q32_erase_sector(uint8_t block, uint8_t sector)
{
    // 1. дʹ��
    Int_w25q32_write_enable();

    // 2. ����Ƭѡ
    Int_w25q32_start();
    uint32_t addr = (uint32_t)block * 65536 + (uint32_t)sector * 4096;
    // 3. ���Ͳ���ָ��
    Int_w25q32_write_byte(W25Q32_ERASE_SECTOR);
    // 4. ���͵�ַ
    Int_w25q32_write_byte((addr >> 16) & 0xff);
    Int_w25q32_write_byte((addr >> 8) & 0xff);
    Int_w25q32_write_byte(addr & 0xff);
    // 5. ����Ƭѡ
    Int_w25q32_stop();
    // 6. ���ȴ��������
    Int_w25q32_wait_busy();
}
