#include "wifi_manager.h"
#include "config_manager.h"
#include "log_manager.h"
#include "i18n.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_netif.h>
#include <lwip/ip4_addr.h>

static bool applyCustomDnsEspNetif(const IPAddress& dns1, const IPAddress& dns2, bool hasDns2) {
  esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif == nullptr) {
    LOGW("WIFI", "wifi_netif_not_ready");
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
    LOGW("WIFI", "wifi_ip_info_invalid");
    return false;
  }

  WiFi.disconnect();
  delay(200);

  bool configOk = WiFi.config(ip, gw, mask, dns1, hasDns2 ? dns2 : dns1);
  if (configOk) {
    LOGI("WIFI", "wifi_apply_static_config", ip.toString().c_str(), gw.toString().c_str(), mask.toString().c_str());
  } else {
    LOGW("WIFI", "wifi_apply_static_config_fail", ip.toString().c_str(), gw.toString().c_str(), mask.toString().c_str());
  }

  WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
  int retryAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && retryAttempts < 10) {
    delay(500);
    retryAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    LOGI("WIFI", "wifi_static_reconnect_ok", WiFi.localIP().toString().c_str());
    return true;
  }

  LOGW("WIFI", "wifi_static_reconnect_fail");
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
    LOGE("WIFI", "wifi_ssid_invalid");
    createAP();
    return;
  }
  
  LOGI("WIFI", "wifi_connecting", config.wifi.ssid.c_str());

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
  auto applyCustomDns = [&](bool isRetry) {
    if (!dns1Ok) {
      LOGW("WIFI", "wifi_custom_dns_invalid", dns1Str.c_str());
      return false;
    }
    bool ok = WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2Ok ? dns2 : INADDR_NONE);
    String dnsDisplay = dns1Str + (dns2Ok ? (", " + dns2Str) : "");
    if (ok) {
      LOGI("WIFI", isRetry ? "wifi_custom_dns_retry_ok" : "wifi_custom_dns_ok", dnsDisplay.c_str());
    } else {
      LOGW("WIFI", isRetry ? "wifi_custom_dns_retry_fail" : "wifi_custom_dns_fail", dnsDisplay.c_str());
    }
    return ok;
  };

  if (config.wifi.useCustomDns && config.wifi.forceStaticDns) {
    if (ipOk && gatewayOk && subnetOk && dns1Ok) {
      bool ok = WiFi.config(staticIp, staticGateway, staticSubnet, dns1, dns2Ok ? dns2 : dns1);
      String dnsDisplay = dns1Str + (dns2Ok ? (", " + dns2Str) : "");
      if (ok) {
        LOGI("WIFI", "wifi_static_config_ok", staticIpStr.c_str(), staticGatewayStr.c_str(), staticSubnetStr.c_str(), dnsDisplay.c_str());
      } else {
        LOGW("WIFI", "wifi_static_config_fail", staticIpStr.c_str(), staticGatewayStr.c_str(), staticSubnetStr.c_str(), dnsDisplay.c_str());
      }
      usingStaticConfig = ok;
    } else {
      LOGW("WIFI", "wifi_static_config_incomplete");
    }
  } else if (config.wifi.useCustomDns && !config.wifi.forceStaticDns) {
    applyCustomDns(false);
  } else if (config.wifi.forceStaticDns) {
    LOGW("WIFI", "wifi_static_missing_dns");
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
    LOGI("WIFI", "wifi_connected_ip", WiFi.localIP().toString().c_str());
    if (config.wifi.useCustomDns) {
      IPAddress dns1 = WiFi.dnsIP(0);
      IPAddress dns2 = WiFi.dnsIP(1);
      String dnsDisplay = dns1.toString() + (dns2 == IPAddress(0, 0, 0, 0) ? "" : (", " + dns2.toString()));
      LOGI("WIFI", "wifi_current_dns", dnsDisplay.c_str());
      bool forced = false;
      if (dns1Ok) {
        if (config.wifi.forceStaticDns) {
          if (!usingStaticConfig) {
            forced = reconnectWithStaticDns(dns1, dns2, dns2Ok);
            String dnsForced = dns1Str + (dns2Ok ? (", " + dns2Str) : "");
            if (forced) {
              LOGI("WIFI", "wifi_force_dns_static", dnsForced.c_str());
            } else {
              LOGW("WIFI", "wifi_force_dns_static_fail", dnsForced.c_str());
            }
          } else {
            forced = true;
          }
        } else {
          bool setOk = WiFi.setDNS(dns1, dns2Ok ? dns2 : IPAddress((uint32_t)0));
          String dnsForced = dns1Str + (dns2Ok ? (", " + dns2Str) : "");
          if (setOk) {
            LOGI("WIFI", "wifi_force_dns_setdns", dnsForced.c_str());
          } else {
            LOGW("WIFI", "wifi_force_dns_setdns_fail", dnsForced.c_str());
          }
          if (!setOk) {
            forced = applyCustomDnsEspNetif(dns1, dns2, dns2Ok);
            if (forced) {
              LOGI("WIFI", "wifi_force_dns_netif", dnsForced.c_str());
            } else {
              LOGW("WIFI", "wifi_force_dns_netif_fail", dnsForced.c_str());
            }
          } else {
            forced = true;
          }
        }
      }
        if (!forced && dns1 == IPAddress(0, 0, 0, 0)) {
        LOGW("WIFI", "wifi_dns_zero_retry");
        WiFi.disconnect();
        delay(200);
        if (applyCustomDns(true)) {
          WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
          int retryAttempts = 0;
          while (WiFi.status() != WL_CONNECTED && retryAttempts < 10) {
            delay(500);
            retryAttempts++;
          }
          if (WiFi.status() == WL_CONNECTED) {
            IPAddress newDns1 = WiFi.dnsIP(0);
            IPAddress newDns2 = WiFi.dnsIP(1);
            String dnsAfter = newDns1.toString() + (newDns2 == IPAddress(0, 0, 0, 0) ? "" : (", " + newDns2.toString()));
            LOGI("WIFI", "wifi_dns_after_reconnect", dnsAfter.c_str());
            if (newDns1 == IPAddress(0, 0, 0, 0) && dns1Ok) {
              bool forcedRetry = config.wifi.forceStaticDns
                ? reconnectWithStaticDns(dns1, dns2, dns2Ok)
                : WiFi.setDNS(dns1, dns2Ok ? dns2 : IPAddress((uint32_t)0));
              String dnsForced = dns1Str + (dns2Ok ? (", " + dns2Str) : "");
              if (forcedRetry) {
                if (config.wifi.forceStaticDns) {
                  LOGI("WIFI", "wifi_force_dns_static_retry", dnsForced.c_str());
                } else {
                  LOGI("WIFI", "wifi_force_dns_setdns_retry", dnsForced.c_str());
                }
              } else {
                if (config.wifi.forceStaticDns) {
                  LOGW("WIFI", "wifi_force_dns_static_retry_fail", dnsForced.c_str());
                } else {
                  LOGW("WIFI", "wifi_force_dns_setdns_retry_fail", dnsForced.c_str());
                }
              }
              IPAddress forcedDns1 = WiFi.dnsIP(0);
              IPAddress forcedDns2 = WiFi.dnsIP(1);
              String forcedDisplay = forcedDns1.toString() + (forcedDns2 == IPAddress(0, 0, 0, 0) ? "" : (", " + forcedDns2.toString()));
              LOGI("WIFI", "wifi_dns_after_force", forcedDisplay.c_str());
            }
          } else {
            LOGW("WIFI", "wifi_reconnect_fail_dns");
          }
        }
      } else {
        IPAddress forcedDns1 = WiFi.dnsIP(0);
        IPAddress forcedDns2 = WiFi.dnsIP(1);
        String forcedDisplay = forcedDns1.toString() + (forcedDns2 == IPAddress(0, 0, 0, 0) ? "" : (", " + forcedDns2.toString()));
        LOGI("WIFI", "wifi_dns_after_force", forcedDisplay.c_str());
      }
    }
  } else {
    LOGE("WIFI", "wifi_connect_failed_ap");
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
    
    LOGI("WIFI", "wifi_ap_mode_ip", WiFi.softAPIP().toString().c_str());
  } else {
    LOGE("WIFI", "wifi_ap_create_fail");
  }
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String getWiFiStatusText(wl_status_t status) {
  switch(status) {
    case WL_IDLE_STATUS: return i18nGet("wifi_status_idle");
    case WL_NO_SSID_AVAIL: return i18nGet("wifi_status_no_ssid");
    case WL_SCAN_COMPLETED: return i18nGet("wifi_status_scan_done");
    case WL_CONNECTED: return i18nGet("wifi_status_connected");
    case WL_CONNECT_FAILED: return i18nGet("wifi_status_connect_fail");
    case WL_CONNECTION_LOST: return i18nGet("wifi_status_connection_lost");
    case WL_DISCONNECTED: return i18nGet("wifi_status_disconnected");
    default: return i18nGet("wifi_status_unknown");
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
    LOGI("WIFI", "wifi_ap_reconnect", config.wifi.ssid.c_str());
  } else {
    LOGW("WIFI", "wifi_reconnect", config.wifi.ssid.c_str());
  }

  connectWiFi();
}

