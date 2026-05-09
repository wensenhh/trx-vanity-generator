# TRX Vanity Generator｜本地离线 TRON/TRX 靓号地址生成器

一句话：在你自己的电脑上离线生成符合指定规则的 TRON/TRX 地址，例如 7 连尾、8 连尾、自定义尾号或包含特定字符的地址。

> ⚠️ **安全第一：私钥就是资产控制权。** 谁拿到私钥，谁就能转走这个地址里的 TRX、USDT-TRC20 和其他 TRC20 资产。本项目默认隐藏私钥；不要把私钥发送到 Telegram、微信、GitHub、网盘、截图相册或任何不可信网站。

## 当前可用状态

- **开发者 CLI：可用（alpha）**。适合能安装编译工具、会使用命令行的用户。
- **普通用户 GUI / Windows Installer / macOS DMG：尚未正式发布**。这些属于产品路线图，不应把当前源码版误认为“一键安装版”。
- **生成方式：本地运行、可离线使用**。程序不需要把私钥上传到服务器；你仍需确认运行环境可信、结果文件妥善保存。

## 适合谁 / 不适合谁

**适合：**

- 想要生成 TRON/TRX 靓号地址，并愿意在本机保管私钥的人。
- 开发者、技术用户、能接受命令行 alpha 版本的人。
- 需要 CPU/GPU 两种路径做验证或性能测试的人。

**暂不适合：**

- 只想双击安装、不想接触命令行的普通用户（请等待 GUI / installer）。
- 不能理解“私钥泄露 = 资产可能被盗”的用户。
- 要求某个时间内一定命中特定地址的人：靓号搜索是概率事件，预计时间不是承诺。

## 下载 / 安装入口

当前还没有正式 GUI 安装包。请根据你的身份选择：

