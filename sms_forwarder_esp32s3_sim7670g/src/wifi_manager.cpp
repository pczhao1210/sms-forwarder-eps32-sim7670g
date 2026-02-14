#include "wifi_manager.h"
#include "config_manager.h"
#include "log_manager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_netif.h>
#include <lwip/ip4_addr.h>

static bool applyCustomDnsEspNetif(const IPAddress& dns1, const IPAddress& dns2, bool hasDns2) {
  esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif == nullptr) {
    logManager.addLog(LOG_WARN, "WIFI", "esp_netif未就绪，无法强制设置DNS");
    return false;
  }

  esp_netif_dns_info_t dnsInfo;
  dnsInfo.ip.type = IPADDR_TYPE_V4;
  IP4_ADDR(&dnsInfo.ip.u_addr.ip4, dns1[0], dns1[1], dns1[2], dns1[3]);
  esp_err_t errMain = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dnsInfo);
  if (hasDns2) {
    IP4_ADDR(&dnsInfo.ip.u_addr.ip4, dns2[0], dns2[1], dns2[2], dns2[3]);
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dnsInfo);
  }

  return errMain == ESP_OK;
}

static bool reconnectWithStaticDns(const IPAddress& dns1, const IPAddress& dns2, bool hasDns2) {
  IPAddress ip = WiFi.localIP();
  IPAddress gw = WiFi.gatewayIP();
  IPAddress mask = WiFi.subnetMask();
  if (ip == IPAddress(0, 0, 0, 0) || gw == IPAddress(0, 0, 0, 0) || mask == IPAddress(0, 0, 0, 0)) {
    logManager.addLog(LOG_WARN, "WIFI", "当前IP信息无效，无法切换静态IP");
    return false;
  }

  WiFi.disconnect();
  delay(200);

  bool configOk = WiFi.config(ip, gw, mask, dns1, hasDns2 ? dns2 : dns1);
  logManager.addLog(configOk ? LOG_INFO : LOG_WARN, "WIFI",
    String("应用静态IP/DNS: ip=") + ip.toString() + ", gw=" + gw.toString() + ", mask=" + mask.toString());

  WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
  int retryAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && retryAttempts < 10) {
    delay(500);
    retryAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    logManager.addLog(LOG_INFO, "WIFI", "静态配置重连成功，IP: " + WiFi.localIP().toString());
    return true;
  }

  logManager.addLog(LOG_WARN, "WIFI", "静态配置重连失败");
  return false;
}

static String getDefaultTestUrl() {
  if (config.bark.url.length() > 0) {
    return config.bark.url;
  }
  return "https://api.day.app";
}

static void parseUrl(const String& url, String& scheme, String& host, uint16_t& port) {
  scheme = "http";
  host = "";
  port = 80;
  int schemePos = url.indexOf("://");
  int hostStart = 0;
  if (schemePos >= 0) {
    scheme = url.substring(0, schemePos);
    hostStart = schemePos + 3;
  }
  int pathStart = url.indexOf('/', hostStart);
  String hostPort = (pathStart >= 0) ? url.substring(hostStart, pathStart) : url.substring(hostStart);
  int colonPos = hostPort.indexOf(':');
  if (colonPos >= 0) {
    host = hostPort.substring(0, colonPos);
    port = hostPort.substring(colonPos + 1).toInt();
  } else {
    host = hostPort;
    port = (scheme == "https") ? 443 : 80;
  }
  if (host.length() == 0) {
    host = "api.day.app";
    scheme = "https";
    port = 443;
  }
}

static bool testTcpConnection(const String& host, uint16_t port, bool tls) {
  if (tls) {
    WiFiClientSecure client;
    client.setTimeout(5000);
    client.setInsecure();
    bool connected = client.connect(host.c_str(), port);
    client.stop();
    return connected;
  }
  WiFiClient client;
  client.setTimeout(5000);
  bool connected = client.connect(host.c_str(), port);
  client.stop();
  return connected;
}

