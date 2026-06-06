# FSMC 与硬件 I2C 总线仲裁冲突问题

**项目：** Vehicle_screen_controller
**日期：** 2026-06-06
**芯片：** STM32F103ZET6 (LQFP144, 512KB Flash, 64KB SRAM)

---

## 问题现象

I2C1 在 bootloader 中可以正常工作，但在主应用程序中无法通信，总线上无设备应答。

**具体表现：**
- `HAL_I2C_IsDeviceReady(&hi2c1, 0xA0, 5, 10)` 返回 `HAL_ERROR` 或 `HAL_TIMEOUT`
- W24C02 EEPROM 无法读写
- OTA 升级标志无法写入

**关键线索：**
- 注释掉 `MX_FSMC_Init()` 后，I2C1 恢复正常
- bootloader 中没有 FSMC 初始化，I2C 正常工作

---

## 根因分析

### 外设总线分配

| 外设 | 总线 | 时钟频率 |
|------|------|----------|
| **FSMC** | AHB | 72 MHz |
| **I2C1** | APB1 | 36 MHz |

```
                    ┌─────────────────────────────────────┐
                    │              AHB 总线 (72MHz)        │
                    │                                     │
                    │   ┌─────────┐      ┌─────────┐     │
                    │   │  FSMC   │      │  Flash  │     │
                    │   │ (LCD)   │      │         │     │
                    │   └────┬────┘      └────┬────┘     │
                    │        │                │          │
                    │   ┌────┴────────────────┴────┐     │
                    │   │     AHB 总线仲裁器        │     │
                    │   └────────────┬─────────────┘     │
                    │                │                    │
                    │   ┌────────────┴─────────────┐     │
                    │   │    APB1-to-AHB 桥        │     │
                    │   └────────────┬─────────────┘     │
                    └────────────────┼───────────────────┘
                                     │
                    ┌────────────────┼───────────────────┐
                    │           APB1 总线 (36MHz)         │
                    │                │                    │
                    │   ┌────────────┴─────────────┐     │
                    │   │         I2C1             │     │
                    │   │   (W24C02 EEPROM)        │     │
                    │   └──────────────────────────┘     │
                    └────────────────────────────────────┘
```

### 冲突机制

**1. AHB 总线仲裁竞争**

FSMC 挂在 AHB 总线上。当 FSMC Bank4 使能后，FSMC 控制器参与 AHB 总线仲裁。即使没有显式的 FSMC 事务，FSMC 外设也可能占用总线资源或产生周期性的总线活动（LCD 控制器状态机运行）。

**2. APB1-to-AHB 桥延迟**

I2C1 在 APB1 总线上，所有 APB1 事务必须经过 APB1-to-AHB 桥到达系统总线。当 FSMC 控制器积极争夺 AHB 访问时，桥的延迟增加，导致 I2C 时序违规或超时。

**3. GPIO 引脚状态残留**

虽然 FSMC 使用 PD/PE/PG 端口，I2C1 使用 PB 端口，**没有直接的 GPIO 引脚冲突**。但 FSMC 使能后，相关 GPIO 引脚被配置为复用推挽输出，可能影响总线电气特性。

---

## 时钟配置详情

```c
// main.c - SystemClock_Config()
RCC_OscInitStruct.HSEState = RCC_HSE_ON;                    // 外部晶振 8MHz
RCC_OscInitStruct.PLLMul = RCC_PLL_MUL9;                    // PLL x9 = 72MHz
RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;   // SYSCLK = 72MHz
RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;          // HCLK = 72MHz
RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;           // PCLK1 = 36MHz
RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;           // PCLK2 = 72MHz
```

---

## 问题背景

I2C 仅在 OTA 任务中使用（访问 W24C02 EEPROM 写入升级标志），而 OTA 任务是在 FreeRTOS 启动后才运行的。此时 LVGL 任务已经初始化了 FSMC 并在持续刷新 LCD。