- **普通用户：**建议等待 GUI 和正式 Release。路线图见 [#4 GUI](https://github.com/wensenhh/trx-vanity-generator/issues/4)、[#5 Windows](https://github.com/wensenhh/trx-vanity-generator/issues/5)、[#6 macOS](https://github.com/wensenhh/trx-vanity-generator/issues/6)、[#11 Release](https://github.com/wensenhh/trx-vanity-generator/issues/11)。
- **开发者 / 技术用户：**从源码构建 CLI，见下方 Windows / macOS / Linux 步骤。
- **官网与截图素材：**当前仅提供占位说明，不能使用真实私钥或真实结果 CSV。见 [docs/WEBSITE_COPY_zh.md](docs/WEBSITE_COPY_zh.md) 和 [docs/assets/README.md](docs/assets/README.md)。
- **发布检查与反馈：**维护者发布前请使用 [docs/RELEASE_CHECKLIST_zh.md](docs/RELEASE_CHECKLIST_zh.md)；用户反馈请使用 [docs/RELEASE_FEEDBACK_TEMPLATE_zh.md](docs/RELEASE_FEEDBACK_TEMPLATE_zh.md)，且不要上传私钥、结果 CSV、`.env` 或 token。

## 3 分钟快速理解

1. 选择规则：例如 `consecutive 8 7` 表示找结尾 7 个 8。
2. 先用 CPU 小规则测试：确认程序能跑。
3. 再按需启用 GPU：`--gpu --batch-size 65536`。
4. 命中后先只保存地址；如必须导出私钥，离线、加密、最小化暴露。
5. 规则越长越难：7 连尾、8 连尾可能需要很久，预计时间只是概率估算。

## 快速使用（CLI alpha）

> 以下命令假设你已经完成后文构建，并在 `build` 目录中运行。Windows 请把 `./trx_vanity` 替换为 `.\build\Release\trx_vanity.exe` 或你的实际路径。

```bash
# 先做一个 CPU smoke test：确认程序能启动和退出
./trx_vanity prefix T --max-attempts 64 -t 1

# 找结尾 7 个 8
./trx_vanity consecutive 8 7 -t 8 -v

# 找结尾 8 个 8（更难，耗时显著增加）
./trx_vanity consecutive 8 8 -t 8 -v

# 自定义尾号，例如 5201314
./trx_vanity suffix 5201314 -t 8 -v

# GPU 模式：先小批量测试，再正式搜索
./trx_vanity prefix T --gpu --batch-size 64 --batches 1
./trx_vanity consecutive 8 7 --gpu --batch-size 65536 -v

# 保存命中结果到文件；默认不会写入私钥
./trx_vanity suffix 8888 -o results.csv
```

## 命中后如何安全保存私钥

默认行为：

- 终端输出会隐藏私钥。
- `-o/--output` 结果文件默认不包含私钥，只保存地址、规则、尝试次数和 `private_key_hidden` 标记。
- 明文结果文件导出私钥已禁用；不要依赖 CSV 保存私钥。

需要保存含私钥结果时，请使用加密导出：

```bash
# 加密导出会把地址、私钥、规则和尝试次数写入 AES-256-GCM 保护的记录
export TRX_EXPORT_PASSWORD="use-a-strong-unique-password"
./trx_vanity suffix 8888 --encrypted-output private-results.trxenc --export-password-env TRX_EXPORT_PASSWORD
```

高风险显式选项：

```bash
# 高风险：把私钥打印到终端
./trx_vanity suffix 8888 --show-private-key
```

使用这些选项前请确认：

- 当前电脑可信，屏幕录制、终端日志、剪贴板同步都已关闭或可控。
- 不把结果文件提交到 Git，不发到聊天软件，不上传网盘。
- 优先使用离线环境和加密存储；高价值资产建议使用专用离线机器。
- 先用小额资产测试地址可控性，再考虑正式使用。

## Windows 构建路径（当前为源码 CLI）

正式 installer 还没有发布；当前 Windows 用户需要自行编译。

1. 安装 Visual Studio 2022 Community，并勾选 `Desktop development with C++`。
2. 安装 CMake，或使用 Visual Studio 自带 CMake。
3. 使用 vcpkg 安装 OpenSSL：

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install openssl:x64-windows
```

4. 配置并编译：

```powershell
cd C:\path\to\trx_addr
cmake -S . -B build -A x64 -DBUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

5. 运行：

```powershell
.\build\Release\trx_vanity.exe prefix T --max-attempts 64 -t 1
.\build\Release\trx_vanity.exe consecutive 8 7 -t 8 -v
.\build\Release\trx_vanity.exe consecutive 8 7 --gpu --batch-size 65536 -v
```

GPU 模式需要可用的 OpenCL 驱动：NVIDIA / AMD / Intel 显卡请安装对应最新版驱动。

## macOS 构建路径（当前为源码 CLI）

正式 DMG 还没有发布；当前 macOS 用户需要自行编译。

```bash
xcode-select --install
brew install cmake openssl

git clone https://github.com/wensenhh/trx-vanity-generator.git
cd trx-vanity-generator
mkdir -p build
cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . --parallel
ctest --output-on-failure

./trx_vanity prefix T --max-attempts 64 -t 1
./trx_vanity consecutive 8 7 -t 8 -v
./trx_vanity consecutive 8 7 --gpu --batch-size 65536 -v
```

macOS 通常自带 OpenCL Framework；如果 CMake 找不到 OpenSSL，可参考 [中文用户指南](docs/USER_GUIDE_zh.md)。

## Linux 构建路径（当前为源码 CLI）

Ubuntu / Debian 示例：

```bash
sudo apt update
sudo apt install -y build-essential cmake libssl-dev ocl-icd-opencl-dev opencl-headers

git clone https://github.com/wensenhh/trx-vanity-generator.git
cd trx-vanity-generator
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/trx_vanity prefix T --max-attempts 64 -t 1
./build/trx_vanity consecutive 8 7 -t 8 -v
./build/trx_vanity consecutive 8 7 --gpu --batch-size 65536 -v
```

NVIDIA / AMD / Intel GPU 用户需要安装对应驱动和 OpenCL runtime。

## 速度和概率：越长越难，预计时间不是承诺

TRON 地址使用 Base58 字符集。简单理解：每多指定 1 位尾号，平均搜索难度大约乘以 58。

- 3 位后缀：相对容易。
- 4 位后缀：普通测试可尝试。
- 7 连尾：可能需要较长时间和较多算力。
- 8 连尾：明显更难，可能长时间无结果。

`-v` 可以查看 Attempts、Rate、平均等待时间估算（ETA(avg)）和理论命中概率进度（Prob）。首次启动时 CLI 也会打印规则难度、单次命中概率和平均尝试次数。Prob 是基于当前尝试次数的概率进度，不是确定完成度；如果 Attempts 持续增长，通常说明程序还在正常搜索，不代表卡住。

性能受 CPU/GPU 型号、驱动、OpenCL 实现、batch size、散热、系统负载影响。任何 README 中的速度或预计时间都只能作为特定环境样例，无法承诺你的机器一定达到，也无法承诺指定时间内一定命中。

## 常见规则

```bash
# 后缀匹配
./trx_vanity suffix 8888
./trx_vanity suffix 5201314

# 7 连尾 / 8 连尾
./trx_vanity consecutive 8 7
./trx_vanity consecutive 8 8

# 顺子尾号
./trx_vanity sequential 1 7

# 包含指定字符
./trx_vanity contains 520

# 前缀匹配：TRON 地址固定以 T 开头，这里的 prefix 指 T 后面的内容
./trx_vanity prefix ABC
```

## FAQ

**Q：这个项目会把私钥上传到服务器吗？**
A：不会。当前 CLI 在本机生成地址。你仍需自己确认运行环境可信，不要运行来路不明的二进制文件。

**Q：为什么 README 说普通用户不要急着用？**
A：当前是开发者 alpha，缺少 GUI、安装器和更完整的新手保护；CLI 已提供基础加密导出。普通用户更适合等待正式 Release。

**Q：我能把 `results.csv` 发给别人确认吗？**
A：不建议。默认结果不含私钥，但你仍可能误用高风险选项导出私钥。需要协助时请只发脱敏日志，不发私钥、不发完整结果文件。

**Q：可以承诺多久找到 8 连尾吗？**
A：不能。靓号是概率搜索，只能根据速度估算期望时间，无法承诺必定在某个时间内命中。

**Q：GPU 一定比 CPU 快吗？**
A：不一定。GPU 速度取决于设备、驱动、OpenCL、batch size 和当前实现。先跑小批量测试，再决定是否长期使用 GPU。

**Q：如何停止搜索？**
A：终端按 `Ctrl + C`。

## 文档与路线图

- [中文用户指南](docs/USER_GUIDE_zh.md)：更完整的安装、使用、排错和安全说明。
- [发布包与首次运行指南](docs/PACKAGING_zh.md)：TGZ/ZIP 包内容、首次运行 smoke test、GUI wrapper 最小范围。
- [安全说明](docs/SECURITY_zh.md)：私钥、结果文件、分享和发布前检查。
- [#4 GUI](https://github.com/wensenhh/trx-vanity-generator/issues/4)
- [#5 Windows 安装包](https://github.com/wensenhh/trx-vanity-generator/issues/5)
- [#6 macOS DMG](https://github.com/wensenhh/trx-vanity-generator/issues/6)
- [#7 概率/预计时间](https://github.com/wensenhh/trx-vanity-generator/issues/7)
- [#8 加密导出](https://github.com/wensenhh/trx-vanity-generator/issues/8)
- [#11 Release](https://github.com/wensenhh/trx-vanity-generator/issues/11)

## 开发者安装 / 包布局

```bash
cmake --install build --prefix /opt/trx_vanity
```

安装树包含：

```text
/opt/trx_vanity/bin/trx_vanity
/opt/trx_vanity/share/trx_vanity/kernel/*.cl
```

运行安装版 GPU binary 时，如部署环境不保留源码或 build-tree kernel 布局，请设置：

```bash
TRX_KERNEL_DIR=/opt/trx_vanity/share/trx_vanity/kernel \
  /opt/trx_vanity/bin/trx_vanity suffix 8888 --gpu --batch-size 65536
```

TGZ 包可由已配置的 build 目录生成：

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G TGZ
cpack --config build/CPackConfig.cmake -G ZIP
```

当前 CPack 安装树会包含 `README.md`、中文文档和 `kernel/*.cl`，适合继续演进为 macOS/Linux TGZ 和 Windows ZIP 发布包。
## Architecture

See `ARCHITECTURE.md` for detailed design.
