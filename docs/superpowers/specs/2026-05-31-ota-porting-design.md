# OTA 模块移植到 Vehicle_screen_controller 设计文档

**日期**: 2026-05-31
**状态**: 待实现

---

## 1. 目标

将 `app_rec` 中的 OTA 接收模块移植到 `Vehicle_screen_controller`，使其能通过 CAN 总线接收固件更新，存入外部 W25Q32 Flash，设置 EEPROM 升级标志，然后系统复位。

## 2. 内存布局

### 2.1 Flash 分区

| 区域 | 地址范围 | 大小 | 用途 |
|------|---------|------|------|
| Bootloader（预留） | `0x08000000 - 0x0800BFFF` | 48KB | 未来 bootloader |
| Application | `0x0800C000 - 0x0807FFFF` | 464KB | screen controller 应用 |

### 2.2 外部存储

| 设备 | 接口 | 用途 |
|------|------|------|
| W25Q32 | SPI2 | 固件暂存（sector 0: 元数据, sector 1-30: 固件数据） |
| W24C02 | I2C1 | 升级标志（地址 0x10: flag + check key） |

### 2.3 Linker Script 修正

**问题**: 当前 `CAR.sct` load region size 为 `0x00080000`，从 `0x0800C000` 开始会超出 flash 末尾。
**修正**: 改为 `0x00074000`（464KB）。

```
LR_IROM1 0x0800C000 0x00074000  {
```

---

## 3. 架构设计

### 3.1 OTA 模块分层（不修改核心模块）

```
┌─────────────────────────────────────────────┐
│  app_update.c/h          （OTA 状态机）       │  ← 修改：删除 UART 回调，简化 init
├─────────────────────────────────────────────┤
│  can_bus.c/h  │  storage.c/h                │  ← 不修改
├─────────────────────────────────────────────┤
│  can_adapter.c │  storage_adapter.c          │  ← 不修改（已适配）
├─────────────────────────────────────────────┤
│  Int_can.c    │  Int_w25q32.c │ Int_w24c02.c │  ← 修改：Int_can.c FilterBank 改为 1
└─────────────────────────────────────────────┘
```

### 3.2 CAN FIFO 隔离

| FilterBank | FIFO | 用途 | ID 过滤 |
|------------|------|------|---------|
| 0 | FIFO0 | 应用逻辑（原有） | 接收所有 ID（mask=0x0000） |
| 1 | FIFO1 | OTA 更新 | 仅接收 ID=0（0x0020/0xFFE0） |

- `HAL_CAN_RxFifo0MsgPendingCallback` → 现有 `can.c`，分发到 `can_rx_queue` / `mqtt_can_rx_queue`
- `HAL_CAN_RxFifo1MsgPendingCallback` → `app_update.c`，OTA 数据接收

### 3.3 UART 回调合并

**冲突**: `app_update.c` 和 `usart.c` 各自定义了 `HAL_UARTEx_RxEventCallback`。

**方案**: 删除 `app_update.c` 中的回调，合并到 `usart.c`：

```
HAL_UARTEx_RxEventCallback
├── USART1 → 检测 "update" → vTaskNotifyGiveFromISR(ota_task_handle)
├── USART2 → 原有 voice 模块逻辑
└── USART3 → 原有 ESP8266 逻辑
```

### 3.4 FreeRTOS 任务优先级

| 任务 | 优先级 | Handle | 状态 |
|------|--------|--------|------|
| oled_task | 15 | `oled_handler` | 正常运行，OTA 时挂起 |
| AT_parse | 15 | `AT_pars_handle` | 正常运行，OTA 时挂起 |
| MQTT_Task | 15 | `mqtt_handler`（新增） | 正常运行，OTA 时挂起 |
| voice_task | 14 | `voice_handler` | 正常运行，OTA 时挂起 |
| can_task | 13 | `can_handler` | 正常运行，OTA 时挂起 |
| **ota_task** | **20** | `ota_task_handle`（新增） | 创建后自我挂起，收到通知后唤醒 |

### 3.5 OTA 触发与执行流程

```
USART1 收到 "update"
    ↓
HAL_UARTEx_RxEventCallback (usart.c)
    ↓ strstr → vTaskNotifyGiveFromISR(ota_task_handle)
    ↓
ota_task 被唤醒（优先级 20，抢占所有其他任务）
    ↓
vTaskSuspend(oled_handler)
vTaskSuspend(AT_pars_handle)
vTaskSuspend(mqtt_handler)
vTaskSuspend(voice_handler)
vTaskSuspend(can_handler)
    ↓
vTaskDelay(100ms) — 等待被挂起的任务完成当前操作
    ↓
App_update_init() → can_bus_init() → CAN FIFO1 filter + 中断使能
update_state = UPDATE_RECV_SEND_CMD
    ↓
can_bus_send(ID=0, "sgg") → 发送端开始传输固件
    ↓
CAN FIFO1 中断接收 → 双缓冲 → 主循环写入 W25Q32
    ↓
超时 1s → 写入剩余数据 → 等待 CRC
    ↓
CRC 校验通过 → 写元数据到 W25Q32 sector 0 → 写 boot flag 到 W24C02
    ↓
HAL_NVIC_SystemReset()
```

