# TRX 靓号地址生成器使用文档（小白版）

这份文档用大白话说明：这个项目是干什么的、怎么编译、怎么运行、macOS 怎么用、Windows 怎么用、常见问题怎么处理。

> 当前版本定位：**V1 correctness-first / alpha 版本**。也就是说：优先保证地址生成正确，GPU 路径已经能跑通，但还不是最终极限性能优化版。

---

## 1. 这个工具是干什么的？

这是一个 **TRON / TRX 靓号地址生成器**。

普通 TRX 地址长这样：

```text
Txxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

你可以用这个工具去找一些“好看”的地址，比如：

```text
T......8888
T......520
TABC......
T......1234567
```

找到以后，程序会输出：

- 地址：`Address`
- 私钥：`Private Key`
- 命中的规则：`Pattern`
- 尝试次数：`Attempts`

⚠️ **非常重要：私钥就是资产控制权。**

谁拿到私钥，谁就能控制这个地址里的 TRX / USDT / 其他 TRC20 资产。不要截图发给别人，不要上传网盘，不要提交到 Git。

---

## 2. 当前版本支持什么？

支持两种运行模式：

### CPU 模式

不用显卡，直接用 CPU 算。

优点：

- 最稳定；
- 最容易跑起来；
- 适合先验证环境是否正常。

缺点：

- 速度比 GPU 慢。

### GPU 模式

使用 OpenCL 调用显卡 / Apple Silicon GPU。

优点：

- 理论速度更快；
- 当前版本已经能跑完整 GPU 地址生成流程。

缺点：

- 需要系统有 OpenCL；
- Windows 下显卡驱动 / OpenCL 环境可能需要额外安装；
- 当前 V1 是正确性优先版本，还不是最终性能优化版。

---

## 3. 支持哪些靓号规则？

程序命令格式大概是：

```bash
./trx_vanity <规则类型> <规则内容> [选项]
```

Windows 下是：

```powershell
.\trx_vanity.exe <规则类型> <规则内容> [选项]
```

### 3.1 后缀匹配：suffix

找结尾是指定字符串的地址。

例子：找结尾是 `8888` 的地址：

```bash
./trx_vanity suffix 8888
```

可能命中：

```text
Txxxxxxxxxxxxxxxxxxxxxxxxxxxxx8888
```

常用例子：

```bash
./trx_vanity suffix 888
./trx_vanity suffix 8888
./trx_vanity suffix 520
./trx_vanity suffix 1314
```

---

### 3.2 前缀匹配：prefix

找 `T` 后面紧跟指定字符串的地址。

TRX 地址固定以 `T` 开头，所以这里的 prefix 指的是 **T 后面的内容**。

例子：

```bash
./trx_vanity prefix ABC
```

可能命中：

```text
TABCxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

如果只是想快速测试程序能不能跑，可以用：

```bash
./trx_vanity prefix T --max-attempts 64
```

因为 TRX 地址本来就以 `T` 开头，这个测试通常很快结束。

---

### 3.3 包含匹配：contains

找地址中间包含某段字符串。

例子：

```bash
./trx_vanity contains 520
```

可能命中：

```text
Txxxxxx520xxxxxxxxxxxxxxxxxxxxxx
```

---

### 3.4 连续数字后缀：consecutive

找结尾是连续相同数字的地址。

格式：

```bash
./trx_vanity consecutive <数字> <长度>
```

例子：找结尾 7 个 8：

```bash
./trx_vanity consecutive 8 7
```

等价于找：

```text
T........................8888888
```

---

### 3.5 顺子后缀：sequential

找结尾是顺子数字的地址。

格式：

```bash
./trx_vanity sequential <起始数字> <长度>
```

例子：

```bash
./trx_vanity sequential 1 7
```

表示找结尾类似：

```text
1234567
```

---

## 4. 常用参数说明

### `--gpu`

使用 GPU 模式。

```bash
./trx_vanity suffix 8888 --gpu
```

不加这个参数就是 CPU 模式。

---

### `--batch-size <数量>`

GPU 每一批生成多少个地址。

```bash
./trx_vanity suffix 8888 --gpu --batch-size 65536
```

建议：

- 普通测试：`1024`、`4096`、`65536`
- 跑不动 / 报显存相关错误：调小
- 想压性能：慢慢调大
- 不确定该设多少：用下面的 `--auto-tune` 自动选择

---