void initWiFi() {
  // 添加延迟确保系统稳定
  delay(100);
  
  // 先断开所有连接
  WiFi.disconnect(true);
  delay(100);
  
  WiFi.mode(WIFI_STA);
  delay(100);
  
  if (config.wifi.ssid.length() > 0 && config.wifi.ssid != "SMS-Forwarder-Setup") {
    connectWiFi();
  } else {
    createAP();
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  delay(50);

  // 检查SSID有效性
  if (config.wifi.ssid.length() == 0 || config.wifi.ssid.length() > 32) {
    logManager.addLog(LOG_ERROR, "WIFI", "SSID无效，创建AP");
    createAP();
    return;
  }
  
  logManager.addLog(LOG_INFO, "WIFI", "连接WiFi: " + config.wifi.ssid);

  IPAddress dns1;
  IPAddress dns2;
  IPAddress staticIp;
  IPAddress staticGateway;
  IPAddress staticSubnet;
  String dns1Str = config.wifi.dns1;
  String dns2Str = config.wifi.dns2;
  String staticIpStr = config.wifi.staticIp;
  String staticGatewayStr = config.wifi.staticGateway;
  String staticSubnetStr = config.wifi.staticSubnet;
  dns1Str.trim();
  dns2Str.trim();
  staticIpStr.trim();
  staticGatewayStr.trim();
  staticSubnetStr.trim();
  bool dns1Ok = dns1.fromString(dns1Str);
  bool dns2Ok = dns2.fromString(dns2Str);
  bool ipOk = staticIp.fromString(staticIpStr);
  bool gatewayOk = staticGateway.fromString(staticGatewayStr);
  bool subnetOk = staticSubnet.fromString(staticSubnetStr);
  bool usingStaticConfig = false;
  auto applyCustomDns = [&](const String& prefix) {
    if (!dns1Ok) {
      logManager.addLog(LOG_WARN, "WIFI", "自定义DNS无效，忽略: " + dns1Str);
      return false;
    }
    bool ok = WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2Ok ? dns2 : INADDR_NONE);
    logManager.addLog(ok ? LOG_INFO : LOG_WARN, "WIFI",
      prefix + "使用自定义DNS: " + dns1Str + (dns2Ok ? (", " + dns2Str) : "") + (ok ? "" : " (设置失败)"));
    return ok;
  };

  if (config.wifi.useCustomDns && config.wifi.forceStaticDns) {
    if (ipOk && gatewayOk && subnetOk && dns1Ok) {
      bool ok = WiFi.config(staticIp, staticGateway, staticSubnet, dns1, dns2Ok ? dns2 : dns1);
      logManager.addLog(ok ? LOG_INFO : LOG_WARN, "WIFI",
        String("静态IP配置: ip=") + staticIpStr + ", gw=" + staticGatewayStr + ", mask=" + staticSubnetStr + ", dns=" +
        dns1Str + (dns2Ok ? (", " + dns2Str) : ""));
      usingStaticConfig = ok;
    } else {
      logManager.addLog(LOG_WARN, "WIFI", "静态IP参数不完整，已回退DHCP连接");
    }
  } else if (config.wifi.useCustomDns && !config.wifi.forceStaticDns) {
    applyCustomDns("");
  } else if (config.wifi.forceStaticDns) {
    logManager.addLog(LOG_WARN, "WIFI", "已启用静态IP但未配置DNS，建议同时填写DNS");
  }
  
  // 安全的WiFi连接
  WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(1000);
    attempts++;
    
    // 避免看门狗复位
    if (attempts % 5 == 0) {
      yield();
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    logManager.addLog(LOG_INFO, "WIFI", "连接成功，IP: " + WiFi.localIP().toString());
    if (config.wifi.useCustomDns) {
      IPAddress dns1 = WiFi.dnsIP(0);
      IPAddress dns2 = WiFi.dnsIP(1);
      logManager.addLog(LOG_INFO, "WIFI", "当前DNS: " + dns1.toString() + (dns2 == IPAddress(0, 0, 0, 0) ? "" : (", " + dns2.toString())));
      bool forced = false;
      if (dns1Ok) {
        if (config.wifi.forceStaticDns) {
          if (!usingStaticConfig) {
            forced = reconnectWithStaticDns(dns1, dns2, dns2Ok);
            logManager.addLog(forced ? LOG_INFO : LOG_WARN, "WIFI",
              String("强制设置DNS(静态IP): ") + dns1Str + (dns2Ok ? (", " + dns2Str) : ""));
          } else {
            forced = true;
          }
        } else {
          bool setOk = WiFi.setDNS(dns1, dns2Ok ? dns2 : IPAddress((uint32_t)0));
          logManager.addLog(setOk ? LOG_INFO : LOG_WARN, "WIFI",
            String("强制设置DNS(WiFi.setDNS): ") + dns1Str + (dns2Ok ? (", " + dns2Str) : ""));
          if (!setOk) {
            forced = applyCustomDnsEspNetif(dns1, dns2, dns2Ok);
            logManager.addLog(forced ? LOG_INFO : LOG_WARN, "WIFI",
              String("强制设置DNS(esp_netif): ") + dns1Str + (dns2Ok ? (", " + dns2Str) : ""));
          } else {
            forced = true;
          }
        }
      }
      if (!forced && dns1 == IPAddress(0, 0, 0, 0)) {
        logManager.addLog(LOG_WARN, "WIFI", "DNS仍为0.0.0.0，尝试重连并重新应用DNS");
        WiFi.disconnect();
        delay(200);
        if (applyCustomDns("重试")) {
          WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
          int retryAttempts = 0;
          while (WiFi.status() != WL_CONNECTED && retryAttempts < 10) {
            delay(500);
            retryAttempts++;
          }
          if (WiFi.status() == WL_CONNECTED) {
            IPAddress newDns1 = WiFi.dnsIP(0);
            IPAddress newDns2 = WiFi.dnsIP(1);
            logManager.addLog(LOG_INFO, "WIFI", "重连后DNS: " + newDns1.toString() + (newDns2 == IPAddress(0, 0, 0, 0) ? "" : (", " + newDns2.toString())));
            if (newDns1 == IPAddress(0, 0, 0, 0) && dns1Ok) {
              bool forcedRetry = config.wifi.forceStaticDns
                ? reconnectWithStaticDns(dns1, dns2, dns2Ok)
                : WiFi.setDNS(dns1, dns2Ok ? dns2 : IPAddress((uint32_t)0));
              logManager.addLog(forcedRetry ? LOG_INFO : LOG_WARN, "WIFI",
                String("强制设置DNS(") + (config.wifi.forceStaticDns ? "静态IP" : "WiFi.setDNS") + "): " + dns1Str + (dns2Ok ? (", " + dns2Str) : ""));
              IPAddress forcedDns1 = WiFi.dnsIP(0);
              IPAddress forcedDns2 = WiFi.dnsIP(1);
              logManager.addLog(LOG_INFO, "WIFI", "强制后DNS: " + forcedDns1.toString() + (forcedDns2 == IPAddress(0, 0, 0, 0) ? "" : (", " + forcedDns2.toString())));
            }
          } else {
            logManager.addLog(LOG_WARN, "WIFI", "重连失败，DNS仍可能无效");
          }
        }
      } else {
        IPAddress forcedDns1 = WiFi.dnsIP(0);
        IPAddress forcedDns2 = WiFi.dnsIP(1);
        logManager.addLog(LOG_INFO, "WIFI", "强制后DNS: " + forcedDns1.toString() + (forcedDns2 == IPAddress(0, 0, 0, 0) ? "" : (", " + forcedDns2.toString())));
      }
    }
  } else {
    logManager.addLog(LOG_ERROR, "WIFI", "连接失败，创建AP");
    createAP();
  }
}

