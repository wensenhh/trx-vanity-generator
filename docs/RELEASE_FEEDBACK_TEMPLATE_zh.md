# Release 反馈模板（中文）

请复制本模板提交 Release 反馈。为保护资产安全，**禁止上传或粘贴私钥、助记词、keystore、完整结果 CSV、`.env`、token、API key**。

## 反馈类型

- 类型：Bug / 安装问题 / 运行问题 / 文档问题 / 安全疑虑 / 其他
- 严重程度：阻塞 / 高 / 中 / 低
- 是否影响资产安全：是 / 否 / 不确定

## 版本与来源

- Release 版本或 tag：`vX.Y.Z`
- 下载的 artifact 文件名：`trx_vanity-...tar.gz` / `trx_vanity-...zip`
- Commit SHA（如 Release notes 有写）：`<sha>`
- 下载渠道：GitHub Release / 其他（请说明）
- SHA256 是否已验证：是 / 否
- 本地计算的 SHA256（可填写，不要附带敏感文件）：`<sha256>`
- 签名/公证状态（按 Release notes 填写）：未签名 / 已签名 / 未公证 / 已公证 / 不确定

## 运行环境

- 操作系统：macOS / Linux / Windows + 版本
- CPU 架构：arm64 / x86_64 / 其他
- GPU 型号（如使用 GPU）：`<model>`
- OpenCL runtime/驱动版本（如知道）：`<version>`
- 终端/shell：Terminal / PowerShell / cmd / 其他

## 复现步骤

请填写可以复现问题的最小步骤。命令行参数可以保留，但请删除任何真实私钥、真实路径用户名或敏感 token。

```bash
# 示例：
./bin/trx_vanity prefix T --max-attempts 64 -t 1
```

## 实际结果

请粘贴错误信息或日志片段。允许提供：

- 命令退出码
- 不含私钥的 stderr/stdout
- 崩溃信息
- 脱敏后的路径和设备信息

```text
<粘贴脱敏日志>
```

## 期望结果

请描述你原本期望看到的行为。

```text
<期望结果>
```

## 安全确认

提交前请逐项确认：

- [ ] 我没有上传或粘贴私钥、助记词、keystore。
- [ ] 我没有上传完整 `results.csv`，也没有上传任何可能含私钥的导出文件。
- [ ] 我没有上传 `.env`、token、API key、钱包配置或云服务凭据。
- [ ] 我已脱敏用户名、绝对路径、机器名、交易哈希或其他不想公开的信息。
- [ ] 如果日志里出现地址或文件名，我确认它们不包含可用于控制资产的秘密。

## 附件

可以附加：

- 脱敏截图
- 脱敏日志文本
- `SHA256SUMS.txt` 内容或本地校验命令输出

不要附加：

- 私钥、助记词、keystore
- 完整 `results.csv` 或任何含私钥的 CSV
- `.env`、token、API key、钱包配置
- 可直接访问钱包、交易所、云账号或机器的凭据