### `--auto-tune`

GPU 自动测试多个 batch size，然后选择吞吐最高的那个继续运行。

```bash
./trx_vanity prefix ZZZ --gpu --auto-tune --batches 10 --profile --benchmark-json
```

默认会测试：`32768,65536,131072,262144,524288`。

也可以自己指定候选值：

```bash
./trx_vanity prefix ZZZ --gpu --auto-tune --auto-tune-sizes 65536,131072,262144 --auto-tune-batches 2 --batches 10
```

参数说明：

- `--auto-tune-sizes`：候选 batch size，用英文逗号分隔；
- `--auto-tune-batches`：每个候选值测试几批，数字越大越稳定，但启动越慢；
- auto-tune 结束后，程序会打印被选中的 batch size，并用它进入正式生成。

---

### `--profile` / `--benchmark-json`

查看 GPU 分阶段耗时，或者输出机器可解析的 benchmark JSON。

```bash
./trx_vanity prefix ZZZ --gpu --batch-size 65536 --batches 3 --profile --benchmark-json
```

---

### `--batches <数量>`

GPU 跑多少批后自动停止。

适合测试环境是否正常。

```bash
./trx_vanity prefix T --gpu --batch-size 64 --batches 1
```

这个命令意思是：

- 用 GPU；
- 一批 64 个地址；
- 只跑 1 批；
- 跑完自动退出。

---

### `--gpu-verify`

调试用。GPU 找到候选后，再用 CPU 复算一次确认。

```bash
./trx_vanity suffix 8888 --gpu --gpu-verify
```

平时不建议开，因为会变慢。

---

### `--max-attempts <数量>`

CPU 模式最多尝试多少个地址后自动停止。

适合测试 / CI。

```bash
./trx_vanity prefix T --max-attempts 64 -t 1
```

---

### `-t` / `--threads <数量>`

CPU 模式用几个线程。

```bash
./trx_vanity suffix 8888 -t 8
```

建议：

- 普通电脑：可以设成 CPU 核心数；
- 机器卡顿：调小一点，比如 `-t 2` 或 `-t 4`。

---

### `-o` / `--output <文件>`

把找到的结果保存到文件。

```bash
./trx_vanity suffix 8888 -o results.csv
```

文件内容大概是：

```text
地址,私钥,命中规则,尝试次数
```

⚠️ 这个文件里有私钥，请妥善保管。

---

### `-v` / `--verbose`

显示实时进度。

```bash
./trx_vanity suffix 8888 -v
```

会显示类似：

```text
Attempts: 123456 | Rate: 50000 addr/s | Matches: 0
```

---

## 5. macOS 下怎么编译和运行

下面以 Apple Silicon Mac / Intel Mac 都适用的方式说明。

### 5.1 安装基础工具

先安装 Xcode Command Line Tools：

```bash
xcode-select --install
```

如果已经安装过，会提示你已经装了。

再安装 Homebrew。如果你已经有 Homebrew，可以跳过。

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

安装依赖：

```bash
brew install cmake openssl
```

macOS 自带 OpenCL Framework，所以一般不用额外安装 OpenCL。

---

### 5.2 下载代码

如果你是从 GitHub 拉代码：

```bash
git clone <你的仓库地址>
cd trx_addr
```

如果你已经在项目目录里了，就直接进入目录：

```bash
cd /path/to/trx_addr
```

---

### 5.3 编译

```bash
mkdir -p build
cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . --parallel
```

编译成功后，会在 `build` 目录里生成：

```text
trx_vanity
```

如果要安装到固定目录，可以在项目根目录执行：

```bash
cmake --install build --prefix /opt/trx_vanity
```

安装后的目录结构大概是：

```text
/opt/trx_vanity/bin/trx_vanity
/opt/trx_vanity/share/trx_vanity/kernel/*.cl
```

使用安装版跑 GPU 时，建议显式指定 kernel 目录：

```bash
TRX_KERNEL_DIR=/opt/trx_vanity/share/trx_vanity/kernel \
  /opt/trx_vanity/bin/trx_vanity suffix 8888 --gpu --batch-size 65536
```

如果需要打包 TGZ：

```bash
cpack --config build/CPackConfig.cmake -G TGZ
```

---

### 5.4 运行测试

在 `build` 目录里执行：

```bash
ctest --output-on-failure
```

如果看到类似：

```text
100% tests passed
```

说明环境基本正常。

