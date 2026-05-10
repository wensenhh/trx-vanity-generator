# Release Checklist（中文）

本清单面向维护者，用于在创建 GitHub Release、发布 TGZ/ZIP 或后续 installer 前统一确认构建、校验和、签名/公证状态、用户反馈入口与撤包标准。

> 当前项目仍是 **CLI alpha**。不要把源码构建产物包装成“普通用户一键安装版”；不要发布、上传或示例化任何真实私钥、结果 CSV、`.env` 或 token。

## 1. 发布基本信息

发布负责人在每次 Release 候选包中记录：

- Release tag：`vX.Y.Z`
- Commit SHA：`<git rev-parse HEAD>`
- 发布日期：`YYYY-MM-DD`
- 发布负责人：`@maintainer`
- 发布渠道：GitHub Release / 内测链接 / 其他（注明）
- 版本阶段：alpha / beta / stable
- 适用人群：开发者 CLI / 普通用户 GUI / installer（当前仅 CLI alpha）
- 签名状态：未签名 / 已签名（说明证书或 key id）
- 公证状态：不适用 / 未公证 / 已公证（说明平台与 ticket）
- 校验和文件：`SHA256SUMS.txt` 是否已生成并随 Release 公布
- 已知问题链接：本文件“已知问题”小节或 GitHub Issue 列表

## 2. 发布前构建与测试矩阵

最低建议矩阵如下。若某个平台暂时无法覆盖，必须在 Release notes 中明示“未覆盖”，不得暗示已验证。

- macOS arm64：
  - 构建：`cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build --parallel`
  - 测试：`ctest --test-dir build --output-on-failure`
  - CPack：`cmake --build build --target package`
  - smoke：解压 TGZ 后运行 CPU smoke；如 OpenCL 可用再运行 GPU smoke。
- macOS x86_64：
  - 同 macOS arm64；如使用交叉编译，标注工具链和未执行的运行时测试。
- Linux x86_64（Ubuntu/Debian 优先）：
  - 构建：`cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build --parallel`
  - 测试：`ctest --test-dir build --output-on-failure`
  - CPack：`cmake --build build --target package`
  - smoke：解压 TGZ 后运行 CPU smoke；如 OpenCL runtime 可用再运行 GPU smoke。
- Windows x64（规划/有 runner 时执行）：
  - 构建：`cmake -S . -B build -A x64 -DBUILD_TESTS=ON ... && cmake --build build --config Release --parallel`
  - 测试：`ctest --test-dir build -C Release --output-on-failure`
  - CPack/ZIP：生成 ZIP 后检查内容布局。

通用检查：

```bash
git diff --check
git status --short
```

## 3. CPack TGZ/ZIP 内容检查

生成包后，不要直接上传。先在临时目录解包并确认内容布局：

```bash
cmake --build build --target package
mkdir -p /tmp/trx_vanity_release_check
# TGZ 示例；实际文件名以 CPack 输出为准
tar -tzf build/trx_vanity-*.tar.gz | sort
```

必须包含：

- `bin/trx_vanity`（Windows ZIP 为 `bin/trx_vanity.exe` 或等效路径）
- `README.md`
- `share/trx_vanity/docs/USER_GUIDE_zh.md`
- `share/trx_vanity/docs/SECURITY_zh.md`
- `share/trx_vanity/docs/PACKAGING_zh.md`
- GPU 构建必须包含：`share/trx_vanity/kernel/*.cl`

建议包含（若 CPack 安装规则已覆盖）：

- `share/trx_vanity/docs/RELEASE_CHECKLIST_zh.md`
- `share/trx_vanity/docs/RELEASE_FEEDBACK_TEMPLATE_zh.md`

解包 smoke test：

```bash
rm -rf /tmp/trx_vanity_release_check
mkdir -p /tmp/trx_vanity_release_check
tar -xzf build/trx_vanity-*.tar.gz -C /tmp/trx_vanity_release_check --strip-components=1
/tmp/trx_vanity_release_check/bin/trx_vanity prefix T --max-attempts 64 -t 1
TRX_KERNEL_DIR=/tmp/trx_vanity_release_check/share/trx_vanity/kernel \
  /tmp/trx_vanity_release_check/bin/trx_vanity prefix T --gpu --batch-size 64 --batches 1
```

如果 GPU smoke 因无 OpenCL 设备失败，应在检查记录中写明“未覆盖 GPU runtime”，不要把它记为通过。

## 4. SHA256 校验和生成与验证

发布 artifacts 生成后，在同一目录创建 `SHA256SUMS.txt`：

```bash
cd build
sha256sum trx_vanity-*.tar.gz trx_vanity-*.zip > SHA256SUMS.txt  # Linux
# 或 macOS：
shasum -a 256 trx_vanity-*.tar.gz trx_vanity-*.zip > SHA256SUMS.txt
```

上传前本地验证：

