#include "W25Q128.h"
#include "stm32f1xx_hal.h"

#define W25Q_DEFAULT_TIMEOUT_MS      1000
#define W25Q_SECTOR_ERASE_TIMEOUT_MS 5000
#define W25Q_BLOCK_ERASE_TIMEOUT_MS  15000
#define W25Q_CHIP_ERASE_TIMEOUT_MS   180000

/* 片选拉低：选中器件 */
static void W25Q_Select(W25Q128_HandleTypeDef *h)
{
	HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_RESET);
}

/* 片选拉高：释放器件 */
static void W25Q_Deselect(W25Q128_HandleTypeDef *h)
{
	HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
}

/*
 * 初始化 W25Q128 句柄
 * - 保存 SPI 句柄与 CS 引脚
 * - 将 CS 置为未选中状态
 */
HAL_StatusTypeDef W25Q128_Init(W25Q128_HandleTypeDef *h, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
	if (!h || !hspi || !cs_port) return HAL_ERROR;
	h->hspi = hspi;
	h->cs_port = cs_port;
	h->cs_pin = cs_pin;
	W25Q_Deselect(h);
	return HAL_OK;
}

/*
 * 读取 JEDEC ID（三字节）
 * 用于识别芯片型号与厂商
 */
HAL_StatusTypeDef W25Q128_ReadJEDECID(W25Q128_HandleTypeDef *h, uint8_t *id3)
{
	uint8_t cmd = W25Q_CMD_JEDEC_ID;
	if (!h || !id3) return HAL_ERROR;
	W25Q_Select(h);
	if (HAL_SPI_Transmit(h->hspi, &cmd, 1, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) {
		W25Q_Deselect(h); return HAL_ERROR;
	}
	if (HAL_SPI_Receive(h->hspi, id3, 3, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) {
		W25Q_Deselect(h); return HAL_ERROR;
	}
	W25Q_Deselect(h);
	return HAL_OK;
}

/*
 * 读取状态寄存器 1
 * 返回值的 BIT0 为 BUSY 标志（1 = 繁忙）
 */
HAL_StatusTypeDef W25Q128_ReadStatus(W25Q128_HandleTypeDef *h, uint8_t *status)
{
	uint8_t cmd = W25Q_CMD_READ_STATUS;
	if (!h || !status) return HAL_ERROR;
	W25Q_Select(h);
	if (HAL_SPI_Transmit(h->hspi, &cmd, 1, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) { W25Q_Deselect(h); return HAL_ERROR; }
	if (HAL_SPI_Receive(h->hspi, status, 1, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) { W25Q_Deselect(h); return HAL_ERROR; }
	W25Q_Deselect(h);
	return HAL_OK;
}

/*
 * 发送写使能命令 WREN
 * 必须在写/擦除命令前调用
 */
HAL_StatusTypeDef W25Q128_WriteEnable(W25Q128_HandleTypeDef *h)
{
	uint8_t cmd = W25Q_CMD_WRITE_ENABLE;
	if (!h) return HAL_ERROR;
	W25Q_Select(h);
	if (HAL_SPI_Transmit(h->hspi, &cmd, 1, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) { W25Q_Deselect(h); return HAL_ERROR; }
	W25Q_Deselect(h);
	return HAL_OK;
}

/*
 * 等待器件 BUSY 清零（就绪）
 * - 每次读取状态寄存器并检查 BUSY 位
 * - 超时返回 HAL_TIMEOUT
 */
HAL_StatusTypeDef W25Q128_WaitUntilReady(W25Q128_HandleTypeDef *h, uint32_t timeout_ms)
{
	uint32_t t0 = HAL_GetTick();
	uint8_t status = 0;
	if (!h) return HAL_ERROR;
	do {
		if (W25Q128_ReadStatus(h, &status) != HAL_OK) return HAL_ERROR;
		if ((status & 0x01) == 0) return HAL_OK; // BUSY bit cleared
		HAL_Delay(1);
	} while ((HAL_GetTick() - t0) < timeout_ms);
	return HAL_TIMEOUT;
}

/*
 * 从指定地址读取数据
 * - addr: 24 位地址
 * - 支持读取任意长度，但注意缓冲区大小
 */
HAL_StatusTypeDef W25Q128_ReadData(W25Q128_HandleTypeDef *h, uint32_t addr, uint8_t *buf, uint32_t len)
{
	uint8_t cmd[4];
	if (!h || !buf || len == 0) return HAL_ERROR;
	cmd[0] = W25Q_CMD_READ_DATA;
	cmd[1] = (uint8_t)(addr >> 16);
	cmd[2] = (uint8_t)(addr >> 8);
	cmd[3] = (uint8_t)(addr);
	W25Q_Select(h);
	if (HAL_SPI_Transmit(h->hspi, cmd, 4, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) { W25Q_Deselect(h); return HAL_ERROR; }
	if (HAL_SPI_Receive(h->hspi, buf, len, W25Q_DEFAULT_TIMEOUT_MS + len) != HAL_OK) { W25Q_Deselect(h); return HAL_ERROR; }
	W25Q_Deselect(h);
	return HAL_OK;
}

/*
 * 单页编程（Page Program）
 * - addr: 24 位地址
 * - len 必须 > 0
 */
HAL_StatusTypeDef W25Q128_PageProgram(W25Q128_HandleTypeDef *h, uint32_t addr, const uint8_t *buf, uint32_t len)
{
	uint8_t header[4];
	if (!h || !buf || len == 0) return HAL_ERROR;

	while (len > 0) {
		uint32_t page_offset = addr % W25Q_PAGE_SIZE;
		uint32_t space_in_page = W25Q_PAGE_SIZE - page_offset;
		uint32_t write_len = (len < space_in_page) ? len : space_in_page;

		/* 使能写 */
		if (W25Q128_WriteEnable(h) != HAL_OK) return HAL_ERROR;

		/* 命令 + 24-bit 地址 */
		header[0] = W25Q_CMD_PAGE_PROGRAM;
		header[1] = (uint8_t)(addr >> 16);
		header[2] = (uint8_t)(addr >> 8);
		header[3] = (uint8_t)(addr);

		W25Q_Select(h);
		if (HAL_SPI_Transmit(h->hspi, header, 4, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) {
			W25Q_Deselect(h);
			return HAL_ERROR;
		}

		if (HAL_SPI_Transmit(h->hspi, (uint8_t*)buf, write_len, W25Q_DEFAULT_TIMEOUT_MS + write_len) != HAL_OK) {
			W25Q_Deselect(h);
			return HAL_ERROR;
		}

		W25Q_Deselect(h);

		/* 等待本页写入完成 */
		if (W25Q128_WaitUntilReady(h, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) return HAL_TIMEOUT;

		/* 更新指针与计数，处理跨页 */
		addr += write_len;
		buf += write_len;
		len -= write_len;
	}

	return HAL_OK;
}

/*
 * 擦除 4KB 扇区（Sector Erase）
 * - 地址低位将被忽略，按扇区对齐
 */
HAL_StatusTypeDef W25Q128_SectorErase4K(W25Q128_HandleTypeDef *h, uint32_t addr)
{
	uint8_t cmd[4];
	if (!h) return HAL_ERROR;
	if (W25Q128_WriteEnable(h) != HAL_OK) return HAL_ERROR;
	cmd[0] = W25Q_CMD_SECTOR_ERASE;
	cmd[1] = (uint8_t)(addr >> 16);
	cmd[2] = (uint8_t)(addr >> 8);
	cmd[3] = (uint8_t)(addr);
	W25Q_Select(h);
	if (HAL_SPI_Transmit(h->hspi, cmd, 4, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) { W25Q_Deselect(h); return HAL_ERROR; }
	W25Q_Deselect(h);
	return W25Q128_WaitUntilReady(h, W25Q_SECTOR_ERASE_TIMEOUT_MS);
}

/*
 * 擦除 64KB 块（Block Erase）
 */
HAL_StatusTypeDef W25Q128_BlockErase64K(W25Q128_HandleTypeDef *h, uint32_t addr)
{
	uint8_t cmd[4];
	if (!h) return HAL_ERROR;
	if (W25Q128_WriteEnable(h) != HAL_OK) return HAL_ERROR;
	cmd[0] = W25Q_CMD_BLOCK_ERASE;
	cmd[1] = (uint8_t)(addr >> 16);
	cmd[2] = (uint8_t)(addr >> 8);
	cmd[3] = (uint8_t)(addr);
	W25Q_Select(h);
	if (HAL_SPI_Transmit(h->hspi, cmd, 4, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) { W25Q_Deselect(h); return HAL_ERROR; }
	W25Q_Deselect(h);
	return W25Q128_WaitUntilReady(h, W25Q_BLOCK_ERASE_TIMEOUT_MS);
}

/*
 * 芯片擦除（Chip Erase）
 * - 请注意：此操作耗时较长，调用前应确认
 */
HAL_StatusTypeDef W25Q128_ChipErase(W25Q128_HandleTypeDef *h)
{
	uint8_t cmd = W25Q_CMD_CHIP_ERASE;
	if (!h) return HAL_ERROR;
	if (W25Q128_WriteEnable(h) != HAL_OK) return HAL_ERROR;
	W25Q_Select(h);
	if (HAL_SPI_Transmit(h->hspi, &cmd, 1, W25Q_DEFAULT_TIMEOUT_MS) != HAL_OK) { W25Q_Deselect(h); return HAL_ERROR; }
	W25Q_Deselect(h);
	return W25Q128_WaitUntilReady(h, W25Q_CHIP_ERASE_TIMEOUT_MS);
}

/*
    例程
*/
#if 0
void W25Q128_Demo(void)
  {
    W25Q128_HandleTypeDef w25;
    uint8_t id[3] = {0};
    const uint32_t test_addr = 0x000000;
    enum { TEST_DATA_LEN = W25Q_PAGE_SIZE + 20 };
    uint8_t tx_data[TEST_DATA_LEN];
    uint8_t rx_data[TEST_DATA_LEN];

    for (uint32_t i = 0; i < TEST_DATA_LEN; i++) {
      tx_data[i] = (uint8_t)((i & 0xFF));
    }

    /* 初始化 W25Q128 驱动 */
    W25Q128_Init(&w25, &hspi2, W25Q128_CS_GPIO_Port, W25Q128_CS_Pin);

    /* 读取 JEDEC ID 并通过 UART 输出 */
    if (W25Q128_ReadJEDECID(&w25, id) == HAL_OK) {
      char msg[64];
      int len = snprintf(msg, sizeof(msg), "W25Q JEDEC ID: %02X %02X %02X\r\n", id[0], id[1], id[2]);
      HAL_UART_Transmit(&huart1, (uint8_t*)msg, len, 200);
    } else {
      const char err[] = "W25Q Read JEDEC ID failed\r\n";
      HAL_UART_Transmit(&huart1, (uint8_t*)err, sizeof(err) - 1, 200);
    }

    /* 擦除扇区、写入并读回验证（跨页写入测试） */
    if (W25Q128_SectorErase4K(&w25, test_addr) == HAL_OK) {
      HAL_Delay(50);
      if (W25Q128_PageProgram(&w25, test_addr, tx_data, TEST_DATA_LEN) == HAL_OK) {
        HAL_Delay(10);
        if (W25Q128_ReadData(&w25, test_addr, rx_data, TEST_DATA_LEN) == HAL_OK) {
          if (memcmp(tx_data, rx_data, TEST_DATA_LEN) == 0) {
            const char ok[] = "W25Q cross-page write/read verify: PASS\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)ok, sizeof(ok) - 1, 200);
						printf("写入数据如下:%s\r\n", rx_data);
            HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
          } else {
            const char fail[] = "W25Q cross-page write/read verify: FAIL\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)fail, sizeof(fail) - 1, 200);
            HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
          }
        }
      }
    }
  }
#endif
