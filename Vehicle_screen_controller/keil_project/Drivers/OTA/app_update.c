#include "app_update.h"
#include "string.h"
static APP_UPDATE_STATE app_update_state = APP_UPDATE_STATE_IDLE;
//串口接收CMD缓冲区
#define UART_REC_BUFF_LEN 32
static uint8_t uart_rec_buff[UART_REC_BUFF_LEN] = {0};
//can接收缓冲区
static CAN_Res_t can_res[3];
//can接收消息数量
static uint8_t msg_count = 0;

//app update接收数据长度
#define APP_UPDATE_REC_BUFF_LEN 20000
//app update接收缓冲区（保留以兼容旧逻辑）
static uint8_t app_update_rec_buff[APP_UPDATE_REC_BUFF_LEN] = {0};
static uint16_t app_update_rec_len = 0;
//时间戳
static uint32_t app_update_rec_time = 0;

// W25Q128 流式写入状态
static W25Q128_HandleTypeDef w25;
static uint32_t app_update_write_addr = W25Q128_APP_ADDR + 8;
static uint32_t app_update_running_crc = 0;
static uint8_t app_update_crc_buf[4] = {0};
static uint8_t app_update_crc_buf_len = 0;
static uint8_t w25_ready = 0;
// W25Q 页缓冲，用于达到 256 字节一次写入
static uint8_t w25_page_buf[W25Q_PAGE_SIZE] = {0};
static uint16_t w25_page_offset = 0;

extern void LED_flash(void);

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1 && app_update_state == APP_UPDATE_STATE_IDLE)
    {
        if(strstr((char*)uart_rec_buff, APP_UPDATE_USART_CMD) != NULL)
        {
            printf("app update command received\r\n");
            app_update_state = APP_UPDATE_STATE_SEND;
            memset(uart_rec_buff, 0, UART_REC_BUFF_LEN);
        }
        else
        {
            printf("unknown command: %s\r\n", uart_rec_buff);
            memset(uart_rec_buff, 0, UART_REC_BUFF_LEN);
        }
        __HAL_UART_CLEAR_OREFLAG(&huart1);
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);  
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, UART_REC_BUFF_LEN);
    }
}

/**
 * @brief 初始化串口接收，can接收
 * @retval None
 */
void app_update_init(void)
{
    app_update_state = APP_UPDATE_STATE_IDLE;
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);  
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, UART_REC_BUFF_LEN);
    INT_CAN_Init();
}

/**
 * @brief 发送应用更新命令
 * @retval None
 */
