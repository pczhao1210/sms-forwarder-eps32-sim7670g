# Waveshare 2026/V2 硬件引脚变更说明 / Hardware V2 Pin Notes

> 重点提示：本固件默认使用 V1 引脚定义。2026 年购买或收到的模块，请先检查背面丝印、摄像头型号或 Waveshare FAQ 中的版本说明；确认是 V2 后，请先替换本文列出的引脚再编译烧录。
>
> Important: this firmware uses the V1 pin mapping by default. For modules purchased or received in 2026 or later, check the rear silkscreen, camera model, or Waveshare FAQ revision notes first. If the board is V2, update the pins listed below before building and flashing.

Waveshare FAQ 提示：如为 2026 年元旦后收到的模块，请使用示例程序 V2。新版本硬件与旧版本在部分外设引脚和摄像头方案上有差异，使用本项目时需要先确认板子版本。

Waveshare's FAQ notes that modules received after 2026-01-01 should use the V2 examples. The new hardware revision changes some peripheral pins and the camera module, so confirm the board revision before using this firmware.

FAQ: <https://docs.waveshare.net/ESP32-S3-SIM7670G-4G/FAQ>

## 如何识别 / How to Identify

1. 2026 年元旦后收到的 ESP32-S3-SIM7670G-4G 模块，优先按 V2 示例处理。/ For ESP32-S3-SIM7670G-4G modules received after 2026-01-01, prefer the V2 examples.
2. FAQ 中说明 2026 年后新版本默认配套 OV5640 摄像头，背面带有 `V2.0` 丝印；旧版本使用 OV2640，两者摄像头程序不通用。/ The FAQ says the 2026+ revision ships with an OV5640 camera by default and has `V2.0` silkscreen on the back. Older boards use OV2640, and the camera programs are not interchangeable.
3. 本项目不启用摄像头，但摄像头/I2C 等引脚变化会影响 MAX17048 电池监控和可选外设文档。/ This firmware does not use the camera, but camera and I2C pin changes affect MAX17048 battery monitoring and optional peripheral references.

## 本项目必须关注的修改 / Required Change For This Firmware

当前固件默认仍按旧版 MAX17048 I2C 引脚定义：

The current firmware still defaults to the old MAX17048 I2C pin mapping:

| 文件 / File | 旧版默认 / Current default | V2 示例值 / V2 sample | 影响 / Impact |
| --- | --- | --- | --- |
| `src/battery_manager.h` | `I2C_SDA_PIN 3` | `I2C_SDA_PIN 15` | MAX17048 电池电量读取 / MAX17048 battery reading |
| `src/battery_manager.h` | `I2C_SCL_PIN 2` | `I2C_SCL_PIN 16` | MAX17048 电池电量读取 / MAX17048 battery reading |
| `src/sim7670g_manager.h` | `I2C_SDA_PIN 3` | `I2C_SDA_PIN 15` | 与全局引脚表保持一致 / Keep the global pin table consistent |
| `src/sim7670g_manager.h` | `I2C_SCL_PIN 2` | `I2C_SCL_PIN 16` | 与全局引脚表保持一致 / Keep the global pin table consistent |

如果使用 V2 模块且电池电量始终读不到、`MAX17048未检测到`，请先把上述两个文件中的 I2C 引脚同步改为：

If you use a V2 module and the battery level cannot be read, or the log shows `MAX17048未检测到`, first change the I2C pins in both files to:

```cpp
#define I2C_SDA_PIN 15
#define I2C_SCL_PIN 16
```

建议后续把硬件版本做成统一的编译开关，避免两个头文件重复维护 I2C 宏。

A future improvement is to add a single compile-time hardware revision switch so the I2C macros are not duplicated in two headers.

## V2 示例中确认仍一致的核心引脚 / Pins That Stay The Same In V2 Samples

这些引脚与当前固件默认值一致，通常不需要为 V2 修改：

These pins match the current firmware defaults and usually do not need V2-specific changes:

| 功能 / Function | 当前固件 / Current firmware | V2 示例 / V2 sample | 来源 / Source |
| --- | --- | --- | --- |
| SIM7670G UART RX/TX | `17 / 18` | `17 / 18` | `v2_change/GNSS-With-WaveshareCloud/GNSS-With-WaveshareCloud.ino` |
| WS2812 RGB LED | `38` | `38` | `v2_change/RGB/RGB.ino` |
| SDMMC CLK/CMD/DATA | `5 / 4 / 6` | `5 / 4 / 6` | `v2_change/SD/SD.ino` |

## 可选外设参考 / Optional Peripheral Reference

本项目当前未启用 LCD、摄像头和 AP 示例里的普通 LED，但如果后续合并 Waveshare V2 示例，请注意这些 V2 引脚：

This firmware currently does not enable the LCD, camera, or the normal LED from the AP example. If you later merge Waveshare V2 examples, check these V2 pins:

| 外设 / Peripheral | V2 示例引脚 / V2 sample pins | 来源 / Source |
| --- | --- | --- |
| LCD ST7789 | `SCLK=39`, `MOSI=41`, `CS=45`, `DC=42`, `RST=1`, `BL=21` | `v2_change/LCD/LCD.ino` |
| AP 示例 LED | `LED_BUILTIN=2` | `v2_change/AP/AP.ino` |
| Camera OV5640/WAVESHARE_7670_BOARD | `XCLK=39`, `SIOD=15`, `SIOC=16`, `Y9=14`, `Y8=13`, `Y7=12`, `Y6=11`, `Y5=10`, `Y4=9`, `Y3=8`, `Y2=7`, `VSYNC=42`, `HREF=41`, `PCLK=46` | `v2_change/CameraWebServer/camera_pins.h` |

## 验证建议 / Verification

1. 修改 I2C 引脚后重新编译并烧录。/ Rebuild and flash the firmware after changing the I2C pins.
2. 开机日志应出现 MAX17048 初始化成功；如果仍失败，确认模块版本、排线/电池状态和是否使用了正确的 V2 示例引脚。/ The boot log should show successful MAX17048 initialization. If it still fails, verify the board revision, cable/battery state, and whether the correct V2 sample pins are used.
3. SMS 收发核心路径依赖 SIM7670G UART `17/18`，V2 示例中该引脚未变；如果 AT 无响应，优先检查拨码开关、电源和串口接线/宏定义。/ SMS send/receive depends on SIM7670G UART `17/18`, which is unchanged in the V2 samples. If AT commands do not respond, first check DIP switches, power, wiring, and UART macros.
