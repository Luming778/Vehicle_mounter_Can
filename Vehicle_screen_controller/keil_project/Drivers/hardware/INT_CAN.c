#include "INT_CAN.h"

/* 环形缓冲区实例 */
static CAN_RingBuf_t can_ring_buf;

/* 环形缓冲区操作函数 */
static void RingBuf_Init(CAN_RingBuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

static uint16_t RingBuf_Available(CAN_RingBuf_t *rb)
{
    return (rb->head - rb->tail) % CAN_RING_BUF_SIZE;
}

static uint8_t RingBuf_Put(CAN_RingBuf_t *rb, CAN_Res_t *msg)
{
    if (RingBuf_Available(rb) >= CAN_RING_BUF_SIZE - 1)
        return 0; // 满
    rb->buf[rb->head] = *msg;
    rb->head = (rb->head + 1) % CAN_RING_BUF_SIZE;
    return 1;
}

static uint8_t RingBuf_Get(CAN_RingBuf_t *rb, CAN_Res_t *msg)
{
    if (rb->head == rb->tail)
        return 0; // 空
    *msg = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % CAN_RING_BUF_SIZE;
    return 1;
}

/*
   1.配置白名单过滤器 2.开启can 3.初始化环形缓冲区 4.激活中断通知
 * @brief Initialize the CAN interface
*/
void INT_CAN_Init(void)
{
    // 初始化环形缓冲区
    RingBuf_Init(&can_ring_buf);

    // 配置白名单过滤器
    CAN_FilterTypeDef FilterConfig;

    FilterConfig.FilterBank = 0;
    FilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    FilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    FilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    // 5. 配置ID寄存器-FR1  只接收 1 ID的消息
    FilterConfig.FilterIdHigh = 0x0020;
    FilterConfig.FilterIdLow = 0x0000;
    // 6. 配置MASK寄存器-FR1  只接收 1 ID的消息
    FilterConfig.FilterMaskIdHigh = 0xffe0;
    FilterConfig.FilterMaskIdLow = 0x0000;

    FilterConfig.FilterActivation = ENABLE;

    HAL_CAN_ConfigFilter(&hcan, &FilterConfig);
    // 开启CAN
    HAL_CAN_Start(&hcan);
    // 激活RX FIFO0中断通知
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/*
    * @brief CAN RX FIFO0 消息挂起中断回调
    *        中断触发时立即从硬件FIFO读取所有消息存入环形缓冲区
*/
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
    CAN_Res_t msg;
    while (HAL_CAN_GetRxFifoFillLevel(hcan_ptr, CAN_RX_FIFO0) > 0)
    {
        if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0, &msg.rxHeader, msg.data) == HAL_OK)
        {
            RingBuf_Put(&can_ring_buf, &msg);
        }
    }
}

/*
    * @brief Send data over CAN
    * @param data: Pointer to the data buffer to send
    * @param size: Number of bytes to send (must be <= 8)
*/
void INT_CAN_Send(uint32_t id, uint8_t* data, uint8_t size)
{
    /*等有空闲邮箱发送*/
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0);
    /* 发送数据 */
    CAN_TxHeaderTypeDef TxHeader;
    TxHeader.StdId = id;
    TxHeader.ExtId = 0x0000;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.DLC = size;
    uint32_t TxMailbox = 0;
    HAL_CAN_AddTxMessage(&hcan, &TxHeader, data, &TxMailbox);
}

/*
    * @brief Receive data from CAN (从环形缓冲区读取)
    * @param data: Pointer to a CAN_Res_t structure to store the received data 数组
    * @param msg_count: Pointer to a variable to store the number of received messages
*/
void INT_CAN_Receive(CAN_Res_t* data, uint8_t *msg_count)
{
    *msg_count = 0;
    while (*msg_count < 3 && RingBuf_Get(&can_ring_buf, &data[*msg_count]))
    {
        (*msg_count)++;
    }
}
