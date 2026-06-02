#include "storage_adapter.h"
#include "Int_w25q32.h"
#include "Int_w24c02.h"

void storage_adapter_write_flash(uint32_t addr, uint8_t *data, uint16_t len)
{
    Int_w25q32_write_data_with_32addr(addr, data, len);
}

void storage_adapter_read_flash(uint32_t addr, uint8_t *data, uint16_t len)
{
    Int_w25q32_read_data_with_32addr(addr, data, len);
}

void storage_adapter_erase_sector(uint8_t block, uint8_t sector)
{
    Int_w25q32_erase_sector(block, sector);
}

void storage_adapter_write_eeprom(uint8_t byte_addr, uint8_t *data, uint16_t len)
{
    Int_w24c02_write_bytes(byte_addr, data, len);
}
