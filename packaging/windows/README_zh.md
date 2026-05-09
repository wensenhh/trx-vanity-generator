# Windows 打包与首次运行指南

本目录提供 Windows 用户包的最小可交付实现：CPack ZIP 和 NSIS installer（可选）。当前没有原生 GUI；ZIP 包和 installer 提供 CLI 二进制、文档、OpenCL kernel 和一个可选的 `trx_vanity_gui.bat` launcher，双击后打开命令行窗口并展示 CLI 帮助与安全提示。

## 包内容

ZIP / installer 安装后目录结构：

```text
trx_vanity-1.0.0-Windows-x86_64/
  bin/
    trx_vanity.exe          # CLI 主程序
    trx_vanity_gui.bat      # 双击启动器（打开 CMD 展示帮助）
  README.md
  LICENSE                   # 许可证（如存在）
  share/trx_vanity/docs/
    USER_GUIDE_zh.md
    SECURITY_zh.md
    PACKAGING_zh.md
    WINDOWS_INSTALL_zh.md   # 本文档
  share/trx_vanity/kernel/
    vanity.cl               # GPU kernel（如启用 OpenCL）
    ecc.cl
    test_ecc.cl
```

## 构建

### 前提条件

1. Visual Studio 2022 Community（勾选 `Desktop development with C++`）
2. CMake ≥ 3.16
3. vcpkg + OpenSSL：

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install openssl:x64-windows
```

4. NSIS（可选，仅生成 `.exe` installer）：
   - 下载安装 [NSIS](https://nsis.sourceforge.io/Download)
   - 确保 `makensis.exe` 在 `PATH` 中

### 编译与打包

```powershell
cd C:\path\to\trx_addr
cmake -S . -B build -A x64 -DBUILD_TESTS=ON `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

cmake --build build --config Release --parallel

ctest --test-dir build -C Release --output-on-failure

# ZIP（始终可用）
cpack --config build\CPackConfig.cmake -G ZIP

# NSIS installer（需安装 NSIS）
cpack --config build\CPackConfig.cmake -G NSIS
```

## 使用方式

### ZIP 包

1. 解压 `trx_vanity-1.0.0-Windows-x86_64.zip` 到任意目录（支持含空格和中文的路径）。
2. 进入解压后的文件夹。
3. 双击 `bin\trx_vanity_gui.bat`：
   - 打开命令行窗口
   - 自动设置 `TRX_KERNEL_DIR`
   - 展示 CLI 帮助与安全提示
   - 不会自动生成地址，也不会输出私钥
4. 在打开的命令行中手动执行示例命令：

```powershell
.\bin\trx_vanity.exe suffix 8888 --max-attempts 64 -t 1
```

### NSIS Installer

1. 运行 `trx_vanity-1.0.0-Windows-x86_64.exe`。  
2. 按向导完成安装（默认安装到 `%ProgramFiles%\trx_vanity`）。  
3. 安装程序会在开始菜单和桌面创建快捷方式：
   - **TRX Vanity (GUI Launcher)** → 运行 `trx_vanity_gui.bat`
   - **TRX Vanity CLI** → 打开命令行并进入安装目录
4. 卸载：控制面板 → 程序和功能 → 卸载 TRX Vanity。

### GPU 模式

GPU 模式需要包内的 kernel 文件。launcher 会自动设置：

```powershell
$env:TRX_KERNEL_DIR="$PSScriptRoot\..\share\trx_vanity\kernel"
```

如果手动运行 CLI，可显式设置同一环境变量：

```powershell
$env:TRX_KERNEL_DIR="C:\path\to\trx_vanity\share\trx_vanity\kernel"
.\bin\trx_vanity.exe suffix 8888 --gpu --batch-size 64 --batches 1
```

## 依赖说明

### Visual C++ 运行时

如果构建时采用 **动态 MSVC runtime**（CMake 默认 `/MD`），目标 Windows 机器需要安装对应版本的 **Visual C++ Redistributable**：

- [VC++ 2022 Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe)

**建议发布方式：**

- 静态链接 MSVC runtime（`-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`），消除额外依赖。
- 或随包附带 `vc_redist.x64.exe` 并在 installer 中静默安装。

### OpenSSL

- 若通过 vcpkg 动态链接 OpenSSL，需随包放置 `libcrypto-3-x64.dll` 和 `libssl-3-x64.dll`（或同等版本）。
- 推荐静态链接（vcpkg triplet 使用 `x64-windows-static`）。

### OpenCL（仅 GPU 模式）

- NVIDIA / AMD / Intel 显卡需安装对应最新版驱动和 OpenCL runtime。
- 无 OpenCL 时 CLI 自动回退 CPU 模式；`trx_vanity_gui.bat` 会提示用户。

## 路径兼容性

本包已针对以下场景测试：

- 安装路径含空格：`C:\Program Files\trx_vanity`
- 安装路径含中文：`C:\用户文档\靓号生成器`
- 解压到桌面或下载文件夹等任意位置

`trx_vanity_gui.bat` 使用 `%~dp0` 解析自身位置，不受当前工作目录影响。

## 私钥安全

- 默认 `-o/--output` 不写私钥。
- `--show-private-key` 会把私钥打印到本机终端，仅用于用户主动临时查看。
- 不要截图、录屏、复制或上传私钥、加密私钥文件、`.env` 或任何真实结果文件。
- 反馈 issue、PR、邮件和 Telegram 中都禁止包含私钥或 token。

## 与 macOS 包的差异

| 特性 | macOS | Windows |
|------|-------|---------|
| 原生 GUI | `.app` bundle（最小 launcher） | `.bat` launcher（CMD 窗口） |
| 安装器 | DMG（DragNDrop） | NSIS `.exe`（向导） |
| 开始菜单/桌面 | 无（手动拖入 Applications） | 有（NSIS 创建快捷方式） |
| Gatekeeper | 需签名/公证 | 无 Gatekeeper，但建议代码签名 |
| VC++ 依赖 | 无 | 可能需要 VC++ Redistributable |

完整 GUI 实现见 issue [#4](https://github.com/wensenhh/trx-vanity-generator/issues/4)。
