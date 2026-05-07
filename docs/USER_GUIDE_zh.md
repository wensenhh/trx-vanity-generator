# TRX 靓号地址生成器中文用户指南

这是一份给普通用户和技术用户看的说明：它解释这个工具能做什么、当前能不能给普通用户直接用、怎么在 Windows / macOS / Linux 上从源码运行 CLI，以及私钥应该如何保护。

> 当前版本定位：**开发者 CLI alpha**。CPU/GPU 命令行路径可用；普通用户 GUI、Windows installer、macOS DMG、加密导出和正式 Release 仍在路线图中。

## 1. 先看安全边界

**私钥就是资产控制权。** 谁拿到私钥，谁就能控制对应 TRON 地址里的 TRX、USDT-TRC20 和其他 TRC20 资产。

请记住：

- 本工具默认隐藏私钥。
- `-o/--output` 默认不会把私钥写入结果文件。
- 只有你显式使用 `--show-private-key`，私钥才会显示在终端。
- 只有你显式使用 `--allow-plaintext-private-key-output`，私钥才允许写入明文结果文件。
- 不要把私钥发到 Telegram、微信、GitHub、网盘、邮箱、截图同步相册或不可信网站。
- 高价值地址建议在离线、可信、专用环境中生成，并用加密方式保存私钥。

## 2. 这个工具是干什么的？

这是一个 **本地离线 TRON/TRX 靓号地址生成器**。你可以让电脑不断生成地址，直到找到符合规则的地址，例如：

```text
T...8888888
T...88888888
T...5201314
TABC...
T...1234567
```

生成过程是概率搜索，不是预约号码。规则越长越难，预计时间只是概率估算，不是保证。

## 3. 当前谁可以用？

**现在适合：**

- 会安装编译工具的开发者或技术用户。
- 能理解私钥风险并愿意自己保管结果的人。
- 想测试 CPU / GPU 地址生成能力的人。

**建议等待后续版本：**

- 不会使用命令行，只想双击安装的普通用户。
- 需要 GUI、一键导出、加密保存、安装包签名和完整新手防误操作的人。

