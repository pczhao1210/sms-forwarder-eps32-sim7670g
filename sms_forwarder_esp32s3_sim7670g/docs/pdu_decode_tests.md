# PDU 解码测试 / PDU Decode Tests

本项目提供一个轻量的本地测试脚本，用于验证短信解码逻辑。测试范围已从单纯 payload 编解码扩展到完整 SMS-DELIVER PDU、UDH 拼接、多语言和多段样本。

## 运行方式 / How to Run

在仓库根目录执行：

```bash
node tests/pdu_decode.test.js
```

示例输出：

```
Running 23 PDU decode tests...
ok 1
...
ok 23
All tests passed.
```

## 覆盖范围 / Coverage
- GSM 7-bit 基础字符
- GSM 7-bit 扩展字符
- 7-bit + UDH 对齐
- UCS2（UTF-16BE）与 UTF-16 代理对（emoji）
- 8-bit（Windows-1252/Latin-1 可读文本）
- 完整 SMS-DELIVER PDU 样本（发送方 / DCS / UDH / 正文提取）
- 8-bit / 16-bit 长短信拼接参考号（UDH）
- 短短信 + 2 段 / 3 段 / 4 段长短信拼接
- 多语言样本：英文、简中、繁中、日文、俄文、阿拉伯文、emoji 混合文本

## 备注 / Notes
- 这是本地脚本测试，不依赖硬件。
- 若要新增测试向量，可在 `tests/pdu_decode.test.js` 里追加。
- 当前测试包含“构造样本”和“完整 PDU 样本”两类，后续建议继续补充真实运营商导出的原始 PDU 向量。
