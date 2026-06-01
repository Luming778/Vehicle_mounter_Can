#ifndef __APP_UPDATE__
#define __APP_UPDATE__

#include <stdint.h>

#define CAN_UPDATE_CMD_ID    0
#define APP_UPDATE_CMD       "sgg"
#define APP_UPDATE_CMD_LEN   3

typedef enum
{
    UPDATE_IDLE = 0,
    UPDATE_RECV_SEND_CMD,
    UPDATE_RECV_DATA,
    UPDATE_RECV_CHECK_DATA,
    UPDATE_RECV_BOOT_UPDATE,
    UPDATE_END
} Update_State_t;

// 供中断回调使用的共享变量
extern volatile Update_State_t update_state;
extern volatile uint32_t total_rec_len;
extern volatile uint32_t flash_write_addr;
extern volatile uint32_t can_last_tick;
extern volatile uint8_t page_ready;
extern volatile uint8_t crc_rx_flag;

/**
 * @brief 初始化升级模块
 */
void App_update_init(void);

/**
 * @brief 主循环调用，执行状态机逻辑（完全非阻塞）
 * @note  可直接放入 FreeRTOS 任务的 while(1) 中调用
 */
void App_update_work(void);

#endif // __APP_UPDATE__
