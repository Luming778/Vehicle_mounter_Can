#include "Int_can.h"

/**
 * @brief 配置CAN过滤器（公共函数）
 * @param fifo CAN_RX_FIFO0 或 CAN_RX_FIFO1
 */
static void Int_CAN_config_filter(uint32_t fifo)
{
    CAN_FilterTypeDef filterConfig = {0};
    filterConfig.FilterBank = 0;
    filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    filterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    filterConfig.FilterIdHigh = 0x0020;
    filterConfig.FilterIdLow = 0x0000;
    filterConfig.FilterMaskIdHigh = 0xffe0;
    filterConfig.FilterMaskIdLow = 0x0000;
    filterConfig.FilterFIFOAssignment = fifo;
    filterConfig.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan, &filterConfig);
}

/**
 * @brief 设置过滤器并初始化  手动开启can
 */
void Int_CAN_init(void)
{
    Int_CAN_config_filter(CAN_RX_FIFO0);
    HAL_CAN_Start(&hcan);
}

/**
 * @brief 设置过滤器并初始化  启用CAN RX FIFO1中断  手动开启can
 */
void Int_CAN_init_IT(void)
{
    Int_CAN_config_filter(CAN_RX_FIFO1);

    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO1_MSG_PENDING);
    HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);

    HAL_CAN_Start(&hcan);
}

/**
 * @brief 发送消息
 * id : 消息ID
 * data : 消息数据
 * len : 消息长度
 */
void Int_CAN_send(uint16_t id, uint8_t *data, uint8_t len)
{
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0)
        ;

    CAN_TxHeaderTypeDef txHeader = {0};
    txHeader.StdId = id;
    txHeader.IDE = CAN_ID_STD;
    txHeader.RTR = CAN_RTR_DATA;
    txHeader.DLC = len;
    uint32_t mailbox = 0;
    HAL_CAN_AddTxMessage(&hcan, &txHeader, data, &mailbox);
}

/**
 * @brief 接收消息
 *
 * @param rec_msg 缓冲区  最多一次可获取3个消息
 * @param msg_count
 */
void Int_CAN_receive_msg(CAN_Rec_MSG *rec_msg, uint8_t *msg_count)
{
    *msg_count = HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0);
    for (uint8_t i = 0; i < *msg_count; i++)
    {
        CAN_Rec_MSG *rec_msg_temp = &rec_msg[i];
        memset(rec_msg_temp, 0, sizeof(CAN_Rec_MSG));
        HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &(rec_msg_temp->txHeader), rec_msg_temp->data);
    }
}
