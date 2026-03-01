#include "i18n.h"
#include "config_manager.h"
#include <stdarg.h>
#include <string.h>

struct I18nEntry {
  const char* key;
  const char* zh;
  const char* en;
};

static const I18nEntry I18N_TABLE[] = {
  {"at_error", "\u6307\u4ee4\u9519\u8bef: %s", "AT error: %s"},
  {"at_test_retry", "AT\u6d4b\u8bd5\u5931\u8d25, \u91cd\u8bd5\u6b21\u6570: %s", "AT test failed, retries: %s"},

  {"battery_charge_started", "\u5f00\u59cb\u5145\u7535", "Charging started"},
  {"battery_charge_stopped", "\u505c\u6b62\u5145\u7535", "Charging stopped"},
  {"battery_charging", "\u5145\u7535\u4e2d", "Charging"},
  {"battery_charging_line", "\u5145\u7535\u72b6\u6001: %s", "Charging status: %s"},
  {"battery_critical_sleep", "\u6781\u4f4e\u7535\u91cf, \u8fdb\u5165\u4fdd\u62a4\u6a21\u5f0f", "Critical battery, entering protection mode"},
  {"battery_full", "\u7535\u6c60\u5df2\u5145\u6ee1", "Battery full"},
  {"battery_init_ok", "MAX17048\u521d\u59cb\u5316\u6210\u529f", "MAX17048 initialized"},
  {"battery_level_line", "\u5f53\u524d\u7535\u91cf: %s%%", "Battery level: %s%%"},
  {"battery_low_alert_sent", "\u53d1\u9001\u4f4e\u7535\u91cf\u8b66\u544a", "Low battery alert sent"},
  {"battery_low_header", "\u4f4e\u7535\u91cf\u8b66\u544a", "Low battery warning"},
  {"battery_not_charging", "\u672a\u5145\u7535", "Not charging"},
  {"battery_rate_line", "\u5145\u7535\u901f\u7387: %s%%/h", "Charge rate: %s%%/h"},
  {"battery_status_alert_sent", "\u53d1\u9001\u7535\u6c60\u72b6\u6001\u901a\u77e5: %s", "Battery status notification sent: %s"},
  {"battery_status_title", "\u7535\u6c60\u72b6\u6001\u901a\u77e5", "Battery status"},
  {"battery_update_title", "\u7535\u6c60\u66f4\u65b0", "Battery update"},
  {"battery_voltage_line", "\u7535\u6c60\u7535\u538b: %sV", "Battery voltage: %sV"},

  {"bool_false", "false", "false"},
  {"bool_no", "\u5426", "No"},
  {"bool_true", "true", "true"},
  {"bool_yes", "\u662f", "Yes"},
  {"value_unknown", "\u672a\u77e5", "Unknown"},

  {"cmt_length_parse", "\u89e3\u6790CMT\u957f\u5ea6: TPDU=%s\u5b57\u8282", "Parse CMT length: TPDU=%s bytes"},
  {"cmt_length_parse_simple", "\u4ece\u7b80\u5316\u683c\u5f0f\u89e3\u6790PDU\u957f\u5ea6: %s\u5b57\u8282", "Parse PDU length from simplified format: %s bytes"},
  {"cmt_pdu_length_fail", "PDU\u957f\u5ea6\u9a8c\u8bc1\u5931\u8d25: TPDU=%s\u5b57\u8282, \u5b9e\u9645=%s\u5b57\u7b26", "PDU length check failed: TPDU=%s bytes, actual=%s chars"},
  {"cmt_pdu_length_full", "\u4f7f\u7528\u5b8c\u6574PDU\u6570\u636e, \u957f\u5ea6: %s", "Using full PDU data, length: %s"},
  {"cmt_pdu_length_ok", "PDU\u957f\u5ea6\u9a8c\u8bc1\u901a\u8fc7: TPDU=%s\u5b57\u8282, \u603b\u957f\u5ea6=%s\u5b57\u7b26", "PDU length ok: TPDU=%s bytes, total=%s chars"},

  {"data_gprs_attach_retry", "GPRS\u672a\u9644\u7740, \u5c1d\u8bd5\u91cd\u65b0\u9644\u7740", "GPRS not attached, retrying"},
  {"data_roaming_block", "\u6f2b\u6e38\u4e2d\u7981\u6b62\u5f00\u542f\u6570\u636e\u8fde\u63a5", "Roaming: data connection blocked"},
  {"data_state_no_change", "\u6570\u636e\u8fde\u63a5\u72b6\u6001\u65e0\u9700\u6539\u53d8: %s", "Data connection unchanged: %s"},
  {"data_switch_fail", "\u5207\u6362\u6570\u636e\u8fde\u63a5\u5931\u8d25, \u54cd\u5e94: %s", "Failed to toggle data connection, resp: %s"},

  {"diag_done", "\u7f51\u7edc\u8bca\u65ad\u5b8c\u6210\uff08\u57fa\u4e8e\u7f13\u5b58\u72b6\u6001\uff09", "Network diagnostic complete (cached status)"},
  {"diag_network_connected", "\u7f51\u7edc\u8fde\u63a5: %s", "Network connected: %s"},
  {"diag_network_type", "\u7f51\u7edc\u7c7b\u578b: %s", "Network type: %s"},
  {"diag_operator", "\u8fd0\u8425\u5546: %s", "Operator: %s"},
  {"diag_roaming", "\u6f2b\u6e38\u72b6\u6001: %s", "Roaming: %s"},
  {"diag_signal", "\u4fe1\u53f7\u5f3a\u5ea6: %sdBm", "Signal: %sdBm"},
  {"diag_sim_status", "SIM\u72b6\u6001: %s", "SIM status: %s"},
  {"diag_start", "\u5f00\u59cb\u7f51\u7edc\u8bca\u65ad", "Start network diagnostics"},

  {"filter_blocked_keyword", "\u5305\u542b\u5c4f\u853d\u5173\u952e\u8bcd", "Blocked keyword detected"},
  {"filter_counts", "\u767d\u540d\u5355\u6570\u91cf: %s, \u5173\u952e\u8bcd\u6570\u91cf: %s", "Whitelist count: %s, keyword count: %s"},
  {"filter_not_in_whitelist", "\u53f7\u7801\u4e0d\u5728\u767d\u540d\u5355: %s", "Number not in whitelist: %s"},

  {"http_error", "HTTP\u9519\u8bef: code=%s, err=%s, resp=%s", "HTTP error: code=%s, err=%s, resp=%s"},

  {"init_cmd_fail", "\u6307\u4ee4\u5931\u8d25: %s - %s", "Init cmd failed: %s - %s"},
  {"init_cmd_ok", "\u6307\u4ee4\u6210\u529f: %s", "Init cmd ok: %s"},
  {"init_cmd_skip", "\u6307\u4ee4\u91cd\u8bd5\u5931\u8d25, \u8df3\u8fc7: %s", "Init cmd retries failed, skip: %s"},
  {"init_cmd_timeout_retry", "\u6307\u4ee4\u8d85\u65f6\u91cd\u8bd5: %s", "Init cmd timeout, retry: %s"},
  {"init_cmd_timeout_skip", "\u6307\u4ee4\u8d85\u65f6, \u8df3\u8fc7: %s", "Init cmd timeout, skip: %s"},

  {"ip_address", "\u83b7\u5f97IP\u5730\u5740: %s", "IP address: %s"},

  {"log_cleared", "\u65e5\u5fd7\u5df2\u6e05\u7a7a", "Logs cleared"},
  {"log_trimmed", "\u65e5\u5fd7\u5df2\u4fee\u526a", "Logs trimmed"},

  {"long_sms_parse_fail", "\u65e0\u6cd5\u89e3\u6790\u957f\u77ed\u4fe1\u4fe1\u606f", "Failed to parse long SMS info"},
  {"long_sms_parsed", "\u89e3\u6790\u957f\u77ed\u4fe1: ref=%s, total=%s, seq=%s", "Long SMS parsed: ref=%s, total=%s, seq=%s"},
  {"long_sms_store_fragment", "\u5b58\u50a8\u5206\u7247 %s/%s, \u53c2\u8003\u53f7:%s", "Store fragment %s/%s, ref:%s"},
  {"long_sms_wait_more", "\u7b49\u5f85\u66f4\u591a\u5206\u7247", "Waiting for more fragments"},

  {"net_cfg_apply", "\u914d\u7f51: %s, APN: %s", "Apply network: %s, APN: %s"},
  {"net_cfg_data_disabled", "\u6570\u636e\u7b56\u7565: \u7981\u7528\u79fb\u52a8\u6570\u636e", "Data policy: disable mobile data"},
  {"net_cfg_fail_ready", "\u7f51\u7edc\u914d\u7f6e\u5931\u8d25, \u8fdb\u5165\u5c31\u7eea\u72b6\u6001", "Network config failed, entering ready state"},
  {"net_cfg_ignore_cgact", "\u5ffd\u7565CGACT\u65ad\u5f00\u9650\u5236: %s", "Ignore CGACT disconnect restriction: %s"},
  {"net_cfg_operator_auto", "\u81ea\u52a8\u9009\u7f51", "Operator auto"},
  {"net_cfg_operator_cmcc", "\u9501\u5b9a\u4e2d\u56fd\u79fb\u52a8LTE", "Lock China Mobile LTE"},
  {"net_cfg_operator_ct", "\u9501\u5b9a\u4e2d\u56fd\u7535\u4fe1LTE", "Lock China Telecom LTE"},
  {"net_cfg_operator_cu", "\u9501\u5b9a\u4e2d\u56fd\u8054\u901aLTE", "Lock China Unicom LTE"},
  {"net_cfg_operator_giffgaff", "giffgaff \u9884\u8bbeAPN, \u81ea\u52a8\u9009\u7f51", "giffgaff preset APN, operator auto"},
  {"net_cfg_pdp_activate", "\u6fc0\u6d3bPDP\u4e0a\u4e0b\u6587", "Activate PDP context"},
  {"net_cfg_set_radio", "\u8bbe\u7f6e\u7f51\u7edc\u5236\u5f0f: %s", "Set radio mode: %s"},
  {"net_cfg_timeout_retry", "\u7f51\u7edc\u914d\u7f6e\u8d85\u65f6, \u91cd\u8bd5", "Network config timeout, retry"},

  {"net_diag_dns", "DNS: %s", "DNS: %s"},
  {"net_diag_dns_failed", "DNS\u89e3\u6790\u5931\u8d25: %s", "DNS resolve failed: %s"},
  {"net_diag_dns_resolved", "DNS\u89e3\u6790: %s -> %s", "DNS resolved: %s -> %s"},
  {"net_diag_dns_zero", "DNS\u672a\u914d\u7f6e(0.0.0.0), \u57df\u540d\u89e3\u6790\u53ef\u80fd\u5931\u8d25", "DNS not set (0.0.0.0), resolution may fail"},
  {"net_diag_dns_zero_resolved", "DNS\u89e3\u6790\u5f02\u5e38: %s -> 0.0.0.0", "DNS resolved to 0.0.0.0: %s"},
  {"net_diag_gateway", "\u7f51\u5173: %s", "Gateway: %s"},
  {"net_diag_http_empty", "HTTP\u54cd\u5e94: (empty)", "HTTP response: (empty)"},
  {"net_diag_http_method", "HTTP\u65b9\u6cd5: %s, \u72b6\u6001\u7801: %s", "HTTP method: %s, code: %s"},
  {"net_diag_http_response", "HTTP\u54cd\u5e94: %s", "HTTP response: %s"},
  {"net_diag_local_ip", "\u672c\u673aIP: %s", "Local IP: %s"},
  {"net_diag_method_invalid", "HTTP\u6d4b\u8bd5\u8df3\u8fc7, \u65b9\u6cd5\u65e0\u6548: %s", "HTTP test skipped, invalid method: %s"},
  {"net_diag_no_wifi", "WiFi\u672a\u8fde\u63a5, \u65e0\u6cd5\u8fdb\u884c\u7f51\u7edc\u8bca\u65ad", "WiFi not connected, cannot diagnose"},
  {"net_diag_reachability_fail", "\u7f51\u7edc\u53ef\u8fbe\u6027: %s:%s -> \u4e0d\u53ef\u8fbe", "Reachability: %s:%s -> unreachable"},
  {"net_diag_reachability_ok", "\u7f51\u7edc\u53ef\u8fbe\u6027: %s:%s -> \u53ef\u8fbe", "Reachability: %s:%s -> reachable"},
  {"net_diag_resolve_target", "\u89e3\u6790\u76ee\u6807: %s", "Resolve target: %s"},
  {"net_diag_start", "\u7f51\u7edc\u8bca\u65ad\u5f00\u59cb", "Network diagnostics start"},
  {"net_diag_test_url", "\u6d4b\u8bd5URL: %s", "Test URL: %s"},

  {"net_test_no_network", "\u7f51\u7edc\u672a\u8fde\u63a5", "Network not connected"},
  {"net_test_operator_signal", "\u8fd0\u8425\u5546: %s, \u4fe1\u53f7: %sdBm", "Operator: %s, signal: %sdBm"},
  {"net_test_pass", "\u7f51\u7edc\u8fde\u901a\u6027\u68c0\u67e5\u901a\u8fc7\uff08\u57fa\u4e8e\u72b6\u6001\u5224\u65ad\uff09", "Connectivity check passed (status-based)"},
  {"net_test_signal_weak", "\u4fe1\u53f7\u5f3a\u5ea6\u8fc7\u5f31: %sdBm", "Signal too weak: %sdBm"},
  {"net_test_sim_not_ready", "SIM\u6a21\u5757\u672a\u5c31\u7eea, \u65e0\u6cd5\u6d4b\u8bd5\u8fde\u901a\u6027", "SIM not ready, cannot test"},
  {"net_test_start", "\u5f00\u59cb\u7f51\u7edc\u8fde\u901a\u6027\u6d4b\u8bd5", "Start connectivity test"},

  {"network_init", "\u521d\u59cb\u5316\u7f51\u7edc\u7ba1\u7406", "Initialize network manager"},
  {"network_not_connected", "\u7f51\u7edc\u672a\u8fde\u63a5, \u5c1d\u8bd5\u91cd\u65b0\u6ce8\u518c", "Network not connected, trying to re-register"},
  {"network_roaming_detected", "\u68c0\u6d4b\u5230\u56fd\u9645\u6f2b\u6e38", "International roaming detected"},
  {"network_roaming_end", "\u6f2b\u6e38\u72b6\u6001\u7ed3\u675f", "Roaming ended"},
  {"network_signal_unavailable", "\u65e0\u6cd5\u83b7\u53d6\u4fe1\u53f7\u5f3a\u5ea6", "Signal strength unavailable"},
  {"network_signal_weak", "\u4fe1\u53f7\u5f3a\u5ea6\u8f83\u5f31: %sdBm", "Weak signal: %sdBm"},
  {"network_sim_not_ready", "SIM\u6a21\u5757\u672a\u5c31\u7eea, \u72b6\u6001: %s", "SIM not ready, state: %s"},

  {"notify_content", "\u5185\u5bb9: %s", "Content: %s"},
  {"notify_send_fail", "\u53d1\u9001\u5931\u8d25", "Send failed"},
  {"notify_send_success", "\u53d1\u9001\u6210\u529f", "Send succeeded"},
  {"notify_url", "URL: %s", "URL: %s"},

  {"ping_done", "\u7f51\u7edc\u8fde\u901a\u6027\u6d4b\u8bd5\u5b8c\u6210", "Ping test complete"},
  {"ping_line", "%s", "%s"},

  {"push_all_failed", "\u6240\u6709\u63a8\u9001\u5e73\u53f0\u5747\u5931\u8d25", "All push channels failed"},
  {"push_success_rate", "\u63a8\u9001\u6210\u529f %s/%s (%s%%)", "Push success %s/%s (%s%%)"},

  {"retry_cleared", "\u6e05\u7a7a\u91cd\u8bd5\u961f\u5217", "Retry queue cleared"},
  {"retry_give_up", "\u91cd\u8bd5\u6b21\u6570\u8d85\u9650, \u653e\u5f03\u63a8\u9001", "Retry limit reached, giving up"},
  {"retry_reschedule", "\u91cd\u8bd5\u5931\u8d25, \u91cd\u65b0\u5b89\u6392\u7b2c %s \u6b21\u5c1d\u8bd5", "Retry failed, rescheduling attempt %s"},
  {"retry_scheduled", "\u5b89\u6392\u91cd\u8bd5\u63a8\u9001: %s", "Scheduled retry: %s"},
  {"retry_still_failed", "\u91cd\u8bd5\u63a8\u9001\u4ecd\u5931\u8d25", "Retry push still failed"},
  {"retry_success", "\u91cd\u8bd5\u6210\u529f, \u79fb\u9664\u4efb\u52a1", "Retry succeeded, task removed"},
  {"retry_task_exists", "\u5df2\u5b58\u5728\u76f8\u540c\u4efb\u52a1, \u8df3\u8fc7: %s", "Task exists, skip: %s"},

  {"roam_alert_footer", "\u8bf7\u6ce8\u610f\u6f2b\u6e38\u8d39\u7528", "Please note roaming charges"},
  {"roam_alert_line_current", "\u5f53\u524d\u7f51\u7edc: %s", "Current network: %s"},
  {"roam_alert_line_home_code", "\u672c\u5730\u7f51\u7edc: %s", "Home network: %s"},
  {"roam_alert_line_operator_code", "\u7f51\u7edc\u4ee3\u7801: %s", "Network code: %s"},
  {"roam_alert_line_signal", "\u4fe1\u53f7\u5f3a\u5ea6: %sdBm", "Signal: %sdBm"},
  {"roam_alert_line_sim", "SIM/\u6f2b\u6e38: %s | %s", "Home/Roaming: %s | %s"},
  {"roam_alert_recent_skip", "\u6f2b\u6e38\u544a\u8b661\u5206\u949f\u5185\u5df2\u53d1\u9001, \u8df3\u8fc7\u901a\u77e5", "Roaming alert sent within 1 minute, skipped"},
  {"roam_alert_sent", "\u53d1\u9001\u6f2b\u6e38\u8b66\u544a", "Roaming alert sent"},
  {"roam_alert_title", "\u6f2b\u6e38\u8b66\u544a", "Roaming alert"},
  {"roam_data_disabled", "\u6f2b\u6e38\u65f6\u81ea\u52a8\u5173\u95ed\u6570\u636e\u8fde\u63a5", "Data disabled while roaming"},
  {"roam_data_restored", "\u6062\u590d\u6570\u636e\u8fde\u63a5", "Data connection restored"},

  {"sim_at_retry", "AT\u6d4b\u8bd5\u91cd\u8bd5: %s", "AT test retry: %s"},
  {"sim_at_test_fail_restart", "AT\u6d4b\u8bd5\u5931\u8d25, \u91cd\u65b0\u542f\u52a8\u6a21\u5757", "AT test failed, restarting module"},
  {"sim_at_test_start", "\u5f00\u59cbAT\u6d4b\u8bd5", "Start AT test"},
  {"sim_init_done", "\u521d\u59cb\u5316\u5b8c\u6210, \u914d\u7f6e\u7f51\u7edc", "Init done, configuring network"},
  {"sim_network_ready", "\u7f51\u7edc\u914d\u7f6e\u5b8c\u6210, \u6a21\u5757\u5c31\u7eea", "Network configured, module ready"},
  {"sim_power_on_start", "\u5f00\u59cb\u4e0a\u7535", "Power on start"},
  {"sim_power_pulse_done", "\u7535\u6e90\u63a7\u5236\u5b8c\u6210", "Power control done"},
  {"sim_ready", "SIM\u5361\u5c31\u7eea", "SIM ready"},
  {"sim_reset_check", "\u91cd\u7f6eSIM\u68c0\u6d4b\u72b6\u6001", "Reset SIM check status"},
  {"sim_state_change", "State: %s -> %s", "State: %s -> %s"},
  {"sim_state_unknown", "\u672a\u77e5\u72b6\u6001: %s", "Unknown state: %s"},
  {"sim_status", "State: %s, Ready: %s", "State: %s, Ready: %s"},

  {"sleep_config", "\u914d\u7f6e -> enabled=%s, timeout=%ss, mode=%s", "Config -> enabled=%s, timeout=%ss, mode=%s"},
  {"sleep_enter", "\u8fdb\u5165\u4f11\u7720\u6a21\u5f0f", "Entering sleep mode"},
  {"sleep_resume", "\u7cfb\u7edf\u5df2\u4ece\u4f11\u7720\u6062\u590d", "System resumed from sleep"},
  {"sleep_wakeup_reason", "\u5524\u9192, \u539f\u56e0: %s", "Wakeup reason: %s"},

  {"sms_batch_done", "\u6279\u91cf\u77ed\u4fe1\u5904\u7406\u5b8c\u6210", "Batch SMS processing done"},
  {"sms_batch_long_count", "\u53d1\u73b0 %s \u4e2a\u957f\u77ed\u4fe1\u5206\u7247", "Found %s long SMS fragments"},
  {"sms_batch_long_groups", "\u5904\u7406\u4e86 %s \u4e2a\u957f\u77ed\u4fe1\u7ec4", "Processed %s long SMS groups"},
  {"sms_batch_normal_count", "\u5904\u7406\u4e86 %s \u6761\u666e\u901a\u77ed\u4fe1", "Processed %s normal SMS"},
  {"sms_batch_start", "\u5f00\u59cb\u5904\u7406\u6279\u91cf\u77ed\u4fe1", "Start batch SMS processing"},
  {"sms_batch_temp_missing", "\u4e34\u65f6\u6587\u4ef6\u4e0d\u5b58\u5728", "Temp file missing"},
  {"sms_batch_temp_size", "\u4e34\u65f6\u6587\u4ef6\u5927\u5c0f: %s \u5b57\u8282", "Temp file size: %s bytes"},

  {"sms_buffer_overflow", "\u77ed\u4fe1\u7f13\u51b2\u533a\u6ea2\u51fa, \u5f3a\u5236\u5904\u7406", "SMS buffer overflow, forcing process"},
  {"sms_cfg_check", "\u68c0\u67e5\u77ed\u4fe1\u901a\u77e5\u914d\u7f6e", "Check SMS notification config"},
  {"sms_cmgl_content", "CMGL\u5185\u5bb9: %s", "CMGL content: %s"},
  {"sms_cmgl_line", "CMGL\u6570\u636e: %s", "CMGL line: %s"},
  {"sms_cmgl_manual_done", "\u624b\u52a8CMGL\u67e5\u8be2\u5b8c\u6210, \u5f00\u59cb\u5904\u7406\u6570\u636e", "Manual CMGL done, processing"},
  {"sms_cmgl_manual_fail", "\u624b\u52a8CMGL\u67e5\u8be2\u5931\u8d25", "Manual CMGL failed"},
  {"sms_cmgl_manual_start", "\u5f00\u59cb\u63a5\u6536\u624b\u52a8CMGL\u6570\u636e", "Start receiving manual CMGL"},
  {"sms_cmgl_manual_timeout", "\u624b\u52a8CMGL\u63a5\u6536\u8d85\u65f6, \u5f00\u59cb\u5904\u7406\u6570\u636e", "Manual CMGL timeout, processing"},
  {"sms_cmgl_new_count", "\u53d1\u73b0 %s \u6761\u65b0\u77ed\u4fe1", "Found %s new SMS"},
  {"sms_cmgl_no_sms", "\u6ca1\u6709\u77ed\u4fe1\u9700\u8981\u5904\u7406", "No SMS to process"},
  {"sms_cmgl_process_start", "\u5f00\u59cb\u5904\u7406CMGL\u54cd\u5e94, \u957f\u5ea6: %s", "Process CMGL response, length: %s"},
  {"sms_cmgl_query_fail", "CMGL\u67e5\u8be2\u5931\u8d25", "CMGL query failed"},
  {"sms_cmgl_receive_start", "\u5f00\u59cb\u63a5\u6536CMGL\u6570\u636e", "Start receiving CMGL"},
  {"sms_cmgl_start_all", "\u5f00\u59cb\u4f7f\u7528CMGL\u8bfb\u53d6\u6240\u6709\u77ed\u4fe1", "Start CMGL read all SMS"},
  {"sms_cmgl_timeout_process", "CMGL\u63a5\u6536\u8d85\u65f6, \u5f00\u59cb\u5904\u7406\u5df2\u6536\u96c6\u6570\u636e", "CMGL timeout, processing collected data"},
  {"sms_cmgr_empty", "\u7d22\u5f15 %s \u4e3a\u7a7a", "Index %s is empty"},
  {"sms_cmgr_found", "\u627e\u5230\u77ed\u4fe1\u7d22\u5f15 %s, \u5df2\u627e\u5230 %s/%s", "Found SMS index %s, total %s/%s"},
  {"sms_cmgr_parse_fail", "\u65e0\u6cd5\u89e3\u6790CMGR\u77ed\u4fe1\u7d22\u5f15 %s", "Failed to parse CMGR index %s"},
  {"sms_cmgr_poll_done", "CMGR\u8f6e\u8be2\u5b8c\u6210, \u627e\u5230 %s \u6761\u77ed\u4fe1", "CMGR polling done, found %s SMS"},
  {"sms_cmgr_poll_next", "\u7ee7\u7eed\u8bfb\u53d6\u7d22\u5f15 %s", "Continue reading index %s"},
  {"sms_cmgr_read_fail", "\u7d22\u5f15 %s \u8bfb\u53d6\u5931\u8d25", "Read index %s failed"},
  {"sms_cmgr_store", "\u5b58\u50a8CMGR\u77ed\u4fe1\u7d22\u5f15 %s, \u53d1\u9001\u65b9: %s", "Store CMGR index %s, sender: %s"},
  {"sms_cmt_direct", "\u6536\u5230\u76f4\u63a5\u77ed\u4fe1: %s", "Direct SMS received: %s"},
  {"sms_cmt_first_wait", "\u6536\u5230\u7b2c\u4e00\u6761CMT\u77ed\u4fe1, \u7b49\u5f855\u79d2", "First CMT SMS, wait 5s"},
  {"sms_cmt_long_fragment", "\u957f\u77ed\u4fe1\u5206\u7247: %s ref=%s %s/%s", "Long SMS fragment: %s ref=%s %s/%s"},
  {"sms_cmt_parse_fail", "\u65e0\u6cd5\u89e3\u6790CMT PDU\u6570\u636e", "Failed to parse CMT PDU"},
  {"sms_cmt_parsed", "\u89e3\u6790PDU: \u53d1\u9001\u65b9=%s, DCS=0x%s", "Parsed PDU: sender=%s, DCS=0x%s"},
  {"sms_cmt_pending_count", "\u5b58\u50a8CMT PDU, \u5f85\u5904\u7406\u6570\u91cf: %s", "Stored CMT PDU, pending: %s"},
  {"sms_cmt_process_start", "\u5f00\u59cb\u5904\u7406 %s \u6761CMT PDU", "Processing %s CMT PDU"},
  {"sms_cmt_sender_parse_fail", "\u65e0\u6cd5\u4ecePDU\u89e3\u6790\u53d1\u9001\u65b9", "Failed to parse sender from PDU"},
  {"sms_cmt_store_pending", "\u5b58\u50a8CMT\u77ed\u4fe1, \u5f85\u5904\u7406\u6570\u91cf: %s", "Stored CMT SMS, pending: %s"},
  {"sms_decode_fallback_ucs2", "7-bit\u89e3\u7801\u7591\u4f3c\u5f02\u5e38, \u56de\u9000\u4e3aUCS2\u89e3\u7801", "7-bit decode looks invalid, fallback to UCS2"},
  {"sms_delete", "\u5220\u9664\u77ed\u4fe1: %s", "Delete SMS: %s"},
  {"sms_delete_skip_cmt", "CMT\u77ed\u4fe1\u65e0\u9700\u5220\u9664, \u7d22\u5f15: %s", "CMT SMS no delete needed, index: %s"},
  {"sms_filtered", "\u77ed\u4fe1\u88ab\u8fc7\u6ee4\u5668\u62e6\u622a - \u53d1\u4ef6\u4eba: %s", "SMS filtered - sender: %s"},
  {"sms_first_notify_wait", "\u6536\u5230\u7b2c\u4e00\u6761\u77ed\u4fe1\u901a\u77e5\uff08\u7d22\u5f15 %s\uff09, \u7b49\u5f855\u79d2", "First SMS notify (index %s), wait 5s"},
  {"sms_forward_prepare", "\u51c6\u5907\u8f6c\u53d1\u6765\u81ea %s \u7684\u77ed\u4fe1", "Prepare to forward SMS from %s"},
  {"sms_forward_prepare_retry", "[\u91cd\u8bd5] \u51c6\u5907\u8f6c\u53d1\u6765\u81ea %s \u7684\u77ed\u4fe1", "[Retry] Prepare to forward SMS from %s"},
  {"sms_forward_title", "\u77ed\u4fe1\u8f6c\u53d1 - %s", "SMS Forward - %s"},
  {"sms_garbled_filtered", "[\u4e71\u7801\u5185\u5bb9\u5df2\u8fc7\u6ee4]", "[Garbled content filtered]"},
  {"sms_garbled_skip", "\u68c0\u6d4b\u5230\u4e71\u7801\u5185\u5bb9, \u8df3\u8fc7\u8f6c\u53d1 - \u53d1\u4ef6\u4eba: %s", "Garbled content, skip forwarding - sender: %s"},
  {"sms_list_found", "\u53d1\u73b0\u77ed\u4fe1: %s", "SMS found: %s"},
  {"sms_manual_check_start", "\u5f00\u59cb\u624b\u52a8\u67e5\u8be2\u6240\u6709\u77ed\u4fe1", "Manual check all SMS"},
  {"sms_manual_cmgr_start", "\u5f00\u59cbCMGR\u8f6e\u8be2, \u4ece\u7d22\u5f150\u523049", "Start CMGR polling, index 0-49"},
  {"sms_manual_count", "\u53d1\u73b0 %s \u6761\u77ed\u4fe1, \u5bb9\u91cf %s", "Found %s SMS, capacity %s"},
  {"sms_manual_none", "\u6ca1\u6709\u77ed\u4fe1\u9700\u8981\u5904\u7406", "No SMS to process"},
  {"sms_merge_timeout_process", "5\u79d2\u7b49\u5f85\u7ed3\u675f, \u5f00\u59cb\u5904\u7406\u77ed\u4fe1", "5s wait over, processing SMS"},
  {"sms_notify", "\u6536\u5230\u77ed\u4fe1\u901a\u77e5: %s", "SMS notification: %s"},
  {"sms_parse_empty", "\u65e0\u6cd5\u89e3\u6790\u77ed\u4fe1\u6570\u636e - Sender empty: %s, Content empty: %s", "Failed to parse SMS - Sender empty: %s, Content empty: %s"},
  {"sms_parse_sender_content", "\u53d1\u4ef6\u4eba: '%s', \u5185\u5bb9\u957f\u5ea6: %s", "Sender: '%s', Content length: %s"},
  {"sms_parse_start", "\u5f00\u59cb\u89e3\u6790\u7d22\u5f15 %s, \u6570\u636e\u957f\u5ea6: %s", "Parse index %s, data length: %s"},
  {"sms_pending_add", "\u6b63\u5728\u5904\u7406\u77ed\u4fe1, \u6dfb\u52a0\u7d22\u5f15 %s \u5230\u5f85\u5904\u7406\u6570\u7ec4", "Processing SMS, add index %s to pending"},
  {"sms_process_index", "\u5904\u7406\u77ed\u4fe1\u7d22\u5f15: %s", "Process SMS index: %s"},
  {"sms_raw_entry", "\u7d22\u5f15 %s, \u53d1\u9001\u65b9: %s, \u6570\u636e: %s", "Index %s, sender: %s, data: %s"},
  {"sms_read_start", "\u5f00\u59cb\u8bfb\u53d6\u77ed\u4fe1\u7d22\u5f15: %s", "Read SMS index: %s"},
  {"sms_received_log", "\u6536\u5230\u77ed\u4fe1, \u65f6\u95f4%s, \u53d1\u4ef6\u4eba%s, \u5185\u5bb9%s", "SMS received, time %s, sender %s, content %s"},
  {"sms_send_busy", "\u6a21\u5757\u5fd9\u788c, \u7a0d\u540e\u91cd\u8bd5", "Modem busy, retry later"},
  {"sms_send_cmgs_fail", "CMGS\u547d\u4ee4\u5931\u8d25: %s", "CMGS command failed: %s"},
  {"sms_send_fail", "\u77ed\u4fe1\u53d1\u9001\u5931\u8d25: %s", "SMS send failed: %s"},
  {"sms_send_not_registered", "\u7f51\u7edc\u672a\u6ce8\u518c, \u53d6\u6d88\u53d1\u9001", "Network not registered, cancel sending"},
  {"sms_send_prompt_timeout", "\u7b49\u5f85>\u63d0\u793a\u7b26\u8d85\u65f6", "Waiting for prompt timed out"},
  {"sms_send_sim_not_ready", "SIM\u6a21\u5757\u672a\u5c31\u7eea", "SIM not ready"},
  {"sms_send_success", "\u77ed\u4fe1\u53d1\u9001\u6210\u529f", "SMS sent"},
  {"sms_send_timeout", "\u77ed\u4fe1\u53d1\u9001\u8d85\u65f6", "SMS send timeout"},
  {"time_sync_start", "\u5f00\u59cbNTP\u5bf9\u65f6", "Start NTP sync"},
  {"time_sync_ok", "\u5bf9\u65f6\u6210\u529f, epoch=%s", "Time sync ok, epoch=%s"},
  {"time_sync_fail", "\u5bf9\u65f6\u5931\u8d25", "Time sync failed"},
  {"time_sync_no_wifi", "\u65e0WiFi\u8fde\u63a5, \u8df3\u8fc7\u5bf9\u65f6", "No WiFi, skip time sync"},
  {"time_sync_fallback", "\u65f6\u95f4\u672a\u540c\u6b65, \u4f7f\u7528\u8fd0\u884c\u65f6\u95f4\u6233", "Time not synced, using uptime timestamp"},
  {"time_sync_modem_start", "\u5f00\u59cbSIM\u65f6\u95f4\u5bf9\u65f6", "Start modem time sync"},
  {"time_sync_modem_ok", "SIM\u65f6\u95f4\u5bf9\u65f6\u6210\u529f, epoch=%s", "Modem time sync ok, epoch=%s"},
  {"time_sync_modem_fail", "SIM\u65f6\u95f4\u5bf9\u65f6\u5931\u8d25", "Modem time sync failed"},
  {"time_sync_modem_busy", "SIM\u5fd9, \u6682\u4e0d\u5bf9\u65f6", "Modem busy, skip time sync"},
  {"time_sync_modem_not_ready", "SIM\u672a\u5c31\u7eea, \u8df3\u8fc7\u5bf9\u65f6", "SIM not ready, skip time sync"},
  {"time_sync_manual_start", "手动对时开始", "Manual time sync start"},
  {"time_sync_manual_ok", "手动对时成功, source=%s", "Manual time sync ok, source=%s"},
  {"time_sync_manual_fail", "手动对时失败", "Manual time sync failed"},
  {"sms_send_to", "\u53d1\u9001\u77ed\u4fe1\u5230: %s", "Send SMS to: %s"},
  {"sms_sim_not_ready_skip", "SIM\u672a\u5c31\u7eea, \u8df3\u8fc7\u77ed\u4fe1\u5904\u7406", "SIM not ready, skip SMS"},
  {"sms_temp_stored", "\u5df2\u5b58\u50a8\u77ed\u4fe1\u7d22\u5f15: %s", "Stored SMS index: %s"},
  {"sms_valid_empty", "\u5185\u5bb9\u4e3a\u7a7a", "Content empty"},
  {"sms_valid_garbled", "\u4e71\u7801", "Garbled"},
  {"sms_valid_ok", "\u6709\u6548", "Valid"},
  {"sms_valid_stats", "\u5185\u5bb9\u957f\u5ea6: %s, \u6709\u6548\u5b57\u7b26: %s, \u6bd4\u4f8b: %s%%, %s", "Content length: %s, valid: %s, ratio: %s%%, %s"},

  {"stats_reset", "\u7edf\u8ba1\u6570\u636e\u5df2\u91cd\u7f6e", "Statistics reset"},

  {"status_busy_skip", "\u6a21\u5757\u5fd9\u788c, \u8df3\u8fc7\u72b6\u6001\u67e5\u8be2", "Modem busy, skip status query"},
  {"status_init_cache", "\u521d\u59cb\u5316\u7cfb\u7edf\u72b6\u6001\u7f13\u5b58", "Initialize status cache"},
  {"status_no_reg_keep", "\u672a\u83b7\u53d6\u6ce8\u518c\u72b6\u6001, \u4fdd\u7559\u4e0a\u6b21\u7ed3\u679c", "Registration not found, keep previous"},
  {"status_sim_not_ready_skip", "SIM\u6a21\u5757\u672a\u5c31\u7eea, \u8df3\u8fc7\u72b6\u6001\u67e5\u8be2", "SIM not ready, skip status"},
  {"status_update", "\u72b6\u6001\u66f4\u65b0: \u4fe1\u53f7=%sdBm, \u7f51\u7edc=%s(%s)", "Status update: signal=%sdBm, network=%s(%s)"},

  {"system_alert_title", "\u7cfb\u7edf\u8b66\u544a", "System alert"},

  {"wdt_already_disabled", "\u770b\u95e8\u72d7\u5df2\u5904\u4e8e\u5173\u95ed\u72b6\u6001", "WDT already disabled"},
  {"wdt_deinit_fail", "deinit\u5931\u8d25, err=%s", "WDT deinit failed, err=%s"},
  {"wdt_disable_fail", "\u5173\u95ed\u5931\u8d25, err=%s", "WDT disable failed, err=%s"},
  {"wdt_disabled", "\u770b\u95e8\u72d7\u5df2\u5173\u95ed", "WDT disabled"},
  {"wdt_enabled", "\u770b\u95e8\u72d7\u5df2\u542f\u7528, timeout=%ss", "WDT enabled, timeout=%ss"},
  {"wdt_init_fail", "\u521d\u59cb\u5316\u5931\u8d25, err=%s", "WDT init failed, err=%s"},
  {"wdt_reenable_fail", "\u91cd\u65b0\u542f\u7528\u5931\u8d25, err=%s", "WDT re-enable failed, err=%s"},
  {"wdt_reenabled", "\u770b\u95e8\u72d7\u5df2\u91cd\u65b0\u542f\u7528", "WDT re-enabled"},
  {"wdt_task_register_fail", "\u4efb\u52a1\u6ce8\u518c\u5931\u8d25, err=%s", "WDT task register failed, err=%s"},

  {"web_channel_serverchan", "Server\u9171", "ServerChan"},
  {"web_err_missing_id", "\u7f3a\u5c11ID", "Missing ID"},
  {"web_err_sms_id_invalid", "ID\u65e0\u6548", "Invalid ID"},
  {"web_err_sms_not_found", "\u77ed\u4fe1\u4e0d\u5b58\u5728", "SMS not found"},
  {"web_err_ssid_empty", "SSID\u4e0d\u80fd\u4e3a\u7a7a", "SSID cannot be empty"},
  {"web_invalid_json", "\u65e0\u6548JSON\u683c\u5f0f", "Invalid JSON"},
  {"web_led_hw_done", "LED\u786c\u4ef6\u6d4b\u8bd5\u5b8c\u6210", "LED hardware test completed"},
  {"web_led_state_done", "LED\u72b6\u6001\u6d4b\u8bd5\u5b8c\u6210", "LED state test completed"},
  {"web_led_test_invalid", "\u65e0\u6548\u6d4b\u8bd5\u7c7b\u578b", "Invalid test type"},
  {"web_missing_enabled", "\u7f3a\u5c11enabled\u53c2\u6570", "Missing enabled parameter"},
  {"web_refresh_invalid", "\u65e0\u6548\u7684\u5237\u65b0\u7c7b\u578b", "Invalid refresh type"},
  {"web_signal_refreshed", "\u4fe1\u53f7\u5f3a\u5ea6\u5df2\u5237\u65b0", "Signal refreshed"},
  {"web_sim_not_ready", "SIM\u6a21\u5757\u672a\u5c31\u7eea", "SIM module not ready"},
  {"web_sim_reset", "SIM\u68c0\u6d4b\u5df2\u91cd\u7f6e", "SIM check reset"},
  {"web_sms_check_started", "\u77ed\u4fe1\u67e5\u8be2\u5df2\u542f\u52a8\uff0c\u8bf7\u67e5\u770b\u65e5\u5fd7", "SMS check started, see logs"},
  {"web_sms_send_fail", "\u77ed\u4fe1\u53d1\u9001\u5931\u8d25", "SMS send failed"},
  {"web_sms_send_ok", "\u77ed\u4fe1\u53d1\u9001\u6210\u529f", "SMS sent"},
  {"web_sms_send_required", "\u624b\u673a\u53f7\u548c\u6d88\u606f\u4e0d\u80fd\u4e3a\u7a7a", "Phone number and message required"},
  {"web_status_refreshed", "\u6240\u6709\u72b6\u6001\u5df2\u5237\u65b0", "All status refreshed"},
  {"web_test_message", "\u77ed\u4fe1\u8f6c\u53d1\u5668\u6d4b\u8bd5\u6d88\u606f - %s", "SMS Forwarder test message - %s"},
  {"web_test_title", "\u6d4b\u8bd5\u63a8\u9001", "Test Push"},

  {"web_at_response", "\u54cd\u5e94: %s", "Response: %s"},
  {"web_at_send", "\u53d1\u9001AT\u6307\u4ee4: %s", "Send AT command: %s"},
  {"web_bark_config", "Bark\u914d\u7f6e: enabled=%s, key=%s", "Bark config: enabled=%s, key=%s"},
  {"web_bark_not_enabled", "Bark\u672a\u542f\u7528: enabled=%s, key=%s", "Bark disabled: enabled=%s, key=%s"},
  {"web_bark_test", "Bark\u6d4b\u8bd5: key=%s, url=%s", "Bark test: key=%s, url=%s"},
  {"web_battery_updated", "\u7535\u6c60\u914d\u7f6e\u5df2\u66f4\u65b0", "Battery config updated"},
  {"web_network_updated", "\u7f51\u7edc\u914d\u7f6e\u5df2\u66f4\u65b0", "Network config updated"},
  {"web_notify_test_start", "\u5f00\u59cb\u6d4b\u8bd5\u63a8\u9001", "Start push test"},
  {"web_notify_update_start", "\u5f00\u59cb\u66f4\u65b0\u63a8\u9001\u914d\u7f6e", "Start updating push config"},
  {"web_notify_updated", "\u63a8\u9001\u914d\u7f6e\u5df2\u66f4\u65b0", "Push config updated"},
  {"web_param", "\u53c2\u6570: %s=%s", "Param: %s=%s"},
  {"web_sms_check", "\u624b\u52a8\u67e5\u8be2\u77ed\u4fe1", "Manual SMS check"},
  {"web_sms_cleared", "\u77ed\u4fe1\u5b58\u50a8\u5df2\u6e05\u7a7a", "SMS storage cleared"},
  {"web_sms_deleted", "\u5220\u9664\u77ed\u4fe1: %s", "SMS deleted: %s"},
  {"web_sms_forward_manual", "\u624b\u52a8\u8f6c\u53d1\u77ed\u4fe1: %s, \u53d1\u9001\u65b9: %s", "Manual forward SMS: %s, sender: %s"},
  {"web_sms_forward_success", "\u77ed\u4fe1\u8f6c\u53d1\u6210\u529f: %s", "SMS forwarded: %s"},
  {"web_smsfilter_updated", "\u77ed\u4fe1\u8fc7\u6ee4\u914d\u7f6e\u5df2\u66f4\u65b0", "SMS filter updated"},
  {"web_stats_reset", "\u7edf\u8ba1\u6570\u636e\u5df2\u91cd\u7f6e", "Statistics reset"},
  {"web_system_updated", "\u7cfb\u7edf\u914d\u7f6e\u5df2\u66f4\u65b0", "System config updated"},
  {"web_wifi_updated", "WiFi\u914d\u7f6e\u5df2\u66f4\u65b0: %s", "WiFi config updated: %s"},

  {"wifi_ap_create_fail", "AP\u521b\u5efa\u5931\u8d25", "AP create failed"},
  {"wifi_ap_mode_ip", "AP\u6a21\u5f0f, IP: %s", "AP mode, IP: %s"},
  {"wifi_ap_reconnect", "AP\u6a21\u5f0f\u5b9a\u65f6\u91cd\u8fdeWiFi: %s", "AP mode reconnect WiFi: %s"},
  {"wifi_apply_static_config", "\u5e94\u7528\u9759\u6001IP/DNS: ip=%s, gw=%s, mask=%s", "Apply static IP/DNS: ip=%s, gw=%s, mask=%s"},
  {"wifi_apply_static_config_fail", "\u5e94\u7528\u9759\u6001IP/DNS\u5931\u8d25: ip=%s, gw=%s, mask=%s", "Apply static IP/DNS failed: ip=%s, gw=%s, mask=%s"},
  {"wifi_bssid_label", "BSSID: %s", "BSSID: %s"},
  {"wifi_config_password_len", "\u5bc6\u7801\u957f\u5ea6: %s", "Password length: %s"},
  {"wifi_config_ssid", "\u914d\u7f6eSSID: %s", "Configured SSID: %s"},
  {"wifi_connect_failed_ap", "\u8fde\u63a5\u5931\u8d25, \u521b\u5efaAP", "Connect failed, create AP"},
  {"wifi_connected_ip", "\u8fde\u63a5\u6210\u529f, IP: %s", "Connected, IP: %s"},
  {"wifi_connecting", "\u8fde\u63a5WiFi: %s", "Connecting WiFi: %s"},
  {"wifi_current_dns", "\u5f53\u524dDNS: %s", "Current DNS: %s"},
  {"wifi_custom_dns_invalid", "\u81ea\u5b9a\u4e49DNS\u65e0\u6548, \u5ffd\u7565: %s", "Invalid custom DNS, ignored: %s"},
  {"wifi_diag_start", "WiFi\u8bca\u65ad\u5f00\u59cb", "WiFi diagnostics start"},
  {"wifi_diag_status", "WiFi\u72b6\u6001: %s", "WiFi status: %s"},
  {"wifi_dns_after_force", "\u5f3a\u5236\u540eDNS: %s", "DNS after force: %s"},
  {"wifi_dns_after_reconnect", "\u91cd\u8fde\u540eDNS: %s", "DNS after reconnect: %s"},
  {"wifi_dns_zero_retry", "DNS\u4ecd\u4e3a0.0.0.0, \u5c1d\u8bd5\u91cd\u8fde\u5e76\u91cd\u65b0\u5e94\u7528DNS", "DNS still 0.0.0.0, retry reconnect and apply DNS"},
  {"wifi_force_dns_netif", "\u5f3a\u5236\u8bbe\u7f6eDNS(esp_netif): %s", "Force DNS (esp_netif): %s"},
  {"wifi_force_dns_netif_fail", "\u5f3a\u5236\u8bbe\u7f6eDNS(esp_netif)\u5931\u8d25: %s", "Force DNS (esp_netif) failed: %s"},
  {"wifi_force_dns_setdns", "\u5f3a\u5236\u8bbe\u7f6eDNS(WiFi.setDNS): %s", "Force DNS (WiFi.setDNS): %s"},
  {"wifi_force_dns_setdns_fail", "\u5f3a\u5236\u8bbe\u7f6eDNS(WiFi.setDNS)\u5931\u8d25: %s", "Force DNS (WiFi.setDNS) failed: %s"},
  {"wifi_force_dns_setdns_retry", "\u91cd\u8bd5\u5f3a\u5236\u8bbe\u7f6eDNS(WiFi.setDNS): %s", "Retry force DNS (WiFi.setDNS): %s"},
  {"wifi_force_dns_setdns_retry_fail", "\u91cd\u8bd5\u5f3a\u5236\u8bbe\u7f6eDNS(WiFi.setDNS)\u5931\u8d25: %s", "Retry force DNS (WiFi.setDNS) failed: %s"},
  {"wifi_force_dns_static", "\u5f3a\u5236\u8bbe\u7f6eDNS(\u9759\u6001IP): %s", "Force DNS (static IP): %s"},
  {"wifi_force_dns_static_fail", "\u5f3a\u5236\u8bbe\u7f6eDNS(\u9759\u6001IP)\u5931\u8d25: %s", "Force DNS (static IP) failed: %s"},
  {"wifi_force_dns_static_retry", "\u91cd\u8bd5\u5f3a\u5236\u8bbe\u7f6eDNS(\u9759\u6001IP): %s", "Retry force DNS (static IP): %s"},
  {"wifi_force_dns_static_retry_fail", "\u91cd\u8bd5\u5f3a\u5236\u8bbe\u7f6eDNS(\u9759\u6001IP)\u5931\u8d25: %s", "Retry force DNS (static IP) failed: %s"},
  {"wifi_ip_info_invalid", "\u5f53\u524dIP\u4fe1\u606f\u65e0\u6548, \u65e0\u6cd5\u5207\u6362\u9759\u6001IP", "Invalid current IP info, cannot switch to static"},
  {"wifi_ip_label", "IP\u5730\u5740: %s", "IP address: %s"},
  {"wifi_netif_not_ready", "esp_netif\u672a\u5c31\u7eea, \u65e0\u6cd5\u5f3a\u5236\u8bbe\u7f6eDNS", "esp_netif not ready, cannot force DNS"},
  {"wifi_reconnect", "WiFi\u65ad\u5f00, \u5c1d\u8bd5\u91cd\u8fde: %s", "WiFi disconnected, reconnecting: %s"},
  {"wifi_reconnect_fail_count", "\u8fde\u63a5\u5931\u8d25\u6b21\u6570: %s/%s", "Connect failures: %s/%s"},
  {"wifi_reconnect_fail_threshold", "\u8fde\u63a5\u5931\u8d25\u8fbe\u9608\u503c: %s/%s, \u5207\u6362AP", "Connect failures reached: %s/%s, switch to AP"},
  {"wifi_reconnect_fail_dns", "\u91cd\u8fde\u5931\u8d25, DNS\u4ecd\u53ef\u80fd\u65e0\u6548", "Reconnect failed, DNS may still be invalid"},
  {"wifi_rssi_label", "RSSI: %s dBm", "RSSI: %s dBm"},
  {"wifi_scan_count", "\u626b\u63cf\u5230 %s \u4e2a\u7f51\u7edc", "Found %s networks"},
  {"wifi_scan_found", "\u627e\u5230\u76ee\u6807SSID: %s, RSSI: %s", "Found SSID: %s, RSSI: %s"},
  {"wifi_scan_not_found", "\u672a\u627e\u5230\u76ee\u6807SSID: %s", "Target SSID not found: %s"},
  {"wifi_ssid_invalid", "SSID\u65e0\u6548, \u521b\u5efaAP", "Invalid SSID, creating AP"},
  {"wifi_static_config_fail", "\u9759\u6001IP\u914d\u7f6e\u5931\u8d25: ip=%s, gw=%s, mask=%s, dns=%s", "Static IP config failed: ip=%s, gw=%s, mask=%s, dns=%s"},
  {"wifi_static_config_incomplete", "\u9759\u6001IP\u53c2\u6570\u4e0d\u5b8c\u6574, \u5df2\u56de\u9000DHCP\u8fde\u63a5", "Static IP params incomplete, fallback to DHCP"},
  {"wifi_static_config_ok", "\u9759\u6001IP\u914d\u7f6e: ip=%s, gw=%s, mask=%s, dns=%s", "Static IP config: ip=%s, gw=%s, mask=%s, dns=%s"},
  {"wifi_static_missing_dns", "\u5df2\u542f\u7528\u9759\u6001IP\u4f46\u672a\u914d\u7f6eDNS, \u5efa\u8bae\u540c\u65f6\u586b\u5199DNS", "Static IP enabled but DNS missing"},
  {"wifi_static_reconnect_fail", "\u9759\u6001\u914d\u7f6e\u91cd\u8fde\u5931\u8d25", "Static config reconnect failed"},
  {"wifi_static_reconnect_ok", "\u9759\u6001\u914d\u7f6e\u91cd\u8fde\u6210\u529f, IP: %s", "Static config reconnect ok, IP: %s"},
  {"wifi_status_connect_fail", "\u8fde\u63a5\u5931\u8d25", "Connect failed"},
  {"wifi_status_connected", "\u5df2\u8fde\u63a5", "Connected"},
  {"wifi_status_connection_lost", "\u8fde\u63a5\u4e22\u5931", "Connection lost"},
  {"wifi_status_disconnected", "\u5df2\u65ad\u5f00", "Disconnected"},
  {"wifi_status_idle", "\u7a7a\u95f2", "Idle"},
  {"wifi_status_no_ssid", "SSID\u4e0d\u53ef\u7528", "SSID not available"},
  {"wifi_status_scan_done", "\u626b\u63cf\u5b8c\u6210", "Scan completed"},
  {"wifi_status_unknown", "\u672a\u77e5\u72b6\u6001", "Unknown"}
};

