# SMS 转发器 - ESP32-S3 + SIM7670G

[English](README.md) | 中文

> 重要硬件提示：本固件默认使用 V1 引脚定义。2026 年购买或收到的 ESP32-S3-SIM7670G-4G 模组，请在烧录前先检查是否为 V2 版本；如果是 V2 硬件，需要先按 [硬件 V2 引脚说明](sms_forwarder_esp32s3_sim7670g/docs/hardware_v2_pin_changes.md) 替换受影响的引脚，尤其是 MAX17048 电池监控使用的 I2C 引脚。

这是面向 Waveshare ESP32-S3-SIM7670G-4G 模组的固件。设备通过 SIM7670G 接收短信，将最近短信保存在本地，并通过内置 Web 控制台配置 WiFi 推送渠道进行转发。

## 概览

- 以 PDU 模式接收短信，支持 GSM 7-bit、UCS2 和 8-bit 文本解码。
- 长短信分片收齐后再存储和转发。
- 支持 Bark、Server 酱、钉钉、Telegram、飞书和自定义 Webhook 推送。
- 通过 Web UI 管理 WiFi、推送渠道、短信过滤、日志和诊断工具。
- 在电池监控电路可用时，通过 MAX17048 读取电池状态。
- Web UI 和运行日志支持中文/英文双语。

## 文档入口

| 主题 | 链接 |
| --- | --- |
| Waveshare 2026/V2 硬件引脚变更 | [硬件 V2 引脚说明](sms_forwarder_esp32s3_sim7670g/docs/hardware_v2_pin_changes.md) |
| PDU 解码测试覆盖 | [PDU 解码测试](sms_forwarder_esp32s3_sim7670g/docs/pdu_decode_tests.md) |
| 运营商 MCC/MNC 表 | [运营商表维护](sms_forwarder_esp32s3_sim7670g/docs/operator_readme.md) |
| UI/日志翻译维护 | [i18n 维护指南](sms_forwarder_esp32s3_sim7670g/docs/i18n_readme.md) |
| 投递语义、凭据、TLS 和恢复 | [可靠性与安全说明](sms_forwarder_esp32s3_sim7670g/docs/reliability_security.md) |
| 可复现的主机、浏览器与固件检查 | [测试指南](tests/README.md) |

## 硬件要求

- Waveshare ESP32-S3-SIM7670G-4G 模组
- 支持 4G 的 Nano-SIM 卡
- LTE 天线，建议连接以获得更稳定的信号
- 18650 电池，可选

重要：Waveshare FAQ 提示，如为 2026 年元旦后收到的模块，请使用示例程序 V2。V2 硬件的部分外设引脚有变化，尤其是 MAX17048 电池监控使用的 I2C 引脚。为新模块编译前，请先查看 [硬件 V2 引脚说明](sms_forwarder_esp32s3_sim7670g/docs/hardware_v2_pin_changes.md)。

## 编译与烧录

1. 使用 Arduino IDE 2.x 打开 [sms_forwarder_esp32s3_sim7670g/sms_forwarder_esp32s3_sim7670g.ino](sms_forwarder_esp32s3_sim7670g/sms_forwarder_esp32s3_sim7670g.ino)。
2. 安装 ESP32 core `3.3.0`、ArduinoJson `6.21.5` 和 Adafruit NeoPixel `1.12.5`。这些是已验证版本，ArduinoJson 7 不能直接替换。
3. 选择开发板 `ESP32S3 Dev Module`。
4. 使用 16 MB Flash、OPI PSRAM、Hardware CDC/JTAG、启用 USB CDC On Boot，并使用工程自带的自定义分区表；烧录前核对实际硬件版本。
5. 通过 USB 编译并上传固件。

## 首次启动

1. 打开 115200 波特率的 USB 串口监视器并重启设备。设备首次启动会生成不同的 Web/AP 随机密码，并保存到 NVS。
2. 连接 WiFi 热点 `SMS-Forwarder-Setup`，使用串口打印的 AP 密码。
3. 在浏览器打开 `http://192.168.4.1`。
4. 使用用户名 `admin` 和串口打印的 `[SETUP] Web password` 登录，不再使用公开的默认密码。
5. 配置本地 WiFi 和至少一个推送渠道。
6. 重启设备，或等待设备使用保存的配置重新连接。

升级会保留已有的非默认 Web 密码；空密码或旧的 `admin1234` 会迁移为设备密码。忘记密码时，通过物理 USB 串口发送 `RESET WEB AUTH` 并换行，即可恢复 Web 登录，不会清除 WiFi 设置或短信历史。管理页面仍是 HTTP Basic 鉴权而非 HTTPS，请仅在可信局域网中使用。

## Web 控制台

- Dashboard：电池、SIM 注册、信号、内存和设备状态。
- Config：WiFi、推送渠道、电池告警、网络选项、短信过滤和系统设置。
- SMS：短信列表、手动转发、删除、发送和统计。
- Logs：查看近期运行日志并清理日志。
- Debug：AT 指令测试、WiFi/网络诊断和 LED 测试。

## 使用说明

- 短信收发由 SIM7670G 调制解调器完成，通知推送通过 ESP32 WiFi 发送。
- 部分网络配置 AT 指令在特定 SIM 卡、运营商、漫游状态或模组固件下可能返回 `ERROR` 或 `+CME ERROR`。固件会按需重试并跳过这些指令；这通常不影响短信转发主流程。
- 使用 `networkConnected` 判断蜂窝网络注册状态，使用 `dataAttached` 判断蜂窝数据附着状态。
- 自定义 DNS 可在 Web UI 中配置；只有需要同时固定 IP 和 DNS 时才需要启用静态 IP。
- 短信记录和日志保存在本地，并设置了保留上限以保护 Flash 和内存。
- SIM 短信只有在本地持久化成功后才删除。待投递记录不会被历史清理淘汰；待处理存储满时，新短信保留在 SIM 上等待后续扫描。
- 至少一个已启用渠道确认成功即视为转发成功。重试和重启恢复属于“至少一次”，不保证恰好一次。通知测试改为后台执行，覆盖全部六个渠道。
- 发送短信限制为单段 UCS2，即最多 70 个 UTF-16 代码单元；一个代理对 emoji 占两个。暂不支持发送分段长短信。
- 网络设置保存后需要重新初始化 SIM 或重启。PDP PAP 认证按 SIM767XX 手册的“密码在前、用户名在后”顺序发送；WiFi 推送不依赖蜂窝数据。
- HTTPS 会验证证书和主机名，失败时不会退回不安全模式。私有 CA 配置和系统时间要求见安全说明。配置接口不再返回已保存凭据，表单中可选择保持、替换或清除。

## 项目结构

```text
.
├── README.md
├── README.zh.md
├── sms_forwarder_esp32s3_sim7670g/
│   ├── sms_forwarder_esp32s3_sim7670g.ino
│   ├── data/
│   ├── docs/
│   └── src/
└── tests/
```

## 测试

在仓库根目录运行轻量基线测试：

```bash
node tests/pdu_decode.test.js
g++ -std=c++11 -Wall -Wextra -pedantic tests/millis_utils.test.cpp -o /tmp/millis_utils_test
/tmp/millis_utils_test
```

[测试指南](tests/README.md) 还包含真实 C++ 接收、持久化、重试与 AT 响应代码的故障注入，以及浏览器检查和固定版本的 ESP32-S3 构建。主机测试或编译通过不代表已经完成真机短信、电池、TLS 和断电测试。