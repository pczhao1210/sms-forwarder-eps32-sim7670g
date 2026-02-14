#ifndef WEB_PAGES_FULL_H
#define WEB_PAGES_FULL_H

// 使用PROGMEM存储HTML页面到Flash
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>SMS Forwarder</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; }
        .nav { background: #007bff; padding: 15px; border-radius: 8px; margin-bottom: 20px; }
        .nav-btn { background: rgba(255,255,255,0.2); color: white; border: none; padding: 10px 20px; margin: 0 5px; border-radius: 5px; cursor: pointer; }
        .nav-btn.active { background: rgba(255,255,255,0.3); }
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
            <button class="nav-btn active" onclick="showPage('dashboard')">仪表板</button>
            <button class="nav-btn" onclick="showPage('config')">配置</button>
            <button class="nav-btn" onclick="showPage('sms')">短信</button>
            <button class="nav-btn" onclick="showPage('logs')">日志</button>
            <button class="nav-btn" onclick="showPage('debug')">调试</button>
        </div>

        <!-- 仪表板页面 -->
        <div id="dashboard" class="page">
            <div class="card">
                <h2>系统状态</h2>
                <div class="status-grid" id="statusGrid">
                    <div class="status-item">
                        <div class="status-value" id="batteryLevel">--</div>
                        <div>电池电量</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="batteryVoltage">--</div>
                        <div>电池电压</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="chargingStatus">--</div>
                        <div>充电状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="signalStrength">--</div>
                        <div>信号强度</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="networkOperator">--</div>
                        <div>当前运营商</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="homeOperator">--</div>
                        <div>源运营商</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="networkType">--</div>
                        <div>网络类型</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="simStatusText">--</div>
                        <div>SIM 状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="roamingStatus">--</div>
                        <div>漫游状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="regStatus">--</div>
                        <div>注册状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="freeMemory">--</div>
                        <div>可用内存</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="totalMemory">--</div>
                        <div>总内存</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="cpuFreq">--</div>
                        <div>CPU 频率</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="flashSize">--</div>
                        <div>Flash 大小</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="smsReceived">--</div>
                        <div>接收短信</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="smsForwarded">--</div>
                        <div>转发短信</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="uptime">--</div>
                        <div>运行时间</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="smsStatus">--</div>
                        <div>短信网络</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="dataStatus">--</div>
                        <div>数据连接</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="wifiStatus">--</div>
                        <div>WiFi 状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="wifiIp">--</div>
                        <div>WiFi IP</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="wifiRssi">--</div>
                        <div>WiFi RSSI</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="ledStatus">--</div>
                        <div>LED 状态</div>
                    </div>
                    <div class="status-item">
                        <div class="status-value" id="ledReason">--</div>
                        <div>LED 原因</div>
                    </div>

                </div>
            </div>
        </div>

        <!-- 配置页面 -->
        <div id="config" class="page hidden">
            <div class="card">
                <h2>WiFi配置</h2>
                <form id="wifiForm" onsubmit="saveWiFiConfig(); return false;">
                    <div class="form-group">
                        <label>WiFi名称:</label>
                        <input type="text" id="wifi-ssid" name="ssid" required>
                    </div>
                    <div class="form-group">
                        <label>WiFi密码:</label>
                        <input type="password" id="wifi-password" name="password">
                    </div>
                    <div class="form-group">
                        <label>当前DNS:</label>
                        <input type="text" id="wifi-dns-current" readonly>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="wifi-use-custom-dns" name="useCustomDns"> 使用自定义DNS</label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="wifi-force-static-dns" name="forceStaticDns"> 使用静态IP(停用DHCP)</label>
                    </div>
                    <div class="form-group">
                        <label>静态IP:</label>
                        <input type="text" id="wifi-static-ip" name="staticIp" placeholder="如 192.168.1.171">
                    </div>
                    <div class="form-group">
                        <label>网关:</label>
                        <input type="text" id="wifi-static-gateway" name="staticGateway" placeholder="如 192.168.1.1">
                    </div>
                    <div class="form-group">
                        <label>子网掩码:</label>
                        <input type="text" id="wifi-static-subnet" name="staticSubnet" placeholder="如 255.255.255.0">
                    </div>
                    <div class="form-group">
                        <label>DNS 1:</label>
                        <input type="text" id="wifi-dns1" name="dns1" placeholder="如 1.1.1.1">
                    </div>
                    <div class="form-group">
                        <label>DNS 2:</label>
                        <input type="text" id="wifi-dns2" name="dns2" placeholder="可选，如 8.8.8.8">
                    </div>
                    <button type="submit" class="btn">保存WiFi配置</button>
                </form>
            </div>
            
            <div class="card">
                <h2>推送配置</h2>
                <form id="notificationForm" onsubmit="saveNotificationConfig(); return false;">
                    <h3>Bark推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="bark-enabled" name="bark-enabled"> 启用Bark推送</label>
                    </div>
                    <div class="form-group">
                        <label>Bark密钥:</label>
                        <input type="text" id="bark-key" name="barkKey">
                    </div>
                    <div class="form-group">
                        <label>Bark服务器:</label>
                        <input type="text" id="bark-url" name="barkUrl" value="https://api.day.app">
                    </div>
                    
                    <h3>Server酱推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="serverchan-enabled" name="serverchan-enabled"> 启用Server酱推送</label>
                    </div>
                    <div class="form-group">
                        <label>Server酱密钥:</label>
                        <input type="text" id="serverchan-key" name="serverChanKey">
                    </div>
                    <div class="form-group">
                        <label>Server酱服务器:</label>
                        <input type="text" id="serverchan-url" name="serverChanUrl" value="https://sctapi.ftqq.com">
                    </div>
                    
                    <h3>Telegram推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="telegram-enabled" name="telegram-enabled"> 启用Telegram推送</label>
                    </div>
                    <div class="form-group">
                        <label>Bot Token:</label>
                        <input type="text" id="telegram-token" name="telegramToken">
                    </div>
                    <div class="form-group">
                        <label>Chat ID:</label>
                        <input type="text" id="telegram-chatid" name="telegramChatId">
                    </div>
                    <div class="form-group">
                        <label>Telegram服务器:</label>
                        <input type="text" id="telegram-url" name="telegramUrl" value="https://api.telegram.org">
                    </div>
                    
                    <h3>钉钉推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="dingtalk-enabled" name="dingtalk-enabled"> 启用钉钉推送</label>
                    </div>
                    <div class="form-group">
                        <label>钉钉Webhook:</label>
                        <input type="text" id="dingtalk-webhook" name="dingtalkWebhook">
                    </div>
                    
                    <h3>飞书推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="feishu-enabled" name="feishu-enabled"> 启用飞书推送</label>
                    </div>
                    <div class="form-group">
                        <label>飞书Webhook:</label>
                        <input type="text" id="feishu-webhook" name="feishuWebhook">
                    </div>
                    
                    <h3>自定义推送</h3>
                    <div class="form-group">
                        <label><input type="checkbox" id="custom-enabled" name="custom-enabled"> 启用自定义推送</label>
                    </div>
                    <div class="form-group">
                        <label>自定义URL:</label>
                        <input type="text" id="custom-url" name="customUrl">
                    </div>
                    <div class="form-group">
                        <label>自定义密钥:</label>
                        <input type="text" id="custom-key" name="customKey">
                    </div>
                    
                    <div style="margin-top: 20px;">
                        <button type="submit" class="btn">保存推送配置</button>
                        <button type="button" class="btn btn-success" onclick="testAllNotifications()">测试所有推送</button>
                    </div>
                </form>
            </div>
            
            <div class="card">
                <h2>电池管理配置</h2>
                <form id="batteryForm" onsubmit="saveBatteryConfig(); return false;">
                    <div class="form-group">
                        <label>低电量阈值 (%):</label>
                        <input type="number" id="battery-low-threshold" name="lowThreshold" min="5" max="50" value="15">
                    </div>
                    <div class="form-group">
                        <label>极低电量阈值 (%):</label>
                        <input type="number" id="battery-critical-threshold" name="criticalThreshold" min="1" max="20" value="5">
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="battery-alert-enabled" name="battery-alert-enabled"> 启用电池警告</label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="low-battery-alert-enabled" name="low-battery-alert-enabled"> 低电量警告</label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="charging-alert-enabled" name="charging-alert-enabled"> 充电状态通知</label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="full-charge-alert-enabled" name="full-charge-alert-enabled"> 满电通知</label>
                    </div>
                    <button type="submit" class="btn">保存电池配置</button>
                </form>
            </div>
            
            <div class="card">
                <h2>网络管理配置</h2>
                <form id="networkForm" onsubmit="saveNetworkConfig(); return false;">
                    <div class="form-group">
                        <label><input type="checkbox" id="roaming-alert-enabled" name="roaming-alert-enabled"> 漫游警告</label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="auto-disable-data-roaming" name="auto-disable-data-roaming"> 漫游时自动关闭数据</label>
                    </div>
                    <div class="form-group">
                        <label>移动数据策略:</label>
                        <select id="data-policy" name="dataPolicy">
                            <option value="0">仅短信（禁用移动数据）</option>
                            <option value="1">仅非漫游启用数据</option>
                            <option value="2">始终启用数据</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>信号检查间隔 (秒):</label>
                        <input type="number" id="signal-check-interval" name="signalCheckInterval" min="10" max="300" value="30">
                    </div>
                    <div class="form-group">
                        <label>运营商选择:</label>
                        <select id="operator-mode" name="operatorMode">
                            <option value="0">自动选网</option>
                            <option value="1">中国移动(46000)</option>
                            <option value="2">中国联通(46001)</option>
                            <option value="3">中国电信(46003)</option>
                            <option value="4">英国 giffgaff(APN giffgaff.com)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>网络制式:</label>
                        <select id="radio-mode" name="radioMode">
                            <option value="2">自动 (2G/3G/4G)</option>
                            <option value="38">仅 4G (LTE)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>APN设置:</label>
                        <input type="text" id="apn" name="apn" placeholder="留空自动">
                    </div>
                    <div class="form-group">
                        <label>APN用户名 (可选):</label>
                        <input type="text" id="apn-user" name="apnUser" placeholder="留空表示无需认证">
                    </div>
                    <div class="form-group">
                        <label>APN密码 (可选):</label>
                        <input type="password" id="apn-pass" name="apnPass" placeholder="留空表示无需认证">
                    </div>
                    <button type="submit" class="btn">保存网络配置</button>
                </form>
            </div>
            
            <div class="card">
                <h2>短信过滤配置</h2>
                <form id="smsFilterForm" onsubmit="saveSMSFilterConfig(); return false;">
                    <div class="form-group">
                        <label><input type="checkbox" id="whitelist-enabled" name="whitelist-enabled"> 启用白名单过滤</label>
                    </div>
                    <div class="form-group">
                        <label>白名单 (一行一个号码):</label>
                        <textarea id="whitelist" name="whitelist" rows="3" placeholder="输入允许的号码，一行一个"></textarea>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="keyword-filter-enabled" name="keyword-filter-enabled"> 启用关键词过滤</label>
                    </div>
                    <div class="form-group">
                        <label>屏蔽关键词 (一行一个):</label>
                        <textarea id="blocked-keywords" name="blockedKeywords" rows="3" placeholder="输入需要屏蔽的关键词"></textarea>
                    </div>
                    <button type="submit" class="btn">保存过滤配置</button>
                </form>
            </div>
            
            <div class="card">
                <h2>系统设置</h2>
                <form id="systemForm" onsubmit="saveSystemConfig(); return false;">
                    <div class="form-group">
                        <label><input type="checkbox" id="daily-report-enabled" name="daily-report-enabled"> 启用日报</label>
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="weekly-report-enabled" name="weekly-report-enabled"> 启用周报</label>
                    </div>
                    <div class="form-group">
                        <label>报告时间 (24小时制):</label>
                        <input type="number" id="report-hour" name="reportHour" min="0" max="23" value="9">
                    </div>
                    <div class="form-group">
                        <label><input type="checkbox" id="sleep-enabled" name="sleep-enabled"> 启用休眠</label>
                    </div>
                    <div class="form-group">
                        <label>休眠超时 (秒):</label>
                        <input type="number" id="sleep-timeout" name="sleep-timeout" min="60" max="86400" value="1800">
                    </div>
                    <div class="form-group">
                        <label>休眠模式:</label>
                        <select id="sleep-mode" name="sleep-mode">
                            <option value="0">浅睡眠</option>
                            <option value="1">深度睡眠</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>看门狗超时 (秒):</label>
                        <input type="number" id="wdt-timeout" name="wdt-timeout" min="10" max="300" value="60">
                    </div>
                    <button type="submit" class="btn">保存系统配置</button>
                </form>
            </div>
        </div>

        <!-- 短信管理页面 -->
        <div id="sms" class="page hidden">
            <div class="card">
                <h2>发送短信</h2>
                <form id="sendSMSForm" onsubmit="sendSMS(); return false;">
                    <div class="form-group">
                        <label>手机号码:</label>
                        <input type="tel" id="phoneNumber" placeholder="请输入手机号" required>
                    </div>
                    <div class="form-group">
                        <label>短信内容:</label>
                        <textarea id="smsMessage" rows="3" placeholder="请输入短信内容" maxlength="160" required></textarea>
                    </div>
                    <button type="submit" class="btn">发送短信</button>
                </form>
            </div>
            
            <div class="card">
                <h2>短信管理</h2>
                <div style="margin-bottom: 15px;">
                    <button class="btn" onclick="refreshSMS()">刷新短信</button>
                    <button class="btn btn-success" onclick="checkSMS()">手动查询短信</button>
                    <button class="btn btn-danger" onclick="clearAllSMS()">清空短信</button>
                </div>
                <div id="smsContainer">加载中...</div>
            </div>
        </div>

        <!-- 日志页面 -->
        <div id="logs" class="page hidden">
            <div class="card">
                <h2>系统日志</h2>
                <div style="margin-bottom: 10px;">
                    <button class="btn" onclick="refreshLogs()">刷新日志</button>
                    <button class="btn btn-danger" onclick="clearLogs()">清空日志</button>
                </div>
                <div id="logsContainer" class="logs">加载中...</div>
            </div>
        </div>

        <!-- 调试页面 -->
        <div id="debug" class="page hidden">
            <div class="card">
                <h2>系统调试</h2>
                <button class="btn btn-danger" onclick="restartSystem()">重启系统</button>
                <button class="btn" onclick="testNotification()">测试推送</button>
                <button class="btn" onclick="diagnoseWiFi()">诊断WiFi</button>
                <button class="btn" onclick="diagnoseNetwork()">网络诊断</button>
                <button class="btn" onclick="checkSystem()">系统检查</button>
                <button class="btn btn-success" onclick="checkSMS()">手动查询短信</button>
                <button class="btn" onclick="testLEDHardware()">LED硬件测试</button>
                <button class="btn" onclick="testLEDStates()">LED状态测试</button>
                <div style="margin-top: 10px;">
                    <form id="netDiagForm" onsubmit="diagnoseNetwork(); return false;">
                        <div class="form-group">
                            <label>诊断URL:</label>
                            <input type="text" id="netDiagUrl" name="url" placeholder="留空使用 Bark 服务器或 https://api.day.app">
                        </div>
                        <div class="form-group">
                            <label>诊断方法:</label>
                            <select id="netDiagMethod" name="method">
                                <option value="GET" selected>GET</option>
                                <option value="POST">POST</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>POST内容(可选):</label>
                            <input type="text" id="netDiagPayload" name="payload" placeholder="仅在POST时使用">
                        </div>
                    </form>
                </div>
                <div style="margin-top: 15px;">
                    <label style="display: flex; align-items: center; gap: 10px;">
                        <input type="checkbox" id="atCommandEcho" onchange="toggleATEcho()">
                        <span>AT指令回显调试</span>
                    </label>
                </div>
            </div>
            

            
            <div class="card">
                <h2>AT指令测试</h2>
                <div class="form-group">
                    <input type="text" id="atCommand" placeholder="输入AT指令" value="AT">
                    <button class="btn" onclick="sendATCommand()">发送</button>
                </div>
                <div id="atResponse" class="logs" style="max-height: 150px;">等待指令...</div>
            </div>
        </div>
    </div>

    <script>
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
            if (!value) return '--';
            return String(value).replace(/_/g, ' ').trim().toUpperCase();
        }

        function loadDashboard() {
            // 加载系统状态
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('batteryLevel').textContent = (data.battery || 0) + '%';
                    document.getElementById('batteryVoltage').textContent = (data.voltage || 0) + 'V';
                    document.getElementById('chargingStatus').textContent = data.isCharging ? '充电中' : '未充电';
                    document.getElementById('signalStrength').textContent = (data.signal || 0) + 'dBm';
                    document.getElementById('networkType').textContent = data.networkType || '4G';
                    document.getElementById('simStatusText').textContent = (data.simStatus === 'Ready') ? '就绪' : '未就绪';
                    document.getElementById('roamingStatus').textContent = data.isRoaming ? '漫游' : '本地';
                    const csReg = data.csRegistered ? '是' : '否';
                    const epsReg = data.epsRegistered ? '是' : '否';
                    document.getElementById('regStatus').textContent = 'CS:' + csReg + ' / EPS:' + epsReg;
                    document.getElementById('freeMemory').textContent = (data.memory || 0) + 'KB';
                    document.getElementById('uptime').textContent = Math.floor((data.timestamp || 0) / 1000) + 's';
                    const smsAvailable = (data.smsAvailable !== undefined)
                        ? data.smsAvailable
                        : (data.network === 'Connected');
                    document.getElementById('smsStatus').textContent = smsAvailable ? '可用' : '不可用';

                    const dataPolicy = (data.dataPolicy !== undefined) ? data.dataPolicy : 1;
                    const dataAttached = data.dataAttached || false;
                    let dataText = dataAttached ? '可用' : '不可用';
                    if (dataPolicy === 0) {
                        dataText = '禁用';
                    } else if (dataPolicy === 1 && data.isRoaming) {
                        dataText = '漫游禁用';
                    }
                    document.getElementById('dataStatus').textContent = dataText;

                    const wifiConnected = data.wifiConnected || false;
                    document.getElementById('wifiStatus').textContent = wifiConnected ? '已连接' : '未连接';
                    document.getElementById('wifiIp').textContent = data.wifiIp || '--';
                    const rssi = (data.wifiRssi !== undefined) ? data.wifiRssi : null;
                    document.getElementById('wifiRssi').textContent = (rssi === null ? '--' : (rssi + ' dBm'));
                    const currentOp = data.operator || '未知';
                    const homeOp = data.homeOperator || '';
                    const roaming = data.isRoaming || false;
                    document.getElementById('networkOperator').textContent = currentOp;
                    let homeDisplay = homeOp || currentOp || '未知';
                    if (!roaming) {
                        homeDisplay = currentOp || homeOp || '未知';
                    }
                    document.getElementById('homeOperator').textContent = homeDisplay;
                    document.getElementById('ledStatus').textContent = normalizeStatusText(data.ledStatus);
                    document.getElementById('ledReason').textContent = normalizeStatusText(data.ledReason);
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
                alert(data.success ? '推送配置保存成功' : '保存失败');
            })
            .catch(err => alert('保存失败: ' + err));
        }
        
        function testAllNotifications() {
            if (confirm('确定要测试所有已启用的推送渠道吗？')) {
                fetch('/api/test/notification', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ message: '这是一条测试消息，发送时间: ' + new Date().toLocaleString() })
                })
                .then(response => response.json())
                .then(data => {
                    let result = '测试结果:\n';
                    if (data.results) {
                        Object.keys(data.results).forEach(channel => {
                            result += channel + ': ' + (data.results[channel] ? '成功' : '失败') + '\n';
                        });
                    }
                    alert(result || '测试完成');
                })
                .catch(err => alert('测试失败: ' + err));
            }
        }
        
        function saveBatteryConfig() {
            const formData = new FormData(document.getElementById('batteryForm'));
            fetch('/api/config/battery', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => alert(data.success ? '电池配置保存成功' : '保存失败'))
            .catch(err => alert('保存失败: ' + err));
        }
        
        function saveNetworkConfig() {
            const formData = new FormData(document.getElementById('networkForm'));
            fetch('/api/config/network', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => alert(data.success ? '网络配置保存成功' : '保存失败'))
            .catch(err => alert('保存失败: ' + err));
        }
        
        function saveSMSFilterConfig() {
            const formData = new FormData(document.getElementById('smsFilterForm'));
            fetch('/api/config/smsfilter', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => alert(data.success ? '短信过滤配置保存成功' : '保存失败'))
            .catch(err => alert('保存失败: ' + err));
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
                    alert('WiFi配置保存成功，设备将重启并连接新WiFi');
                    setTimeout(() => location.reload(), 2000);
                } else {
                    alert('保存失败: ' + (data.error || ''));
                }
            })
            .catch(err => alert('保存失败: ' + err));
        }
        
        function saveSystemConfig() {
            const formData = new FormData(document.getElementById('systemForm'));
            fetch('/api/config/system', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => alert(data.success ? '系统配置保存成功' : '保存失败'))
            .catch(err => alert('保存失败: ' + err));
        }

        function refreshLogs() {
            fetch('/api/logs')
                .then(response => {
                    if (!response.ok) {
                        throw new Error('网络请求失败: ' + response.status);
                    }
                    return response.text();
                })
                .then(text => {
                    // 验证JSON格式
                    let data;
                    try {
                        data = JSON.parse(text);
                    } catch (e) {
                        console.error('JSON解析错误:', e);
                        console.error('响应内容:', text.substring(0, 1000));
                        throw new Error('JSON格式错误: ' + e.message);
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
                            html = '<div class="log-entry log-info">显示最近' + data.logs.length + '条日志（共' + data.total + '条）</div>' + html;
                        }
                    } else {
                        html = '<div class="log-entry">暂无日志</div>';
                    }
                    document.getElementById('logsContainer').innerHTML = html;
                    // 自动滚动到底部
                    document.getElementById('logsContainer').scrollTop = document.getElementById('logsContainer').scrollHeight;
                })
                .catch(err => {
                    console.error('日志加载错误:', err);
                    document.getElementById('logsContainer').innerHTML = '<div class="log-entry log-error">加载日志失败: ' + err.message + '</div>';
                });
        }

        function clearLogs() {
            if (confirm('确定要清空所有日志吗？')) {
                fetch('/api/logs', { method: 'DELETE' })
                    .then(() => {
                        refreshLogs();
                        alert('日志已清空');
                    })
                    .catch(err => alert('清空失败: ' + err));
            }
        }

        function restartSystem() {
            if (confirm('确定要重启系统吗？')) {
                fetch('/api/debug/restart', { method: 'POST' })
                    .then(() => alert('系统正在重启...'))
                    .catch(err => alert('重启失败: ' + err));
            }
        }

        function testNotification() {
            fetch('/api/debug/notification', { method: 'POST' })
                .then(response => response.json())
                .then(data => alert(data.success ? '测试推送已发送' : '推送失败'))
                .catch(err => alert('测试失败: ' + err));
        }

        function checkSystem() {
            fetch('/api/debug/system')
                .then(response => response.json())
                .then(data => alert('系统状态: ' + data.status))
                .catch(err => alert('检查失败: ' + err));
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
                    alert('AT指令回显已' + (enabled ? '开启' : '关闭'));
                } else {
                    alert('设置失败');
                }
            })
            .catch(err => alert('设置失败: ' + err));
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
                const showResp = (resp === '') ? '(empty)' : resp;
                document.getElementById('atResponse').innerHTML = 
                    '<div class="log-entry">命令: ' + command + '</div>' +
                    '<div class="log-entry log-info">响应: ' + showResp + '</div>';
            })
            .catch(err => {
                document.getElementById('atResponse').innerHTML = 
                    '<div class="log-entry log-error">错误: ' + err + '</div>';
            });
        }
        

        
        function diagnoseWiFi() {
            fetch('/api/debug/wifi', { method: 'POST' })
                .then(response => response.json())
                .then(data => {
                    alert('WiFi诊断完成，请查看日志获取详细信息');
                })
                .catch(err => alert('诊断失败: ' + err));
        }

        function diagnoseNetwork() {
            const formData = new FormData(document.getElementById('netDiagForm'));
            fetch('/api/debug/network', { method: 'POST', body: formData })
                .then(response => response.json())
                .then(data => {
                    alert('网络诊断完成，请查看日志获取详细信息');
                })
                .catch(err => alert('诊断失败: ' + err));
        }
        
        function testLEDHardware() {
            fetch('/api/debug/led', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'test=hardware'
            })
            .then(response => response.json())
            .then(data => alert(data.message || 'LED硬件测试完成'))
            .catch(err => alert('测试失败: ' + err));
        }
        
        function testLEDStates() {
            fetch('/api/debug/led', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'test=states'
            })
            .then(response => response.json())
            .then(data => alert(data.message || 'LED状态测试完成'))
            .catch(err => alert('测试失败: ' + err));
        }
        
        function refreshSMS() {
            fetch('/api/sms')
                .then(response => response.json())
                .then(data => {
                    let html = '<div style="margin-bottom: 15px; padding: 10px; background: #f8f9fa; border-radius: 5px;">';
                    html += '<strong>统计信息:</strong> ';
                    html += '接收: ' + (data.stats.received || 0) + ' | ';
                    html += '转发: ' + (data.stats.forwarded || 0) + ' | ';
                    html += '过滤: ' + (data.stats.filtered || 0) + ' | ';
                    html += '存储: ' + (data.stats.stored || 0);
                    html += '</div>';
                    
                    if (data.messages && data.messages.length > 0) {
                        html += '<div style="max-height: 400px; overflow-y: auto;">';
                        data.messages.forEach(msg => {
                            const statusColor = msg.forwarded ? '#28a745' : '#dc3545';
                            const statusText = msg.forwarded ? '已转发' : '未转发';
                            const time = new Date(parseInt(msg.timestamp)).toLocaleString();
                            
                            html += '<div style="border: 1px solid #ddd; margin: 10px 0; padding: 15px; border-radius: 5px; background: white;">';
                            html += '<div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">';
                            html += '<strong>发送方: ' + msg.sender + '</strong>';
                            html += '<div>';
                            html += '<span style="color: ' + statusColor + '; font-weight: bold; margin-right: 10px;">' + statusText + '</span>';
                            if (!msg.forwarded) {
                                html += '<button class="btn" style="padding: 5px 10px; font-size: 12px;" onclick="forwardSMS(' + msg.id + ')">转发</button>';
                            }
                            html += '<button class="btn btn-danger" style="padding: 5px 10px; font-size: 12px; margin-left: 5px;" onclick="deleteSMS(' + msg.id + ')">删除</button>';
                            html += '</div>';
                            html += '</div>';
                            html += '<div style="margin-bottom: 10px;">' + msg.content + '</div>';
                            html += '<div style="font-size: 12px; color: #666;">时间: ' + time + ' | ID: ' + msg.id + '</div>';
                            html += '</div>';
                        });
                        html += '</div>';
                    } else {
                        html += '<div style="text-align: center; padding: 20px; color: #666;">暂无短信记录</div>';
                    }
                    
                    document.getElementById('smsContainer').innerHTML = html;
                })
                .catch(err => {
                    document.getElementById('smsContainer').innerHTML = '<div style="color: #dc3545; text-align: center; padding: 20px;">加载失败: ' + err + '</div>';
                });
        }
        
        function clearAllSMS() {
            if (confirm('确定要清空所有短信记录吗？')) {
                fetch('/api/sms', { method: 'DELETE' })
                    .then(response => response.json())
                    .then(data => {
                        if (data.success) {
                            alert('短信记录已清空');
                            refreshSMS();
                        } else {
                            alert('清空失败');
                        }
                    })
                    .catch(err => alert('清空失败: ' + err));
            }
        }
        
        function sendSMS() {
            const phoneNumber = document.getElementById('phoneNumber').value;
            const message = document.getElementById('smsMessage').value;
            
            if (!phoneNumber || !message) {
                alert('请填写手机号和短信内容');
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
                    alert('短信发送成功');
                    document.getElementById('sendSMSForm').reset();
                } else {
                    alert('发送失败: ' + (data.error || '未知错误'));
                }
            })
            .catch(err => alert('发送失败: ' + err));
        }
        
        function checkSMS() {
            fetch('/api/sms/check', { method: 'POST' })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('短信查询已启动，请稍后刷新短信列表或查看日志');
                        // 自动刷新短信列表
                        setTimeout(refreshSMS, 2000);
                    } else {
                        alert('查询失败: ' + (data.error || '未知错误'));
                    }
                })
                .catch(err => alert('查询失败: ' + err));
        }
        
        function forwardSMS(id) {
            if (confirm('确定要转发这条短信吗？')) {
                fetch('/api/sms/forward', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'id=' + id
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('短信转发成功');
                        refreshSMS(); // 刷新短信列表
                    } else {
                        alert('转发失败: ' + (data.error || '未知错误'));
                    }
                })
                .catch(err => alert('转发失败: ' + err));
            }
        }

        function deleteSMS(id) {
            if (confirm('确定要删除这条短信吗？')) {
                fetch('/api/sms/delete', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'id=' + id
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('短信已删除');
                        refreshSMS();
                    } else {
                        alert('删除失败: ' + (data.error || '未知错误'));
                    }
                })
                .catch(err => alert('删除失败: ' + err));
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
            loadDashboard();
        });
    </script>
</body>
</html>
)rawliteral";

#endif
