#include "Int_bootloader.h"

//串口接收数据存放数组
uint8_t receive_buff[buff_len]={0};
//单次串口接收数据长度
uint16_t rec_buf_len=0;
//串口接受数据总和
uint32_t sum=0;
//地址偏移量
uint32_t flash_write_offset =0;
// 末尾可能出现的单独字节 
uint8_t last_byte_flag = 0;
uint8_t last_byte = 0;

//接收到数据的时间
uint32_t last_rec_time = 0;

//擦除函数
static void Int_flash_erase(void)
{    
    uint8_t is_erase = 0;//擦除标志位
    uint32_t page_addr = 0;//页地址
    for (uint16_t i = 0; i < rec_buf_len; i++)
    {
        // 读取每一个位置的值
        uint8_t data = *(volatile uint8_t *)(APP_START_ADDR + i + flash_write_offset);
        if (data != 0xff)
        {
            // printf("erase:%d,%d,%c", i, flash_write_offset, data);
            is_erase = 1;//该页需要擦除
            // 记录当前页的起始地址
            page_addr = (APP_START_ADDR + i + flash_write_offset) - (APP_START_ADDR + i + flash_write_offset) % FLASH_PAGE_SIZE;
            break;
        }
    }
    // 2.2 如果需要擦除  则擦除当前页
    if (is_erase)
    {
        FLASH_EraseInitTypeDef erase_init;
        // 擦除单独页
        erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
        // 擦除第1个bank的页
        erase_init.Banks = FLASH_BANK_1;
        // 擦除页的起始地址
        erase_init.PageAddress = page_addr;
        // 擦除几页
        erase_init.NbPages = 1;
			
        uint32_t page_error = 0;
        // flash擦除比较耗费性能
        HAL_FLASHEx_Erase(&erase_init, &page_error);
    }
}

//这次发送有剩余字节
static void Int_flash_write_with_last(void)
{
    for (uint16_t i = 0; i < rec_buf_len; i += 2)
    {
        uint32_t flash_addr = APP_START_ADDR + i + flash_write_offset;
        uint16_t data16;
        if (i == 0)
        {
            // 拼接上一次的字节
            data16 = last_byte | (receive_buff[i] << 8);
        }
        else
        {
            data16 = receive_buff[i - 1] | (receive_buff[i] << 8);
        }
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, flash_addr, data16);//一次发送半个字 （两个字节）
    }
}
//这次发送无剩余字节
static void Int_flash_write_no_last(void)
{
    // 正好能够写入 => 不再有遗留的字节  0   6
    for (uint16_t i = 0; i < rec_buf_len; i += 2)
    {
        uint32_t flash_addr = APP_START_ADDR + i + flash_write_offset;
        uint16_t data16;

        if (i + 1 < rec_buf_len)
        {
            data16 = receive_buff[i] | (receive_buff[i + 1] << 8);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, flash_addr, data16);//一次发送半个字 （两个字节）
        }
    }
}

static void Int_flash_write_halfword(void)
{
    // 本次发送没剩余
    if ((rec_buf_len + last_byte_flag) % 2 == 0)
    {
        if (last_byte_flag)
        {
            // 上次有剩余 数据长度是奇数 5 => 这次需要作为第一个字节写入  1  5
            Int_flash_write_with_last();
            // 2.4 记录偏移量
            flash_write_offset += rec_buf_len + 1;
        }
        else
        {
            // 上次无剩余  数据长度是偶数 => 不再有遗留的字节  0   6
            Int_flash_write_no_last();
            // 2.4 记录偏移量
            flash_write_offset += rec_buf_len;
        }
        last_byte_flag = 0;
    }
    // 本次发送有剩余
    else
    {
        if (last_byte_flag)
        {
            // 上次有剩余 数据长度是偶数
            Int_flash_write_with_last();
            // 修改最后剩下的字节
            last_byte = receive_buff[rec_buf_len - 1];
            // 2.4 记录偏移量
            flash_write_offset += rec_buf_len;
        }
        else
        {
            // 上次没有遗留字节  这次会留下一个
            Int_flash_write_no_last();

            last_byte = receive_buff[rec_buf_len - 1];
            // 2.4 记录偏移量
            flash_write_offset += rec_buf_len - 1;
        }
        last_byte_flag = 1;
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if(huart->Instance==USART1)
	{
		last_rec_time = HAL_GetTick();//获取时间
		
		//记录收到的数据长度
		rec_buf_len=Size;
		sum+=rec_buf_len;
		//printf("len:%d",sum);
		//flash解锁
		HAL_FLASH_Unlock();
		//擦除flash
		Int_flash_erase();
		//写入数据，使用16位写入
    Int_flash_write_halfword();
		
		//上锁
		HAL_FLASH_Lock();
		//清除这次数据接收长度
		memset(receive_buff,0,buff_len);
		
		__HAL_UART_CLEAR_OREFLAG(&huart1);
	__HAL_UART_CLEAR_IDLEFLAG(&huart1);
		HAL_UARTEx_ReceiveToIdle_IT(&huart1,receive_buff,buff_len);
	}
}

//串口初始化 串口接收 => 准备接收A程序
void int_bootloader_init()
{
	// // 清空掉初始化串口使用之前的所有问题
	__HAL_UART_CLEAR_OREFLAG(&huart1);
	__HAL_UART_CLEAR_IDLEFLAG(&huart1);
	//启动串口空闲中断接收函数
	HAL_UARTEx_ReceiveToIdle_IT(&huart1,receive_buff,buff_len);
}

//程序跳转函数
uint8_t Int_bootloader_jump_to_app(void)
{

    typedef void (*pFunc)(void);
    // 1. 校验
    // 取出栈顶地址的值和复位中断地址的值
    uint32_t app_stack_ptr = *(volatile uint32_t *)(APP_START_ADDR);
    uint32_t app_reset_handle = *(volatile uint32_t *)(APP_START_ADDR + 4);

    // 1.1 校验栈顶地址
    if ((app_stack_ptr & 0xFFFF0000) != STACK_ADDR)
    {
        printf("stack addr error\n");
        return 1;
    }

    // 1.2 校验复位中断地址
    if (app_reset_handle < APP_START_ADDR || app_reset_handle > APP_END_ADDR)
    {
        printf("reset handle error\n");
        return 1;
    }

    // 2. 注销boot loader程序
    // 2.1 关闭中断
    __disable_irq();

    // 注销hal库设置 在bootloader程序里  使用hal的话 不要使用HAL_DeInit();
    // HAL_DeInit();

    // 2.2 设置堆栈指针
    __set_MSP(app_stack_ptr);

    // 2.3 重定向中断向量表
    SCB->VTOR = APP_START_ADDR;

    // 2.4 跳转到A程序复位中断
    pFunc jump_to_app = (pFunc)app_reset_handle;
    // 跳转代码之后的内容是执行不到的
    jump_to_app();

    return 0;
	
}

//flash擦除函数
void Int_bootloader_erase_flash(uint32_t page_addr, uint16_t pages)
{
    // 解锁flash
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase_init;
    // 擦除单独页
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    // 擦除第1个bank的页
    erase_init.Banks = FLASH_BANK_1;
    // 擦除页的起始地址
    erase_init.PageAddress = page_addr;
    // 擦除几页
    erase_init.NbPages = pages;
    uint32_t page_error = 0;
    // flash擦除比较耗费性能
    HAL_FLASHEx_Erase(&erase_init, &page_error);
    // 加锁flash
    HAL_FLASH_Lock();
}
