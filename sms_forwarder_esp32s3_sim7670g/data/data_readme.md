# 配置示例文件 / Sample Config Files

本目录包含配置文件的示例，仅供参考。实际运行时，系统会在SPIFFS中自动生成配置文件。  
This directory contains sample config files for reference only. At runtime, the system will generate config files in SPIFFS automatically.

## 文件说明 / Files

### config.json
系统配置示例，包含：  
System config sample, includes:
- WiFi连接配置 / WiFi connection
- 推送平台配置（Bark、Server酱、Telegram等）/ Notification channels (Bark, ServerChan, Telegram, etc.)
- 电池管理配置 / Battery settings
- 网络配置 / Network settings
- 短信过滤配置 / SMS filters
- 系统设置 / System settings

### sms.json
短信存储格式示例，包含：  
SMS storage sample, includes:
- 短信记录结构 / SMS record structure
- ID管理 / ID management
- 转发状态 / Forward status

## 注意事项 / Notes

- 这些文件**不会**被上传到ESP32设备  
  These files are **not** uploaded to the ESP32.
- 系统启动时会自动在SPIFFS中创建配置文件  
  Config files are created automatically in SPIFFS on boot.
- 如果需要预设配置，请修改代码中的默认值  
  If you need presets, change the default values in code.
- 配置文件存储在ESP32的SPIFFS文件系统中，路径为 `/config.json` 和 `/sms.json`  
  Config files are stored in SPIFFS at `/config.json` and `/sms.json`.
