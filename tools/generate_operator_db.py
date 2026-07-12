#!/usr/bin/env python3
"""Generate the built-in MCC/MNC operator table from mcc-mnc.com."""

import json
import urllib.request
from pathlib import Path


SOURCE_URL = "https://mcc-mnc.com/api/v1/mcc-mnc.php"
REPO_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_PATH = REPO_ROOT / "sms_forwarder_esp32s3_sim7670g/src/operator_db.cpp"

MANUAL_OVERRIDES = {
    "23410": ("giffgaff", "giffgaff"),
    "46000": ("\u4e2d\u56fd\u79fb\u52a8", "China Mobile"),
    "46001": ("\u4e2d\u56fd\u8054\u901a", "China Unicom"),
    "46002": ("\u4e2d\u56fd\u79fb\u52a8", "China Mobile"),
    "46003": ("\u4e2d\u56fd\u7535\u4fe1", "China Telecom"),
    "46004": ("\u4e2d\u56fd\u79fb\u52a8", "China Mobile"),
    "46005": ("\u4e2d\u56fd\u7535\u4fe1", "China Telecom"),
    "46006": ("\u4e2d\u56fd\u8054\u901a", "China Unicom"),
    "46007": ("\u4e2d\u56fd\u79fb\u52a8", "China Mobile"),
    "46009": ("\u4e2d\u56fd\u8054\u901a", "China Unicom"),
    "46011": ("\u4e2d\u56fd\u7535\u4fe1", "China Telecom"),
}


def escape_cpp(value):
    output = []
    for char in value:
        codepoint = ord(char)
        if char == "\\":
            output.append("\\\\")
        elif char == '"':
            output.append('\\"')
        elif char == "\n":
            output.append("\\n")
        elif char == "\r":
            output.append("\\r")
        elif char == "\t":
            output.append("\\t")
        elif 32 <= codepoint <= 126:
            output.append(char)
        else:
            output.append("\\u%04x" % codepoint)
    return "".join(output)


def normalize_code(mcc, mnc):
    mnc_text = str(mnc).strip()
    if len(mnc_text) == 1:
        mnc_text = "0" + mnc_text
    return str(mcc).strip() + mnc_text


def fetch_source_data():
    request = urllib.request.Request(
        SOURCE_URL,
        headers={"User-Agent": "sms-forwarder-operator-db-generator"},
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def build_entries(payload):
    entries = {}
    for row in payload["data"]:
        code = normalize_code(row["mcc"], row["mnc"])
        network = row["network"].strip()
        entries[code] = (network, network)

    entries.update(MANUAL_OVERRIDES)
    return sorted(entries.items())


def render_operator_db(payload, items):
    generated = payload.get("generated", "unknown")
    lines = [
        '#include "operator_db.h"',
        "#include <string.h>",
        "",
        "struct OperatorEntry {",
        "  const char* code;",
        "  const char* nameZh;",
        "  const char* nameEn;",
        "};",
        "",
        f"// Source: {SOURCE_URL} (generated: {generated}).",
        "static const OperatorEntry OPERATOR_TABLE[] = {",
    ]

    for index, (code, names) in enumerate(items):
        comma = "," if index + 1 < len(items) else ""
        name_zh = escape_cpp(names[0])
        name_en = escape_cpp(names[1])
        lines.append(f'  {{"{code}", "{name_zh}", "{name_en}"}}{comma}')

    lines.extend([
        "};",
        "",
        "static const size_t OPERATOR_COUNT = sizeof(OPERATOR_TABLE) / sizeof(OPERATOR_TABLE[0]);",
        "",
        "static bool isDigitsOnly(const String& value) {",
        "  if (value.isEmpty()) return false;",
        "  for (int i = 0; i < value.length(); i++) {",
        "    char c = value.charAt(i);",
        "    if (c < '0' || c > '9') return false;",
        "  }",
        "  return true;",
        "}",
        "",
        "static String normalizeOperatorCode(const String& value) {",
        "  String normalized = value;",
        "  if (normalized.length() == 4) {",
        "    normalized = normalized.substring(0, 3) + \"0\" + normalized.substring(3);",
        "  }",
        "  return normalized;",
        "}",
        "",
        "static const OperatorEntry* findOperatorEntry(const String& code) {",
        "  size_t left = 0;",
        "  size_t right = OPERATOR_COUNT;",
        "  const char* target = code.c_str();",
        "  while (left < right) {",
        "    size_t mid = left + (right - left) / 2;",
        "    int cmp = strcmp(target, OPERATOR_TABLE[mid].code);",
        "    if (cmp == 0) {",
        "      return &OPERATOR_TABLE[mid];",
        "    }",
        "    if (cmp > 0) {",
        "      left = mid + 1;",
        "    } else {",
        "      right = mid;",
        "    }",
        "  }",
        "  return nullptr;",
        "}",
        "",
        "String getOperatorNameByCode(const String& code, const char* lang) {",
        "  if (code.isEmpty()) return \"\";",
        "  String trimmed = code;",
        "  trimmed.trim();",
        "  if (!isDigitsOnly(trimmed)) {",
        "    return code;",
        "  }",
        "  String normalized = normalizeOperatorCode(trimmed);",
        "  const OperatorEntry* entry = findOperatorEntry(normalized);",
        "  if (entry) {",
        "    if (lang && strcmp(lang, \"en\") == 0) {",
        "      return String(entry->nameEn);",
        "    }",
        "    return String(entry->nameZh);",
        "  }",
        "  return code;",
        "}",
        "",
        "bool isKnownOperatorCode(const String& code) {",
        "  if (code.isEmpty()) return false;",
        "  String trimmed = code;",
        "  trimmed.trim();",
        "  if (!isDigitsOnly(trimmed)) return false;",
        "  String normalized = normalizeOperatorCode(trimmed);",
        "  return findOperatorEntry(normalized) != nullptr;",
        "}",
    ])
    return "\n".join(lines) + "\n"


def main():
    payload = fetch_source_data()
    items = build_entries(payload)
    codes = [code for code, _ in items]
    if len(codes) != len(set(codes)):
        raise RuntimeError("operator codes are not unique")
    if codes != sorted(codes):
        raise RuntimeError("operator codes are not sorted")

    OUTPUT_PATH.write_text(render_operator_db(payload, items), encoding="utf-8")
    print(f"generated={payload.get('generated', 'unknown')}")
    print(f"api_count={payload.get('count', 'unknown')}")
    print(f"final_entries={len(items)}")


if __name__ == "__main__":
    main()