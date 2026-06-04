#include "storage_adapter.h"
#include "Int_w25q32.h"
#include "Int_w24c02.h"
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t xStorageMutex = NULL;

void storage_adapter_init(void)
{
    if (xStorageMutex == NULL)
    {
        xStorageMutex = xSemaphoreCreateMutex();
    }
}

static void storage_adapter_lock(void)
{
    if (xStorageMutex != NULL)
    {
        xSemaphoreTake(xStorageMutex, portMAX_DELAY);
    }
}

static void storage_adapter_unlock(void)
{
    if (xStorageMutex != NULL)
    {
        xSemaphoreGive(xStorageMutex);
    }
}

void storage_adapter_write_flash(uint32_t addr, uint8_t *data, uint16_t len)
{
    storage_adapter_lock();
    Int_w25q32_write_data_with_32addr(addr, data, len);
    storage_adapter_unlock();
}

void storage_adapter_read_flash(uint32_t addr, uint8_t *data, uint16_t len)
{
    storage_adapter_lock();
    Int_w25q32_read_data_with_32addr(addr, data, len);
    storage_adapter_unlock();
}

void storage_adapter_erase_sector(uint8_t block, uint8_t sector)
{
    storage_adapter_lock();
    Int_w25q32_erase_sector(block, sector);
    storage_adapter_unlock();
}

void storage_adapter_write_eeprom(uint8_t byte_addr, uint8_t *data, uint16_t len)
{
    storage_adapter_lock();
    Int_w24c02_write_bytes(byte_addr, data, len);
    storage_adapter_unlock();
}
