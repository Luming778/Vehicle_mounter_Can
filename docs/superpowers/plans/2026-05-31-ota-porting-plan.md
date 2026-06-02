# OTA 模块移植到 Vehicle_screen_controller 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 app_rec 中的 OTA 接收模块移植到 Vehicle_screen_controller，通过 USART1 "update" 指令触发，CAN FIFO1 接收固件，写入 W25Q32，设置 EEPROM 升级标志后复位。

**Architecture:** OTA 模块采用分层架构（状态机 → 功能模块 → 适配层），通过 CAN FIFO0/FIFO1 隔离应用与 OTA 通信。OTA 任务优先级 20，触发后挂起所有其他任务独占运行。

**Tech Stack:** STM32F103ZET6, STM32 HAL, FreeRTOS CMSIS-RTOS V2, CAN 166.67kbps, SPI W25Q32, I2C W24C02, Hardware CRC32

**Spec:** `docs/superpowers/specs/2026-05-31-ota-porting-design.md`

---

## 文件结构

| 文件 | 操作 | 职责 |
|------|------|------|
| `Vehicle_screen_controller/MDK-ARM/CAR/CAR.sct` | 修改 | 修正 load region size |
| `Vehicle_screen_controller/Drivers/bootloader/Int_can.c` | 修改 | FilterBank 改为 1，删除重复 CAN_Start |
| `Vehicle_screen_controller/Drivers/bootloader/app_update.c` | 修改 | 删除 UART 回调，简化 init |
| `Vehicle_screen_controller/Core/Src/usart.c` | 修改 | 合并 UART 回调，增加 USART1 OTA 触发 |
| `Vehicle_screen_controller/Core/Src/freertos.c` | 修改 | 新增 ota_task，MQTT_Task 加 handle |

---

### Task 1: 修正 Linker Script

**Files:**
- Modify: `Vehicle_screen_controller/MDK-ARM/CAR/CAR.sct`

- [ ] **Step 1: 修改 load region size**

将 `CAR.sct` 第 5 行的 `0x00080000` 改为 `0x00074000`：

```
LR_IROM1 0x0800C000 0x00074000  {    ; load region size_region
  ER_IROM1 0x0800C000 0x00074000  {  ; load address = execution address
   *.o (RESET, +First)
   *(InRoot$$Sections)
   .ANY (+RO)
   .ANY (+XO)
  }
  RW_IRAM1 0x20000000 0x00010000  {  ; RW data
   .ANY (+RW +ZI)
  }
}
```

- [ ] **Step 2: 验证**

在 Keil 中打开工程，确认 Linker 设置中 ROM1 起始地址为 `0x0800C000`，大小为 `0x74000`。

- [ ] **Step 3: 提交**

```bash
git add Vehicle_screen_controller/MDK-ARM/CAR/CAR.sct
git commit -m "fix: correct linker load region size for bootloader offset"
```

---

### Task 2: 修改 Int_can.c FilterBank

**Files:**
- Modify: `Vehicle_screen_controller/Drivers/bootloader/Int_can.c`

- [ ] **Step 1: 修改 Int_CAN_config_filter**

将 `FilterBank` 从 0 改为 1，避免与应用层 FilterBank 0 冲突：

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
```

- [ ] **Step 2: 修改 Int_CAN_init_IT**

删除 `HAL_CAN_Start(&hcan)` 调用（main.c 已调用），NVIC 优先级改为 6（与 FIFO0 一致）：

```c
void Int_CAN_init_IT(void)
{
    Int_CAN_config_filter(CAN_RX_FIFO1);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO1_MSG_PENDING);
    HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 6, 0);  // 与 FIFO0 优先级一致
    HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
    // 不调用 HAL_CAN_Start — main.c 已调用
}
```

- [ ] **Step 3: 验证**

在 Keil 中编译，确认无错误。

- [ ] **Step 4: 提交**

```bash
git add Vehicle_screen_controller/Drivers/bootloader/Int_can.c
git commit -m "fix: use FilterBank 1 for OTA, remove duplicate CAN_Start"
```

---

### Task 3: 修改 app_update.c

**Files:**
- Modify: `Vehicle_screen_controller/Drivers/bootloader/app_update.c`

- [ ] **Step 1: 删除 HAL_UARTEx_RxEventCallback**

删除 `app_update.c` 中的整个 `HAL_UARTEx_RxEventCallback` 函数（约第 81-93 行）：

```c
// 删除这段代码
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if ((huart->Instance == USART1) && (update_state == UPDATE_IDLE))
    {
        if (strstr((char *)uart_rec_buf, "cmd"))
        {
            update_state = UPDATE_RECV_SEND_CMD;
        }
        __HAL_UART_CLEAR_OREFLAG(&huart1);
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buf, sizeof(uart_rec_buf));
    }
}
```

- [ ] **Step 2: 删除 uart_rec_buf 变量**

删除 `static uint8_t uart_rec_buf[32];`（约第 25 行）。

- [ ] **Step 3: 修改 App_update_init**

将 `App_update_init()` 改为：

```c
void App_update_init(void)
{
    // UART 初始化已在 usart.c 中完成，此处不再配置
    can_bus_init();
    update_state = UPDATE_RECV_SEND_CMD;  // 直接进入发送命令状态
}
```

- [ ] **Step 4: 验证**

编译通过，无重复定义错误。

- [ ] **Step 5: 提交**

```bash
git add Vehicle_screen_controller/Drivers/bootloader/app_update.c
git commit -m "refactor: remove UART callback from app_update, simplify init"
```

---

### Task 4: 修改 usart.c — 合并 UART 回调

**Files:**
- Modify: `Vehicle_screen_controller/Core/Src/usart.c`

- [ ] **Step 1: 添加 Includes**

在 `/* USER CODE BEGIN Includes */` 区域添加：

```c
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "app_update.h"
/* USER CODE END Includes */
```

- [ ] **Step 2: 添加变量声明**

在 `/* USER CODE BEGIN PV */` 区域添加：

```c
/* USER CODE BEGIN PV */
extern TaskHandle_t ota_task_handle;
static uint8_t usart1_rx_buf[32];
/* USER CODE END PV */
```

- [ ] **Step 3: 启动 USART1 空闲中断接收**

在 `MX_USART1_UART_Init()` 末尾的 `/* USER CODE BEGIN USART1_Init 2 */` 区域添加：

```c
/* USER CODE BEGIN USART1_Init 2 */
HAL_UARTEx_ReceiveToIdle_IT(&huart1, usart1_rx_buf, sizeof(usart1_rx_buf));
/* USER CODE END USART1_Init 2 */
```

- [ ] **Step 4: 修改 HAL_UARTEx_RxEventCallback**

在现有回调中增加 USART1 分支（保留 USART2/USART3 原有逻辑不变）：

```c
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