void diagnoseWiFi() {
  LOGI("WIFI", "wifi_diag_start");
  
  wl_status_t status = WiFi.status();
  LOGI("WIFI", "wifi_diag_status", getWiFiStatusText(status).c_str());
  
  if (status == WL_CONNECTED) {
    LOGI("WIFI", "wifi_ip_label", WiFi.localIP().toString().c_str());
    LOGI("WIFI", "wifi_rssi_label", String(WiFi.RSSI()).c_str());
    LOGI("WIFI", "wifi_bssid_label", WiFi.BSSIDstr().c_str());
  } else {
    LOGI("WIFI", "wifi_config_ssid", config.wifi.ssid.c_str());
    LOGI("WIFI", "wifi_config_password_len", String(config.wifi.password.length()).c_str());
    
    // 扫描可用网络
    int n = WiFi.scanNetworks();
    LOGI("WIFI", "wifi_scan_count", String(n).c_str());
    
    bool ssidFound = false;
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == config.wifi.ssid) {
        ssidFound = true;
        LOGI("WIFI", "wifi_scan_found", WiFi.SSID(i).c_str(), String(WiFi.RSSI(i)).c_str());
        break;
      }
    }
    
    if (!ssidFound) {
      LOGE("WIFI", "wifi_scan_not_found", config.wifi.ssid.c_str());
    }
  }
}