---

## 4. 文件改动清单

### 4.1 `Vehicle_screen_controller/Core/Src/usart.c`

**改动内容**:
1. 新增 Includes: `FreeRTOS.h`, `task.h`, `app_update.h`
2. 新增变量: `extern TaskHandle_t ota_task_handle;` 和 `uint8_t usart1_rx_buf[32];`
3. 在 `MX_USART1_UART_Init()` 末尾 USER CODE 区域添加: `HAL_UARTEx_ReceiveToIdle_IT(&huart1, usart1_rx_buf, sizeof(usart1_rx_buf));`
4. 修改 `HAL_UARTEx_RxEventCallback`，增加 USART1 分支

**新增/修改的代码**:
```c
// USER CODE BEGIN Includes
#include "FreeRTOS.h"
#include "task.h"
#include "app_update.h"
// USER CODE END Includes

// USER CODE BEGIN PV
extern TaskHandle_t ota_task_handle;
static uint8_t usart1_rx_buf[32];
// USER CODE END PV

// 在 MX_USART1_UART_Init() 末尾:
/* USER CODE BEGIN USART1_Init 2 */
HAL_UARTEx_ReceiveToIdle_IT(&huart1, usart1_rx_buf, sizeof(usart1_rx_buf));
/* USER CODE END USART1_Init 2 */

// 修改 HAL_UARTEx_RxEventCallback:
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        if (update_state == UPDATE_IDLE && strstr((char *)usart1_rx_buf, "update"))
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            vTaskNotifyGiveFromISR(ota_task_handle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, usart1_rx_buf, sizeof(usart1_rx_buf));
    }
    else if (huart->Instance == USART2)
    {
        // 原有 voice 模块逻辑（保持不变）
    }
    else if (huart->Instance == USART3)
    {
        // 原有 ESP8266 逻辑（保持不变）
    }
}
```

### 4.2 `Vehicle_screen_controller/Core/Src/freertos.c`

**改动内容**:
1. 新增 Includes: `app_update.h`
2. 新增变量: `TaskHandle_t ota_task_handle;` 和 `TaskHandle_t mqtt_handler;`
3. 新增函数声明: `void ota_task(void *param);`
4. 修改 `StartDefaultTask`: MQTT_Task 传入 `&mqtt_handler`，新增 `ota_task` 创建
5. 新增 `ota_task` 函数实现