**时序关系：**
```
系统启动
    │
    ▼
MX_I2C1_Init()          // I2C 初始化（此时无 FSMC，正常）
    │
    ▼
FreeRTOS 启动
    │
    ├── lvgl_task 启动 → atk_md0280_fsmc_init()  // FSMC 使能
    │                    → 持续刷新 LCD           // FSMC 持续活跃
    │
    ├── ota_task 启动 → 等待触发信号
    │                   → 需要访问 W24C02 (I2C)  // 此时 FSMC 已使能！
    │                   → I2C 通信失败！
```

---

## 解决方案

### 方案一：动态禁用 FSMC（当前采用）

OTA 任务运行时，先挂起 LVGL 任务，再禁用 FSMC，然后进行 I2C 操作。

```c
// atk_md0280.c 中实现
void fsmc_deinit_for_i2c(void)
{
    /* 1. 禁用 FSMC Bank4 (LCD 使用 NE4 = Bank4) */
    FSMC_NORSRAM_DEVICE->BTCR[3] &= ~FSMC_BCRx_MBKEN;

    /* 2. 禁用 FSMC 外设时钟 */
    __HAL_RCC_FSMC_CLK_DISABLE();
}
```

**OTA 任务流程：**

```c
void ota_task(void *param)
{
    // 等待触发信号
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // 挂起所有其他任务（包括 LVGL）
    if (lvgl_handle)    vTaskSuspend(lvgl_handle);
    if (oled_handler)   vTaskSuspend(oled_handler);
    if (AT_pars_handle) vTaskSuspend(AT_pars_handle);
    if (mqtt_handler)   vTaskSuspend(mqtt_handler);
    if (voice_handler)  vTaskSuspend(voice_handler);
    if (can_handler)    vTaskSuspend(can_handler);

    // 释放 FSMC 总线资源，确保 I2C 不会冲突
    fsmc_deinit_for_i2c();

    // 禁用 USART2/USART3 中断
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_NVIC_DisableIRQ(USART3_IRQn);

    vTaskDelay(pdMS_TO_TICKS(100));  // 等待总线释放

    // 此时 I2C 可以正常工作
    App_update_init();
    while (1)
    {
        App_update_work();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

**优点：** 实现简单，I2C 恢复正常速度
**缺点：** OTA 期间 LCD 无显示（但 OTA 完成后会复位，可接受）

---

### 方案二：改用软件 I2C

将硬件 I2C1 替换为软件模拟 I2C，使用任意 GPIO 引脚（如 PB8/PB9），完全避开总线仲裁问题。

```c
// 软件 I2C 引脚定义
#define SOFT_I2C_SCL_PORT   GPIOB
#define SOFT_I2C_SCL_PIN    GPIO_PIN_8
#define SOFT_I2C_SDA_PORT   GPIOB
#define SOFT_I2C_SDA_PIN    GPIO_PIN_9

// 软件 I2C 时序延时 (根据主频调整)
#define I2C_DELAY()         delay_us(5)  // 5us → 100kHz

void soft_i2c_start(void)
{
    SDA_HIGH();
    SCL_HIGH();
    I2C_DELAY();
    SDA_LOW();      // SDA 下降沿 = START
    I2C_DELAY();
    SCL_LOW();
    I2C_DELAY();
}

void soft_i2c_stop(void)
{
    SDA_LOW();
    SCL_HIGH();
    I2C_DELAY();
    SDA_HIGH();     // SDA 上升沿 = STOP
    I2C_DELAY();
}

uint8_t soft_i2c_write_byte(uint8_t data)
{
    for (int i = 7; i >= 0; i--)
    {
        if (data & (1 << i))
            SDA_HIGH();
        else
            SDA_LOW();
        I2C_DELAY();
        SCL_HIGH();
        I2C_DELAY();
        SCL_LOW();
    }

    // 读取 ACK
    SDA_HIGH();  // 释放 SDA
    I2C_DELAY();
    SCL_HIGH();
    I2C_DELAY();
    uint8_t ack = SDA_READ();
    SCL_LOW();

    return ack;  // 0 = ACK, 1 = NACK
}

