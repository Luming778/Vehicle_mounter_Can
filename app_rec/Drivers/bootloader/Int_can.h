#ifndef __INT_CAN__
#define __INT_CAN__

#include "can.h"
#include "string.h"
typedef struct
{
    CAN_RxHeaderTypeDef txHeader;
    uint8_t data[8];
} CAN_Rec_MSG;

/**
 * @brief ���ð�����������  �ֶ�����can
 *
 */
void Int_CAN_init(void);

/**
 * @brief 设置过滤器并初始化  启用CAN RX FIFO1中断  手动开启can
 */
void Int_CAN_init_IT(void);

/**
 * @brief ������Ϣ
 * id : ��ϢID
 * data : ��Ϣ����
 * len : ��Ϣ����
 *
 */
void Int_CAN_send(uint16_t id, uint8_t *data, uint8_t len);

/**
 * @brief ������Ϣ
 * 
 * @param rec_msg ����  ���һ�ο��Ի�ȡ3����Ϣ
 * @param msg_count 
 */
void Int_CAN_receive_msg(CAN_Rec_MSG *rec_msg,uint8_t *msg_count);

#endif // __INT_CAN__
