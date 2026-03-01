# 国际化（i18n）维护指南 / i18n Maintenance Guide

本文档说明项目里的国际化结构、维护流程与联调方法，适用于 UI 文案、后端日志文案与 API 错误信息。

This guide describes i18n architecture, update workflow, and validation steps for UI text, backend logs, and API messages.

## 1. 架构概览 / Architecture

### 前端 UI
- 文件: `sms_forwarder_esp32s3_sim7670g/src/web_pages_full.h`
- 方式: 页面内 `I18N` 对象（`zh` / `en`）
- 切换: 顶栏按钮调用语言切换并保存到 `/api/config/lang`

### 后端日志与消息
- 文件: `sms_forwarder_esp32s3_sim7670g/src/i18n.cpp`, `sms_forwarder_esp32s3_sim7670g/src/i18n.h`
- 方式: `I18N_TABLE` 按 `key -> zh/en` 映射
- 调用:
  - `i18nGet("key")`
  - `i18nFormat("key", ...)`
  - `LOGI/LOGW/LOGE` 等宏内部使用 i18n key

## 2. 关键行为 / Key Behaviors

1. `config.lang` 支持 `zh` / `en` / `auto`。
2. 后端在 `auto` 下默认按 `zh` 输出（见 `getCurrentLangCode()`）。
3. Web 首次加载会按浏览器语言决定 UI 语言，并持久化到配置。
4. 日志是“生成时翻译”，不是“展示时翻译”。

## 3. 新增或修改 i18n 文案 / Adding or Updating i18n Text

### 后端（日志/API）
1. 在 `src/i18n.cpp` 的 `I18N_TABLE` 增加或修改 key。
2. 保持 `zh/en` 两列都完整，不留空字符串。
3. 若使用占位符，`zh/en` 的参数数量和顺序必须一致。
4. 在代码中使用 `LOGI("TAG", "your_key", ...)` 或 `i18nFormat("your_key", ...)`。

### 前端（页面文本）
1. 在 `web_pages_full.h` 的 `I18N.zh` 与 `I18N.en` 同时新增 key。
2. 页面静态文案用 `data-i18n` / `data-i18n-placeholder`。
3. 动态文案通过 `t('key')` 或 `tFmt('key', args...)`。

## 4. 占位符规范 / Placeholder Rules

### 后端 C 风格
- 使用 `%s`（和已有实现保持一致）。
- 示例:
  - key: `wifi_connected_ip`
  - zh: `WiFi已连接, IP: %s`
  - en: `WiFi connected, IP: %s`

### 前端模板风格
- 使用 `{0}`, `{1}`。
- 示例:
  - key: `logs_show_recent`
  - zh: `仅显示最近 {0} 条（总计 {1} 条）`
  - en: `Showing latest {0} entries (total {1})`

## 5. 联调检查清单 / Validation Checklist

1. 切换语言后，Dashboard / Config / SMS / Logs / Debug 页面文案是否都更新。
2. API 错误返回（如短信发送失败、参数缺失）是否按当前语言输出。
3. 关键日志（WiFi、SMS、Network）是否显示正确语言与参数。
4. 长文本是否被截断（当前日志单条消息会被限制长度以控制内存）。

## 6. 与近期修复相关的 key / Keys Related to Recent Fixes

近期短信解码、Web 响应、DNS 强制应用修复相关日志 key（示例）：
- `sms_decode_fallback_ucs2`
- `wifi_force_dns_setdns`
- `wifi_force_dns_netif`
- `wifi_dns_after_force`
- `wifi_dns_after_reconnect`

> 如果新增了上面的 key，请同步更新中英文并完成一次实际链路验证。

## 7. 常见错误 / Common Pitfalls

1. 只改了 `zh` 没改 `en`（或反过来）。
2. 占位符数量不一致，导致日志参数错位。
3. 前端 key 与后端 key 同名但语义不同，造成维护混乱。
4. 新增 key 后未在 UI 中绑定 `data-i18n`，页面仍显示硬编码文本。