static HAL_StatusTypeDef app_update_prepare_flash(void)
{
    if (W25Q128_Init(&w25, &hspi2, W25Q128_CS_GPIO_Port, W25Q128_CS_Pin) != HAL_OK)
    {
        return HAL_ERROR;
    }

    uint32_t total_size = 8 + APP_UPDATE_REC_BUFF_LEN;
    uint32_t erase_sectors = (total_size + W25Q_SECTOR_SIZE - 1) / W25Q_SECTOR_SIZE;

    for (uint32_t i = 0; i < erase_sectors; i++)
    {
        uint32_t sector_addr = W25Q128_APP_ADDR + i * W25Q_SECTOR_SIZE;
        if (W25Q128_SectorErase4K(&w25, sector_addr) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

static void app_update_crc_feed_byte(uint8_t byte)
{
    app_update_crc_buf[app_update_crc_buf_len++] = byte;
    if (app_update_crc_buf_len >= 4)
    {
        uint32_t word = app_update_crc_buf[0]
                      | ((uint32_t)app_update_crc_buf[1] << 8)
                      | ((uint32_t)app_update_crc_buf[2] << 16)
                      | ((uint32_t)app_update_crc_buf[3] << 24);
        app_update_running_crc = HAL_CRC_Accumulate(&hcrc, &word, 1);
        app_update_crc_buf_len = 0;
    }
}

static uint32_t app_update_crc_finalize(void)
{
    if (app_update_crc_buf_len > 0)
    {
        while (app_update_crc_buf_len < 4)
        {
            app_update_crc_buf[app_update_crc_buf_len++] = 0;
        }

        uint32_t word = app_update_crc_buf[0]
                      | ((uint32_t)app_update_crc_buf[1] << 8)
                      | ((uint32_t)app_update_crc_buf[2] << 16)
                      | ((uint32_t)app_update_crc_buf[3] << 24);
        app_update_running_crc = HAL_CRC_Accumulate(&hcrc, &word, 1);
        app_update_crc_buf_len = 0;
    }
    return app_update_running_crc;
}

void app_update_send_cmd(void)
{
    printf("app update command sent\r\n");
    INT_CAN_Send(0, (uint8_t*)APP_UPDATE_CAN_CMD, strlen(APP_UPDATE_CAN_CMD));

    app_update_rec_len = 0;
    app_update_rec_time = 0;
    app_update_write_addr = W25Q128_APP_ADDR + 8;
    app_update_running_crc = 0;
    app_update_crc_buf_len = 0;
    __HAL_CRC_DR_RESET(&hcrc);
    w25_ready = 0;

    if (app_update_prepare_flash() != HAL_OK)
    {
        printf("W25Q128 prepare failed\r\n");
        app_update_state = APP_UPDATE_STATE_IDLE;
        return;
    }

    w25_ready = 1;
    app_update_state = APP_UPDATE_STATE_RECEIVE;
    printf("receiving app data...\r\n");
    /*1.关闭其他应用程序中断*/
    // __disable_irq();
}

/**
 * @brief 接收应用程序时停止其他函数执行，接收应用数据存储在w25Q128中
 * @retval None
 */
void app_update_receive_app(void)
{
    /*2.进行can数据接收*/
    INT_CAN_Receive(can_res, &msg_count);
    for (int i = 0; i < msg_count; i++)
    {
        app_update_rec_time = HAL_GetTick();
        uint8_t len = can_res[i].rxHeader.DLC;

        if (len == 0)
            continue;

        if (!w25_ready)
        {
            printf("W25Q128 not ready\r\n");
            app_update_state = APP_UPDATE_STATE_IDLE;
            return;
        }

        uint8_t *data = can_res[i].data;
        for (uint8_t j = 0; j < len; j++)
        {
            // 拷贝到页缓冲
            w25_page_buf[w25_page_offset++] = data[j];

            // 同时累加 CRC
            app_update_crc_feed_byte(data[j]);

            app_update_rec_len++;

            // 当页缓冲满 256 字节，写入一次
            if (w25_page_offset >= W25Q_PAGE_SIZE)
            {
                if (W25Q128_PageProgram(&w25, app_update_write_addr, w25_page_buf, W25Q_PAGE_SIZE) != HAL_OK)
                {
                    printf("W25Q128 page write failed at 0x%06lX\r\n", (unsigned long)app_update_write_addr);
                    app_update_state = APP_UPDATE_STATE_IDLE;
                    return;
                }
                app_update_write_addr += W25Q_PAGE_SIZE;
                // 清空页缓冲
                w25_page_offset = 0;
                memset(w25_page_buf, 0, W25Q_PAGE_SIZE);
            }
        }
    }

    msg_count = 0;

    /*3.判断接收是否完成，超时时间为2秒*/
    if (app_update_rec_len > 0 && (HAL_GetTick() - app_update_rec_time) > 2000)
    {
        // 刷出剩余不足 256 字节的数据
        if (w25_page_offset > 0)
        {
            if (W25Q128_PageProgram(&w25, app_update_write_addr, w25_page_buf, w25_page_offset) != HAL_OK)
            {
                printf("W25Q128 final write failed at 0x%06lX\r\n", (unsigned long)app_update_write_addr);
                app_update_state = APP_UPDATE_STATE_IDLE;
                return;
            }
            app_update_write_addr += w25_page_offset;
            w25_page_offset = 0;
            memset(w25_page_buf, 0, W25Q_PAGE_SIZE);
        }

        app_update_running_crc = app_update_crc_finalize();
        printf("app data received and written, length: %d\r\n", app_update_rec_len);
        memset(can_res, 0, sizeof(can_res));
        msg_count = 0;
        printf("app update check app data...\r\n");
        app_update_state = APP_UPDATE_STATE_CHECK;
    }
}

/**
 * @brief 检查应用数据
 * @retval None
 */
void app_update_check_app(void)
{
    //1. 接收上位机的crc校验值，进行校验
    INT_CAN_Receive(can_res, &msg_count);
    for (int i = 0; i < msg_count; i++)
    {
        uint32_t crc_received = (can_res[i].data[3] << 24) |
                                (can_res[i].data[2] << 16) |
                                (can_res[i].data[1] << 8) |
                                can_res[i].data[0];

        if (crc_received == app_update_running_crc)
        {
            //3. 校验通过，更新AT24C02中的标志位
            msg_count = 0;
            memset(can_res, 0, sizeof(can_res));
            printf("app data check passed\r\n");
            app_update_state = APP_UPDATE_STATE_WRITE;
            printf("app update write app data...\r\n");
        }
        else
        {
            printf("app data check failed, crc_calculated: %08X, crc_received: %08X\r\n", app_update_running_crc, crc_received);
            //4. 校验失败，重置状态，等待重新接收
            app_update_rec_time = 0;
            app_update_rec_len = 0;
            app_update_write_addr = W25Q128_APP_ADDR + 8;
            app_update_running_crc = 0;
            app_update_crc_buf_len = 0;
            w25_ready = 0;
            __HAL_CRC_DR_RESET(&hcrc);
            msg_count = 0;
            memset(can_res, 0, sizeof(can_res));
            app_update_state = APP_UPDATE_STATE_IDLE;
        }
    }
}

/**
 * @brief 校验通过后写入应用数据到w25Q128中
 * @retval None
 */
void app_update_write_app(void)
{
    uint32_t app_data_len = app_update_rec_len;

    if (app_data_len == 0)
    {
        printf("app update write failed: no app data\r\n");
        app_update_state = APP_UPDATE_STATE_IDLE;
        return;
    }

    if (!w25_ready)
    {
        if (W25Q128_Init(&w25, &hspi2, W25Q128_CS_GPIO_Port, W25Q128_CS_Pin) != HAL_OK)
        {
            printf("W25Q128 init failed\r\n");
            app_update_state = APP_UPDATE_STATE_IDLE;
            return;
        }
    }

    /* 写入头部信息：内部 Flash 起始地址 + 应用大小 */
    uint8_t header[8];
    header[0] = (uint8_t)(APP_START_ADDR_UPDATE);
    header[1] = (uint8_t)(APP_START_ADDR_UPDATE >> 8);
    header[2] = (uint8_t)(APP_START_ADDR_UPDATE >> 16);
    header[3] = (uint8_t)(APP_START_ADDR_UPDATE >> 24);
    header[4] = (uint8_t)(app_data_len);
    header[5] = (uint8_t)(app_data_len >> 8);
    header[6] = (uint8_t)(app_data_len >> 16);
    header[7] = (uint8_t)(app_data_len >> 24);

    if (W25Q128_PageProgram(&w25, W25Q128_APP_ADDR, header, sizeof(header)) != HAL_OK)
    {
        printf("W25Q128 header write failed\r\n");
        app_update_state = APP_UPDATE_STATE_IDLE;
        return;
    }

    /* 写入更新标志位到 AT24C02 */
    uint8_t key_buf[2];
    key_buf[0] = (uint8_t)(CHECK_KEY & 0xFF);
    key_buf[1] = (uint8_t)((CHECK_KEY >> 8) & 0xFF);
    AT24C02_write_bytes(CHECK_KEY_ADDR, key_buf, sizeof(key_buf));
    AT24C02_write_one_byte(BOOTLOADER_UPDATE_ADDR, BOOTLOADER_UPDATE);

    printf("app update write complete, length: %lu\r\n", (unsigned long)app_data_len);
    app_update_state = APP_UPDATE_STATE_JUMP;
}

uint8_t app_update_jump_to_bootloader(void)
{

    typedef void (*pFunc)(void);
    // 1. 检查
    // 获取栈顶指针
    uint32_t app_stack_ptr = *(volatile uint32_t *)(BOOTLOADER_START_ADDR);
    uint32_t app_reset_handle = *(volatile uint32_t *)(BOOTLOADER_START_ADDR + 4);

    // 1.1 检查栈地址合法性
    if ((app_stack_ptr & 0xFFFF0000) != STACK_ADDR)
    {
        printf("stack addr error\n");
        return 1;
    }

    // 1.2 检查复位向量地址合法性
    if (app_reset_handle < BOOTLOADER_START_ADDR || app_reset_handle > BOOTLOADER_END_ADDR)
    {
        printf("reset handle error\n");
        return 1;
    }

    // 2. 关闭boot loader相关资源
    // 2.1 关闭全局中断
    __disable_irq();

    // 关闭hal库外设初始化，解除hal库占用  调用hal库反初始化HAL_DeInit();
    HAL_DeInit();


    // 2.2 关闭滴答定时器
    SysTick->CTRL = 0;
    SysTick->VAL  = 0;
    // 2.3 关闭所有中断，清除所有挂起的中断
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;

    // 2.3 配置向量表偏移寄存器
    SCB->VTOR = BOOTLOADER_START_ADDR;
    __DSB();
    __ISB();

    __set_MSP(app_stack_ptr);
    // 2.4 跳转到APP程序入口地址
    pFunc jump_to_app = (pFunc)app_reset_handle;
    // 跳转到APP程序不复位此函数
    jump_to_app();
		
		return 0;
}

/**
 * @brief 更新模块工作函数,main循环调用
 * @retval None
 */
void app_update_work(void)
{
    switch (app_update_state)
    {
        case APP_UPDATE_STATE_IDLE:
            LED_flash();    // 主函数循环
            break;
        case APP_UPDATE_STATE_SEND:
            app_update_send_cmd();
            memset(app_update_rec_buff, 0, APP_UPDATE_REC_BUFF_LEN);
            break;
        case APP_UPDATE_STATE_RECEIVE:
            app_update_receive_app();
            break;
        case APP_UPDATE_STATE_CHECK:
            app_update_check_app();
            break;
        case APP_UPDATE_STATE_WRITE:
            app_update_write_app();
            break;
        case APP_UPDATE_STATE_JUMP:
						
            app_update_jump_to_bootloader();
            break;
        default:
            break;
    }
}
