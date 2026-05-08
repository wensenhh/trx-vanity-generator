# 发布包与首次运行指南（草案）

本文件面向准备下载 Release 包的普通用户，以及制作 Release 的维护者。当前项目仍是 **CLI alpha**；GUI、DMG、MSI/NSIS installer 还在路线图中。

## 1. 发布包应该包含什么

每个面向用户的压缩包至少应包含：

```text
bin/trx_vanity             # Windows 为 bin/trx_vanity.exe 或包内同等位置
README.md                  # 项目简介、快速使用、安全提醒
share/trx_vanity/docs/USER_GUIDE_zh.md
share/trx_vanity/docs/SECURITY_zh.md
share/trx_vanity/docs/PACKAGING_zh.md
share/trx_vanity/kernel/*.cl
```

如果启用 GPU，`kernel/*.cl` 必须随包发布；否则用户复制二进制后容易遇到找不到 `vanity.cl` 的错误。

## 2. macOS / Linux TGZ 用户首次运行

```bash
# 解压
mkdir -p trx_vanity
tar -xzf trx_vanity-*.tar.gz -C trx_vanity --strip-components=1
cd trx_vanity

# CPU smoke test：确认程序能运行
./bin/trx_vanity prefix T --max-attempts 64 -t 1

# GPU smoke test：确认 kernel 路径与 OpenCL 可用
TRX_KERNEL_DIR="$PWD/share/trx_vanity/kernel" \
  ./bin/trx_vanity prefix T --gpu --batch-size 64 --batches 1
```

正式搜索前建议先阅读：

- `README.md`
- `share/trx_vanity/docs/USER_GUIDE_zh.md`
- `share/trx_vanity/docs/SECURITY_zh.md`

## 3. Windows ZIP 用户首次运行（规划）

Windows ZIP 应尽量保持同样布局：

```text
bin\trx_vanity.exe
README.md
share\trx_vanity\docs\USER_GUIDE_zh.md
share\trx_vanity\docs\SECURITY_zh.md
share\trx_vanity\kernel\*.cl
```

PowerShell smoke test：

```powershell
.\bin\trx_vanity.exe prefix T --max-attempts 64 -t 1
$env:TRX_KERNEL_DIR="$PWD\share\trx_vanity\kernel"
.\bin\trx_vanity.exe prefix T --gpu --batch-size 64 --batches 1
```

Windows 用户还需要：

- Visual C++ 运行库（如果发布包采用动态 MSVC runtime）。
- OpenSSL runtime DLL（如果没有静态链接或随包带 DLL）。
- NVIDIA / AMD / Intel 显卡驱动和 OpenCL runtime（仅 GPU 模式）。

正式 Windows installer（MSI/NSIS）发布前，不应在官网文案中写“普通用户一键安装”。

## 4. 结果文件与私钥提醒

默认 `-o/--output` 不写私钥。只有显式使用高风险参数时，私钥才会显示或写入明文文件：

```bash
--show-private-key
--allow-plaintext-private-key-output
```

含私钥的结果文件必须视为资产控制文件：

- 不发送到 Telegram、微信、邮箱、网盘或 GitHub。
- 不放入云同步目录。
- 不截图、不录屏、不贴给客服。
- 优先离线保存，并使用系统加密磁盘、加密压缩包或专用密码管理器。

## 5. GUI wrapper 的最小可交付范围

短期 GUI 可以先作为本地 wrapper 调用现有 `trx_vanity` CLI，不引入服务器端私钥处理。最小功能：

1. 首次运行安全向导：解释“私钥 = 资产控制权”。
2. 模式选择：prefix / suffix / contains / consecutive / sequential。
3. 规则输入与难度提示。
4. CPU/GPU 切换、线程数、batch size、max attempts / batches。
5. 开始 / 停止。
6. 实时显示 Attempts、Rate、ETA(avg)、Prob、错误信息。
7. 命中结果默认隐藏私钥。
8. 导出前二次确认；明文私钥导出必须红色高风险提示。
9. 不上传私钥、不内置远程日志采集私钥内容。

优先评估 Tauri 或 Qt；Electron 体积较大，可作为备选。

## 6. Release 制作检查

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G TGZ
cpack --config build/CPackConfig.cmake -G ZIP
```

发布前人工检查压缩包内容：

```bash
tar -tzf trx_vanity-*.tar.gz | grep -E 'README.md|USER_GUIDE_zh.md|SECURITY_zh.md|kernel/vanity.cl'
unzip -l trx_vanity-*.zip | grep -E 'README.md|USER_GUIDE_zh.md|SECURITY_zh.md|kernel/vanity.cl'
```

同时执行敏感信息检查：不包含真实私钥、bot token、API key、`.env`、真实结果 CSV。文档中可以出现高风险参数名称，但必须明确标注风险。
