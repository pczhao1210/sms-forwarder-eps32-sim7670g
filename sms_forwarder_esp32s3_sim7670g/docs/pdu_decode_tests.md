# PDU 解码测试 / PDU Decode Tests

本项目提供一个轻量的本地测试脚本，用于验证短信解码逻辑（7-bit/UDH/UCS2/8-bit）。

## 运行方式 / How to Run

在仓库根目录执行：

```bash
node tests/pdu_decode.test.js
```

示例输出：

```
Running 5 PDU decode tests...
ok 1
ok 2
ok 3
ok 4
ok 5
All tests passed.
```

## 覆盖范围 / Coverage
- GSM 7-bit 基础字符
- GSM 7-bit 扩展字符
- 7-bit + UDH 对齐
- UCS2（UTF-16BE）
- 8-bit（可读 ASCII 子集）

## 备注 / Notes
- 这是本地脚本测试，不依赖硬件。
- 若要新增测试向量，可在 `tests/pdu_decode.test.js` 里追加。
