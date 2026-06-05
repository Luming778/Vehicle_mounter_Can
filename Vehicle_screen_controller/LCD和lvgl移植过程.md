# LVGL v9 移植教程 — 基于 STM32F103 + ATK-MD0280 (ILI9341)

本文档基于本项目的实际移植过程编写，详细说明如何将 LVGL v9.5.0-dev 移植到 STM32F103ZE 平台，使用 ATK-MD0280（ILI9341）2.8 寸 TFT LCD + XPT2046 电阻触摸屏。

---

## 目录

1. [移植概览](#1-移植概览)
2. [硬件与软件准备](#2-硬件与软件准备)
3. [LVGL 源码集成](#3-lvgl-源码集成)
4. [lv_conf.h 配置详解](#4-lv_confh-配置详解)
5. [显示驱动移植（Display Porting）](#5-显示驱动移植display-porting)
6. [输入设备移植（Input Device Porting）](#6-输入设备移植input-device-porting)
7. [FreeRTOS 集成](#7-freertos-集成)
8. [主任务编写](#8-主任务编写)
9. [编译与烧录](#9-编译与烧录)
10. [常见问题与排查](#10-常见问题与排查)
11. [性能优化建议](#11-性能优化建议)

---

## 1. 移植概览

LVGL 移植的核心工作只有 **两件事**：

1. **显示驱动移植** — 告诉 LVGL 如何把像素数据写到 LCD 上（实现 `flush` 回调）
2. **输入设备移植** — 告诉 LVGL 如何读取触摸屏状态（实现 `read` 回调）

其余都是配置工作。整体架构如下：

```
LVGL 核心库 (lvgl/src/)
    |
    |-- lv_conf.h          你写的：裁剪功能、设置内存
    |
    |-- porting/
    |   |-- lv_port_lcd_stm32.c   你写的：flush 回调 -> LCD 硬件
    |   |-- lv_port_indev.c       你写的：read 回调 -> 触摸硬件
    |
    |-- Drivers/hardware/         已有的：LCD + Touch 底层驱动
        |-- atk_md0280.c          ATK-MD0280 LCD 驱动
        |-- atk_md0280_fsmc.c     FSMC 并口接口
        |-- atk_md0280_touch.c    XPT2046 触摸驱动
        |-- atk_md0280_touch_spi.c 触摸 SPI 通信
```

**关键原则**：LVGL 不关心你的 LCD 是什么型号、用什么接口。它只通过两个回调函数与硬件交互。你只需要在回调里调用已有的底层驱动即可。

---

## 2. 硬件与软件准备

### 硬件

| 组件 | 型号          | 接口                | 说明                                     |
| ---- | ------------- | ------------------- | ---------------------------------------- |
| MCU  | STM32F103ZET6 | -                   | Cortex-M3, 72MHz, 64KB SRAM, 512KB Flash |
| LCD  | ATK-MD0280    | FSMC 16-bit 并口    | ILI9341 控制器, 240x320, RGB565          |
| 触摸 | XPT2046       | 软件 SPI (bit-bang) | 电阻式触摸屏，集成在 ATK-MD0280 模块上   |

### LCD 引脚连接

| 功能                | STM32 引脚                   | 说明            |
| ------------------- | ---------------------------- | --------------- |
| FSMC 数据线 D0-D15  | PD0/1/4/5/8-10/14-15, PE7-15 | 16-bit 并口数据 |
| FSMC 片选 CS        | PG12                         | FSMC Bank4      |
| 寄存器选择 RS (A10) | PG0                          | 命令/数据切换   |
| 读使能 RD           | PD4                          | FSMC 读信号     |
| 写使能 WR           | PD5                          | FSMC 写信号     |
| 背光 BL             | PB0                          | GPIO 推挽输出   |

### 触摸引脚连接

| 功能       | STM32 引脚 | 说明                       |
| ---------- | ---------- | -------------------------- |
| SPI CLK    | PB1        | 时钟                       |
| SPI MISO   | PB2        | 数据输入（XPT2046 -> MCU） |
| SPI MOSI   | PF9        | 数据输出（MCU -> XPT2046） |
| SPI CS     | PF11       | 片选                       |
| PEN (中断) | PF10       | 触摸检测（低电平=按下）    |

### 软件

- **Keil MDK-ARM** (ARMCC V5.06)
- **LVGL v9.5.0-dev** 源码
- **ATK-MD0280 底层驱动**（已提供，FSMC + SPI 触摸）
- **FreeRTOS v10.3.1**（已集成）

---

## 3. LVGL 源码集成

### 3.1 获取 LVGL 源码

从 LVGL 官方 GitHub 下载源码，本项目使用 `v9.5.0-dev` 版本。

### 3.2 目录结构

将 LVGL 源码放在项目中的结构如下：

```
Vehicle_screen_controller/
|-- lvgl/                          LVGL 根目录
|   |-- lv_conf.h                  配置文件（你编写/修改）
|   |-- lvgl.h                     主头文件
|   |-- src/                       LVGL 源码（不要修改）
|   |   |-- core/                  核心对象系统
|   |   |-- draw/                  渲染引擎
|   |   |-- font/                  字体
|   |   |-- widgets/               控件
|   |   |-- osal/                  OS 抽象层（含 lv_freertos.c）
|   |   |-- ...
|   |-- porting/                   移植层（你编写）
|   |   |-- lv_port_lcd_stm32.c    显示驱动移植
|   |   |-- lv_port_indev.c        输入设备移植
|   |-- demos/                     Demo 代码（可选）
```

### 3.3 Keil 工程配置

在 Keil 工程中需要添加的文件组：

1. **LVGL/src/** — 添加 `src/` 下所有 `.c` 文件（递归搜索子目录）
2. **LVGL/porting/** — 添加 `porting/lv_port_lcd_stm32.c` 和 `porting/lv_port_indev.c`
3. **LVGL/demos/** — 如果需要 demo，添加 `demos/` 下的文件

Include Path 需要添加：

- `lvgl/`（包含 `lvgl.h` 和 `lv_conf.h`）
- `lvgl/src/`（LVGL 内部头文件）

### 3.4 lv_conf.h 的位置

`lv_conf.h` 必须放在 LVGL 能找到的位置。有两种方式：

- **方式 A**：放在 `lvgl/` 目录下（与 `lvgl.h` 同级）— 本项目采用此方式
- **方式 B**：放在项目根目录，需要在编译选项中定义 `LV_CONF_INCLUDE_SIMPLE`

---

## 4. lv_conf.h 配置详解

这是移植中最关键的配置文件。以下逐项说明本项目的配置及原因。

### 4.1 颜色深度

```c
#define LV_COLOR_DEPTH 16
```

ILI9341 原生支持 RGB565（16-bit），设置为 16 与硬件匹配。不要设置为 32（浪费内存）或 8（ILI9341 不支持）。

### 4.2 内存配置

```c
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN   // 使用 LVGL 自带内存管理
#define LV_MEM_SIZE             (12 * 1024U)         // 12KB LVGL 堆
#define LV_MEM_ADR              0                    // 内部 SRAM
#define LV_MEM_POOL_EXPAND_SIZE 0                    // 不扩展
```

**为什么是 12KB？** STM32F103ZE 只有 64KB SRAM，需要分配给：

- FreeRTOS 堆：32KB
- LVGL 堆：12KB
- LVGL 显示缓冲：7.68KB
- 各任务栈：~8KB
- 全局变量：~3KB

总计约 63KB，几乎用满。12KB 是经过权衡的最小可用值。

### 4.3 显示刷新

```c
#define LV_DEF_REFR_PERIOD  33    // 33ms = ~30 FPS
```

对于 72MHz 的 Cortex-M3，30 FPS 是合理的性能目标。设置太小会增加 CPU 负担。

### 4.4 OS 集成

```c
#define LV_USE_OS                       LV_OS_FREERTOS
#define LV_USE_FREERTOS_TASK_NOTIFY     1
```

本项目使用 FreeRTOS，必须设置 `LV_USE_OS` 为 `LV_OS_FREERTOS`。开启 task notification 可以提升 45% 的同步性能。

### 4.5 渲染器

```c
#define LV_USE_DRAW_SW              1    // 软件渲染
#define LV_DRAW_SW_DRAW_UNIT_CNT    1    // 单线程渲染
#define LV_DRAW_SW_COMPLEX          1    // 启用复杂渲染（圆角、阴影等）
#define LV_DRAW_SW_SUPPORT_RGB565   1    // 原生 RGB565 支持
```

STM32F103 没有 GPU 加速，只能用软件渲染。`LV_DRAW_SW_SUPPORT_RGB565 = 1` 启用针对 RGB565 的优化渲染路径，避免运行时颜色转换。

### 4.6 缓冲区对齐

```c
#define LV_DRAW_BUF_STRIDE_ALIGN    1    // 无 stride 对齐
#define LV_DRAW_BUF_ALIGN           4    // 4 字节对齐
```

Cortex-M3 不需要特殊的 stride 对齐。4 字节对齐是 ARM 的通用要求。

### 4.7 字体

```c
#define LV_FONT_MONTSERRAT_14   1               // 只启用 14px
#define LV_FONT_DEFAULT         &lv_font_montserrat_14
```

每个字体约占用 2-4KB Flash。在 512KB Flash 的限制下，只启用需要的字体。如需中文显示，需要额外添加中文字体（本项目未启用 LVGL 中文，中文显示通过 OLED 独立处理）。

### 4.8 控件裁剪

根据实际需要启用/禁用控件。本项目启用了常用的 button、label、image、arc、bar 等，禁用了不常用的 textarea、table、tabview 等以节省 Flash。

### 4.9 系统监控

```c
#define LV_USE_SYSMON       1
#define LV_USE_PERF_MONITOR 1    // 右下角显示 FPS 和 CPU 使用率
#define LV_USE_MEM_MONITOR  1    // 左下角显示内存使用情况
```

开发阶段建议开启，方便调试性能。生产环境可关闭以减少开销。

---

## 5. 显示驱动移植（Display Porting）

这是移植的**核心工作**。需要实现两个文件：

- `lv_port_lcd_stm32.h` — 头文件，声明初始化函数
- `lv_port_lcd_stm32.c` — 实现 flush 回调

### 5.1 原理

LVGL 的显示刷新机制：

```
LVGL 渲染引擎计算出一片矩形区域的像素数据
    |
    v
调用 flush 回调，传入：(display, area, pixel_data)
    |
    v
你的代码把 pixel_data 写入 LCD 的对应区域
    |
    v
调用 lv_display_flush_ready() 通知 LVGL 可以继续
```

LVGL 不会一次性刷新整个屏幕（240x320 = 153,600 像素 = 307,200 字节，远超 64KB SRAM）。它使用**部分渲染（Partial Rendering）**模式，每次只渲染一小块区域（如 16 行），写入 LCD 后释放缓冲区，再渲染下一块。

### 5.2 头文件 `lv_port_lcd_stm32.h`

```c
#ifndef LV_PORT_LCD_STM32_H
#define LV_PORT_LCD_STM32_H

#include "lvgl/lvgl.h"    // 包含 LVGL 头文件

#define LVGL_LCD_HOR_RES    240    // LCD 水平分辨率
#define LVGL_LCD_VER_RES    320    // LCD 垂直分辨率

void lv_port_disp_init(void);      // 初始化函数，LVGL 任务中调用

#endif
```

### 5.3 源文件 `lv_port_lcd_stm32.c`

#### 5.3.1 显示缓冲区

```c
#define MY_DISP_HOR_RES     240
#define MY_DISP_VER_RES     320

// 显示缓冲区：240 像素 x 16 行 x 2 字节/像素 = 7,680 字节
static lv_color_t buf1[MY_DISP_HOR_RES * 16];
```

**缓冲区大小的选择**：

- 缓冲区越大，LVGL 每次渲染的行数越多，刷新效率越高
- 但缓冲区占用 SRAM，64KB 总内存下不能太大
- 本项目选择 16 行（7.68KB），是性能和内存的平衡点
- 最小可以是 1 行（240x2 = 480 字节），但会显著降低渲染速度

**单缓冲 vs 双缓冲**：

- 单缓冲：LVGL 渲染到 buf1，然后 flush 到 LCD，期间 LVGL 等待
- 双缓冲：LVGL 渲染到 buf1 的同时，buf2 正在写入 LCD，实现流水线
- 双缓冲需要 2 倍内存（15.36KB），本项目内存紧张，使用单缓冲

#### 5.3.2 初始化函数

```c
void lv_port_disp_init(void)
{
    // 1. 创建显示对象
    lv_display_t *disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);

    // 2. 设置显示方向（竖屏）
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    // 3. 设置颜色格式（RGB565，与 ILI9341 硬件匹配）
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    // 4. 设置显示缓冲区（单缓冲，部分渲染模式）
    lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 5. 注册 flush 回调函数
    lv_display_set_flush_cb(disp, lcd_flush_cb);
}
```

**调用顺序很重要**：必须在 `lv_init()` 之后调用此函数。

#### 5.3.3 Flush 回调函数（核心）

```c
static void lcd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    // 从 area 参数获取要刷新的矩形区域
    uint16_t x1 = area->x1;
    uint16_t y1 = area->y1;
    uint16_t x2 = area->x2;
    uint16_t y2 = area->y2;

    uint16_t width  = x2 - x1 + 1;
    uint16_t height = y2 - y1 + 1;
    uint32_t size   = width * height;

    // 步骤 1：设置列地址（ILI9341 命令 0x2A）
    ATK_MD0280_FSMC_CMD_REG = 0x2A;
    ATK_MD0280_FSMC_DAT_REG = (x1 >> 8) & 0xFF;    // 起始列高字节
    ATK_MD0280_FSMC_DAT_REG = x1 & 0xFF;            // 起始列低字节
    ATK_MD0280_FSMC_DAT_REG = (x2 >> 8) & 0xFF;    // 结束列高字节
    ATK_MD0280_FSMC_DAT_REG = x2 & 0xFF;            // 结束列低字节

    // 步骤 2：设置页地址（ILI9341 命令 0x2B）
    ATK_MD0280_FSMC_CMD_REG = 0x2B;
    ATK_MD0280_FSMC_DAT_REG = (y1 >> 8) & 0xFF;    // 起始行高字节
    ATK_MD0280_FSMC_DAT_REG = y1 & 0xFF;            // 起始行低字节
    ATK_MD0280_FSMC_DAT_REG = (y2 >> 8) & 0xFF;    // 结束行高字节
    ATK_MD0280_FSMC_DAT_REG = y2 & 0xFF;            // 结束行低字节

    // 步骤 3：发送写内存命令（ILI9341 命令 0x2C）
    ATK_MD0280_FSMC_CMD_REG = 0x2C;

    // 步骤 4：写入像素数据
    uint16_t *color_p = (uint16_t *)px_map;
    for (uint32_t i = 0; i < size; i++) {
        ATK_MD0280_FSMC_DAT_REG = color_p[i];
    }

    // 步骤 5：通知 LVGL 刷新完成
    lv_display_flush_ready(disp);
}
```

**为什么直接操作 FSMC 寄存器？**

本项目的底层驱动提供了 `atk_md0280_fill()` 和 `atk_md0280_fsmc_write_dat()` 等函数，但 flush 回调选择直接写 FSMC 数据寄存器 `ATK_MD0280_FSMC_DAT_REG`，原因是：

1. `atk_md0280_fill()` 内部会重复设置列地址/页地址，而 flush 回调已经设置过了
2. `atk_md0280_fsmc_write_dat()` 是 inline 函数，虽然开销很小，但在循环中每像素调用一次仍有函数调用开销
3. 直接写寄存器是最快的路径，对 30 FPS 的刷新率至关重要

**ILI9341 写像素的协议**：

```
MCU 发送:  [0x2A] [x1_hi] [x1_lo] [x2_hi] [x2_lo]    <- 设置列范围
MCU 发送:  [0x2B] [y1_hi] [y1_lo] [y2_hi] [y2_lo]    <- 设置行范围
MCU 发送:  [0x2C]                                       <- 开始写内存
MCU 发送:  [pixel0_hi] [pixel0_lo] [pixel1_hi] ...     <- 像素数据流
```

ILI9341 收到 0x2C 后，每收到 2 字节就自动写入一个像素到显存，并自动递增地址。这就是为什么可以用一个简单的 for 循环连续写入。

**FSMC 地址映射原理**：

```
FSMC Bank4 基地址: 0x6C000000

A10 = 0 (地址 0x6C000000): 写入被 LCD 解释为 COMMAND
A10 = 1 (地址 0x6C000800): 写入被 LCD 解释为 DATA

ATK_MD0280_FSMC_CMD_REG = (*(volatile uint16_t *)0x6C000000)  // 写命令
ATK_MD0280_FSMC_DAT_REG = (*(volatile uint16_t *)0x6C000800)  // 写数据
```

FSMC 硬件自动处理时序，CPU 只需要往对应地址写数据，硬件会自动产生 CS、RD、WR 等信号。写时序配置为 AST=0, DST=1 HCLK（72MHz 下约 14ns），非常快。

---

## 6. 输入设备移植（Input Device Porting）

### 6.1 原理

LVGL 通过轮询方式读取输入设备状态。每个刷新周期（约 5ms），LVGL 调用注册的 `read` 回调获取当前触摸状态。

```
lv_timer_handler() 每 5ms 调用一次
    |
    v
调用 touchpad_read(indev, data)
    |
    v
你的代码读取触摸硬件，填充 data 结构体：
  - data->point.x, data->point.y  触摸坐标
  - data->state                   按下/释放状态
    |
    v
LVGL 根据坐标进行命中测试，生成事件（PRESSED, CLICKED 等）
```

### 6.2 头文件 `lv_port_indev.h`

```c
#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#include "lvgl/lvgl.h"

void lv_port_indev_init(void);    // 初始化函数

#endif
```

### 6.3 源文件 `lv_port_indev.c`

#### 6.3.1 初始化函数

```c
static lv_indev_t *indev_touchpad;

void lv_port_indev_init(void)
{
    touchpad_init();    // 硬件初始化（在本项目中为空，因为 atk_md0280_init() 已完成）

    // 创建输入设备
    indev_touchpad = lv_indev_create();

    // 设置为触摸类型（LVGL 支持多种输入：鼠标、键盘、编码器等）
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);

    // 注册读取回调
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);
}
```

#### 6.3.2 触摸读取回调（核心）

```c
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;
    uint16_t x, y;

    // 调用底层驱动读取触摸坐标
    uint8_t ret = atk_md0280_touch_scan(&x, &y);

    if (ret == ATK_MD0280_TOUCH_EOK) {
        // 触摸按下
        last_x = x;
        last_y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        // 触摸释放
        data->state = LV_INDEV_STATE_RELEASED;
    }

    // 始终设置坐标（释放时使用最后已知坐标）
    data->point.x = last_x;
    data->point.y = last_y;
}
```

**为什么释放时还要设置坐标？**

LVGL 需要根据"释放时的坐标"来判断用户是否在同一个控件上释放手指。如果释放时坐标为 (0,0)，LVGL 会认为用户把手指移到了左上角，导致 CLICKED 事件无法正确触发。

#### 6.3.3 底层触摸数据流

`atk_md0280_touch_scan()` 的内部流程：

```
1. 检测 PEN 引脚（PF10）是否为低电平（有触摸）
   |
   v (有触摸)
2. 通过软件 SPI 读取 XPT2046 的 X 轴 ADC（命令 0xD0）
   - 读取 5 次采样
   - 排序后去掉最大最小值
   - 对剩余 3 个取平均
   |
3. 同样方式读取 Y 轴 ADC（命令 0x90）
   |
   v
4. 使用校准参数将 ADC 值转换为像素坐标：
   x_pixel = (x_adc - center_x) / factor_x + 120
   y_pixel = (y_adc - center_y) / factor_y + 160
   |
   v
5. 根据显示方向（0/90/180/270）进行坐标旋转变换
   |
   v
6. 边界检查，返回像素坐标
```

**XPT2046 SPI 通信**（软件 bit-bang）：

```
时序：
  CS 拉低
  发送 8-bit 命令（MSB 先发）：0xD0=读X, 0x90=读Y
  等待 BUSY 周期
  读取 16-bit 数据（实际有效 12-bit，在高 12 位）
  CS 拉高

SPI 引脚：
  CLK: PB1    (时钟)
  MOSI: PF9   (MCU -> XPT2046)
  MISO: PB2   (XPT2046 -> MCU)
  CS: PF11    (片选)
```

---

## 7. FreeRTOS 集成

### 7.1 为什么用 FreeRTOS？

本项目除了 LVGL GUI 外，还有 OLED 显示、CAN 通信、语音识别等子系统，需要并发运行。FreeRTOS 提供：

- 任务调度：各子系统独立运行
- 队列通信：ISR 到任务的数据传递
- 互斥保护：LVGL 内部线程安全

### 7.2 lv_conf.h 中的 FreeRTOS 配置

```c
#define LV_USE_OS                       LV_OS_FREERTOS
#define LV_USE_FREERTOS_TASK_NOTIFY     1
```

设置 `LV_USE_OS = LV_OS_FREERTOS` 后，LVGL 会：

- 在内部使用 FreeRTOS 互斥锁保护共享资源
- 使用 task notification 替代信号量进行同步（更快）
- 在 `lv_timer_handler()` 中自动处理 FreeRTOS 的任务切换

### 7.3 FreeRTOSConfig.h 中的 LVGL 集成

为了支持 LVGL 的性能监控（CPU 使用率），需要在 `FreeRTOSConfig.h` 中添加 FreeRTOS 追踪钩子：

```c
/* USER CODE BEGIN Defines */
extern void lv_freertos_task_switch_in(const char * name);
extern void lv_freertos_task_switch_out(void);
#define traceTASK_SWITCHED_IN()   lv_freertos_task_switch_in((const char *)pxCurrentTCB->pcTaskName)
#define traceTASK_SWITCHED_OUT()  lv_freertos_task_switch_out()
/* USER CODE END Defines */
```

这两个宏在 FreeRTOS 每次任务切换时被调用，LVGL 通过它们统计 CPU 空闲率。如果不添加，LVGL 的性能监视器会每 5 秒打印一次警告。

### 7.4 LVGL Tick 源

LVGL 需要一个 1ms 的 tick 来驱动动画和定时器。本项目使用 TIM4 硬件定时器（不使用 SysTick，因为 SysTick 被 FreeRTOS 占用）：

```c
// main.c 中的 HAL 回调
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) {
        HAL_IncTick();      // HAL 时间基准
        lv_tick_inc(1);     // LVGL 时间基准（1ms）
    }
}
```

### 7.5 lv_timer_handler() 的调用

`lv_timer_handler()` 必须在**一个单独的任务**中周期性调用，不能在中断中调用：

```c
while (1) {
    lv_timer_handler();                    // 处理 LVGL 所有定时器、输入、渲染
    vTaskDelay(pdMS_TO_TICKS(5));          // 每 5ms 调用一次
}
```

5ms 间隔意味着 LVGL 最高 200 次/秒的处理频率，但实际刷新率受 `LV_DEF_REFR_PERIOD`（33ms）限制为约 30 FPS。

---

## 8. 主任务编写

将所有部分组合在一起的完整任务函数：

```c
void lvgl_demo_task(void *pvParameters)
{
    // 第 1 步：初始化 LCD 硬件（FSMC + LCD 寄存器 + 触摸校准）
    // 注意：此函数会阻塞，等待用户完成 5 点触摸校准
    uint8_t ret = atk_md0280_init();
    if (ret != ATK_MD0280_EOK) {
        printf("ATK-MD0280 init failed!\r\n");
        vTaskDelete(NULL);
    }

    // 第 2 步：初始化 LVGL 核心
    lv_init();

    // 第 3 步：初始化 LVGL 显示驱动（注册 flush 回调）
    lv_port_disp_init();

    // 第 4 步：初始化 LVGL 输入设备（注册 touch 回调）
    lv_port_indev_init();

    // 第 5 步：创建 UI（这里是一个简单的 demo）
    lv_obj_t *scr = lv_screen_active();
    // ... 创建 label、button 等控件 ...

    // 第 6 步：进入主循环
    while (1) {
        lv_timer_handler();                    // LVGL 主循环
        vTaskDelay(pdMS_TO_TICKS(5));          // 5ms 周期
    }
}
```

**初始化顺序**：

1. `atk_md0280_init()` 必须最先调用，因为后续的 flush 和 touch 回调依赖硬件
2. `lv_init()` 必须在所有 LVGL API 调用之前
3. `lv_port_disp_init()` 和 `lv_port_indev_init()` 必须在 `lv_init()` 之后
4. UI 创建必须在 display 和 indev 初始化之后

**任务栈大小**：本项目设置为 1024 words（4096 字节）。LVGL 在渲染复杂 UI 时可能使用较多栈空间，建议至少 2KB，推荐 4KB。

---

## 9. 编译与烧录

### 9.1 编译

1. 打开 `MDK-ARM/CAR.uvprojx`
2. Project -> Build Target (F7)
3. 检查输出：
   - 无 error
   - 查看 `CAR.map` 确认内存使用情况（Flash 和 SRAM 不能溢出）

### 9.2 常见编译错误

| 错误                               | 原因                   | 解决                                                             |
| ---------------------------------- | ---------------------- | ---------------------------------------------------------------- |
| `lv_conf.h not found`            | 头文件路径未配置       | 在 Keil Include Path 中添加 `lvgl/` 目录                       |
| `undefined reference to lv_init` | LVGL 源码未加入工程    | 在 Keil 中添加 `lvgl/src/` 下所有 .c 文件                      |
| `LV_COLOR_DEPTH not defined`     | `lv_conf.h` 未被包含 | 确认 `lv_conf.h` 在正确位置，或定义 `LV_CONF_INCLUDE_SIMPLE` |
| 内存溢出 (data + bss > 64KB)       | SRAM 不够              | 减小 `LV_MEM_SIZE` 或 `configTOTAL_HEAP_SIZE`                |

### 9.3 烧录

使用 Keil 内置的烧录工具或 ST-Link Utility 烧录 `CAR.hex`。

### 9.4 验证

烧录成功后：

1. LCD 应显示白色背景（`atk_md0280_clear(WHITE)`）
2. 触摸校准界面出现，按提示点击 5 个点
3. 校准完成后显示 LVGL demo UI
4. 触摸 "Touch Me" 按钮，计数器应递增
5. 屏幕右下角应显示 FPS 和 CPU 使用率（如果启用了性能监视器）

---

## 10. 常见问题与排查

### 10.1 屏幕全白/全黑/花屏

**可能原因**：

- FSMC 时序不正确 — 检查 `atk_md0280_fsmc.c` 中的读写时序参数
- LCD 寄存器初始化不正确 — 检查 `atk_md0280_reg_init()` 中的 ILI9341 初始化序列
- 颜色格式不匹配 — 确认 `LV_COLOR_DEPTH = 16` 且 ILI9341 设置为 RGB565（寄存器 0x3A 写入 0x55）

**排查方法**：

- 先用 `atk_md0280_fill(0, 0, 239, 319, 0xF800)` 填充红色，确认底层驱动正常
- 如果底层驱动正常但 LVGL 显示异常，检查 flush 回调中的命令/数据写入顺序

### 10.2 屏幕显示但触摸无响应

**可能原因**：

- 触摸 SPI 引脚配置错误 — 检查 PB1/PB2/PF9/PF11
- 触摸校准失败 — 重新上电，仔细点击校准点
- PEN 引脚（PF10）未正确配置为输入

**排查方法**：

- 在 `touchpad_read` 中打印 `atk_md0280_touch_scan()` 的返回值和坐标
- 用万用表测量 PEN 引脚：未触摸时为高电平，触摸时为低电平

### 10.3 显示方向不对

**可能原因**：

- LVGL rotation 与 LCD scan direction 不匹配

**解决方法**：

- 在 `lv_port_disp_init()` 中调整 `lv_display_set_rotation()` 的参数
- 同步调整 `atk_md0280_set_disp_dir()` 的方向设置
- 如果坐标翻转，需要在 `touchpad_read` 中手动交换 x/y 或取反

### 10.4 画面撕裂/闪烁

**可能原因**：

- 单缓冲模式下，LVGL 可能在 flush 未完成时就开始渲染下一帧

**解决方法**：

- 确保 `lv_display_flush_ready()` 在像素数据完全写入 LCD 后才调用
- 如果有条件，启用双缓冲（需要额外 7.68KB 内存）

### 10.5 FreeRTOS 栈溢出

**可能原因**：

- LVGL 任务栈太小，复杂 UI 渲染时栈溢出

**排查方法**：

- 在任务中调用 `uxTaskGetStackHighWaterMark(NULL)` 查看剩余栈空间
- 如果返回值很小（<50 words），增大任务栈
- 避免在 LVGL 任务中使用大局部数组

### 10.6 LVGL 性能监视器显示 CPU 100% 或警告

**可能原因**：

- `traceTASK_SWITCHED_IN/OUT` 宏未定义（见第 7.3 节）

**解决方法**：

- 在 `FreeRTOSConfig.h` 中添加追踪宏定义
- 或者在 `lv_conf.h` 中关闭 `LV_USE_PERF_MONITOR`

---

## 11. 性能优化建议

### 11.1 Flush 回调优化

- 直接写 FSMC 数据寄存器，避免函数调用开销（本项目已采用）
- 如果 LCD 控制器支持，使用 DMA 传输像素数据（ILI9341 不支持 DMA 写入，但 STM32 的 FSMC 可以配合 DMA）

### 11.2 缓冲区优化

- 增大显示缓冲区（如 32 行 = 15.36KB）可以减少 flush 次数
- 启用双缓冲可以实现渲染和传输的流水线

### 11.3 渲染优化

- 禁用不需要的控件，减少代码体积
- 关闭 `LV_DRAW_SW_COMPLEX` 可以禁用圆角、阴影等复杂渲染（节省 CPU）
- 使用 `LV_COLOR_FORMAT_RGB565` 避免运行时颜色转换

### 11.4 刷新策略

- 对于静态 UI，不需要每 5ms 调用一次 `lv_timer_handler()`，可以增大 `vTaskDelay` 到 10-20ms
- 使用 `lv_obj_invalidate()` 手动标记需要刷新的区域，避免全屏刷新

### 11.5 字体优化

- 启用字体压缩：`LV_USE_FONT_COMPRESSED = 1`
- 只包含需要的字符子集（LVGL 支持按需加载字形）

---

## 附录 A：LVGL v9 API 速查

### 显示相关

| API                                                      | 用途            |
| -------------------------------------------------------- | --------------- |
| `lv_display_create(w, h)`                              | 创建显示对象    |
| `lv_display_set_rotation(disp, rot)`                   | 设置旋转方向    |
| `lv_display_set_color_format(disp, fmt)`               | 设置颜色格式    |
| `lv_display_set_buffers(disp, buf1, buf2, size, mode)` | 设置渲染缓冲区  |
| `lv_display_set_flush_cb(disp, cb)`                    | 注册 flush 回调 |
| `lv_display_flush_ready(disp)`                         | 通知 flush 完成 |

### 输入设备相关

| API                                 | 用途         |
| ----------------------------------- | ------------ |
| `lv_indev_create()`               | 创建输入设备 |
| `lv_indev_set_type(indev, type)`  | 设置输入类型 |
| `lv_indev_set_read_cb(indev, cb)` | 注册读取回调 |

### UI 创建

| API                                                | 用途             |
| -------------------------------------------------- | ---------------- |
| `lv_screen_active()`                             | 获取当前活跃屏幕 |
| `lv_obj_create(parent)`                          | 创建基础对象     |
| `lv_label_create(parent)`                        | 创建标签         |
| `lv_button_create(parent)`                       | 创建按钮         |
| `lv_image_create(parent)`                        | 创建图片         |
| `lv_obj_set_pos(obj, x, y)`                      | 设置位置         |
| `lv_obj_set_size(obj, w, h)`                     | 设置大小         |
| `lv_obj_add_event_cb(obj, cb, event, user_data)` | 添加事件回调     |

---

## 附录 B：本项目移植涉及的全部文件

| 文件                                        | 角色             | 是否需要修改           |
| ------------------------------------------- | ---------------- | ---------------------- |
| `lvgl/lv_conf.h`                          | LVGL 配置        | 是（根据项目需求裁剪） |
| `lvgl/porting/lv_port_lcd_stm32.c`        | 显示驱动移植     | 是（编写 flush 回调）  |
| `lvgl/porting/lv_port_lcd_stm32.h`        | 显示驱动头文件   | 是（声明初始化函数）   |
| `lvgl/porting/lv_port_indev.c`            | 输入设备移植     | 是（编写 read 回调）   |
| `lvgl/porting/lv_port_indev.h`            | 输入设备头文件   | 是（声明初始化函数）   |
| `Core/Inc/FreeRTOSConfig.h`               | FreeRTOS 配置    | 是（添加 trace 宏）    |
| `Core/Src/freertos.c`                     | 任务定义         | 是（编写 LVGL 任务）   |
| `Core/Src/main.c`                         | 系统初始化       | 否（CubeMX 生成）      |
| `Drivers/hardware/LCD_touch_ATK_MD0280/*` | LCD+触摸底层驱动 | 否（已有驱动）         |
| `lvgl/src/*`                              | LVGL 核心源码    | 否（库代码，不要修改） |
