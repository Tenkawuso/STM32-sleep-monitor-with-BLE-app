# STM32 Sleep Monitor with BLE App

基于 STM32 的智能助眠与睡眠质量监测系统，包含嵌入式固件（HAL）与 Android 应用（BluetoothMonitor）。

> 该项目全程由 GPT 与 Gemini 模型构建。

## 项目功能

- 生理信号监测：通过 MAX30102 采集 PPG，计算心率（HR）与血氧（SpO2）。
- 环境信号监测：通过 MAX9814 统计噪声强度（NS），通过 BH1750 获取光照（LGT）。
- 睡眠状态估计：基于 HR/SpO2/噪声/光照进行状态判定（清醒、浅睡、深睡）。
- 自适应灯光调节：根据睡眠状态和环境亮度，自动调节 PWM 灯光亮度（仅自动模式）。
- 打鼾检测：基于音频帧特征（类似 MFCC-lite 的能量/变化量特征与节律规则）统计打鼾事件次数（SNR）。
- 手机联动：MCU 通过串口发送标准数据帧，App 解析后实时展示，并支持趋势图与日志。
- 助眠音乐触发：MCU 在高噪声时发送 `CMD:PLAY`，噪声回落后发送 `CMD:STOP`。

## 通信协议

### MCU -> App 数据帧

```text
DATA:HR=<hr>,SPO2=<spo2>,SLP=<slp>,LGT=<lux>,NS=<noise>,SNR=<snore_count>
```

- `HR`：心率 bpm（无效时为 0）
- `SPO2`：血氧百分比（无效时为 0）
- `SLP`：睡眠阶段（App 映射：0=清醒，1=浅睡，2=深睡）
- `LGT`：光照值
- `NS`：噪声百分比
- `SNR`：累计打鼾次数

### MCU -> App 控制帧

```text
CMD:PLAY
CMD:STOP
```

## 仓库结构

```text
HAL/                 STM32 固件工程（CubeMX + HAL + CMake）
  Core/Src/main.c    主控制逻辑：采样、判定、显示、调光、协议发送
  Core/Src/ppg_algo.c
  Drivers/           传感器与外设驱动（MAX30102/MAX9814/BH1750/HC06/OLED）

BluetoothMonitor/    Android 应用
  app/src/main/java/com/example/hc06app/
    MainActivity.kt          蓝牙连接、协议解析、UI 更新
    BluetoothDataBridge.kt   全局数据中转
    TrendDataRepository.kt   趋势数据缓存
```

## 当前实现说明

- MCU 端已实现：
  - ADC DMA 双缓冲采样音频并进行噪声/打鼾处理
  - MAX30102 FIFO 批量读取与 PPG 算法计算
  - OLED 三页面显示（环境/生理/睡眠）
  - KEY 短按切页（PC13 EXTI）
  - 自适应 PWM 调光与串口遥测输出
- Android 端已实现：
  - 蓝牙连接管理（当前以 BLE 路径为主）
  - 分包/粘包处理与 `DATA` / `CMD` 解析
  - 实时数据显示、打鼾计数展示、趋势查看与日志记录

## 快速开始

### 1) STM32 固件

1. 打开 `HAL/HAL.ioc`（CubeMX）确认引脚与外设配置。
2. 使用 CMake 或你常用的 STM32 工具链构建并烧录。
3. 硬件连接 MAX30102、MAX9814、BH1750、OLED、蓝牙模块后上电运行。

### 2) Android App

1. 使用 Android Studio 打开 `BluetoothMonitor/`。
2. 同步 Gradle 后编译安装到手机。
3. 授予蓝牙相关权限，点击连接并观察实时数据。

## 注意事项

- 本仓库保留了 STM32/IDE 相关配置文件，便于工程直接复现。
- 已通过 `.gitignore` 排除常见构建产物与大型无关目录（如 `HAL/Drivers/CMSIS/NN/`）。
- 若目标蓝牙模块实际仅支持经典蓝牙 SPP，请根据硬件情况补充/切换 App 连接策略。

## 开发状态

当前版本已打通“采集 -> 判定 -> 传输 -> App 展示”主流程，可作为毕业设计原型基础继续迭代（算法调参与连接稳定性优化）。
