# OTA 模块移植指南

## 概述

本模块实现 CAN 总线 OTA 升级功能，采用模块化设计，可方便移植到其他 STM32 工程。

## 架构

```
┌─────────────────────────────────────────────────┐
│                 app_update.c/h                   │  ← OTA 业务模块（不修改）
├─────────────────────────────────────────────────┤
│  can_bus.c/h      │  storage.c/h                │  ← 功能模块（不修改）
├─────────────────────────────────────────────────┤
│  can_adapter.c    │  storage_adapter.c           │  ← 适配层（需修改）
└─────────────────────────────────────────────────┘
```

## 移植到 FreeRTOS 工程

本模块完全非阻塞，可直接放入 FreeRTOS 任务：

```c
void ota_task(void *param)
{
    App_update_init();
    while (1)
    {
        App_update_work();
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 周期
    }
}
```

## 需要复制的文件

| 文件 | 是否修改 | 说明 |
|------|---------|------|
| `app_update.c/h` | 不修改 | OTA 业务模块 |
| `can_bus.c/h` | 不修改 | CAN 功能模块 |
| `storage.c/h` | 不修改 | 存储功能模块 |
| `can_adapter.c/h` | **需修改** | CAN 适配层 |
| `storage_adapter.c/h` | **需修改** | 存储适配层 |

## 移植步骤

### 1. 复制文件

将上述 5 组文件复制到目标工程的 BSP 目录。

### 2. 修改 can_adapter.c

将适配函数改为调用目标工程的 CAN 函数：

```c
#include "can_adapter.h"
// #include "Int_can.h"  ← 删除，改为包含目标工程的 CAN 头文件

void can_adapter_init(void)
{
    // 改为调用目标工程的 CAN 初始化函数
    // 例如：CAN_FilterConfig(); HAL_CAN_Start(&hcan);
}

void can_adapter_send(uint16_t id, uint8_t *data, uint8_t len)
{
    // 改为调用目标工程的 CAN 发送函数
    CAN_SendMsg(id, data, len);
}
```

### 3. 修改 storage_adapter.c

将适配函数改为调用目标工程的 SPI/I2C 函数。

### 4. CubeMX 配置

- 启用 CAN 外设
- 配置 CAN 过滤器，指定 `CAN_RX_FIFO1`
- 启用 CAN RX FIFO1 中断

### 5. 添加中断回调

在目标工程的中断文件（如 `stm32f1xx_it.c`）中添加：

```c
extern CAN_HandleTypeDef hcan;

// 如果目标工程已有 FIFO0 回调，保留它，新增 FIFO1 回调
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcanx)
{
    // 由 app_update.c 实现，此处留空或包含头文件即可
}
```

### 6. Keil 工程配置

将新文件添加到 Keil 工程的文件组中，并在 Include Path 中添加文件所在目录。

## FIFO 隔离

| 用途 | FIFO | 回调函数 |
|------|------|---------|
| 目标工程原有逻辑 | FIFO0 | `HAL_CAN_RxFifo0MsgPendingCallback` |
| OTA 模块 | FIFO1 | `HAL_CAN_RxFifo1MsgPendingCallback` |

两者互不干扰，无需修改目标工程原有代码。

## 注意事项

1. **CAN ID 过滤**：OTA 模块接收 ID=0 的命令帧，发送 ID=1 的数据帧。如果目标工程也使用这些 ID，需要调整。
2. **Flash 地址**：默认固件存储在 W25Q32 的 `0x1000` 地址，元数据在 `0x0000`。如需修改，改 `storage.h` 中的宏。
3. **EEPROM 地址**：升级标志存储在 `0x10` 地址。确保目标工程的 EEPROM 该地址未被占用。