void diagnoseNetwork(const String& url, const String& method, const String& payload) {
  LOGI("NET", "net_diag_start");
  if (WiFi.status() != WL_CONNECTED) {
    LOGE("NET", "net_diag_no_wifi");
    return;
  }

  LOGI("NET", "net_diag_local_ip", WiFi.localIP().toString().c_str());
  LOGI("NET", "net_diag_gateway", WiFi.gatewayIP().toString().c_str());
  IPAddress dnsIp = WiFi.dnsIP();
  LOGI("NET", "net_diag_dns", dnsIp.toString().c_str());
  if (dnsIp == IPAddress(0, 0, 0, 0)) {
    LOGE("NET", "net_diag_dns_zero");
  }

  String testUrl = url.length() > 0 ? url : getDefaultTestUrl();
  String scheme;
  String host;
  uint16_t port = 0;
  parseUrl(testUrl, scheme, host, port);

  LOGI("NET", "net_diag_test_url", testUrl.c_str());
  String resolveTarget = scheme + "://" + host + ":" + String(port);
  LOGI("NET", "net_diag_resolve_target", resolveTarget.c_str());

  IPAddress ip;
  if (WiFi.hostByName(host.c_str(), ip)) {
    if (ip == IPAddress(0, 0, 0, 0)) {
      LOGE("NET", "net_diag_dns_zero_resolved", host.c_str());
      return;
    }
    LOGI("NET", "net_diag_dns_resolved", host.c_str(), ip.toString().c_str());
  } else {
    LOGE("NET", "net_diag_dns_failed", host.c_str());
    return;
  }

  bool tls = (scheme == "https");
  bool reachable = testTcpConnection(host, port, tls);
  if (reachable) {
    LOGI("NET", "net_diag_reachability_ok", host.c_str(), String(port).c_str());
  } else {
    LOGE("NET", "net_diag_reachability_fail", host.c_str(), String(port).c_str());
  }

  if (!reachable) {
    return;
  }

  String methodUpper = method;
  methodUpper.toUpperCase();
  if (methodUpper != "GET" && methodUpper != "POST") {
    LOGW("NET", "net_diag_method_invalid", method.c_str());
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

  LOGI("NET", "net_diag_http_method", methodUpper.c_str(), String(code).c_str());
  if (response.length() > 0) {
    String snippet = response.length() > 200 ? response.substring(0, 200) : response;
    LOGI("NET", "net_diag_http_response", snippet.c_str());
  } else {
    LOGI("NET", "net_diag_http_empty");
  }
}