- [ ] **Step 5: 验证**

编译通过，无重复定义错误。

- [ ] **Step 6: 提交**

```bash
git add Vehicle_screen_controller/Core/Src/usart.c
git commit -m "feat: merge UART callback, add USART1 OTA trigger"
```

---

### Task 5: 修改 freertos.c — 新增 OTA 任务

**Files:**
- Modify: `Vehicle_screen_controller/Core/Src/freertos.c`

- [ ] **Step 1: 添加 Includes**

在 `/* USER CODE BEGIN Includes */` 区域添加：

```c
/* USER CODE BEGIN Includes */
// ... 原有 includes 保持不变 ...
#include "app_update.h"
/* USER CODE END Includes */
```

- [ ] **Step 2: 添加变量声明**

在 `/* USER CODE BEGIN Variables */` 区域添加：

```c
/* USER CODE BEGIN Variables */
// ... 原有变量保持不变 ...
TaskHandle_t ota_task_handle;
TaskHandle_t mqtt_handler;
/* USER CODE END Variables */
```

- [ ] **Step 3: 添加函数声明**

在 `/* USER CODE BEGIN PM */` 区域添加：

```c
/* USER CODE BEGIN PM */
// ... 原有声明保持不变 ...
void ota_task(void *param);
/* USER CODE END PM */
```

- [ ] **Step 4: 修改 StartDefaultTask**

将 `StartDefaultTask` 中的 `xTaskCreate` 调用改为：

```c
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
```

- [ ] **Step 5: 新增 ota_task 函数**

在 `freertos.c` 末尾添加：

```c
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

- [ ] **Step 6: 验证**

编译通过，无错误。特别检查：
- `ota_task_handle` 和 `mqtt_handler` 无未定义引用
- `App_update_init` 和 `App_update_work` 链接正常

- [ ] **Step 7: 提交**

```bash
git add Vehicle_screen_controller/Core/Src/freertos.c
git commit -m "feat: add OTA task with priority 20, suspend others on update"
```

---

### Task 6: 完整编译验证

**Files:**
- 无新增/修改

- [ ] **Step 1: 全量编译**

在 Keil 中执行 Rebuild All，确认：
- 0 Error, 0 Warning（或仅有已知无害警告）
- 无重复符号错误（`HAL_UARTEx_RxEventCallback`、`HAL_CAN_RxFifo1MsgPendingCallback`）
- 无未定义符号错误

- [ ] **Step 2: 检查 map 文件**

查看 `CAR.map` 文件，确认：
- `.text` 段起始地址为 `0x0800C000`
- 总 flash 使用量不超过 464KB (0x74000)
- RAM 使用量不超过 64KB

- [ ] **Step 3: 最终提交**

```bash
git add -A
git commit -m "feat: complete OTA module porting to Vehicle_screen_controller"
```

---

## 验证清单

编译通过后，在硬件上验证：

| # | 测试项 | 方法 | 预期结果 |
|---|--------|------|---------|
| 1 | 正常启动 | 上电 | OLED 显示正常，CAN 通信正常 |
| 2 | OTA 不干扰正常运行 | 不发 "update"，正常使用 | 所有功能正常 |
| 3 | OTA 触发 | USART1 发送 "update" | OLED 停止刷新，LED 停止闪烁 |
| 4 | 固件接收 | CAN 总线发送固件数据 | W25Q32 写入正确 |
| 5 | CRC 校验 | 发送端发送 CRC | 校验通过，boot flag 写入 EEPROM |
| 6 | 系统复位 | OTA 完成 | 1 秒后自动复位，正常启动 |
