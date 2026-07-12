# 运营商表维护 / Operator Table Maintenance

本项目使用内置的运营商表来将 MCC/MNC 网络代码映射为运营商名称。生成入口在 `tools/generate_operator_db.py`，输出文件为 `src/operator_db.cpp`。

The project uses a built-in operator table to map MCC/MNC network codes to operator names. The generator lives in `tools/generate_operator_db.py` and writes `src/operator_db.cpp`.

## 快速规则 / Quick Rules
1. 仅使用数字形式的 MCC/MNC（如 `46000`、`23410`）。  
   Use numeric MCC/MNC only (e.g. `46000`, `23410`).
2. 每个代码必须唯一。  
   Each code must be unique.
3. `nameZh` 与 `nameEn` 分别为中文/英文显示名。  
   `nameZh` and `nameEn` are the Chinese/English display names.
4. 保持表项按代码升序排列，便于维护与对比。  
   Keep entries sorted by code for easier maintenance.

## 数据来源 / Data Source
当前内置表由 `mcc-mnc.com` API 生成，并保留少量本项目人工显示名覆盖。

The current built-in table is generated from the `mcc-mnc.com` API with a few local display-name overrides.

```bash
python3 tools/generate_operator_db.py
```

生成脚本会将一位 MNC 补齐为两位（如 `270` + `2` -> `27002`），并保持表项唯一、按代码升序排列。

The generator pads one-digit MNC values to two digits (for example `270` + `2` -> `27002`) and keeps entries unique and sorted by code.

`mcc-mnc.com` API 免费使用，但完整静态再分发许可未明确；本项目按私有使用场景内置该数据。

The `mcc-mnc.com` API is free to use, but full static redistribution terms are not explicit; this project embeds the data for private use.

## 如何新增 / How to Add
优先在 `mcc-mnc.com` 数据源中维护新增项；如需保留本项目自己的显示名，在 `tools/generate_operator_db.py` 的 `MANUAL_OVERRIDES` 中新增一行，然后重新运行生成脚本。

Prefer maintaining new entries in the `mcc-mnc.com` source data. For local display-name overrides, add an entry to `MANUAL_OVERRIDES` in `tools/generate_operator_db.py`, then rerun the generator.

```python
      "46099": ("示例运营商", "Example Operator"),
```

建议在重新生成后执行一次编译，确保无语法错误。

Rebuild once to ensure the code compiles.

## 如何删除 / How to Remove
如果条目来自 `mcc-mnc.com` 数据源，需要从源数据修正；如果条目来自本地覆盖，删除 `MANUAL_OVERRIDES` 中对应的行并重新运行生成脚本。

If the entry comes from the `mcc-mnc.com` source data, fix it at the source. If it comes from a local override, remove the matching `MANUAL_OVERRIDES` entry and rerun the generator.

## 注意事项 / Notes
1. 如果网络代码不是纯数字，系统会直接回显原始值，不做映射。  
   Non-numeric codes are returned as-is without mapping.
2. 如果未匹配到表项，系统会回显网络代码本身。  
   Unmatched codes fall back to the raw MCC/MNC.
3. UI 显示会根据当前语言显示 `nameZh` 或 `nameEn`。  
   UI uses `nameZh` or `nameEn` based on the current language.