---

### 5.5 macOS CPU 模式运行

在 `build` 目录里：

```bash
./trx_vanity suffix 8888 -t 8 -v
```

保存结果到文件：

```bash
./trx_vanity suffix 8888 -t 8 -v -o results.csv
```

快速 smoke test：

```bash
./trx_vanity prefix T --max-attempts 64 -t 1
```

---

### 5.6 macOS GPU 模式运行

在 `build` 目录里：

```bash
./trx_vanity suffix 8888 --gpu --batch-size 65536 -v
```

先做一个很小的 GPU 测试：

```bash
./trx_vanity prefix T --gpu --batch-size 64 --batches 1
```

如果这个能正常结束，说明 GPU / OpenCL 基本能用。

---

### 5.7 macOS 下找不到 `vanity.cl` 怎么办？

GPU 模式需要找到 OpenCL kernel 文件：`vanity.cl`。

正常情况下，从项目根目录或 `build` 目录运行都能自动找到。

如果你把可执行文件复制到了别的地方，就需要指定 kernel 目录：

```bash
TRX_KERNEL_DIR=/path/to/trx_addr/kernel ./trx_vanity suffix 8888 --gpu
```

例如：

```bash
TRX_KERNEL_DIR=$HOME/code/trx_addr/kernel ./trx_vanity suffix 8888 --gpu
```

---

## 6. Windows 下怎么编译和运行

Windows 推荐两种方式：

1. **Visual Studio + CMake**：最推荐；
2. **命令行 PowerShell + CMake**：适合熟悉命令行的人。

---

## 6.1 Windows 准备工作

### 必装 1：Visual Studio 2022

安装 Visual Studio 2022 Community 即可。

安装时勾选：

```text
Desktop development with C++
```

也就是“使用 C++ 的桌面开发”。

里面会包含：

- MSVC 编译器；
- Windows SDK；
- CMake 支持。

---

### 必装 2：CMake

如果 Visual Studio 已经带了 CMake，可以先不用单独装。

如果命令行里找不到 `cmake`，再去安装：

```text
https://cmake.org/download/
```

安装时记得勾选：

```text
Add CMake to the system PATH
```

---

### 必装 3：OpenSSL

项目依赖 OpenSSL。