static const size_t I18N_TABLE_SIZE = sizeof(I18N_TABLE) / sizeof(I18N_TABLE[0]);

const char* normalizeLangCode(const String& lang) {
  String value = lang;
  value.trim();
  value.toLowerCase();
  if (value.startsWith("zh")) return "zh";
  if (value.startsWith("en")) return "en";
  return "auto";
}

const char* getCurrentLangCode() {
  const char* normalized = normalizeLangCode(config.lang);
  if (strcmp(normalized, "en") == 0) return "en";
  if (strcmp(normalized, "zh") == 0) return "zh";
  return "zh";
}

const char* i18nGet(const char* key, const char* lang) {
  if (!key || key[0] == '\0') return "";
  const char* useLang = lang ? lang : getCurrentLangCode();
  for (size_t i = 0; i < I18N_TABLE_SIZE; i++) {
    if (strcmp(I18N_TABLE[i].key, key) == 0) {
      if (strcmp(useLang, "en") == 0) {
        return I18N_TABLE[i].en;
      }
      return I18N_TABLE[i].zh;
    }
  }
  return key;
}

const char* i18nGet(const char* key) {
  return i18nGet(key, getCurrentLangCode());
}

String i18nFormat(const char* key, ...) {
  const char* format = i18nGet(key, getCurrentLangCode());
  char buffer[256];
  va_list args;
  va_start(args, key);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  return String(buffer);
}
