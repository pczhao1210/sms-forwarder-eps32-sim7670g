# 运营商表维护 / Operator Table Maintenance

本项目使用内置的运营商表来将 MCC/MNC 网络代码映射为运营商名称。维护入口在 `src/operator_db.cpp`。

The project uses a built-in operator table to map MCC/MNC network codes to operator names. The table lives in `src/operator_db.cpp`.

## 快速规则 / Quick Rules
1. 仅使用数字形式的 MCC/MNC（如 `46000`、`23410`）。  
   Use numeric MCC/MNC only (e.g. `46000`, `23410`).
2. 每个代码必须唯一。  
   Each code must be unique.
3. `nameZh` 与 `nameEn` 分别为中文/英文显示名。  
   `nameZh` and `nameEn` are the Chinese/English display names.
4. 保持表项按代码升序排列，便于维护与对比。  
   Keep entries sorted by code for easier maintenance.

## 如何新增 / How to Add
在 `OPERATOR_TABLE` 里新增一行：  
Add a new entry to `OPERATOR_TABLE`:

```cpp
  {"46099", "示例运营商", "Example Operator"},
```

建议在新增后执行一次编译，确保无语法错误。  
Rebuild once to ensure the code compiles.

## 如何删除 / How to Remove
删除 `OPERATOR_TABLE` 中对应的行即可。  
Remove the matching row from `OPERATOR_TABLE`.

## 注意事项 / Notes
1. 如果网络代码不是纯数字，系统会直接回显原始值，不做映射。  
   Non-numeric codes are returned as-is without mapping.
2. 如果未匹配到表项，系统会回显网络代码本身。  
   Unmatched codes fall back to the raw MCC/MNC.
3. UI 显示会根据当前语言显示 `nameZh` 或 `nameEn`。  
   UI uses `nameZh` or `nameEn` based on the current language.