uint8_t soft_i2c_read_byte(uint8_t ack)
{
    uint8_t data = 0;
    SDA_HIGH();  // 释放 SDA

    for (int i = 7; i >= 0; i--)
    {
        SCL_HIGH();
        I2C_DELAY();
        if (SDA_READ())
            data |= (1 << i);
        SCL_LOW();
        I2C_DELAY();
    }

    // 发送 ACK/NACK
    if (ack)
        SDA_LOW();   // ACK
    else
        SDA_HIGH();  // NACK
    I2C_DELAY();
    SCL_HIGH();
    I2C_DELAY();
    SCL_LOW();

    return data;
}
```

**优点：**
- 完全不依赖 AHB/APB 总线，无仲裁冲突
- LCD 刷新不影响 I2C 通信
- 引脚可任意选择，灵活性高

**缺点：**
- CPU 占用率高（软件模拟时序）
- 速度比硬件 I2C 慢（通常 100kHz 或更低）
- 需要精确的延时函数支持

---

### 方案三：降低 FSMC 总线使用时间占比

通过优化 LCD 刷新策略，减少 FSMC 总线占用时间，给 I2C 留出总线访问窗口。

**方法 1：降低 LCD 刷新频率**

```c
void lvgl_task(void *param)
{
    while (1)
    {
        lv_timer_handler();

        // 增加延时，降低刷新率
        vTaskDelay(pdMS_TO_TICKS(20));  // 50fps → 改为 10fps
    }
}
```

**方法 2：仅在数据变化时刷新**

```c
void lvgl_task(void *param)
{
    while (1)
    {
        // 检查是否有数据更新
        if (display_dirty_flag)
        {
            lv_timer_handler();
            display_dirty_flag = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**方法 3：I2C 操作插入 LCD 帧间隙**

```c
void lvgl_task(void *param)
{
    while (1)
    {
        // LCD 帧刷新
        lv_timer_handler();

        // 帧间隙：临时禁用 FSMC，允许 I2C 操作
        fsmc_bus_release();
        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms 窗口期

        // 如果有 I2C 请求，在此处理
        if (i2c_pending_request)
        {
            process_i2c_request();
        }

        // 重新使能 FSMC
        fsmc_bus_acquire();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**优点：**
- LCD 和 I2C 可以同时工作
- 不需要完全禁用 FSMC

**缺点：**
- I2C 响应延迟增加（取决于帧间隙大小）
- 实现复杂度较高
- 需要修改 LCD 驱动和 I2C 驱动的协作方式

---

## 方案对比

| 方案 | 实现难度 | I2C 速度 | LCD 影响 | 可靠性 | 适用场景 |
|------|---------|----------|----------|--------|---------|
| 动态禁用 FSMC | ★★☆ | 正常 | 禁用期间无显示 | 高 | OTA 等短时间 I2C 操作 |
| 软件 I2C | ★★☆ | 较慢 | 无影响 | 高 | I2C 使用频率低 |
| 降低 FSMC 占比 | ★★★ | 有延迟 | 刷新率降低 | 中 | I2C 和 LCD 同时使用 |

---

## 当前采用的方案

本项目采用 **方案一（动态禁用 FSMC）**：

1. `main()` 中注释掉 `MX_FSMC_Init()`，避免启动时冲突
2. LCD 的 FSMC 由 `atk_md0280_fsmc_init()` 在 LVGL 任务中初始化
3. OTA 任务需要使用 I2C 时：
   - 挂起 LVGL 任务
   - 调用 `fsmc_deinit_for_i2c()` 禁用 FSMC
   - 正常进行 I2C 操作
   - OTA 完成后系统复位

```c
// main.c 初始化顺序
//MX_FSMC_Init();     // 注释掉
MX_I2C1_Init();       // I2C 正常初始化

// freertos.c - lvgl_task 中
atk_md0280_fsmc_init();  // 延迟初始化 FSMC

// freertos.c - ota_task 中
fsmc_deinit_for_i2c();   // OTA 时禁用 FSMC
```

---

## 诊断代码

当 I2C 通信异常时，可使用以下代码打印寄存器状态：

```c
void i2c1_diagnostic(void)
{
    printf("=== I2C1 Diagnostic ===\r\n");
    printf("I2C1->CR2   = 0x%04X\r\n", I2C1->CR2);
    printf("I2C1->CCR   = 0x%04X\r\n", I2C1->CCR);
    printf("I2C1->TRISE = 0x%04X\r\n", I2C1->TRISE);
    printf("I2C1->SR1   = 0x%04X\r\n", I2C1->SR1);
    printf("I2C1->SR2   = 0x%04X\r\n", I2C1->SR2);
    printf("GPIOB->CRL  = 0x%08X\r\n", GPIOB->CRL);
    printf("RCC->APB1ENR = 0x%08X\r\n", RCC->APB1ENR);
    printf("RCC->AHBENR  = 0x%08X\r\n", RCC->AHBENR);

    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1, 0xA0, 5, 10);
    printf("W24C02 probe: %s\r\n",
           status == HAL_OK ? "OK" : "FAIL");
    printf("=======================\r\n");
}
```

---

## 引脚分配表

| 外设 | 引脚 | 功能 |
|------|------|------|
| **I2C1** | PB6 | SCL |
| | PB7 | SDA |
| **FSMC** | PD0, PD1, PD14, PD15 | D2, D3, D0, D1 |
| | PD4, PD5 | NOE(RD), NWE(WR) |
| | PD8-PD10 | D13-D15 |
| | PE7-PE15 | D4-D12 |
| | PG0 | A10(RS) |
| | PG12 | NE4(CS) |

**结论：** FSMC 和 I2C1 没有 GPIO 引脚复用冲突，冲突发生在 AHB/APB 总线仲裁层面。

---

## 注意事项

1. **不要在 FSMC 使能期间进行 I2C 操作** — 会导致 I2C 超时或无应答
2. **OTA 任务必须先挂起 LVGL 再禁用 FSMC** — 避免 LVGL 任务访问已禁用的 FSMC
3. **系统复位后 FSMC 状态清除** — OTA 完成后调用 `HAL_NVIC_SystemReset()` 恢复正常状态
4. **软件 I2C 需要精确延时** — 使用 SysTick 或 TIM 实现微秒级延时
5. **降低 FSMC 占比方案需要协调 LCD 和 I2C 驱动** — 实现复杂度较高

---

## 相关文件

| 文件 | 说明 |
|------|------|
| `Core/Src/main.c` | `MX_FSMC_Init()` 已注释 (line 107) |
| `Core/Src/i2c.c` | I2C1 初始化配置 |
| `Drivers/hardware/LCD_touch_ATK_MD0280/atk_md0280.c` | `fsmc_deinit_for_i2c()` 实现 |
| `Drivers/hardware/LCD_touch_ATK_MD0280/atk_md0280_fsmc.c` | LCD FSMC 实际初始化 |
| `Drivers/bootloader/Int_w24c02.c` | W24C02 EEPROM I2C 驱动 |
| `Core/Src/freertos.c` | OTA 任务中调用 FSMC 禁用 |

---

## 总结

| 项目 | 说明 |
|------|------|
| **问题** | FSMC 使能后 I2C1 无法通信 |
| **根因** | AHB/APB 总线仲裁冲突，非 GPIO 引脚冲突 |
| **表现** | I2C 设备探测超时，EEPROM 无法读写 |
| **解决** | 动态禁用 FSMC / 软件 I2C / 降低 FSMC 占比 |
| **影响范围** | OTA 任务中使用 I2C 访问 W24C02 EEPROM |
