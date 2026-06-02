#include "can_bus.h"
#include "can_adapter.h"

void can_bus_init(void)
{
    can_adapter_init();
}

void can_bus_send(uint16_t id, uint8_t *data, uint8_t len)
{
    can_adapter_send(id, data, len);
}