相关路线图：[#4 GUI](https://github.com/wensenhh/trx-vanity-generator/issues/4)、[#5 Windows](https://github.com/wensenhh/trx-vanity-generator/issues/5)、[#6 macOS](https://github.com/wensenhh/trx-vanity-generator/issues/6)、[#7 概率](https://github.com/wensenhh/trx-vanity-generator/issues/7)、[#8 加密导出](https://github.com/wensenhh/trx-vanity-generator/issues/8)、[#11 Release](https://github.com/wensenhh/trx-vanity-generator/issues/11)。

## 4. 支持哪些靓号规则？

命令格式：

```bash
./trx_vanity <规则类型> <规则内容> [选项]
```

Windows PowerShell 示例路径：

```powershell
.\build\Release\trx_vanity.exe <规则类型> <规则内容> [选项]
```

常用规则：

```bash
# 后缀匹配：结尾是 8888
./trx_vanity suffix 8888

# 自定义尾号：结尾是 5201314
./trx_vanity suffix 5201314

# 7 连尾：结尾是 7 个 8
./trx_vanity consecutive 8 7

# 8 连尾：结尾是 8 个 8，更难
./trx_vanity consecutive 8 8

# 顺子后缀：结尾类似 1234567
./trx_vanity sequential 1 7

# 包含某段字符
./trx_vanity contains 520

# 前缀匹配：TRON 地址固定以 T 开头，这里的 prefix 指 T 后面的内容
./trx_vanity prefix ABC
```

## 5. 推荐第一次使用流程

不要一上来就跑 7 连尾或 8 连尾。先确认环境正常。

### 第一步：CPU smoke test

macOS / Linux：

```bash
./trx_vanity prefix T --max-attempts 64 -t 1
```

Windows：

```powershell
.\build\Release\trx_vanity.exe prefix T --max-attempts 64 -t 1
```

如果命令能正常结束，说明基础程序可运行。

### 第二步：跑一个简单尾号

```bash
./trx_vanity suffix 888 -v -o results.csv
```

默认保存结果时不包含私钥。

### 第三步：测试 GPU（可选）

```bash
./trx_vanity prefix T --gpu --batch-size 64 --batches 1
```

如果成功，再尝试：

```bash
./trx_vanity consecutive 8 7 --gpu --batch-size 65536 -v -o results.csv
```

## 6. 常用参数

- `-t, --threads <数量>`：CPU 模式线程数，例如 `-t 8`。
- `--gpu`：使用 OpenCL GPU 模式。
- `--batch-size <数量>`：GPU 每批处理数量。先用 `64` 测试，再尝试 `65536`。
- `--auto-tune`：GPU 自动测试多个 batch size。
- `--batches <数量>`：GPU 跑固定批数后停止，适合测试。
- `--max-attempts <数量>`：CPU 模式最多尝试多少次，适合 smoke test。
- `-v, --verbose`：显示 Attempts、Rate、Matches 等进度。
- `-o, --output <文件>`：保存命中结果；默认不写私钥。
- `--show-private-key`：高风险，把私钥打印到终端。
- `--allow-plaintext-private-key-output`：高风险，允许明文结果文件包含私钥。

## 7. Windows 从源码构建

当前没有正式 Windows installer。

1. 安装 Visual Studio 2022 Community，勾选 `Desktop development with C++`。
2. 安装 CMake，或使用 Visual Studio 自带 CMake。
3. 用 vcpkg 安装 OpenSSL：

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install openssl:x64-windows
```

4. 编译测试：

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

GPU 模式需要显卡驱动和 OpenCL runtime。

## 8. macOS 从源码构建

当前没有正式 macOS DMG。

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

如果 CMake 找不到 OpenSSL：

```bash
# Apple Silicon 常见路径
cmake .. -DBUILD_TESTS=ON -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl

# Intel Mac 常见路径
cmake .. -DBUILD_TESTS=ON -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
```

## 9. Linux 从源码构建

Ubuntu / Debian：

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

## 10. 找不到 `vanity.cl` 怎么办？

GPU 模式需要 OpenCL kernel 文件。一般从源码目录或 build 目录运行可以自动找到；如果你复制了可执行文件，请设置 `TRX_KERNEL_DIR`。

macOS / Linux：

```bash
TRX_KERNEL_DIR=/path/to/trx_addr/kernel ./trx_vanity suffix 8888 --gpu
```

Windows PowerShell：

```powershell
$env:TRX_KERNEL_DIR="C:\path\to\trx_addr\kernel"
.\build\Release\trx_vanity.exe suffix 8888 --gpu
```

安装到固定目录后：

```bash
cmake --install build --prefix /opt/trx_vanity
TRX_KERNEL_DIR=/opt/trx_vanity/share/trx_vanity/kernel \
  /opt/trx_vanity/bin/trx_vanity suffix 8888 --gpu --batch-size 65536
```

## 11. 难度和预计时间怎么理解？

Base58 字符集大约有 58 个字符。每多指定 1 位，平均搜索空间大约乘以 58。

- `888`：相对容易。
- `8888`：适合普通功能测试。
- `8888888`：难度明显上升，可能需要很久。
- `88888888`：比 7 连尾再难约 58 倍。

如果程序一直跑但 `Attempts` 在增长，通常说明程序没有卡住，只是还没命中。预计时间只能按当前速度粗略估算，不是承诺。

## 12. 输出结果怎么看？

命中时会看到类似：

```text
MATCH FOUND!
Address:     Txxxxxxxxxxxxxxxxxxxxxxxxxxxxx8888
Security:    Private key hidden by default
Attempts:    12345678
```

你可以公开地址，但不要公开私钥。如果你使用了高风险私钥显示或明文导出选项，请立即离线、加密、妥善保存。

## 13. 常见问题

**Q1：程序一直跑，没有输出，是不是卡住了？**
A：不一定。加 `-v` 看 Attempts 是否增长；规则越长，越久没有结果越正常。

**Q2：怎么停止？**
A：按 `Ctrl + C`。

**Q3：GPU 报错怎么办？**
A：先跑 CPU smoke test。如果 CPU 正常，检查显卡驱动、OpenCL runtime、`TRX_KERNEL_DIR` 和 `--batch-size` 是否过大。

**Q4：为什么 GPU 没有想象中快？**
A：速度取决于设备、驱动、OpenCL、batch size 和实现状态。当前 alpha 以正确性优先，不承诺极限性能。

**Q5：结果文件能上传给别人看吗？**
A：不建议。尤其不要上传含私钥的结果文件。需要求助时只提供脱敏日志。

## 14. 一句话总结

当前最安全的使用方式：先从源码构建 CLI，先跑 smoke test，默认隐藏私钥，普通规则确认环境，复杂规则耐心等待；任何私钥都不要发给任何人。
