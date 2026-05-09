# macOS .app / .dmg 打包与首次运行

本目录提供 macOS 用户包的最小可交付实现：`TRX Vanity.app` bundle 和 CPack `DragNDrop` `.dmg`。当前 app 是安全 launcher：双击后打开 Terminal，展示 CLI 帮助、设置 app 内置 OpenCL kernel 路径，并提醒私钥默认隐藏；完整原生 GUI 仍在后续 issue 中实现。

## 构建

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G DragNDrop
```

生成的 DMG 包含：

```text
TRX Vanity.app/
  Contents/MacOS/TRX Vanity
  Contents/Resources/bin/trx_vanity
  Contents/Resources/kernel/*.cl
  Contents/Resources/README.md
  Contents/Resources/README_zh.md
  Contents/Resources/docs/*.md
```

## 使用方式

1. 打开 `.dmg`。
2. 将 `TRX Vanity.app` 拖入 `Applications`。
3. 双击 `TRX Vanity.app`。
4. app 会打开 Terminal 并运行内置 CLI 的 `--help`，不会自动生成地址，也不会输出私钥。
5. 可以在 Terminal 中手动执行示例命令：

```bash
"/Applications/TRX Vanity.app/Contents/Resources/bin/trx_vanity" suffix 8888 --max-attempts 64 -t 1
```

GPU 模式需要 app 内的 kernel 路径。launcher 会自动设置：

```bash
TRX_KERNEL_DIR="/Applications/TRX Vanity.app/Contents/Resources/kernel"
```

如果手动运行 CLI，可显式设置同一环境变量：

```bash
export TRX_KERNEL_DIR="/Applications/TRX Vanity.app/Contents/Resources/kernel"
"/Applications/TRX Vanity.app/Contents/Resources/bin/trx_vanity" suffix 8888 --gpu --batch-size 64 --batches 1
```

## Gatekeeper / 未签名 Beta

未签名 Beta 可能被 Gatekeeper 拦截。推荐维护者优先发布已签名和公证的 DMG。仅当用户明确信任该构建来源时，才使用以下方式打开未签名版本：

1. Finder 中右键 `TRX Vanity.app`。
2. 选择“打开”。
3. 在系统弹窗中再次选择“打开”。

不要要求用户关闭 Gatekeeper；不要让用户运行来源不明的二进制。

## 私钥安全

- 默认 `-o/--output` 不写私钥。
- `--show-private-key` 会把私钥打印到本机 Terminal，仅用于用户主动临时查看。
- 不要截图、录屏、复制或上传私钥、加密私钥文件、`.env` 或任何真实结果文件。
- 反馈 issue、PR、邮件和 Telegram 中都禁止包含私钥或 token。