void createAP() {
  // 先断开所有连接
  WiFi.disconnect(true);
  delay(100);
  
  WiFi.mode(WIFI_AP);
  delay(100);
  
  bool apStarted = WiFi.softAP("SMS-Forwarder-Setup", "12345678");
  
  if (apStarted) {
    Serial.println("已启动AP模式");
    Serial.println("SSID: SMS-Forwarder-Setup");
    Serial.println("Password: 12345678");
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    
    logManager.addLog(LOG_INFO, "WIFI", "AP模式，IP: " + WiFi.softAPIP().toString());
  } else {
    logManager.addLog(LOG_ERROR, "WIFI", "AP创建失败");
  }
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String getWiFiStatusText(wl_status_t status) {
  switch(status) {
    case WL_IDLE_STATUS: return "空闲";
    case WL_NO_SSID_AVAIL: return "SSID不可用";
    case WL_SCAN_COMPLETED: return "扫描完成";
    case WL_CONNECTED: return "已连接";
    case WL_CONNECT_FAILED: return "连接失败";
    case WL_CONNECTION_LOST: return "连接丢失";
    case WL_DISCONNECTED: return "已断开";
    default: return "未知状态";
  }
}

void pollWiFiReconnect() {
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 30000) {
    return;
  }
  lastAttempt = millis();

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED || status == WL_IDLE_STATUS) {
    return;
  }

  if (config.wifi.ssid.isEmpty() || config.wifi.ssid == "SMS-Forwarder-Setup") {
    return;
  }

  WiFiMode_t mode = WiFi.getMode();
  if (mode == WIFI_AP) {
    logManager.addLog(LOG_INFO, "WIFI", "AP模式定时重连WiFi: " + config.wifi.ssid);
  } else {
    logManager.addLog(LOG_WARN, "WIFI", "WiFi断开，尝试重连: " + config.wifi.ssid);
  }

  connectWiFi();
}

