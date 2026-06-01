# OTA 移植问题记录

**项目：** Vehicle_screen_controller  
**日期：** 2026-05-31  
**目标：** 将 app_rec 的 OTA 接收模块移植到 screen_controller（FreeRTOS 环境）

---

## 问题 1：发送 "update" 无反应

**现象：** 通过 USART1 发送 "update"，串口有回显，但 OTA 任务没有启动。

**原因：** `HAL_UARTEx_ReceiveToIdle_IT` 接收完成后**不会自动添加 `\0` 结尾符**，导致 `strstr()` 在无终止符的缓冲区中匹配失败。

**修复：** 在 `HAL_UARTEx_RxEventCallback` 中手动添加 `\0`：
```c
if (Size < sizeof(usart1_rx_buf))
    usart1_rx_buf[Size] = '\0';
else
    usart1_rx_buf[sizeof(usart1_rx_buf) - 1] = '\0';
```

---

## 问题 2：OTA 任务无法被唤醒（vTaskSuspend 死锁）

**现象：** 发送 "update" 后，调试打印显示 USART1 收到数据，但 OTA 任务没有继续执行。

**原因：** OTA 任务用 `vTaskSuspend(NULL)` 自我挂起，等待唤醒。但 `vTaskNotifyGiveFromISR` 只能唤醒正在 `ulTaskNotifyTake` 上**阻塞等待**的任务，无法唤醒被 `vTaskSuspend` 挂起的任务。两者是不同的 FreeRTOS 机制。

**修复：** 将 `vTaskSuspend(NULL)` 改为 `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`：
```c
// 修复前
vTaskSuspend(NULL);

// 修复后
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

---

## 问题 3：vTaskSuspend(NULL) 挂起自身导致死锁

**现象：** OTA 任务被唤醒后，所有其他任务没有被挂起，系统无响应。

**原因：** `AT_Parse` 和 `MQTT_Task` 的创建代码被注释掉了，`AT_pars_handle` 和 `mqtt_handler` 为 NULL。FreeRTOS 中 `vTaskSuspend(NULL)` 的含义是**挂起当前任务自身**，不是跳过。OTA 任务刚唤醒就把自己挂死了。

```c
// 这两个任务未创建，句柄为 NULL
// xTaskCreate(AT_Parse, ...);   ← 被注释
// xTaskCreate(MQTT_Task, ...);  ← 被注释

vTaskSuspend(AT_pars_handle);  // NULL → 等价于 vTaskSuspend(NULL) → 挂起自己！
vTaskSuspend(mqtt_handler);    // NULL → 同上
```

**修复：** 加 NULL 检查：
```c
if (oled_handler)   vTaskSuspend(oled_handler);
if (AT_pars_handle) vTaskSuspend(AT_pars_handle);
if (mqtt_handler)   vTaskSuspend(mqtt_handler);
if (voice_handler)  vTaskSuspend(voice_handler);
if (can_handler)    vTaskSuspend(can_handler);
```

---

## 问题 4：CAN FIFO1 永远收不到数据（过滤器冲突）

**现象：** OTA 流程启动正常（"recv cmd" → "erasing flash..." → "erase done" → "send cmd done"），但之后卡在等待 CAN 数据，发送端显示已完成发送。

**原因：** 主应用的 CAN 过滤器（FilterBank 0）配置为 **mask=0x0000（接受所有消息）**：
```c
// can.c 原始配置
filterConfig.FilterMaskIdHigh = 0x0000;  // 接受所有！
filterConfig.FilterMaskIdLow = 0x0000;
```

STM32 CAN 过滤器从低编号到高编号匹配，**第一个匹配的过滤器决定消息去哪个 FIFO**。FilterBank 0 匹配所有消息 → 全部进 FIFO0 → FilterBank 1（OTA/FIFO1）永远收不到任何帧。

**修复：** 将 FilterBank 0 改为精确匹配，新增 FilterBank 2：
```c
// FilterBank 0：精确匹配 0x666 → FIFO0（节点1）
filterConfig.FilterBank = 0;
filterConfig.FilterIdHigh = (0x666 << 5);     // 0xCCC0
filterConfig.FilterMaskIdHigh = (0x7FF << 5); // 0xFFE0 精确匹配

// FilterBank 1：OTA 用（Int_can.c 配置，0x020 → FIFO1）

// FilterBank 2：精确匹配 0x555 → FIFO0（节点2）
filterConfig.FilterBank = 2;
filterConfig.FilterIdHigh = (0x555 << 5);
filterConfig.FilterMaskIdHigh = (0x7FF << 5);
```

| FilterBank | CAN ID | FIFO | 用途 |
|---|---|---|---|
| 0 | 0x666 | FIFO0 | 主应用（节点1 灯/窗/天窗） |
| 1 | 0x020 | FIFO1 | OTA 固件数据 |
| 2 | 0x555 | FIFO0 | 主应用（节点2 车门） |

---

## 问题 5：Flash 擦除卡死（wait_busy 死循环）

**现象：** OTA 流程执行到 "erasing flash..." 后永久卡住，无 "erase done" 输出。

**原因：** `Int_w25q32_wait_busy()` 是一个无超时的死循环。如果 W25Q32 SPI 通信异常（未初始化、硬件故障、接线错误），读取状态寄存器返回值的 bit0 永远为 1，函数永远不会退出。

```c
static void Int_w25q32_wait_busy(void)
{
    while (1)  // ← 无超时保护！
    {
        Int_w25q32_write_byte(W25Q32_READ_STATUS_REG);
        uint8_t status = Int_w25q32_read_byte();
        if ((status & 0x01) == 0)
            break;
    }
}
```

另外，擦除 30 个扇区（每个 50-400ms）总共需要 1.5-12 秒，在此期间没有进度输出，容易误判为卡死。

**建议修复：**
```c
static void Int_w25q32_wait_busy(void)
{
    Int_w25q32_start();
    uint32_t timeout = HAL_GetTick() + 5000; // 5秒超时
    while (1)
    {
        Int_w25q32_write_byte(W25Q32_READ_STATUS_REG);
        uint8_t status = Int_w25q32_read_byte();
        if ((status & 0x01) == 0)
            break;
        if (HAL_GetTick() > timeout)
        {
            printf("W25Q32 timeout!\r\n");
            break;
        }
    }
    Int_w25q32_stop();
}
```

---

## 总结

| 问题 | 根因 | 影响范围 |
|---|---|---|
| "update" 无反应 | `HAL_UARTEx_ReceiveToIdle_IT` 不添加 `\0` | USART1 接收 |
| OTA 任务不唤醒 | `vTaskSuspend` 与 `vTaskNotifyGiveFromISR` 机制不互通 | FreeRTOS 任务同步 |
| 任务挂起死锁 | 未创建的任务句柄为 NULL，`vTaskSuspend(NULL)` 挂起自身 | FreeRTOS 任务管理 |
| CAN FIFO1 无数据 | FilterBank 0 mask=0x0000 拦截所有消息 | CAN 过滤器配置 |
| Flash 擦除卡死 | `wait_busy` 无超时保护 | W25Q32 SPI 驱动 |

**关键教训：**
1. `vTaskSuspend(NULL)` = 挂起自己，不是跳过
2. `vTaskSuspend` 无法被 `vTaskNotifyGiveFromISR` 唤醒
3. CAN 过滤器 mask=0x0000 会拦截所有消息，影响其他 FilterBank
4. 外设驱动的忙等待必须有超时保护
5. 中断接收缓冲区需手动添加 `\0` 终止符