推荐用 vcpkg 安装：

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install openssl:x64-windows
```

后面 CMake 配置时要带上 vcpkg toolchain。

---

### 可选但建议：GPU / OpenCL 驱动

如果你要用 GPU 模式，需要 OpenCL 环境。

一般情况：

- NVIDIA 显卡：安装最新版 NVIDIA 驱动，通常会带 OpenCL；
- AMD 显卡：安装 AMD Adrenalin 驱动，通常会带 OpenCL；
- Intel 核显 / Arc：安装 Intel 显卡驱动或 OpenCL Runtime。

如果只是 CPU 模式，可以先不管 OpenCL。

---

## 6.2 Windows 使用 Visual Studio 编译

### 方法 A：Visual Studio 图形界面

1. 打开 Visual Studio；
2. 选择 “Open a local folder”；
3. 打开项目根目录 `trx_addr`；
4. 等 Visual Studio 自动识别 CMake；
5. 选择 x64 配置；
6. 点击 Build / 生成。

如果 OpenSSL 找不到，建议改用下面的 PowerShell 方式，因为可以明确指定 vcpkg。

---

## 6.3 Windows 使用 PowerShell 编译（推荐可复现）

在 PowerShell 里进入项目目录：

```powershell
cd C:\path\to\trx_addr
```

创建 build 目录并配置：

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

编译 Release 版本：

```powershell
cmake --build build --config Release --parallel
```

编译成功后，可执行文件通常在：

```text
build\Release\trx_vanity.exe
```

有些 CMake Generator 可能会放在：

```text
build\trx_vanity.exe
```

如果找不到，可以在 `build` 目录里搜索 `trx_vanity.exe`。

---

## 6.4 Windows 运行测试

```powershell
ctest --test-dir build -C Release --output-on-failure
```

如果看到：

```text
100% tests passed
```

说明编译和基础功能正常。

如果 GPU 测试失败，但 CPU 能跑，通常是 OpenCL 驱动 / 运行时问题。

---

## 6.5 Windows CPU 模式运行

```powershell
.\build\Release\trx_vanity.exe suffix 8888 -t 8 -v
```

保存结果到文件：

```powershell
.\build\Release\trx_vanity.exe suffix 8888 -t 8 -v -o results.csv
```

快速 smoke test：

```powershell
.\build\Release\trx_vanity.exe prefix T --max-attempts 64 -t 1
```

---

## 6.6 Windows GPU 模式运行

先跑一个小测试：

```powershell
.\build\Release\trx_vanity.exe prefix T --gpu --batch-size 64 --batches 1
```

如果成功，再跑正常搜索：

```powershell
.\build\Release\trx_vanity.exe suffix 8888 --gpu --batch-size 65536 -v
```

---

## 6.7 Windows 下找不到 `vanity.cl` 怎么办？

如果报错说找不到 `vanity.cl`，说明程序找不到 kernel 文件。

PowerShell 下设置环境变量：

```powershell
$env:TRX_KERNEL_DIR="C:\path\to\trx_addr\kernel"
.\build\Release\trx_vanity.exe suffix 8888 --gpu
```

也可以先复制 `kernel` 目录里的 `.cl` 文件到 exe 所在目录，但更推荐设置 `TRX_KERNEL_DIR`。

---

## 7. Linux 下怎么编译和运行

虽然你主要问 macOS / Windows，这里也简单放一下 Linux。

Ubuntu / Debian：

```bash
sudo apt update
sudo apt install -y build-essential cmake libssl-dev ocl-icd-opencl-dev opencl-headers
```

如果要用 NVIDIA GPU：

```bash
sudo apt install -y nvidia-driver-xxx
```

`xxx` 换成你的系统推荐版本。

编译：

```bash
mkdir -p build
cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . --parallel
ctest --output-on-failure
```

运行：

```bash
./trx_vanity suffix 8888 -t 8 -v
./trx_vanity suffix 8888 --gpu --batch-size 65536 -v
```

---

## 8. 推荐使用流程

第一次使用建议按这个顺序来，不要一上来就跑超长后缀。

### 第一步：先确认 CPU 能跑

macOS / Linux：

```bash
./trx_vanity prefix T --max-attempts 64 -t 1
```

Windows：

```powershell
.\build\Release\trx_vanity.exe prefix T --max-attempts 64 -t 1
```

如果这个能跑完，说明基础程序没问题。

---

### 第二步：再确认 GPU 能跑

macOS / Linux：

```bash
./trx_vanity prefix T --gpu --batch-size 64 --batches 1
```

Windows：

```powershell
.\build\Release\trx_vanity.exe prefix T --gpu --batch-size 64 --batches 1
```

如果这个能跑完，说明 GPU / OpenCL 基本可用。

---

### 第三步：跑一个简单靓号

例如后缀 3 个 8：

```bash
./trx_vanity suffix 888 -v -o results.csv
```

GPU：

```bash
./trx_vanity suffix 888 --gpu --batch-size 65536 -v -o results.csv
```

---

### 第四步：再跑更难的规则

比如：

```bash
./trx_vanity suffix 8888 --gpu --batch-size 65536 -v -o results.csv
./trx_vanity suffix 5201314 --gpu --batch-size 65536 -v -o results.csv
./trx_vanity consecutive 8 7 --gpu --batch-size 65536 -v -o results.csv
```

规则越长，越难找到，需要的时间越久。

---

## 9. 难度大概怎么理解？

Base58 字符集大约有 58 个字符。

如果你要找后缀：

- 3 位：大概 `58^3` 次里出一个；
- 4 位：大概 `58^4` 次里出一个；
- 5 位：大概 `58^5` 次里出一个；
- 7 位：非常难，需要很久。

大白话：

- `888`：相对容易；
- `8888`：还能接受；
- `88888`：开始变难；
- `8888888`：需要非常多算力和时间。

所以测试时不要一开始就用 `8888888`，容易以为程序卡住，其实只是概率太低。

---

## 10. 输出结果怎么看？

命中时会看到类似：

```text
MATCH FOUND!
Address:     Txxxxxxxxxxxxxxxxxxxxxxxxxxxxx8888
Private Key: abcdef123456...
Attempts:    12345678
```

你需要保存：

```text
Address + Private Key
```

尤其是 Private Key。

如果你用了：

```bash
-o results.csv
```

结果会追加写入文件。

---

## 11. 安全提醒：私钥怎么保管？

请认真看这一段。

### 不要做的事

不要把私钥：

- 发给别人；
- 发到微信群 / Telegram；
- 上传到 GitHub；
- 放到公开网盘；
- 截图保存到同步相册；
- 粘贴到不可信网站。

### 建议做的事

- 只在可信电脑上生成；
- 生成结果文件加密保存；
- 大额资产不要直接用测试机器生成的私钥；
- 先小额转账测试；
- 真正高价值地址建议离线环境生成。

---

## 12. 常见问题

### Q1：程序一直跑，没有输出，是不是卡住了？

不一定。

靓号搜索本质是撞概率。规则越长，越久没有结果很正常。

建议加 `-v` 看速度：

```bash
./trx_vanity suffix 8888 -v
```

如果 Attempts 一直增长，说明程序在正常工作。

---

### Q2：我要怎么停止程序？

按：

```text
Ctrl + C
```

程序会收到停止信号并退出。

---

### Q3：GPU 模式报错怎么办？

先跑 CPU：

```bash
./trx_vanity prefix T --max-attempts 64 -t 1
```

如果 CPU 正常，GPU 报错，重点检查：

- 显卡驱动是否安装；
- OpenCL 是否可用；
- `vanity.cl` 是否能找到；
- `--batch-size` 是否太大。

可以先用小 batch：

```bash
./trx_vanity prefix T --gpu --batch-size 64 --batches 1
```

---

### Q4：报 `Unable to locate OpenCL kernel vanity.cl` 怎么办？

设置 `TRX_KERNEL_DIR`。

macOS / Linux：

```bash
TRX_KERNEL_DIR=/path/to/trx_addr/kernel ./trx_vanity suffix 8888 --gpu
```

Windows PowerShell：

```powershell
$env:TRX_KERNEL_DIR="C:\path\to\trx_addr\kernel"
.\build\Release\trx_vanity.exe suffix 8888 --gpu
```

---

### Q5：Windows 找不到 OpenSSL 怎么办？

推荐用 vcpkg：

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install openssl:x64-windows
```

