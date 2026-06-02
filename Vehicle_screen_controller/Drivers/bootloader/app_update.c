#include "app_update.h"
#include "can_bus.h"
#include "storage.h"
#include "usart.h"
#include "crc.h"
#include <string.h>

volatile Update_State_t update_state = UPDATE_IDLE;

// 双缓冲
static uint8_t page_buf_a[STORAGE_PAGE_SIZE];
static uint8_t page_buf_b[STORAGE_PAGE_SIZE];
static uint8_t *volatile active_buf = page_buf_a;
static uint8_t *volatile ready_buf  = NULL;
static volatile uint16_t page_offset = 0;

volatile uint32_t total_rec_len = 0;
volatile uint32_t flash_write_addr = STORAGE_FW_ADDR;
volatile uint32_t can_last_tick = 0;
volatile uint8_t page_ready = 0;
volatile uint8_t crc_rx_flag = 0;
static uint8_t crc_data[4];

// 复位延时计时
static uint32_t reset_tick = 0;

// ============================================================
// CAN RX FIFO1 中断回调（只做 memcpy，不操作 Flash）
// ============================================================
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcanx)
{
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];
    HAL_CAN_GetRxMessage(hcanx, CAN_RX_FIFO1, &header, data);

    if (update_state == UPDATE_RECV_DATA)
    {
        for (uint8_t i = 0; i < header.DLC; i++)
        {
            active_buf[page_offset++] = data[i];
            total_rec_len++;

            if (page_offset >= STORAGE_PAGE_SIZE)
            {
                ready_buf = active_buf;
                active_buf = (active_buf == page_buf_a) ? page_buf_b : page_buf_a;
                page_offset = 0;
                page_ready = 1;
            }
        }
        // // 调试：打印帧间隔（仅每 32 帧打印一次，避免过多输出）
        // {
        //     static uint32_t last_print_tick = 0;
        //     static uint32_t frame_count = 0;
        //     frame_count++;
        //     if (frame_count % 32 == 0)
        //     {
        //         uint32_t now = HAL_GetTick();
        //         printf("frame %d, interval=%ldms\r\n", total_rec_len, now - last_print_tick);
        //         last_print_tick = now;
        //     }
        // }
        can_last_tick = HAL_GetTick();
    }
    else if (update_state == UPDATE_RECV_CHECK_DATA)
    {
        for (uint8_t i = 0; i < header.DLC && i < 4; i++)
        {
            crc_data[i] = data[i];
        }
        crc_rx_flag = 1;
    }
}

// ============================================================
// 状态机处理函数（全部非阻塞）
// ============================================================
static void handle_send_cmd(void)
{
     printf("recv cmd\r\n");
     printf("erasing flash...\r\n");
    storage_erase_firmware_area();
     printf("erase done\r\n");

    page_offset = 0;
    total_rec_len = 0;
    flash_write_addr = STORAGE_FW_ADDR;
    can_last_tick = 0;
    page_ready = 0;
    crc_rx_flag = 0;
    active_buf = page_buf_a;
    ready_buf = NULL;

    __HAL_CRC_DR_RESET(&hcrc);

    can_bus_send(CAN_UPDATE_CMD_ID, (uint8_t *)APP_UPDATE_CMD, APP_UPDATE_CMD_LEN);
    update_state = UPDATE_RECV_DATA;
     printf("send cmd done\r\n");
}

static void handle_recv_data(void)
{
    // 写入已满的页（在主循环中执行，不阻塞中断）
    if (page_ready)
    {
        uint8_t *buf = ready_buf;
        page_ready = 0;
        storage_write_firmware(flash_write_addr, buf, STORAGE_PAGE_SIZE);
        flash_write_addr += STORAGE_PAGE_SIZE;
    }

    // 超时 1 秒：写入最后不足一页的数据
    // 发送端每 256 字节延时 100ms，1 秒足够检测传输结束
    // 发送端发完固件后等 2.1 秒再发 CRC，1 秒超时会在 CRC 到达前触发
    if (can_last_tick != 0 && (HAL_GetTick() - can_last_tick > 1000))
    {
        if (page_offset > 0)
        {
            storage_write_firmware(flash_write_addr, active_buf, page_offset);
        }
         printf("can_rec_msg_len:%d\n", total_rec_len);
        update_state = UPDATE_RECV_CHECK_DATA;
    }
}

static void handle_check_data(void)
{
    if (!crc_rx_flag)
        return;

    crc_rx_flag = 0;
    uint32_t rec_crc = crc_data[0] | (crc_data[1] << 8) | (crc_data[2] << 16) | (crc_data[3] << 24);

    __HAL_CRC_DR_RESET(&hcrc);
    uint32_t words_remaining = (total_rec_len + 3) / 4;
    uint32_t addr = STORAGE_FW_ADDR;
    uint8_t read_buf[256];

    while (words_remaining > 0)
    {
        uint32_t chunk = words_remaining;
        if (chunk > 64) chunk = 64;
        storage_read_firmware(addr, read_buf, chunk * 4);
        HAL_CRC_Accumulate(&hcrc, (uint32_t *)read_buf, chunk);
        addr += chunk * 4;
        words_remaining -= chunk;
    }

    uint32_t calc_crc = hcrc.Instance->DR;

    if (rec_crc == calc_crc)
    {
         printf("crc check pass\r\n");
        update_state = UPDATE_RECV_BOOT_UPDATE;
    }
    else
    {
         printf("crc check fail: rec=%08X calc=%08X len=%d\r\n", rec_crc, calc_crc, total_rec_len);
        update_state = UPDATE_IDLE;
    }
}

static void handle_boot_update(void)
{
    storage_write_metadata(STORAGE_FW_ADDR, total_rec_len);
    storage_set_boot_flag();
    reset_tick = HAL_GetTick();
    update_state = UPDATE_END;
}

// ============================================================
// 公共接口
// ============================================================
void App_update_init(void)
{
    // UART 初始化已在 usart.c 中完成，此处不再配置
    can_bus_init();
    update_state = UPDATE_RECV_SEND_CMD;  // 直接进入发送命令状态
}

void App_update_work(void)
{
    switch (update_state)
    {
    case UPDATE_IDLE:
        // 非阻塞，直接返回
        break;

    case UPDATE_RECV_SEND_CMD:
        handle_send_cmd();
        break;

    case UPDATE_RECV_DATA:
        handle_recv_data();
        break;

    case UPDATE_RECV_CHECK_DATA:
        handle_check_data();
        break;

    case UPDATE_RECV_BOOT_UPDATE:
        handle_boot_update();
        break;

    case UPDATE_END:
        // 非阻塞延时 1 秒后复位
        if (HAL_GetTick() - reset_tick >= 1000)
        {
            HAL_NVIC_SystemReset();
        }
        break;

    default:
        break;
    }
}