void diagnoseWiFi() {
  logManager.addLog(LOG_INFO, "WIFI", "WiFi诊断开始");
  
  wl_status_t status = WiFi.status();
  logManager.addLog(LOG_INFO, "WIFI", "WiFi状态: " + getWiFiStatusText(status));
  
  if (status == WL_CONNECTED) {
    logManager.addLog(LOG_INFO, "WIFI", "IP地址: " + WiFi.localIP().toString());
    logManager.addLog(LOG_INFO, "WIFI", "RSSI: " + String(WiFi.RSSI()) + "dBm");
    logManager.addLog(LOG_INFO, "WIFI", "BSSID: " + WiFi.BSSIDstr());
  } else {
    logManager.addLog(LOG_INFO, "WIFI", "配置SSID: " + config.wifi.ssid);
    logManager.addLog(LOG_INFO, "WIFI", "密码长度: " + String(config.wifi.password.length()));
    
    // 扫描可用网络
    int n = WiFi.scanNetworks();
    logManager.addLog(LOG_INFO, "WIFI", "扫描到 " + String(n) + " 个网络");
    
    bool ssidFound = false;
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == config.wifi.ssid) {
        ssidFound = true;
        logManager.addLog(LOG_INFO, "WIFI", "找到目标SSID: " + WiFi.SSID(i) + ", RSSI: " + String(WiFi.RSSI(i)));
        break;
      }
    }
    
    if (!ssidFound) {
      logManager.addLog(LOG_ERROR, "WIFI", "未找到目标SSID: " + config.wifi.ssid);
    }
  }
}

void diagnoseNetwork(const String& url, const String& method, const String& payload) {
  logManager.addLog(LOG_INFO, "NET", "网络诊断开始");
  if (WiFi.status() != WL_CONNECTED) {
    logManager.addLog(LOG_ERROR, "NET", "WiFi未连接，无法进行网络诊断");
    return;
  }

  logManager.addLog(LOG_INFO, "NET", "本机IP: " + WiFi.localIP().toString());
  logManager.addLog(LOG_INFO, "NET", "网关: " + WiFi.gatewayIP().toString());
  IPAddress dnsIp = WiFi.dnsIP();
  logManager.addLog(LOG_INFO, "NET", "DNS: " + dnsIp.toString());
  if (dnsIp == IPAddress(0, 0, 0, 0)) {
    logManager.addLog(LOG_ERROR, "NET", "DNS未配置(0.0.0.0)，域名解析可能失败");
  }

  String testUrl = url.length() > 0 ? url : getDefaultTestUrl();
  String scheme;
  String host;
  uint16_t port = 0;
  parseUrl(testUrl, scheme, host, port);

  logManager.addLog(LOG_INFO, "NET", "测试URL: " + testUrl);
  logManager.addLog(LOG_INFO, "NET", "解析目标: " + scheme + "://" + host + ":" + String(port));

  IPAddress ip;
  if (WiFi.hostByName(host.c_str(), ip)) {
    if (ip == IPAddress(0, 0, 0, 0)) {
      logManager.addLog(LOG_ERROR, "NET", "DNS解析异常: " + host + " -> 0.0.0.0");
      return;
    }
    logManager.addLog(LOG_INFO, "NET", "DNS解析: " + host + " -> " + ip.toString());
  } else {
    logManager.addLog(LOG_ERROR, "NET", "DNS解析失败: " + host);
    return;
  }

  bool tls = (scheme == "https");
  bool reachable = testTcpConnection(host, port, tls);
  logManager.addLog(reachable ? LOG_INFO : LOG_ERROR, "NET",
    String("网络可达性: ") + host + ":" + String(port) + " -> " + (reachable ? "可达" : "不可达"));

  if (!reachable) {
    return;
  }

  String methodUpper = method;
  methodUpper.toUpperCase();
  if (methodUpper != "GET" && methodUpper != "POST") {
    logManager.addLog(LOG_WARN, "NET", "HTTP测试跳过，方法无效: " + method);
    return;
  }

  HTTPClient http;
  if (tls) {
    WiFiClientSecure client;
    client.setInsecure();
    http.begin(client, testUrl);
  } else {
    WiFiClient client;
    http.begin(client, testUrl);
  }
  http.setTimeout(8000);

  int code = -1;
  if (methodUpper == "POST") {
    code = http.POST(payload);
  } else {
    code = http.GET();
  }
  String response = http.getString();
  http.end();

  logManager.addLog(LOG_INFO, "NET", "HTTP方法: " + methodUpper + ", 状态码: " + String(code));
  if (response.length() > 0) {
    String snippet = response.length() > 200 ? response.substring(0, 200) : response;
    logManager.addLog(LOG_INFO, "NET", "HTTP响应: " + snippet);
  } else {
    logManager.addLog(LOG_INFO, "NET", "HTTP响应: (empty)");
  }
}
