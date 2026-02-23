#ifndef WEB_PAGES_FULL_H
#define WEB_PAGES_FULL_H

// Use PROGMEM to store HTML in Flash / 使用PROGMEM存储HTML页面到Flash
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>SMS Forwarder</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; }
        .nav { background: #007bff; padding: 15px; border-radius: 8px; margin-bottom: 20px; display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
        .nav-btn { background: rgba(255,255,255,0.2); color: white; border: none; padding: 10px 20px; margin: 0 5px; border-radius: 5px; cursor: pointer; }
        .nav-btn.active { background: rgba(255,255,255,0.3); }
        .nav-spacer { flex: 1; }
        .lang-toggle { display: flex; gap: 6px; }
        .lang-btn { background: rgba(255,255,255,0.2); color: white; border: none; padding: 6px 10px; border-radius: 5px; cursor: pointer; }
        .lang-btn.active { background: rgba(255,255,255,0.4); }
        .card { background: white; padding: 20px; margin: 10px 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
        .status-item { background: #f8f9fa; padding: 15px; border-radius: 5px; text-align: center; }
        .status-value { font-size: 24px; font-weight: bold; color: #007bff; }
        .form-group { margin: 15px 0; }
        .form-group label { display: block; margin-bottom: 5px; font-weight: bold; }
        .form-group input, .form-group select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; }
        .btn { background: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; margin: 5px; }
        .btn:hover { background: #0056b3; }
        .btn-danger { background: #dc3545; }
        .btn-success { background: #28a745; }
        .logs { background: #f8f9fa; padding: 15px; border-radius: 5px; max-height: 300px; overflow-y: auto; font-family: monospace; font-size: 12px; }
        .log-entry { margin: 2px 0; padding: 2px 5px; border-radius: 3px; }
        .log-debug { background: #e9ecef; }
        .log-info { background: #d1ecf1; }
        .log-warn { background: #fff3cd; }
        .log-error { background: #f8d7da; }
        .hidden { display: none; }
    </style>
</head>
<body>
    <div class="container">
        <div class="nav">
            <button class="nav-btn active" onclick="showPage('dashboard')" data-i18n="nav_dashboard">仪表板</button>
            <button class="nav-btn" onclick="showPage('config')" data-i18n="nav_config">配置</button>
            <button class="nav-btn" onclick="showPage('sms')" data-i18n="nav_sms">短信</button>
            <button class="nav-btn" onclick="showPage('logs')" data-i18n="nav_logs">日志</button>
            <button class="nav-btn" onclick="showPage('debug')" data-i18n="nav_debug">调试</button>
            <div class="nav-spacer"></div>
            <div class="lang-toggle">
                <button class="lang-btn" id="lang-zh" onclick="switchLang('zh')" data-i18n="lang_zh">中文</button>
                <button class="lang-btn" id="lang-en" onclick="switchLang('en')" data-i18n="lang_en">EN</button>
            </div>
        </div>

        <!-- Dashboard / 仪表板页面 -->
        <div id="dashboard" class="page">
            <div class="card">
                <h2 data-i18n="dashboard_title">系统状态</h2>
                <div class="status-grid" id="statusGrid">
                    <div class="status-item">
                        <div class="status-value" id="batteryLevel">--</div>
                        <div data-i18n="status_battery_level">电池电量</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="batteryVoltage">--</div>
                        <div data-i18n="status_battery_voltage">电池电压</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="chargingStatus">--</div>
                        <div data-i18n="status_charging_status">充电状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="signalStrength">--</div>
                        <div data-i18n="status_signal_strength">信号强度</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="networkOperator">--</div>
                        <div data-i18n="status_operator">当前运营商</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="homeOperator">--</div>
                        <div data-i18n="status_home_operator">源运营商</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="networkType">--</div>
                        <div data-i18n="status_network_type">网络类型</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="simStatusText">--</div>
                        <div data-i18n="status_sim_status">SIM 状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="roamingStatus">--</div>
                        <div data-i18n="status_roaming_status">漫游状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="regStatus">--</div>
                        <div data-i18n="status_reg_status">注册状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="freeMemory">--</div>
                        <div data-i18n="status_free_memory">可用内存</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="totalMemory">--</div>
                        <div data-i18n="status_total_memory">总内存</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="cpuFreq">--</div>
                        <div data-i18n="status_cpu_freq">CPU 频率</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="flashSize">--</div>
                        <div data-i18n="status_flash_size">Flash 大小</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="smsReceived">--</div>
                        <div data-i18n="status_sms_received">接收短信</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="smsForwarded">--</div>
                        <div data-i18n="status_sms_forwarded">转发短信</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="uptime">--</div>
                        <div data-i18n="status_uptime">运行时间</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="timeNow">--</div>
                        <div data-i18n="status_time_now">当前时间</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="timeSource">--</div>
                        <div data-i18n="status_time_source">对时来源</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="smsStatus">--</div>
                        <div data-i18n="status_sms_network">短信网络</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="dataStatus">--</div>
                        <div data-i18n="status_data_connection">数据连接</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="wifiStatus">--</div>
                        <div data-i18n="status_wifi_status">WiFi 状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="wifiIp">--</div>
                        <div data-i18n="status_wifi_ip">WiFi IP</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="wifiRssi">--</div>
                        <div data-i18n="status_wifi_rssi">WiFi RSSI</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="ledStatus">--</div>
                        <div data-i18n="status_led_status">LED 状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="ledReason">--</div>
                        <div data-i18n="status_led_reason">LED 原因</div>
                    </div>

                </div>
            </div>
        </div>

        <!-- Config / 配置页面 -->
        <div id="config" class="page hidden">
            <div class="card">
                <h2 data-i18n="wifi_config_title">WiFi配置</h2>
                <form id="wifiForm" onsubmit="saveWiFiConfig(); return false;">
                    <div class="form-group">
                        <label data-i18n="wifi_ssid_label">WiFi名称:</label>
                        <input type="text" id="wifi-ssid" name="ssid" required>
                    </div>
                    <div class="form-group">
                        <label data-i18n="wifi_password_label">WiFi密码:</label>
                        <input type="password" id="wifi-password" name="password">
                    </div>
                    <div class="form-group">
                        <label data-i18n="wifi_dns_current_label">当前DNS:</label>
                        <input type="text" id="wifi-dns-current" readonly>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="wifi-use-custom-dns" name="useCustomDns"> <span data-i18n="wifi_use_custom_dns">使用自定义DNS</span></label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="wifi-force-static-dns" name="forceStaticDns"> <span data-i18n="wifi_static_ip_enable">使用静态IP(停用DHCP)</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="wifi_static_ip_label">静态IP:</label>
                        <input type="text" id="wifi-static-ip" name="staticIp" placeholder="如 192.168.1.171" data-i18n-placeholder="wifi_static_ip_placeholder">
                    </div>
                    <div class="form-group">
                        <label data-i18n="wifi_static_gateway_label">网关:</label>
                        <input type="text" id="wifi-static-gateway" name="staticGateway" placeholder="如 192.168.1.1" data-i18n-placeholder="wifi_static_gateway_placeholder">
                    </div>
                    <div class="form-group">
                        <label data-i18n="wifi_static_subnet_label">子网掩码:</label>
                        <input type="text" id="wifi-static-subnet" name="staticSubnet" placeholder="如 255.255.255.0" data-i18n-placeholder="wifi_static_subnet_placeholder">
                    </div>
                    <div class="form-group">
                        <label data-i18n="wifi_dns1_label">DNS 1:</label>
                        <input type="text" id="wifi-dns1" name="dns1" placeholder="如 1.1.1.1" data-i18n-placeholder="wifi_dns1_placeholder">
                    </div>
                    <div class="form-group">
                        <label data-i18n="wifi_dns2_label">DNS 2:</label>
                        <input type="text" id="wifi-dns2" name="dns2" placeholder="可选，如 8.8.8.8" data-i18n-placeholder="wifi_dns2_placeholder">
                    </div>
                    <button type="submit" class="btn" data-i18n="wifi_save_btn">保存WiFi配置</button>
                </form>
            </div>
            
            <div class="card">
                <h2 data-i18n="notification_config_title">推送配置</h2>
                <form id="notificationForm" onsubmit="saveNotificationConfig(); return false;">
                    <h3 data-i18n="bark_title">Bark推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="bark-enabled" name="bark-enabled"> <span data-i18n="bark_enable">启用Bark推送</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="bark_key_label">Bark密钥:</label>
                        <input type="text" id="bark-key" name="barkKey">
                    </div>
                    <div class="form-group">
                        <label data-i18n="bark_url_label">Bark服务器:</label>
                        <input type="text" id="bark-url" name="barkUrl" value="https://api.day.app">
                    </div>
                    
                    <h3 data-i18n="serverchan_title">Server酱推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="serverchan-enabled" name="serverchan-enabled"> <span data-i18n="serverchan_enable">启用Server酱推送</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="serverchan_key_label">Server酱密钥:</label>
                        <input type="text" id="serverchan-key" name="serverChanKey">
                    </div>
                    <div class="form-group">
                        <label data-i18n="serverchan_url_label">Server酱服务器:</label>
                        <input type="text" id="serverchan-url" name="serverChanUrl" value="https://sctapi.ftqq.com">
                    </div>
                    
                    <h3 data-i18n="telegram_title">Telegram推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="telegram-enabled" name="telegram-enabled"> <span data-i18n="telegram_enable">启用Telegram推送</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="telegram_token_label">Bot Token:</label>
                        <input type="text" id="telegram-token" name="telegramToken">
                    </div>
                    <div class="form-group">
                        <label data-i18n="telegram_chatid_label">Chat ID:</label>
                        <input type="text" id="telegram-chatid" name="telegramChatId">
                    </div>
                    <div class="form-group">
                        <label data-i18n="telegram_url_label">Telegram服务器:</label>
                        <input type="text" id="telegram-url" name="telegramUrl" value="https://api.telegram.org">
                    </div>
                    
                    <h3 data-i18n="dingtalk_title">钉钉推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="dingtalk-enabled" name="dingtalk-enabled"> <span data-i18n="dingtalk_enable">启用钉钉推送</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="dingtalk_webhook_label">钉钉Webhook:</label>
                        <input type="text" id="dingtalk-webhook" name="dingtalkWebhook">
                    </div>
                    
                    <h3 data-i18n="feishu_title">飞书推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="feishu-enabled" name="feishu-enabled"> <span data-i18n="feishu_enable">启用飞书推送</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="feishu_webhook_label">飞书Webhook:</label>
                        <input type="text" id="feishu-webhook" name="feishuWebhook">
                    </div>
                    
                    <h3 data-i18n="custom_title">自定义推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="custom-enabled" name="custom-enabled"> <span data-i18n="custom_enable">启用自定义推送</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="custom_url_label">自定义URL:</label>
                        <input type="text" id="custom-url" name="customUrl">
                    </div>
                    <div class="form-group">
                        <label data-i18n="custom_key_label">自定义密钥:</label>
                        <input type="text" id="custom-key" name="customKey">
                    </div>
                    
                    <div style="margin-top: 20px;">
                        <button type="submit" class="btn" data-i18n="notification_save_btn">保存推送配置</button>
                        <button type="button" class="btn btn-success" onclick="testAllNotifications()" data-i18n="notification_test_all_btn">测试所有推送</button>
                    </div>
                </form>
            </div>
            
            <div class="card">
                <h2 data-i18n="battery_config_title">电池管理配置</h2>
                <form id="batteryForm" onsubmit="saveBatteryConfig(); return false;">
                    <div class="form-group">
                        <label data-i18n="battery_low_threshold_label">低电量阈值 (%):</label>
                        <input type="number" id="battery-low-threshold" name="lowThreshold" min="5" max="50" value="15">
                    </div>
                    <div class="form-group">
                        <label data-i18n="battery_critical_threshold_label">极低电量阈值 (%):</label>
                        <input type="number" id="battery-critical-threshold" name="criticalThreshold" min="1" max="20" value="5">
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="battery-alert-enabled" name="battery-alert-enabled"> <span data-i18n="battery_alert_enable">启用电池警告</span></label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="low-battery-alert-enabled" name="low-battery-alert-enabled"> <span data-i18n="battery_low_alert_enable">低电量警告</span></label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="charging-alert-enabled" name="charging-alert-enabled"> <span data-i18n="battery_charging_alert_enable">充电状态通知</span></label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="full-charge-alert-enabled" name="full-charge-alert-enabled"> <span data-i18n="battery_full_alert_enable">满电通知</span></label>
                    </div>
                    <button type="submit" class="btn" data-i18n="battery_save_btn">保存电池配置</button>
                </form>
            </div>
            
            <div class="card">
                <h2 data-i18n="network_config_title">网络管理配置</h2>
                <form id="networkForm" onsubmit="saveNetworkConfig(); return false;">
                    <div class="form-group">
                        <label><input type="checkbox" id="roaming-alert-enabled" name="roaming-alert-enabled"> <span data-i18n="roaming_alert_enable">漫游警告</span></label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="auto-disable-data-roaming" name="auto-disable-data-roaming"> <span data-i18n="roaming_disable_data">漫游时自动关闭数据</span></label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="allow-sms-data-roaming" name="allow-sms-data-roaming"> <span data-i18n="roaming_allow_sms_data">漫游时允许短信所需数据</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="data_policy_label">移动数据策略:</label>
                        <select id="data-policy" name="dataPolicy">
                            <option value="0" data-i18n="data_policy_off">仅短信（禁用移动数据）</option>
                            <option value="1" data-i18n="data_policy_roaming_only">仅非漫游启用数据</option>
                            <option value="2" data-i18n="data_policy_on">始终启用数据</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label data-i18n="signal_interval_label">信号检查间隔 (秒):</label>
                        <input type="number" id="signal-check-interval" name="signalCheckInterval" min="10" max="300" value="30">
                    </div>
                    <div class="form-group">
                        <label data-i18n="operator_mode_label">运营商选择:</label>
                        <select id="operator-mode" name="operatorMode">
                            <option value="0" data-i18n="operator_auto">自动选网</option>
                            <option value="1" data-i18n="operator_cmcc">中国移动(46000)</option>
                            <option value="2" data-i18n="operator_cu">中国联通(46001)</option>
                            <option value="3" data-i18n="operator_ct">中国电信(46003)</option>
                            <option value="4" data-i18n="operator_giffgaff">英国 giffgaff(APN giffgaff.com)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label data-i18n="radio_mode_label">网络制式:</label>
                        <select id="radio-mode" name="radioMode">
                            <option value="2" data-i18n="radio_mode_auto">自动 (2G/3G/4G)</option>
                            <option value="38" data-i18n="radio_mode_lte_only">仅 4G (LTE)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label data-i18n="apn_label">APN设置:</label>
                        <input type="text" id="apn" name="apn" placeholder="留空自动" data-i18n-placeholder="apn_placeholder">
                    </div>
                    <div class="form-group">
                        <label data-i18n="apn_user_label">APN用户名 (可选):</label>
                        <input type="text" id="apn-user" name="apnUser" placeholder="留空表示无需认证" data-i18n-placeholder="apn_user_placeholder">
                    </div>
                    <div class="form-group">
                        <label data-i18n="apn_pass_label">APN密码 (可选):</label>
                        <input type="password" id="apn-pass" name="apnPass" placeholder="留空表示无需认证" data-i18n-placeholder="apn_pass_placeholder">
                    </div>
                    <button type="submit" class="btn" data-i18n="network_save_btn">保存网络配置</button>
                </form>
            </div>
            
            <div class="card">
                <h2 data-i18n="sms_filter_title">短信过滤配置</h2>
                <form id="smsFilterForm" onsubmit="saveSMSFilterConfig(); return false;">
                    <div class="form-group">
                        <label><input type="checkbox" id="whitelist-enabled" name="whitelist-enabled"> <span data-i18n="whitelist_enable">启用白名单过滤</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="whitelist_label">白名单 (一行一个号码):</label>
                        <textarea id="whitelist" name="whitelist" rows="3" placeholder="输入允许的号码，一行一个" data-i18n-placeholder="whitelist_placeholder"></textarea>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="keyword-filter-enabled" name="keyword-filter-enabled"> <span data-i18n="keyword_enable">启用关键词过滤</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="keyword_label">屏蔽关键词 (一行一个):</label>
                        <textarea id="blocked-keywords" name="blockedKeywords" rows="3" placeholder="输入需要屏蔽的关键词" data-i18n-placeholder="keyword_placeholder"></textarea>
                    </div>
                    <button type="submit" class="btn" data-i18n="sms_filter_save_btn">保存过滤配置</button>
                </form>
            </div>
            
            <div class="card">
                <h2 data-i18n="system_config_title">系统设置</h2>
                <form id="systemForm" onsubmit="saveSystemConfig(); return false;">
                    <div class="form-group">
                        <label><input type="checkbox" id="daily-report-enabled" name="daily-report-enabled"> <span data-i18n="daily_report_enable">启用日报</span></label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="weekly-report-enabled" name="weekly-report-enabled"> <span data-i18n="weekly_report_enable">启用周报</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="report_hour_label">报告时间 (24小时制):</label>
                        <input type="number" id="report-hour" name="reportHour" min="0" max="23" value="9">
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="sleep-enabled" name="sleep-enabled"> <span data-i18n="sleep_enable">启用休眠</span></label>
                    </div>
                    <div class="form-group">
                        <label data-i18n="sleep_timeout_label">休眠超时 (秒):</label>
                        <input type="number" id="sleep-timeout" name="sleep-timeout" min="60" max="86400" value="1800">
                    </div>
                    <div class="form-group">
                        <label data-i18n="sleep_mode_label">休眠模式:</label>
                        <select id="sleep-mode" name="sleep-mode">
                            <option value="0" data-i18n="sleep_mode_light">浅睡眠</option>
                            <option value="1" data-i18n="sleep_mode_deep">深度睡眠</option>
                        </select>
                        <div style="font-size: 12px; color: #666; margin-top: 6px;" data-i18n="sleep_mode_help">浅睡眠保留更多状态，唤醒快但更耗电；深度睡眠省电但唤醒更慢。</div>
                    </div>
                    <div class="form-group">
                        <label data-i18n="wdt_timeout_label">看门狗超时 (秒):</label>
                        <input type="number" id="wdt-timeout" name="wdt-timeout" min="10" max="300" value="60">
                    </div>
                    <button type="submit" class="btn" data-i18n="system_save_btn">保存系统配置</button>
                </form>
            </div>
        </div>

        <!-- SMS / 短信管理页面 -->
        <div id="sms" class="page hidden">
            <div class="card">
                <h2 data-i18n="sms_send_title">发送短信</h2>
                <form id="sendSMSForm" onsubmit="sendSMS(); return false;">
                    <div class="form-group">
                        <label data-i18n="sms_phone_label">手机号码:</label>
                        <input type="tel" id="phoneNumber" placeholder="请输入手机号" data-i18n-placeholder="sms_phone_placeholder" required>
                    </div>
                    <div class="form-group">
                        <label data-i18n="sms_message_label">短信内容:</label>
                        <textarea id="smsMessage" rows="3" placeholder="请输入短信内容" data-i18n-placeholder="sms_message_placeholder" maxlength="160" required></textarea>
                    </div>
                    <button type="submit" class="btn" data-i18n="sms_send_btn">发送短信</button>
                </form>
            </div>
            
            <div class="card">
                <h2 data-i18n="sms_manage_title">短信管理</h2>
                <div style="margin-bottom: 15px;">
                    <button class="btn" onclick="refreshSMS()" data-i18n="sms_refresh_btn">刷新短信</button>
                    <button class="btn btn-success" onclick="checkSMS()" data-i18n="sms_manual_check_btn">手动查询短信</button>
                    <button class="btn btn-danger" onclick="clearAllSMS()" data-i18n="sms_clear_btn">清空短信</button>
                </div>
                <div id="smsContainer" data-i18n="sms_loading">加载中...</div>
            </div>
        </div>

        <!-- Logs / 日志页面 -->
        <div id="logs" class="page hidden">
            <div class="card">
                <h2 data-i18n="logs_title">系统日志</h2>
                <div style="margin-bottom: 10px;">
                    <button class="btn" onclick="refreshLogs()" data-i18n="logs_refresh_btn">刷新日志</button>
                    <button class="btn btn-danger" onclick="clearLogs()" data-i18n="logs_clear_btn">清空日志</button>
                </div>
                <div id="logsContainer" class="logs" data-i18n="logs_loading">加载中...</div>
            </div>
        </div>

        <!-- Debug / 调试页面 -->
        <div id="debug" class="page hidden">
            <div class="card">
                <h2 data-i18n="debug_title">系统调试</h2>
                <button class="btn btn-danger" onclick="restartSystem()" data-i18n="restart_btn">重启系统</button>
                <button class="btn" onclick="testNotification()" data-i18n="test_push_btn">测试推送</button>
                <button class="btn" onclick="diagnoseWiFi()" data-i18n="diag_wifi_btn">诊断WiFi</button>
                <button class="btn" onclick="diagnoseNetwork()" data-i18n="diag_net_btn">网络诊断</button>
                <button class="btn" onclick="checkSystem()" data-i18n="system_check_btn">系统检查</button>
                <button class="btn" onclick="syncTimeNow()" data-i18n="time_sync_btn">手动对时</button>
                <button class="btn btn-success" onclick="checkSMS()" data-i18n="sms_manual_check_btn">手动查询短信</button>
                <button class="btn" onclick="testLEDHardware()" data-i18n="led_test_hw_btn">LED硬件测试</button>
                <button class="btn" onclick="testLEDStates()" data-i18n="led_test_states_btn">LED状态测试</button>
                <div style="margin-top: 10px;">
                    <form id="netDiagForm" onsubmit="diagnoseNetwork(); return false;">
                        <div class="form-group">
                            <label data-i18n="netdiag_url_label">诊断URL:</label>
                            <input type="text" id="netDiagUrl" name="url" placeholder="留空使用 Bark 服务器或 https://api.day.app" data-i18n-placeholder="netdiag_url_placeholder">
                        </div>
                        <div class="form-group">
                            <label data-i18n="netdiag_method_label">诊断方法:</label>
                            <select id="netDiagMethod" name="method">
                                <option value="GET" selected>GET</option>
                                <option value="POST">POST</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label data-i18n="netdiag_payload_label">POST内容(可选):</label>
                            <input type="text" id="netDiagPayload" name="payload" placeholder="仅在POST时使用" data-i18n-placeholder="netdiag_payload_placeholder">
                        </div>
                    </form>
                </div>
                <div style="margin-top: 15px;">
                    <label style="display: flex; align-items: center; gap: 10px;">
                        <input type="checkbox" id="atCommandEcho" onchange="toggleATEcho()">
                        <span data-i18n="at_echo_label">AT指令回显调试</span>
                    </label>
                </div>
            </div>
            

            
            <div class="card">
                <h2 data-i18n="at_test_title">AT指令测试</h2>
                <div class="form-group">
                    <input type="text" id="atCommand" placeholder="输入AT指令" data-i18n-placeholder="at_command_placeholder" value="AT">
                    <button class="btn" onclick="sendATCommand()" data-i18n="at_send_btn">发送</button>
                </div>
                <div id="atResponse" class="logs" style="max-height: 150px;" data-i18n="at_waiting">等待指令...</div>
            </div>
        </div>
    </div>

    <script>
        const I18N = {
            zh: {
                ui_title: 'SMS Forwarder',
                nav_dashboard: '仪表板',
                nav_config: '配置',
                nav_sms: '短信',
                nav_logs: '日志',
                nav_debug: '调试',
                lang_zh: '中文',
                lang_en: 'English',
                dashboard_title: '系统状态',
                status_battery_level: '电池电量',
                status_battery_voltage: '电池电压',
                status_charging_status: '充电状态',
                status_signal_strength: '信号强度',
                status_operator: '当前运营商',
                status_home_operator: '源运营商',
                status_network_type: '网络类型',
                status_sim_status: 'SIM 状态',
                status_roaming_status: '漫游状态',
                status_reg_status: '注册状态',
                status_free_memory: '可用内存',
                status_total_memory: '总内存',
                status_cpu_freq: 'CPU 频率',
                status_flash_size: 'Flash 大小',
                status_sms_received: '接收短信',
                status_sms_forwarded: '转发短信',
                status_uptime: '运行时间',
                status_time_now: '当前时间',
                status_time_source: '对时来源',
                status_sms_network: '短信网络',
                status_data_connection: '数据连接',
                status_wifi_status: 'WiFi 状态',
                status_wifi_ip: 'WiFi IP',
                status_wifi_rssi: 'WiFi RSSI',
                status_led_status: 'LED 状态',
                status_led_reason: 'LED 原因',
                wifi_config_title: 'WiFi配置',
                wifi_ssid_label: 'WiFi名称:',
                wifi_password_label: 'WiFi密码:',
                wifi_dns_current_label: '当前DNS:',
                wifi_use_custom_dns: '使用自定义DNS',
                wifi_static_ip_enable: '使用静态IP(停用DHCP)',
                wifi_static_ip_label: '静态IP:',
                wifi_static_gateway_label: '网关:',
                wifi_static_subnet_label: '子网掩码:',
                wifi_dns1_label: 'DNS 1:',
                wifi_dns2_label: 'DNS 2:',
                wifi_static_ip_placeholder: '如 192.168.1.171',
                wifi_static_gateway_placeholder: '如 192.168.1.1',
                wifi_static_subnet_placeholder: '如 255.255.255.0',
                wifi_dns1_placeholder: '如 1.1.1.1',
                wifi_dns2_placeholder: '可选，如 8.8.8.8',
                wifi_save_btn: '保存WiFi配置',
                notification_config_title: '推送配置',
                bark_title: 'Bark推送',
                bark_enable: '启用Bark推送',
                bark_key_label: 'Bark密钥:',
                bark_url_label: 'Bark服务器:',
                serverchan_title: 'Server酱推送',
                serverchan_enable: '启用Server酱推送',
                serverchan_key_label: 'Server酱密钥:',
                serverchan_url_label: 'Server酱服务器:',
                telegram_title: 'Telegram推送',
                telegram_enable: '启用Telegram推送',
                telegram_token_label: 'Bot Token:',
                telegram_chatid_label: 'Chat ID:',
                telegram_url_label: 'Telegram服务器:',
                dingtalk_title: '钉钉推送',
                dingtalk_enable: '启用钉钉推送',
                dingtalk_webhook_label: '钉钉Webhook:',
                feishu_title: '飞书推送',
                feishu_enable: '启用飞书推送',
                feishu_webhook_label: '飞书Webhook:',
                custom_title: '自定义推送',
                custom_enable: '启用自定义推送',
                custom_url_label: '自定义URL:',
                custom_key_label: '自定义密钥:',
                notification_save_btn: '保存推送配置',
                notification_test_all_btn: '测试所有推送',
                battery_config_title: '电池管理配置',
                battery_low_threshold_label: '低电量阈值 (%):',
                battery_critical_threshold_label: '极低电量阈值 (%):',
                battery_alert_enable: '启用电池警告',
                battery_low_alert_enable: '低电量警告',
                battery_charging_alert_enable: '充电状态通知',
                battery_full_alert_enable: '满电通知',
                battery_save_btn: '保存电池配置',
                network_config_title: '网络管理配置',
                roaming_alert_enable: '漫游警告',
                roaming_disable_data: '漫游时自动关闭数据',
                roaming_allow_sms_data: '漫游时允许短信所需数据',
                data_policy_label: '移动数据策略:',
                data_policy_off: '仅短信（禁用移动数据）',
                data_policy_roaming_only: '仅非漫游启用数据',
                data_policy_on: '始终启用数据',
                signal_interval_label: '信号检查间隔 (秒):',
                operator_mode_label: '运营商选择:',
                operator_auto: '自动选网',
                operator_cmcc: '中国移动(46000)',
                operator_cu: '中国联通(46001)',
                operator_ct: '中国电信(46003)',
                operator_giffgaff: '英国 giffgaff(APN giffgaff.com)',
                radio_mode_label: '网络制式:',
                radio_mode_auto: '自动 (2G/3G/4G)',
                radio_mode_lte_only: '仅 4G (LTE)',
                apn_label: 'APN设置:',
                apn_user_label: 'APN用户名 (可选):',
                apn_pass_label: 'APN密码 (可选):',
                apn_placeholder: '留空自动',
                apn_user_placeholder: '留空表示无需认证',
                apn_pass_placeholder: '留空表示无需认证',
                network_save_btn: '保存网络配置',
                sms_filter_title: '短信过滤配置',
                whitelist_enable: '启用白名单过滤',
                whitelist_label: '白名单 (一行一个号码):',
                whitelist_placeholder: '输入允许的号码，一行一个',
                keyword_enable: '启用关键词过滤',
                keyword_label: '屏蔽关键词 (一行一个):',
                keyword_placeholder: '输入需要屏蔽的关键词',
                sms_filter_save_btn: '保存过滤配置',
                system_config_title: '系统设置',
                daily_report_enable: '启用日报',
                weekly_report_enable: '启用周报',
                report_hour_label: '报告时间 (24小时制):',
                sleep_enable: '启用休眠',
                sleep_timeout_label: '休眠超时 (秒):',
                sleep_mode_label: '休眠模式:',
                sleep_mode_light: '浅睡眠',
                sleep_mode_deep: '深度睡眠',
                sleep_mode_help: '浅睡眠保留更多状态，唤醒快但更耗电；深度睡眠省电但唤醒更慢。',
                wdt_timeout_label: '看门狗超时 (秒):',
                system_save_btn: '保存系统配置',
                sms_send_title: '发送短信',
                sms_phone_label: '手机号码:',
                sms_phone_placeholder: '请输入手机号',
                sms_message_label: '短信内容:',
                sms_message_placeholder: '请输入短信内容',
                sms_send_btn: '发送短信',
                sms_manage_title: '短信管理',
                sms_refresh_btn: '刷新短信',
                sms_manual_check_btn: '手动查询短信',
                sms_clear_btn: '清空短信',
                sms_loading: '加载中...',
                logs_title: '系统日志',
                logs_refresh_btn: '刷新日志',
                logs_clear_btn: '清空日志',
                logs_loading: '加载中...',
                debug_title: '系统调试',
                restart_btn: '重启系统',
                test_push_btn: '测试推送',
                diag_wifi_btn: '诊断WiFi',
                diag_net_btn: '网络诊断',
                system_check_btn: '系统检查',
                time_sync_btn: '手动对时',
                led_test_hw_btn: 'LED硬件测试',
                led_test_states_btn: 'LED状态测试',
                netdiag_url_label: '诊断URL:',
                netdiag_url_placeholder: '留空使用 Bark 服务器或 https://api.day.app',
                netdiag_method_label: '诊断方法:',
                netdiag_payload_label: 'POST内容(可选):',
                netdiag_payload_placeholder: '仅在POST时使用',
                at_echo_label: 'AT指令回显调试',
                at_test_title: 'AT指令测试',
                at_command_placeholder: '输入AT指令',
                at_send_btn: '发送',
                at_waiting: '等待指令...',
                time_sync_ok: '对时成功 ({0})',
                time_sync_fail: '对时失败',
                time_sync_fail_detail: '对时失败: {0}',
                time_sync_source_ntp: 'NTP',
                time_sync_source_modem: 'SIM时间',
                time_sync_source_none: '未知',
                value_unknown: '未知',
                value_empty: '(空)',
                status_charging: '充电中',
                status_not_charging: '未充电',
                status_ready: '就绪',
                status_not_ready: '未就绪',
                status_roaming: '漫游',
                status_local: '本地',
                status_yes: '是',
                status_no: '否',
                status_available: '可用',
                status_unavailable: '不可用',
                data_status_disabled: '禁用',
                data_status_roaming_disabled: '漫游禁用',
                status_connected: '已连接',
                status_disconnected: '未连接',
                status_reg_format: 'CS:{0} / EPS:{1}',
                status_uptime_suffix: 's',
                logs_show_recent: '显示最近{0}条日志（共{1}条）',
                logs_empty: '暂无日志',
                logs_load_error: '加载日志失败: {0}',
                http_status_error: '网络请求失败: {0}',
                json_parse_error: 'JSON格式错误: {0}',
                logs_clear_confirm: '确定要清空所有日志吗？',
                logs_cleared: '日志已清空',
                logs_clear_fail: '清空失败: {0}',
                restart_confirm: '确定要重启系统吗？',
                restart_in_progress: '系统正在重启...',
                restart_fail: '重启失败',
                notify_save_success: '推送配置保存成功',
                save_fail: '保存失败',
                save_fail_detail: '保存失败: {0}',
                notify_test_confirm: '确定要测试所有已启用的推送渠道吗？',
                notify_test_result: '测试结果:\\n',
                notify_test_message: '短信转发器测试消息 - {0}',
                result_success: '成功',
                result_fail: '失败',
                notify_test_done: '测试完成',
                notify_test_fail: '测试失败',
                battery_save_success: '电池配置保存成功',
                network_save_success: '网络配置保存成功',
                smsfilter_save_success: '短信过滤配置保存成功',
                wifi_save_success: 'WiFi配置保存成功，设备将重启并连接新WiFi',
                wifi_save_fail: '保存失败: {0}',
                system_save_success: '系统配置保存成功',
                at_echo_on: 'AT指令回显已开启',
                at_echo_off: 'AT指令回显已关闭',
                at_echo_fail: '设置失败',
                at_command_label: '命令: ',
                at_response_label: '响应: ',
                at_error_label: '错误: ',
                wifi_diag_done: 'WiFi诊断完成，请查看日志获取详细信息',
                wifi_diag_fail: '诊断失败: {0}',
                net_diag_done: '网络诊断完成，请查看日志获取详细信息',
                net_diag_fail: '诊断失败: {0}',
                led_test_done: 'LED硬件测试完成',
                led_state_test_done: 'LED状态测试完成',
                test_push_success: '测试推送成功',
                test_fail: '测试失败: {0}',
                sms_stats_label: '统计信息:',
                sms_stats_received: '接收',
                sms_stats_forwarded: '转发',
                sms_stats_filtered: '过滤',
                sms_stats_stored: '存储',
                sms_sender_label: '发送方',
                sms_status_forwarded: '已转发',
                sms_status_not_forwarded: '未转发',
                sms_forward_btn: '转发',
                sms_delete_btn: '删除',
                sms_time_label: '时间',
                sms_id_label: 'ID',
                sms_no_records: '暂无短信记录',
                sms_load_fail: '加载失败: {0}',
                sms_clear_confirm: '确定要清空所有短信记录吗？',
                sms_clear_success: '短信记录已清空',
                sms_clear_fail: '清空失败',
                sms_send_missing: '请填写手机号和短信内容',
                sms_send_success_msg: '短信发送成功',
                sms_send_fail_msg: '发送失败: {0}',
                sms_check_started: '短信查询已启动，请稍后刷新短信列表或查看日志',
                sms_check_fail: '查询失败: {0}',
                sms_forward_confirm: '确定要转发这条短信吗？',
                sms_forward_success_msg: '短信转发成功',
                sms_forward_fail_msg: '转发失败: {0}',
                sms_delete_confirm: '确定要删除这条短信吗？',
                sms_delete_success_msg: '短信已删除',
                sms_delete_fail_msg: '删除失败: {0}',
                system_status_prefix: '系统状态: {0}',
                led_status_init: '初始化',
                led_status_ready: '就绪',
                led_status_working: '工作中',
                led_status_error: '错误',
                led_status_low_battery: '低电量',
                led_status_charging: '充电中',
                led_status_off: '关闭',
                led_status_ap: 'AP模式',
                led_reason_low_battery: '低电量',
                led_reason_sim_init_timeout: 'SIM初始化超时',
                led_reason_sms_network_timeout: '短信网络超时',
                led_reason_charging: '充电中',
                led_reason_sim_not_ready: 'SIM未就绪',
                led_reason_wifi_not_connected: 'WiFi未连接',
                led_reason_wifi_ap_mode: 'AP模式',
                led_reason_ready: '就绪'
            },
            en: {
                ui_title: 'SMS Forwarder',
                nav_dashboard: 'Dashboard',
                nav_config: 'Config',
                nav_sms: 'SMS',
                nav_logs: 'Logs',
                nav_debug: 'Debug',
                lang_zh: '中文',
                lang_en: 'English',
                dashboard_title: 'System Status',
                status_battery_level: 'Battery',
                status_battery_voltage: 'Voltage',
                status_charging_status: 'Charging',
                status_signal_strength: 'Signal',
                status_operator: 'Operator',
                status_home_operator: 'Home Operator',
                status_network_type: 'Network Type',
                status_sim_status: 'SIM Status',
                status_roaming_status: 'Roaming',
                status_reg_status: 'Registration',
                status_free_memory: 'Free Memory',
                status_total_memory: 'Total Memory',
                status_cpu_freq: 'CPU Frequency',
                status_flash_size: 'Flash Size',
                status_sms_received: 'SMS Received',
                status_sms_forwarded: 'SMS Forwarded',
                status_uptime: 'Uptime',
                status_time_now: 'System Time',
                status_time_source: 'Last Sync Source',
                status_sms_network: 'SMS Network',
                status_data_connection: 'Data',
                status_wifi_status: 'WiFi Status',
                status_wifi_ip: 'WiFi IP',
                status_wifi_rssi: 'WiFi RSSI',
                status_led_status: 'LED Status',
                status_led_reason: 'LED Reason',
                wifi_config_title: 'WiFi Config',
                wifi_ssid_label: 'WiFi SSID:',
                wifi_password_label: 'WiFi Password:',
                wifi_dns_current_label: 'Current DNS:',
                wifi_use_custom_dns: 'Use custom DNS',
                wifi_static_ip_enable: 'Use static IP (disable DHCP)',
                wifi_static_ip_label: 'Static IP:',
                wifi_static_gateway_label: 'Gateway:',
                wifi_static_subnet_label: 'Subnet Mask:',
                wifi_dns1_label: 'DNS 1:',
                wifi_dns2_label: 'DNS 2:',
                wifi_static_ip_placeholder: 'e.g. 192.168.1.171',
                wifi_static_gateway_placeholder: 'e.g. 192.168.1.1',
                wifi_static_subnet_placeholder: 'e.g. 255.255.255.0',
                wifi_dns1_placeholder: 'e.g. 1.1.1.1',
                wifi_dns2_placeholder: 'optional, e.g. 8.8.8.8',
                wifi_save_btn: 'Save WiFi',
                notification_config_title: 'Notifications',
                bark_title: 'Bark',
                bark_enable: 'Enable Bark',
                bark_key_label: 'Bark Key:',
                bark_url_label: 'Bark Server:',
                serverchan_title: 'ServerChan',
                serverchan_enable: 'Enable ServerChan',
                serverchan_key_label: 'ServerChan Key:',
                serverchan_url_label: 'ServerChan Server:',
                telegram_title: 'Telegram',
                telegram_enable: 'Enable Telegram',
                telegram_token_label: 'Bot Token:',
                telegram_chatid_label: 'Chat ID:',
                telegram_url_label: 'Telegram Server:',
                dingtalk_title: 'DingTalk',
                dingtalk_enable: 'Enable DingTalk',
                dingtalk_webhook_label: 'DingTalk Webhook:',
                feishu_title: 'Feishu',
                feishu_enable: 'Enable Feishu',
                feishu_webhook_label: 'Feishu Webhook:',
                custom_title: 'Custom',
                custom_enable: 'Enable Custom',
                custom_url_label: 'Custom URL:',
                custom_key_label: 'Custom Key:',
                notification_save_btn: 'Save Notifications',
                notification_test_all_btn: 'Test All',
                battery_config_title: 'Battery',
                battery_low_threshold_label: 'Low Battery Threshold (%):',
                battery_critical_threshold_label: 'Critical Threshold (%):',
                battery_alert_enable: 'Enable Battery Alerts',
                battery_low_alert_enable: 'Low Battery Alert',
                battery_charging_alert_enable: 'Charging Alert',
                battery_full_alert_enable: 'Full Charge Alert',
                battery_save_btn: 'Save Battery',
                network_config_title: 'Network',
                roaming_alert_enable: 'Roaming alert',
                roaming_disable_data: 'Disable data when roaming',
                roaming_allow_sms_data: 'Allow data for SMS when roaming',
                data_policy_label: 'Mobile Data Policy:',
                data_policy_off: 'SMS only (data off)',
                data_policy_roaming_only: 'Data on when not roaming',
                data_policy_on: 'Data always on',
                signal_interval_label: 'Signal check interval (s):',
                operator_mode_label: 'Operator selection:',
                operator_auto: 'Auto',
                operator_cmcc: 'China Mobile (46000)',
                operator_cu: 'China Unicom (46001)',
                operator_ct: 'China Telecom (46003)',
                operator_giffgaff: 'UK giffgaff (APN giffgaff.com)',
                radio_mode_label: 'Radio mode:',
                radio_mode_auto: 'Auto (2G/3G/4G)',
                radio_mode_lte_only: 'LTE only',
                apn_label: 'APN:',
                apn_user_label: 'APN Username (optional):',
                apn_pass_label: 'APN Password (optional):',
                apn_placeholder: 'Leave blank for auto',
                apn_user_placeholder: 'Leave blank if none',
                apn_pass_placeholder: 'Leave blank if none',
                network_save_btn: 'Save Network',
                sms_filter_title: 'SMS Filter',
                whitelist_enable: 'Enable whitelist',
                whitelist_label: 'Whitelist (one per line):',
                whitelist_placeholder: 'Allowed numbers, one per line',
                keyword_enable: 'Enable keyword filter',
                keyword_label: 'Blocked keywords (one per line):',
                keyword_placeholder: 'Keywords to block',
                sms_filter_save_btn: 'Save Filter',
                system_config_title: 'System',
                daily_report_enable: 'Enable daily report',
                weekly_report_enable: 'Enable weekly report',
                report_hour_label: 'Report hour (24h):',
                sleep_enable: 'Enable sleep',
                sleep_timeout_label: 'Sleep timeout (s):',
                sleep_mode_label: 'Sleep mode:',
                sleep_mode_light: 'Light sleep',
                sleep_mode_deep: 'Deep sleep',
                sleep_mode_help: 'Light sleep wakes faster but uses more power; deep sleep saves power but wakes slower.',
                wdt_timeout_label: 'WDT timeout (s):',
                system_save_btn: 'Save System',
                sms_send_title: 'Send SMS',
                sms_phone_label: 'Phone number:',
                sms_phone_placeholder: 'Enter phone number',
                sms_message_label: 'Message:',
                sms_message_placeholder: 'Enter message',
                sms_send_btn: 'Send SMS',
                sms_manage_title: 'SMS Management',
                sms_refresh_btn: 'Refresh',
                sms_manual_check_btn: 'Check SMS',
                sms_clear_btn: 'Clear SMS',
                sms_loading: 'Loading...',
                logs_title: 'System Logs',
                logs_refresh_btn: 'Refresh Logs',
                logs_clear_btn: 'Clear Logs',
                logs_loading: 'Loading...',
                debug_title: 'System Debug',
                restart_btn: 'Restart',
                test_push_btn: 'Test Push',
                diag_wifi_btn: 'Diagnose WiFi',
                diag_net_btn: 'Diagnose Network',
                system_check_btn: 'System Check',
                time_sync_btn: 'Sync Time',
                led_test_hw_btn: 'LED Hardware Test',
                led_test_states_btn: 'LED State Test',
                netdiag_url_label: 'Test URL:',
                netdiag_url_placeholder: 'Leave blank to use Bark or https://api.day.app',
                netdiag_method_label: 'Method:',
                netdiag_payload_label: 'POST payload (optional):',
                netdiag_payload_placeholder: 'Only for POST',
                at_echo_label: 'AT echo debug',
                at_test_title: 'AT Command Test',
                at_command_placeholder: 'Enter AT command',
                at_send_btn: 'Send',
                at_waiting: 'Waiting...',
                time_sync_ok: 'Time sync ok ({0})',
                time_sync_fail: 'Time sync failed',
                time_sync_fail_detail: 'Time sync failed: {0}',
                time_sync_source_ntp: 'NTP',
                time_sync_source_modem: 'Modem time',
                time_sync_source_none: 'Unknown',
                value_unknown: 'Unknown',
                value_empty: '(empty)',
                status_charging: 'Charging',
                status_not_charging: 'Not charging',
                status_ready: 'Ready',
                status_not_ready: 'Not ready',
                status_roaming: 'Roaming',
                status_local: 'Local',
                status_yes: 'Yes',
                status_no: 'No',
                status_available: 'Available',
                status_unavailable: 'Unavailable',
                data_status_disabled: 'Disabled',
                data_status_roaming_disabled: 'Disabled (roaming)',
                status_connected: 'Connected',
                status_disconnected: 'Disconnected',
                status_reg_format: 'CS:{0} / EPS:{1}',
                status_uptime_suffix: 's',
                logs_show_recent: 'Showing latest {0} logs (total {1})',
                logs_empty: 'No logs',
                logs_load_error: 'Failed to load logs: {0}',
                http_status_error: 'Request failed: {0}',
                json_parse_error: 'JSON parse error: {0}',
                logs_clear_confirm: 'Clear all logs?',
                logs_cleared: 'Logs cleared',
                logs_clear_fail: 'Clear failed: {0}',
                restart_confirm: 'Restart system?',
                restart_in_progress: 'System restarting...',
                restart_fail: 'Restart failed',
                notify_save_success: 'Notification settings saved',
                save_fail: 'Save failed',
                save_fail_detail: 'Save failed: {0}',
                notify_test_confirm: 'Test all enabled channels?',
                notify_test_result: 'Test results:\\n',
                notify_test_message: 'SMS Forwarder test message - {0}',
                result_success: 'Success',
                result_fail: 'Fail',
                notify_test_done: 'Test complete',
                notify_test_fail: 'Test failed',
                battery_save_success: 'Battery settings saved',
                network_save_success: 'Network settings saved',
                smsfilter_save_success: 'Filter settings saved',
                wifi_save_success: 'WiFi saved. Rebooting to connect.',
                wifi_save_fail: 'Save failed: {0}',
                system_save_success: 'System settings saved',
                at_echo_on: 'AT echo enabled',
                at_echo_off: 'AT echo disabled',
                at_echo_fail: 'Failed to set AT echo',
                at_command_label: 'Command: ',
                at_response_label: 'Response: ',
                at_error_label: 'Error: ',
                wifi_diag_done: 'WiFi diagnostics complete. Check logs.',
                wifi_diag_fail: 'Diagnosis failed: {0}',
                net_diag_done: 'Network diagnostics complete. Check logs.',
                net_diag_fail: 'Diagnosis failed: {0}',
                led_test_done: 'LED hardware test completed',
                led_state_test_done: 'LED state test completed',
                test_push_success: 'Test push succeeded',
                test_fail: 'Test failed: {0}',
                sms_stats_label: 'Statistics:',
                sms_stats_received: 'Received',
                sms_stats_forwarded: 'Forwarded',
                sms_stats_filtered: 'Filtered',
                sms_stats_stored: 'Stored',
                sms_sender_label: 'Sender',
                sms_status_forwarded: 'Forwarded',
                sms_status_not_forwarded: 'Not forwarded',
                sms_forward_btn: 'Forward',
                sms_delete_btn: 'Delete',
                sms_time_label: 'Time',
                sms_id_label: 'ID',
                sms_no_records: 'No SMS records',
                sms_load_fail: 'Load failed: {0}',
                sms_clear_confirm: 'Clear all SMS?',
                sms_clear_success: 'SMS cleared',
                sms_clear_fail: 'Clear failed',
                sms_send_missing: 'Please enter phone number and message',
                sms_send_success_msg: 'SMS sent',
                sms_send_fail_msg: 'Send failed: {0}',
                sms_check_started: 'SMS query started. Refresh later or check logs.',
                sms_check_fail: 'Query failed: {0}',
                sms_forward_confirm: 'Forward this SMS?',
                sms_forward_success_msg: 'SMS forwarded',
                sms_forward_fail_msg: 'Forward failed: {0}',
                sms_delete_confirm: 'Delete this SMS?',
                sms_delete_success_msg: 'SMS deleted',
                sms_delete_fail_msg: 'Delete failed: {0}',
                system_status_prefix: 'System status: {0}',
                led_status_init: 'Init',
                led_status_ready: 'Ready',
                led_status_working: 'Working',
                led_status_error: 'Error',
                led_status_low_battery: 'Low battery',
                led_status_charging: 'Charging',
                led_status_off: 'Off',
                led_status_ap: 'AP mode',
                led_reason_low_battery: 'Low battery',
                led_reason_sim_init_timeout: 'SIM init timeout',
                led_reason_sms_network_timeout: 'SMS network timeout',
                led_reason_charging: 'Charging',
                led_reason_sim_not_ready: 'SIM not ready',
                led_reason_wifi_not_connected: 'WiFi not connected',
                led_reason_wifi_ap_mode: 'AP mode',
                led_reason_ready: 'Ready'
            }
        };

        let currentLang = 'zh';
        let languageInitialized = false;

        function t(key) {
            return (I18N[currentLang] && I18N[currentLang][key]) || key;
        }

        function tFmt(key, ...args) {
            return t(key).replace(/\{(\d+)\}/g, (match, idx) => {
                const i = parseInt(idx, 10);
                return (i >= 0 && i < args.length) ? args[i] : match;
            });
        }

        function detectBrowserLang() {
            const lang = (navigator.language || '').toLowerCase();
            return lang.startsWith('zh') ? 'zh' : 'en';
        }

        function renderI18n() {
            document.title = t('ui_title');
            document.querySelectorAll('[data-i18n]').forEach(el => {
                const key = el.getAttribute('data-i18n');
                if (key) el.textContent = t(key);
            });
            document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
                const key = el.getAttribute('data-i18n-placeholder');
                if (key) el.placeholder = t(key);
            });
        }

        function updateLangButtons() {
            const zhBtn = document.getElementById('lang-zh');
            const enBtn = document.getElementById('lang-en');
            if (zhBtn) zhBtn.classList.toggle('active', currentLang === 'zh');
            if (enBtn) enBtn.classList.toggle('active', currentLang === 'en');
        }

        function saveLanguage(lang) {
            const formData = new FormData();
            formData.set('lang', lang);
            return fetch('/api/config/lang', { method: 'POST', body: formData });
        }

        function setLanguage(lang, persist) {
            currentLang = lang;
            document.documentElement.lang = lang;
            renderI18n();
            updateLangButtons();
            if (persist) {
                saveLanguage(lang)
                    .catch(err => console.error('save language failed', err))
                    .finally(() => refreshCurrentPage());
            } else {
                refreshCurrentPage();
            }
        }

        function switchLang(lang) {
            if (lang === currentLang) return;
            setLanguage(lang, true);
        }

        function initLanguage(configLang) {
            if (languageInitialized) return;
            let lang = configLang;
            if (!lang || lang === 'auto') {
                lang = detectBrowserLang();
                setLanguage(lang, true);
            } else {
                setLanguage(lang, false);
            }
            languageInitialized = true;
        }

        function loadLanguageConfig() {
            fetch('/api/config')
                .then(response => response.json())
                .then(data => initLanguage(data.lang))
                .catch(() => initLanguage('auto'));
        }

        function refreshCurrentPage() {
            const pages = [
                { id: 'dashboard', fn: loadDashboard },
                { id: 'config', fn: loadConfig },
                { id: 'sms', fn: refreshSMS },
                { id: 'logs', fn: refreshLogs },
                { id: 'debug', fn: loadDebugConfig }
            ];
            pages.forEach(page => {
                const el = document.getElementById(page.id);
                if (el && !el.classList.contains('hidden')) {
                    page.fn();
                }
            });
        }
        function showPage(pageId) {
            document.querySelectorAll('.page').forEach(p => p.classList.add('hidden'));
            document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
            document.getElementById(pageId).classList.remove('hidden');
            event.target.classList.add('active');
            
            if (pageId === 'dashboard') loadDashboard();
            if (pageId === 'config') loadConfig();
            if (pageId === 'sms') refreshSMS();
            if (pageId === 'logs') refreshLogs();
            if (pageId === 'debug') loadDebugConfig();
        }
        
        function loadDebugConfig() {
            fetch('/api/config')
                .then(response => response.json())
                .then(data => {
                    if (data.debug) {
                        document.getElementById('atCommandEcho').checked = data.debug.atCommandEcho || false;
                    }
                })
                .catch(err => console.error('加载调试配置失败:', err));
        }

        function normalizeStatusText(value) {
            if (!value) return t('value_unknown');
            return String(value).replace(/_/g, ' ').trim();
        }

        function mapLedStatus(value) {
            if (!value) return t('value_unknown');
            const key = 'led_status_' + String(value).toLowerCase();
            const translated = t(key);
            return translated === key ? normalizeStatusText(value) : translated;
        }

        function mapLedReason(value) {
            if (!value) return t('value_unknown');
            const key = 'led_reason_' + String(value).toLowerCase();
            const translated = t(key);
            return translated === key ? normalizeStatusText(value) : translated;
        }

        function mapTimeSource(value) {
            if (!value) return t('value_unknown');
            const key = 'time_sync_source_' + String(value).toLowerCase();
            const translated = t(key);
            return translated === key ? normalizeStatusText(value) : translated;
        }

        function loadDashboard() {
            // 加载系统状态
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    const batteryDisplay = (data.batteryDisplay !== undefined) ? data.batteryDisplay : data.battery;
                    document.getElementById('batteryLevel').textContent = (batteryDisplay || 0) + '%';
                    document.getElementById('batteryVoltage').textContent = (data.voltage || 0) + 'V';
                    document.getElementById('chargingStatus').textContent = data.isCharging ? t('status_charging') : t('status_not_charging');
                    document.getElementById('signalStrength').textContent = (data.signal || 0) + 'dBm';
                    const networkType = data.networkType && data.networkType !== 'Unknown' ? data.networkType : t('value_unknown');
                    document.getElementById('networkType').textContent = networkType;
                    document.getElementById('simStatusText').textContent = (data.simStatus === 'Ready') ? t('status_ready') : t('status_not_ready');
                    document.getElementById('roamingStatus').textContent = data.isRoaming ? t('status_roaming') : t('status_local');
                    const csReg = data.csRegistered ? t('status_yes') : t('status_no');
                    const epsReg = data.epsRegistered ? t('status_yes') : t('status_no');
                    document.getElementById('regStatus').textContent = tFmt('status_reg_format', csReg, epsReg);
                    document.getElementById('freeMemory').textContent = (data.memory || 0) + 'KB';
                    document.getElementById('uptime').textContent = Math.floor((data.timestamp || 0) / 1000) + t('status_uptime_suffix');
                    const smsAvailable = (data.smsAvailable !== undefined)
                        ? data.smsAvailable
                        : (data.network === 'Connected');
                    document.getElementById('smsStatus').textContent = smsAvailable ? t('status_available') : t('status_unavailable');

                    const dataPolicy = (data.dataPolicy !== undefined) ? data.dataPolicy : 1;
                    const dataAttached = data.dataAttached || false;
                    let dataText = dataAttached ? t('status_available') : t('status_unavailable');
                    if (dataPolicy === 0) {
                        dataText = t('data_status_disabled');
                    } else if (dataPolicy === 1 && data.isRoaming) {
                        dataText = t('data_status_roaming_disabled');
                    }
                    document.getElementById('dataStatus').textContent = dataText;

                    const wifiConnected = data.wifiConnected || false;
                    document.getElementById('wifiStatus').textContent = wifiConnected ? t('status_connected') : t('status_disconnected');
                    document.getElementById('wifiIp').textContent = data.wifiIp || '--';
                    const rssi = (data.wifiRssi !== undefined) ? data.wifiRssi : null;
                    document.getElementById('wifiRssi').textContent = (rssi === null ? '--' : (rssi + ' dBm'));
                    let currentOp = data.operator || t('value_unknown');
                    if (currentOp === 'Unknown') currentOp = t('value_unknown');
                    let homeOp = data.homeOperator || '';
                    if (homeOp === 'Unknown') homeOp = t('value_unknown');
                    const roaming = data.isRoaming || false;
                    document.getElementById('networkOperator').textContent = currentOp;
                    let homeDisplay = homeOp || currentOp || t('value_unknown');
                    if (!roaming) {
                        homeDisplay = currentOp || homeOp || t('value_unknown');
                    }
                    document.getElementById('homeOperator').textContent = homeDisplay;
                    document.getElementById('ledStatus').textContent = mapLedStatus(data.ledStatus);
                    document.getElementById('ledReason').textContent = mapLedReason(data.ledReason);
                })
                .catch(err => console.error('加载状态失败:', err));
            
            // 加载系统信息
            fetch('/api/system/info')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('totalMemory').textContent = (data.totalMemory || 0) + 'KB';
                    document.getElementById('cpuFreq').textContent = (data.cpuFreq || 0) + 'MHz';
                    document.getElementById('flashSize').textContent = (data.flashSize || 0) + 'MB';

                })
                .catch(err => console.error('加载系统信息失败:', err));
                
            // 加载统计信息
            fetch('/api/statistics')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('smsReceived').textContent = data.totalSMSReceived || 0;
                    document.getElementById('smsForwarded').textContent = data.totalSMSForwarded || 0;
                })
                .catch(err => console.error('加载统计失败:', err));

            // 加载时间状态
            fetch('/api/system/time')
                .then(response => response.json())
                .then(data => {
                    const epochMs = data.epochMs || 0;
                    if (data.synced && epochMs > 0) {
                        document.getElementById('timeNow').textContent = new Date(epochMs).toLocaleString();
                    } else {
                        document.getElementById('timeNow').textContent = '--';
                    }
                    document.getElementById('timeSource').textContent = mapTimeSource(data.source);
                })
                .catch(err => console.error('加载时间失败:', err));
        }

        function loadConfig() {
            fetch('/api/config')
                .then(response => response.json())
                .then(data => {
                    // WiFi配置
                    if (data.wifi) {
                        document.getElementById('wifi-ssid').value = data.wifi.ssid || '';
                        document.getElementById('wifi-password').value = data.wifi.password || '';
                        const dns1 = data.wifi.dns1Current || '';
                        const dns2 = data.wifi.dns2Current || '';
                        const dnsDisplay = dns2 ? (dns1 + ', ' + dns2) : dns1;
                        document.getElementById('wifi-dns-current').value = dnsDisplay || '--';
                        document.getElementById('wifi-use-custom-dns').checked = data.wifi.useCustomDns || false;
                        document.getElementById('wifi-force-static-dns').checked = data.wifi.forceStaticDns || false;
                        document.getElementById('wifi-static-ip').value = data.wifi.staticIp || '';
                        document.getElementById('wifi-static-gateway').value = data.wifi.staticGateway || '';
                        document.getElementById('wifi-static-subnet').value = data.wifi.staticSubnet || '';
                        document.getElementById('wifi-dns1').value = data.wifi.dns1 || '';
                        document.getElementById('wifi-dns2').value = data.wifi.dns2 || '';
                    }
                    
                    // Bark配置
                    if (data.bark) {
                        document.getElementById('bark-enabled').checked = data.bark.enabled || false;
                        document.getElementById('bark-key').value = data.bark.key || '';
                        document.getElementById('bark-url').value = data.bark.url || 'https://api.day.app';
                    }
                    
                    // Server酱配置
                    if (data.serverChan) {
                        document.getElementById('serverchan-enabled').checked = data.serverChan.enabled || false;
                        document.getElementById('serverchan-key').value = data.serverChan.key || '';
                        document.getElementById('serverchan-url').value = data.serverChan.url || 'https://sctapi.ftqq.com';
                    }
                    
                    // Telegram配置
                    if (data.telegram) {
                        document.getElementById('telegram-enabled').checked = data.telegram.enabled || false;
                        document.getElementById('telegram-token').value = data.telegram.token || '';
                        document.getElementById('telegram-chatid').value = data.telegram.chatId || '';
                        document.getElementById('telegram-url').value = data.telegram.url || 'https://api.telegram.org';
                    }
                    
                    // 钉钉配置
                    if (data.dingtalk) {
                        document.getElementById('dingtalk-enabled').checked = data.dingtalk.enabled || false;
                        document.getElementById('dingtalk-webhook').value = data.dingtalk.webhook || '';
                    }
                    
                    // 飞书配置
                    if (data.feishu) {
                        document.getElementById('feishu-enabled').checked = data.feishu.enabled || false;
                        document.getElementById('feishu-webhook').value = data.feishu.webhook || '';
                    }
                    
                    // 自定义配置
                    if (data.custom) {
                        document.getElementById('custom-enabled').checked = data.custom.enabled || false;
                        document.getElementById('custom-url').value = data.custom.url || '';
                        document.getElementById('custom-key').value = data.custom.key || '';
                    }
                    

                    
                    // 定时报告配置
                    if (data.reporting) {
                        document.getElementById('daily-report-enabled').checked = data.reporting.dailyReportEnabled || false;
                        document.getElementById('weekly-report-enabled').checked = data.reporting.weeklyReportEnabled || false;
                        document.getElementById('report-hour').value = data.reporting.reportHour || 9;
                    }
                    if (data.sleep) {
                        document.getElementById('sleep-enabled').checked = data.sleep.enabled || false;
                        document.getElementById('sleep-timeout').value = data.sleep.timeout || 1800;
                        document.getElementById('sleep-mode').value = data.sleep.mode || 1;
                    }
                    if (data.watchdog) {
                        document.getElementById('wdt-timeout').value = data.watchdog.timeout || 60;
                    }
                    
                    // 电池配置
                    if (data.battery) {
                        document.getElementById('battery-low-threshold').value = data.battery.lowThreshold || 15;
                        document.getElementById('battery-critical-threshold').value = data.battery.criticalThreshold || 5;
                        document.getElementById('battery-alert-enabled').checked = data.battery.alertEnabled || false;
                        document.getElementById('low-battery-alert-enabled').checked = data.battery.lowBatteryAlertEnabled || false;
                        document.getElementById('charging-alert-enabled').checked = data.battery.chargingAlertEnabled || false;
                        document.getElementById('full-charge-alert-enabled').checked = data.battery.fullChargeAlertEnabled || false;
                    }
                    
                    // 短信过滤配置
                    if (data.smsFilter) {
                        document.getElementById('whitelist-enabled').checked = data.smsFilter.whitelistEnabled || false;
                        document.getElementById('keyword-filter-enabled').checked = data.smsFilter.keywordFilterEnabled || false;
                        document.getElementById('whitelist').value = data.smsFilter.whitelist || '';
                        document.getElementById('blocked-keywords').value = data.smsFilter.blockedKeywords || '';
                    }
                    
                    // 网络配置
                    if (data.network) {
                        document.getElementById('roaming-alert-enabled').checked = data.network.roamingAlertEnabled || false;
                        document.getElementById('auto-disable-data-roaming').checked = data.network.autoDisableDataRoaming || false;
                        document.getElementById('allow-sms-data-roaming').checked = data.network.allowSmsDataRoaming || false;
                        document.getElementById('signal-check-interval').value = data.network.signalCheckInterval || 30;
                        document.getElementById('operator-mode').value = data.network.operatorMode || 0;
                        document.getElementById('radio-mode').value = data.network.radioMode || 38;
                        document.getElementById('data-policy').value = (data.network.dataPolicy !== undefined) ? data.network.dataPolicy : 1;
                        document.getElementById('apn').value = data.network.apn || '';
                        document.getElementById('apn-user').value = data.network.apnUser || '';
                        document.getElementById('apn-pass').value = data.network.apnPass || '';
                    }
                })
                .catch(err => console.error('加载配置失败:', err));
        }



        function saveNotificationConfig() {
            const formData = new FormData(document.getElementById('notificationForm'));
            fetch('/api/config/notification', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                alert(data.success ? t('notify_save_success') : t('save_fail'));
            })
            .catch(err => alert(tFmt('save_fail_detail', err)));
        }
        
        function testAllNotifications() {
            if (confirm(t('notify_test_confirm'))) {
                fetch('/api/test/notification', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ message: tFmt('notify_test_message', new Date().toLocaleString()) })
                })
                .then(response => response.json())
                .then(data => {
                    let result = t('notify_test_result');
                    if (data.results) {
                        Object.keys(data.results).forEach(channel => {
                            result += channel + ': ' + (data.results[channel] ? t('result_success') : t('result_fail')) + '\n';
                        });
                    }
                    alert(result || t('notify_test_done'));
                })
                .catch(err => alert(tFmt('notify_test_fail', err)));
            }
        }
        
        function saveBatteryConfig() {
            const formData = new FormData(document.getElementById('batteryForm'));
            fetch('/api/config/battery', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => alert(data.success ? t('battery_save_success') : t('save_fail')))
            .catch(err => alert(tFmt('save_fail_detail', err)));
        }
        
        function saveNetworkConfig() {
            const formData = new FormData(document.getElementById('networkForm'));
            fetch('/api/config/network', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => alert(data.success ? t('network_save_success') : t('save_fail')))
            .catch(err => alert(tFmt('save_fail_detail', err)));
        }
        
        function saveSMSFilterConfig() {
            const formData = new FormData(document.getElementById('smsFilterForm'));
            fetch('/api/config/smsfilter', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => alert(data.success ? t('smsfilter_save_success') : t('save_fail')))
            .catch(err => alert(tFmt('save_fail_detail', err)));
        }
        
        function saveWiFiConfig() {
            const formData = new FormData(document.getElementById('wifiForm'));
            formData.set('useCustomDns', document.getElementById('wifi-use-custom-dns').checked ? 'true' : 'false');
            formData.set('forceStaticDns', document.getElementById('wifi-force-static-dns').checked ? 'true' : 'false');
            fetch('/api/config/wifi', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert(t('wifi_save_success'));
                    setTimeout(() => location.reload(), 2000);
                } else {
                    alert(tFmt('wifi_save_fail', data.error || ''));
                }
            })
            .catch(err => alert(tFmt('save_fail_detail', err)));
        }
        
        function saveSystemConfig() {
            const formData = new FormData(document.getElementById('systemForm'));
            fetch('/api/config/system', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => alert(data.success ? t('system_save_success') : t('save_fail')))
            .catch(err => alert(tFmt('save_fail_detail', err)));
        }

        function refreshLogs() {
            fetch('/api/logs')
                .then(response => {
                    if (!response.ok) {
                        throw new Error(tFmt('http_status_error', response.status));
                    }
                    return response.text();
                })
                .then(text => {
                    // Validate JSON format / 验证JSON格式
                    let data;
                    try {
                        data = JSON.parse(text);
                    } catch (e) {
                        console.error('JSON解析错误:', e);
                        console.error('响应内容:', text.substring(0, 1000));
                        throw new Error(tFmt('json_parse_error', e.message));
                    }
                    
                    let html = '';
                    if (data.logs && Array.isArray(data.logs) && data.logs.length > 0) {
                        // 后端已限制返回数量（默认100）
                        const recentLogs = data.logs;
                        recentLogs.forEach(log => {
                            if (log && typeof log === 'object') {
                                const levelClass = ['log-debug', 'log-info', 'log-warn', 'log-error'][log.level] || 'log-info';
                                const levelText = ['DEBUG', 'INFO', 'WARN', 'ERROR'][log.level] || 'INFO';
                                const timestamp = new Date(log.timestamp).toLocaleTimeString();
                                const tag = (log.tag || 'UNKNOWN').substring(0, 20); // 限制标签长度
                                const message = (log.message || '').substring(0, 200); // 限制消息长度
                                html += '<div class="log-entry ' + levelClass + '">[' + timestamp + '] [' + levelText + '] ' + tag + ': ' + message + '</div>';
                            }
                        });
                        if (data.total && data.total > data.logs.length) {
                            html = '<div class="log-entry log-info">' +
                                tFmt('logs_show_recent', data.logs.length, data.total) +
                                '</div>' + html;
                        }
                    } else {
                        html = '<div class="log-entry">' + t('logs_empty') + '</div>';
                    }
                    document.getElementById('logsContainer').innerHTML = html;
                    // 自动滚动到底部
                    document.getElementById('logsContainer').scrollTop = document.getElementById('logsContainer').scrollHeight;
                })
                .catch(err => {
                    console.error('日志加载错误:', err);
                    document.getElementById('logsContainer').innerHTML =
                        '<div class="log-entry log-error">' + tFmt('logs_load_error', err.message) + '</div>';
                });
        }

        function clearLogs() {
            if (confirm(t('logs_clear_confirm'))) {
                fetch('/api/logs', { method: 'DELETE' })
                    .then(() => {
                        refreshLogs();
                        alert(t('logs_cleared'));
                    })
                    .catch(err => alert(tFmt('logs_clear_fail', err)));
            }
        }

        function restartSystem() {
            if (confirm(t('restart_confirm'))) {
                fetch('/api/debug/restart', { method: 'POST' })
                    .then(() => alert(t('restart_in_progress')))
                    .catch(err => alert(tFmt('restart_fail', err)));
            }
        }

        function testNotification() {
            fetch('/api/debug/notification', { method: 'POST' })
                .then(response => response.json())
                .then(data => alert(data.success ? t('test_push_success') : t('save_fail')))
                .catch(err => alert(tFmt('notify_test_fail', err)));
        }

        function checkSystem() {
            fetch('/api/debug/system')
                .then(response => response.json())
                .then(data => alert(tFmt('system_status_prefix', data.status)))
                .catch(err => alert(tFmt('notify_test_fail', err)));
        }

        function syncTimeNow() {
            fetch('/api/debug/time', { method: 'POST' })
                .then(response => response.json())
                .then(data => {
                    if (data && data.success) {
                        let sourceKey = 'time_sync_source_none';
                        if (data.source === 'ntp') sourceKey = 'time_sync_source_ntp';
                        if (data.source === 'modem') sourceKey = 'time_sync_source_modem';
                        alert(tFmt('time_sync_ok', t(sourceKey)));
                    } else {
                        alert(t('time_sync_fail'));
                    }
                })
                .catch(err => alert(tFmt('time_sync_fail_detail', err)));
        }
        
        function toggleATEcho() {
            const enabled = document.getElementById('atCommandEcho').checked;
            fetch('/api/debug/echo', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enabled: enabled })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert(enabled ? t('at_echo_on') : t('at_echo_off'));
                } else {
                    alert(t('at_echo_fail'));
                }
            })
            .catch(err => alert(tFmt('save_fail_detail', err)));
        }

        function sendATCommand() {
            const command = document.getElementById('atCommand').value;
            fetch('/api/debug/at', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: command })
            })
            .then(response => response.json())
            .then(data => {
                const resp = (data.response !== undefined) ? data.response : 'OK';
                const showResp = (resp === '') ? t('value_empty') : resp;
                document.getElementById('atResponse').innerHTML = 
                    '<div class="log-entry">' + t('at_command_label') + command + '</div>' +
                    '<div class="log-entry log-info">' + t('at_response_label') + showResp + '</div>';
            })
            .catch(err => {
                document.getElementById('atResponse').innerHTML = 
                    '<div class="log-entry log-error">' + t('at_error_label') + err + '</div>';
            });
        }
        

        
        function diagnoseWiFi() {
            fetch('/api/debug/wifi', { method: 'POST' })
                .then(response => response.json())
                .then(() => {
                    alert(t('wifi_diag_done'));
                })
                .catch(err => alert(tFmt('wifi_diag_fail', err)));
        }

        function diagnoseNetwork() {
            const formData = new FormData(document.getElementById('netDiagForm'));
            fetch('/api/debug/network', { method: 'POST', body: formData })
                .then(response => response.json())
                .then(() => {
                    alert(t('net_diag_done'));
                })
                .catch(err => alert(tFmt('net_diag_fail', err)));
        }
        
        function testLEDHardware() {
            fetch('/api/debug/led', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'test=hardware'
            })
            .then(response => response.json())
            .then(data => alert(data.message || t('led_test_done')))
            .catch(err => alert(tFmt('test_fail', err)));
        }
        
        function testLEDStates() {
            fetch('/api/debug/led', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'test=states'
            })
            .then(response => response.json())
            .then(data => alert(data.message || t('led_state_test_done')))
            .catch(err => alert(tFmt('test_fail', err)));
        }
        
        function refreshSMS() {
            fetch('/api/sms')
                .then(response => response.json())
                .then(data => {
                    let html = '<div style="margin-bottom: 15px; padding: 10px; background: #f8f9fa; border-radius: 5px;">';
                    html += '<strong>' + t('sms_stats_label') + '</strong> ';
                    html += t('sms_stats_received') + ': ' + (data.stats.received || 0) + ' | ';
                    html += t('sms_stats_forwarded') + ': ' + (data.stats.forwarded || 0) + ' | ';
                    html += t('sms_stats_filtered') + ': ' + (data.stats.filtered || 0) + ' | ';
                    html += t('sms_stats_stored') + ': ' + (data.stats.stored || 0);
                    html += '</div>';
                    
                    if (data.messages && data.messages.length > 0) {
                        html += '<div style="max-height: 400px; overflow-y: auto;">';
                        data.messages.forEach(msg => {
                            const statusColor = msg.forwarded ? '#28a745' : '#dc3545';
                            const statusText = msg.forwarded ? t('sms_status_forwarded') : t('sms_status_not_forwarded');
                            const time = new Date(parseInt(msg.timestamp)).toLocaleString();
                            
                            html += '<div style="border: 1px solid #ddd; margin: 10px 0; padding: 15px; border-radius: 5px; background: white;">';
                            html += '<div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">';
                            html += '<strong>' + t('sms_sender_label') + ': ' + msg.sender + '</strong>';
                            html += '<div>';
                            html += '<span style="color: ' + statusColor + '; font-weight: bold; margin-right: 10px;">' + statusText + '</span>';
                            if (!msg.forwarded) {
                                html += '<button class="btn" style="padding: 5px 10px; font-size: 12px;" onclick="forwardSMS(' + msg.id + ')">' + t('sms_forward_btn') + '</button>';
                            }
                            html += '<button class="btn btn-danger" style="padding: 5px 10px; font-size: 12px; margin-left: 5px;" onclick="deleteSMS(' + msg.id + ')">' + t('sms_delete_btn') + '</button>';
                            html += '</div>';
                            html += '</div>';
                            html += '<div style="margin-bottom: 10px;">' + msg.content + '</div>';
                            html += '<div style="font-size: 12px; color: #666;">' + t('sms_time_label') + ': ' + time + ' | ' + t('sms_id_label') + ': ' + msg.id + '</div>';
                            html += '</div>';
                        });
                        html += '</div>';
                    } else {
                        html += '<div style="text-align: center; padding: 20px; color: #666;">' + t('sms_no_records') + '</div>';
                    }
                    
                    document.getElementById('smsContainer').innerHTML = html;
                })
                .catch(err => {
                    document.getElementById('smsContainer').innerHTML = '<div style="color: #dc3545; text-align: center; padding: 20px;">' + tFmt('sms_load_fail', err) + '</div>';
                });
        }
        
        function clearAllSMS() {
            if (confirm(t('sms_clear_confirm'))) {
                fetch('/api/sms', { method: 'DELETE' })
                    .then(response => response.json())
                    .then(data => {
                        if (data.success) {
                            alert(t('sms_clear_success'));
                            refreshSMS();
                        } else {
                            alert(t('sms_clear_fail'));
                        }
                    })
                    .catch(() => alert(t('sms_clear_fail')));
            }
        }
        
        function sendSMS() {
            const phoneNumber = document.getElementById('phoneNumber').value;
            const message = document.getElementById('smsMessage').value;
            
            if (!phoneNumber || !message) {
                alert(t('sms_send_missing'));
                return;
            }
            
            fetch('/api/sms/send', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'phoneNumber=' + encodeURIComponent(phoneNumber) + '&message=' + encodeURIComponent(message)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert(t('sms_send_success_msg'));
                    document.getElementById('sendSMSForm').reset();
                } else {
                    alert(tFmt('sms_send_fail_msg', data.error || t('value_unknown')));
                }
            })
            .catch(err => alert(tFmt('sms_send_fail_msg', err)));
        }
        
        function checkSMS() {
            fetch('/api/sms/check', { method: 'POST' })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert(t('sms_check_started'));
                        // 自动刷新短信列表
                        setTimeout(refreshSMS, 2000);
                    } else {
                        alert(tFmt('sms_check_fail', data.error || t('value_unknown')));
                    }
                })
                .catch(err => alert(tFmt('sms_check_fail', err)));
        }
        
        function forwardSMS(id) {
            if (confirm(t('sms_forward_confirm'))) {
                fetch('/api/sms/forward', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'id=' + id
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert(t('sms_forward_success_msg'));
                        refreshSMS(); // 刷新短信列表
                    } else {
                        alert(tFmt('sms_forward_fail_msg', data.error || t('value_unknown')));
                    }
                })
                .catch(err => alert(tFmt('sms_forward_fail_msg', err)));
            }
        }

        function deleteSMS(id) {
            if (confirm(t('sms_delete_confirm'))) {
                fetch('/api/sms/delete', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'id=' + id
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert(t('sms_delete_success_msg'));
                        refreshSMS();
                    } else {
                        alert(tFmt('sms_delete_fail_msg', data.error || t('value_unknown')));
                    }
                })
                .catch(err => alert(tFmt('sms_delete_fail_msg', err)));
            }
        }

        // 自动刷新状态
        setInterval(() => {
            if (!document.getElementById('dashboard').classList.contains('hidden')) {
                loadDashboard();
            }
        }, 5000);

        // 页面加载完成后初始化
        document.addEventListener('DOMContentLoaded', () => {
            loadLanguageConfig();
        });
    </script>
</body>
</html>
)rawliteral";

#endif
