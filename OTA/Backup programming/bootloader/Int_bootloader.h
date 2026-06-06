#ifndef __INT_BOOTLOADER_H
#define __INT_BOOTLOADER_H

#include "usart.h"
#include "stdlib.h"
#include "string.h"


#define buff_len 1024
//app起始地址
#define APP_START_ADDR 0x0800c000
//app末尾地址
#define APP_END_ADDR 0x08080000
//栈顶指针地址
#define STACK_ADDR 0X20000000

void int_bootloader_init(void);
uint8_t Int_bootloader_jump_to_app(void);

void Int_bootloader_erase_flash(uint32_t page_addr, uint16_t pages);
#endif
