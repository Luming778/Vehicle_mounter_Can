#include "can_adapter.h"
#include "Int_can.h"

void can_adapter_init(void)
{
    Int_CAN_init_IT();  // 使用 FIFO1 中断模式
}

void can_adapter_send(uint16_t id, uint8_t *data, uint8_t len)
{
    Int_CAN_send(id, data, len);
}