然后重新配置：

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

---

### Q6：macOS 上 OpenSSL 找不到怎么办？

先确认 Homebrew OpenSSL 已安装：

```bash
brew install openssl
```

如果 CMake 还是找不到，可以手动指定：

Apple Silicon Mac 常见路径：

```bash
cmake .. -DBUILD_TESTS=ON -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl
```

Intel Mac 常见路径：

```bash
cmake .. -DBUILD_TESTS=ON -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
```

---

### Q7：`--batch-size` 应该设多少？

建议：

- 先测试：`64`、`1024`
- 正常跑：`65536`
- 如果报错或机器卡：调小
- 如果显卡很强：可以尝试更大，但要自己观察稳定性

---

### Q8：为什么 GPU 没有想象中快？

当前 V1 是正确性优先版本。

也就是说：

- GPU 已经负责生成地址；
- CPU 仍然负责 Base58 编码和最终精确匹配；
- GPU pattern filter 目前比较保守，会把很多候选交给 CPU；
- ECC 还不是最终优化实现。

后续优化方向包括：

- 更强的 GPU 预过滤；
- 优化 ECC；
- 减少 CPU / GPU 数据传输；
- benchmark 和多设备支持。

---

## 13. 开发者：重新编译、测试、清理

### 重新编译

```bash
cmake --build build --parallel
```

Windows Release：

```powershell
cmake --build build --config Release --parallel
```

---

### 跑测试

macOS / Linux：

```bash
ctest --test-dir build --output-on-failure
```

Windows：

```powershell
ctest --test-dir build -C Release --output-on-failure
```

---

### 清理重新编译

macOS / Linux：

```bash
rm -rf build
mkdir build
cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . --parallel
```

Windows PowerShell：

```powershell
Remove-Item -Recurse -Force build
cmake -S . -B build -A x64 -DBUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release --parallel
```

---

## 14. 一句话总结

如果你只是想快速用起来：

macOS / Linux：

```bash
mkdir -p build
cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . --parallel
./trx_vanity suffix 8888 -v -o results.csv
```

Windows：

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release --parallel
.\build\Release\trx_vanity.exe suffix 8888 -v -o results.csv
```

GPU 模式加上：

```text
--gpu --batch-size 65536
```

例如：

```bash
./trx_vanity suffix 8888 --gpu --batch-size 65536 -v -o results.csv
```

记住：**地址可以公开，私钥绝对不能公开。**