**新增/修改的代码**:
```c
// USER CODE BEGIN Includes
#include "app_update.h"
// USER CODE END Includes

// USER CODE BEGIN Variables
TaskHandle_t ota_task_handle;
TaskHandle_t mqtt_handler;
// USER CODE END Variables

// USER CODE BEGIN PM
void ota_task(void *param);
// USER CODE END PM

// 修改 StartDefaultTask:
void StartDefaultTask(void *argument)
{
    taskENTER_CRITICAL();
    xTaskCreate(oled_task,  "oled_task",  200, NULL, 15, &oled_handler);
    xTaskCreate(can_task,   "can_task",   128, NULL, 13, &can_handler);
    xTaskCreate(AT_Parse,   "AT_parse",    68, NULL, 15, &AT_pars_handle);
    xTaskCreate(MQTT_Task,  "MQTT_Task",  200, NULL, 15, &mqtt_handler);   // 改：加 handle
    xTaskCreate(voice_task, "voice_task", 128, NULL, 14, &voice_handler);
    xTaskCreate(ota_task,   "ota_task",   256, NULL, 20, &ota_task_handle); // 新增

    can_rx_queue      = xQueueCreate(5, sizeof(CanMsg_t));
    mqtt_can_rx_queue = xQueueCreate(5, sizeof(CanMsg_t));
    voice_rx_queue    = xQueueCreate(5, sizeof(uint8_t));

    taskEXIT_CRITICAL();
    vTaskDelete(NULL);
}

// 新增 ota_task:
void ota_task(void *param)
{
    // 创建后立即自我挂起，等待 USART1 "update" 通知
    vTaskSuspend(NULL);

    // 被唤醒后，挂起所有其他任务
    vTaskSuspend(oled_handler);
    vTaskSuspend(AT_pars_handle);
    vTaskSuspend(mqtt_handler);
    vTaskSuspend(voice_handler);
    vTaskSuspend(can_handler);

    // 等待 100ms，让被挂起的任务完成当前操作
    vTaskDelay(pdMS_TO_TICKS(100));

    // 初始化 OTA 模块
    App_update_init();

    // 运行 OTA 状态机（1ms 轮询，中断驱动接收不会丢包）
    while (1)
    {
        App_update_work();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

### 4.3 `Vehicle_screen_controller/Drivers/bootloader/app_update.c`

**改动内容**:
1. 删除 `HAL_UARTEx_RxEventCallback` 函数（已移到 usart.c）
2. 删除 `static uint8_t uart_rec_buf[32];`
3. 修改 `App_update_init()`: 删除 UART 初始化，直接设置 `update_state = UPDATE_RECV_SEND_CMD`

**修改后的 `App_update_init`**:
```c
void App_update_init(void)
{
    // UART 初始化已在 usart.c 中完成，此处不再配置
    can_bus_init();
    update_state = UPDATE_RECV_SEND_CMD;  // 直接进入发送命令状态
}
```

### 4.4 `Vehicle_screen_controller/Drivers/bootloader/Int_can.c`

**改动内容**:
1. `Int_CAN_config_filter()`: FilterBank 从 0 改为 1
2. `Int_CAN_init_IT()`: 删除 `HAL_CAN_Start(&hcan)`（main.c 已调用），NVIC 优先级改为 6（与 FIFO0 一致）

**修改后的代码**:
```c
static void Int_CAN_config_filter(uint32_t fifo)
{
    CAN_FilterTypeDef filterConfig = {0};
    filterConfig.FilterBank = 1;              // 改为 1，避免与应用 FilterBank 0 冲突
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

void Int_CAN_init_IT(void)
{
    Int_CAN_config_filter(CAN_RX_FIFO1);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO1_MSG_PENDING);
    HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 6, 0);  // 与 FIFO0 优先级一致
    HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
    // 不调用 HAL_CAN_Start — main.c 已调用
}
```

### 4.5 `Vehicle_screen_controller/MDK-ARM/CAR/CAR.sct`

**改动**: load region size 从 `0x00080000` 改为 `0x00074000`

```
LR_IROM1 0x0800C000 0x00074000  {
```

### 4.6 已完成的改动（无需再改）

| 文件 | 改动 | 状态 |
|------|------|------|
| `main.c` | VTOR 重定位到 0x0800C000 | ✅ 已完成 |
| `can.c` | CAN1_RX1_IRQn 使能，优先级 6 | ✅ 已完成 |
| `stm32f1xx_it.c` | CAN1_RX1_IRQHandler | ✅ 已完成 |
| Keil 工程 | 添加 bootloader 源文件 | ✅ 已完成 |

---

## 5. 不需要修改的文件

| 文件 | 原因 |
|------|------|
| `app_update.h` | 接口不变 |
| `can_bus.c/h` | 纯转发层，不变 |
| `can_adapter.c/h` | 已正确适配 |
| `storage.c/h` | 业务逻辑不变 |
| `storage_adapter.c/h` | 已正确适配 |
| `Int_w25q32.c/h` | SPI Flash 驱动不变 |
| `Int_w24c02.c/h` | I2C EEPROM 驱动不变 |

---

## 6. 风险与注意事项

1. **HAL_GetTick() 依赖 TIM4 中断**: OTA 状态机使用 `HAL_GetTick()` 做超时检测。TIM4 中断优先级为 15，不会被 FreeRTOS 屏蔽，可以正常工作。

2. **CRC 硬件单元共享**: OTA 的 CRC 校验和应用代码共用硬件 CRC。由于 OTA 时其他任务已挂起，不会冲突。

3. **W25Q32 CS 引脚 (PB12)**: OTA 和应用代码都通过 SPI2 访问 W25Q32。由于 OTA 时其他任务已挂起，不会冲突。

4. **I2C1 共享**: OTA 写 EEPROM 和 OLED 显示共用 I2C1。由于 OTA 时 oled_task 已挂起，不会冲突。

5. **OTA 期间 CAN FIFO0 消息丢失**: OTA 期间应用任务被挂起，FIFO0 消息不会被处理。OTA 完成后系统复位，这是可接受的行为。

---

## 7. 验证要点

1. 编译通过，无重复定义错误
2. 正常运行时所有功能不受影响（OLED、语音、MQTT、CAN 通信）
3. USART1 发送 "update" 后，其他任务挂起，OTA 任务开始运行
4. CAN FIFO1 正确接收 OTA 数据，双缓冲写入 W25Q32
5. CRC 校验通过后写入元数据和 boot flag
6. 系统复位后正常启动