```bash
cd build
sha256sum -c SHA256SUMS.txt       # Linux
# 或 macOS：
shasum -a 256 -c SHA256SUMS.txt
```

Release notes 必须包含：

- 每个 artifact 的文件名
- 对应 SHA256
- `SHA256SUMS.txt` 文件
- 提醒用户下载后先校验再运行
- GitHub Release 草稿可从 `.github/RELEASE_TEMPLATE.md` 复制模板

用户验证示例：

```bash
shasum -a 256 trx_vanity-*.tar.gz
# 对照 Release notes 或 SHA256SUMS.txt 中的值
```

## 5. 签名与公证状态字段

即使当前未签名，也必须明示状态，避免用户误以为已完成代码签名。

Release notes 推荐字段：

```text
Signing status: unsigned / signed by <key-or-cert>
Notarization status: not applicable / not notarized / notarized for macOS
Checksum: SHA256SUMS.txt attached and verified locally
Build provenance: built from commit <sha> using <runner/toolchain>
```

当前 CLI alpha 默认写法：

```text
Signing status: unsigned
Notarization status: not applicable / not notarized
Checksum: SHA256SUMS.txt attached; users should verify before running
```

不要使用无法证明的表述，例如“官方安全认证”“已审计”“绝对安全”。

## 6. 已知问题

每次 Release 至少检查并更新：

- 当前仍是 CLI alpha；GUI、Windows installer、macOS DMG 未正式发布。
- GPU 依赖 OpenCL 驱动、runtime、设备兼容性；无 GPU 或驱动异常时应使用 CPU 路径。
- 靓号搜索是概率事件，ETA 不是承诺；长尾号可能长时间无结果。
- 私钥一旦泄露即等同资产控制权泄露；默认隐藏私钥不代表可以公开结果文件。
- 若某平台未执行构建/运行时测试，必须在 Release notes 中列出。

## 7. 私钥与敏感文件安全提示

维护者、测试者和用户反馈均必须遵守：

- 禁止提交、上传、截图或粘贴真实私钥、助记词、keystore、完整结果 CSV、`.env`、token、API key。
- 禁止把含私钥的结果文件发到 Telegram、微信、GitHub Issue、网盘或邮件列表。
- 如需排查，请只提供脱敏日志、命令行参数、平台信息和不含私钥的错误信息。
- 维护者在打包前运行 `git status --short`，确认没有误加入 `*.csv`、`.env`、密钥文件或本地构建产物。
- Release 示例截图必须使用虚构/脱敏数据，不使用真实地址对应的私钥。

## 8. 用户反馈模板

用户反馈请优先复制 `docs/RELEASE_FEEDBACK_TEMPLATE_zh.md`，或使用 `.github/ISSUE_TEMPLATE/release_feedback.yml` 提交结构化反馈。维护者在 Issue/表单中重复提醒：

- 不要上传私钥、助记词、keystore。
- 不要上传完整 `results.csv` 或任何可能含私钥的导出文件。
- 不要上传 `.env`、token、API key 或钱包配置。
- 地址、路径、用户名、机器名、交易哈希等可按需脱敏。

## 9. 回滚/撤包标准

满足任一条件，应暂停发布、撤下 Release artifact 或在 Release notes 顶部加醒目警告：

- artifact 校验和与发布说明不一致，或 `SHA256SUMS.txt` 无法验证。
- 包内缺少运行必需文件，例如 `bin/trx_vanity`、README、关键 docs 或 GPU kernel。
- 发现 artifact、日志、示例或文档中包含私钥、结果 CSV、`.env`、token 等敏感信息。
- 发现二进制不是由标注 commit 构建，或构建来源无法追溯。
- macOS/Windows 签名/公证声明与实际状态不符。
- 默认安全行为被破坏，例如默认输出私钥、默认写明文私钥文件。
- 用户可复现的崩溃、错误结果、地址/私钥不匹配或严重误导性文案。
- 恶意软件告警经复核无法解释，或依赖/构建环境疑似被污染。

撤包动作建议：

1. 立即将 Release 标为 pre-release/draft 或删除有问题 artifact。
2. 在 Issue/Release notes 顶部说明影响范围、受影响文件、建议用户删除或停止使用。
3. 重新构建并重新生成 SHA256；新 artifact 使用新的文件名或版本号，避免缓存混淆。
4. 在修复 PR 中记录根因、验证步骤和防复发措施。

## 10. Release notes 最小模板

```text
## trx_vanity vX.Y.Z

Audience: CLI alpha users / developers
Commit: <sha>
Build matrix: <platforms tested; explicitly list not tested platforms>
Signing status: unsigned
Notarization status: not applicable / not notarized
Checksums: SHA256SUMS.txt attached
Known issues:
- <issue 1>
- <issue 2>

Security reminder:
- Verify SHA256 before running.
- Do not share private keys, seed phrases, full results.csv, .env, token or API keys.
- Vanity search is probabilistic; ETA is not a guarantee.

Artifacts:
- <filename>: <sha256>
```
