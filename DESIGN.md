# TRX Vanity Generator - 完整架构文档

## 项目结构

```
trx-vanity/
├── CMakeLists.txt              # 跨平台构建配置
├── README.md                   # 项目说明
├── ARCHITECTURE.md             # 架构设计文档
├── main.cpp                    # CLI 入口
│
├── host/                       # Host 端 C++ 代码
│   ├── cpu_generator.h/.cpp    # CPU 生成器 (Phase 1)
│   ├── opencl_manager.h/.cpp   # OpenCL 管理器 (Phase 2+)
│   └── gpu_generator.h/.cpp    # GPU 生成器 (Phase 2+)
│
├── kernel/                     # OpenCL GPU 内核
│   ├── vanity.cl               # 主内核 (RNG + Keccak256 + 匹配)
│   └── ecc.cl                  # secp256k1 ECC 实现 (Phase 3+)
│
├── utils/                      # 工具库
│   ├── constants.h             # 常量和数据结构
│   ├── crypto.h/.cpp           # Keccak256 + secp256k1 + Base58
│   ├── pattern.h/.cpp          # 靓号规则引擎
│   └── rng.h/.cpp              # 随机数生成器
│
└── tests/                      # 测试代码
    └── (可选)
```

## 开发阶段

### Phase 1: CPU 版本 ✅ 已完成
- **功能**: 完整 TRX 地址生成流程
- **性能**: ~17,000 地址/秒 (4 线程, Apple Silicon)
- **组件**:
  - `Keccak256`: 纯 C++ 实现 (FIPS 202)
  - `Secp256k1`: OpenSSL ECC
  - `Base58`: 自定义编码
  - `Pattern`: 5 种靓号规则
  - `CPUGenerator`: 多线程生成器

### Phase 2: OpenCL 基础版本 ✅ 已完成
- **功能**: GPU 加速 Keccak256 + 地址匹配
- **架构**: CPU ECC -> GPU Hash -> CPU Base58
- **组件**:
  - `OpenCLManager`: OpenCL 生命周期管理
  - `GPUGenerator`: GPU 批次调度
  - `vanity.cl`: OpenCL 内核 (RNG + Keccak256)

### Phase 3: GPU 优化版本 🔄 进行中
- **目标**: ECC 上 GPU, 100万+ 地址/秒
- **关键**:
  - `ecc.cl`: 完整 secp256k1 模运算
  - Montgomery 乘法优化
  - Jacobian 坐标点运算
  - 二进制模式匹配 (无需 Base58)

### Phase 4: 高级优化 📋 计划中
- **多 GPU 支持**
- **持久化内核**
- **SIMD 优化**
- **预计算表**

## 关键设计决策

### 1. Base58 策略
```
GPU:  二进制地址匹配 (避免 Base58 除法)
CPU:  完整 Base58Check 编码 (仅对命中结果)
```

### 2. ECC 放置
```
Phase 1-2: CPU (OpenSSL libsecp256k1)
Phase 3+:  GPU (自定义 Montgomery 模运算)
```

### 3. 匹配策略
```
GPU 侧: 保守二进制检查 (可能有误报)
CPU 侧: 精确 Base58 字符串匹配 (消除误报)
```

## 性能瓶颈分析

| 阶段 | 瓶颈 | 优化策略 |
|------|------|----------|
| Phase 1 | ECC (secp256k1) | 多线程 |
| Phase 2 | CPU-GPU 传输 | 增大批次 |
| Phase 3 | 模运算 | Montgomery, Jacobian |
| Phase 4 | 内存带宽 | 共享内存, 寄存器重用 |

## 跨平台支持

| 平台 | OpenCL 来源 | 测试状态 |
|------|------------|----------|
| macOS (Apple Silicon) | 系统框架 | ✅ 已测试 |
| macOS (Intel) | 系统框架 | 📋 待测试 |
| Windows (NVIDIA) | CUDA Toolkit | 📋 待测试 |
| Windows (AMD) | AMD APP SDK | 📋 待测试 |
| Linux | ocl-icd | 📋 待测试 |

## 编译说明

### macOS
```bash
mkdir build && cd build
cmake ..
cmake --build . --parallel
```

### Windows (Visual Studio)
```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Windows (MinGW)
```cmd
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## 使用示例

```bash
# 7 连尾 8
./trx_vanity consecutive 8 7 -t 8

# 顺子 1234567
./trx_vanity sequential 1 7 -v

# 自定义后缀 5201314
./trx_vanity suffix 5201314 -o results.txt

# 前缀匹配
./trx_vanity prefix ABC

# 包含特定字符串
./trx_vanity contains 888
```

## 安全注意事项

⚠️ **私钥安全**: 本工具生成真实私钥，请注意：
- 不要在共享/不可信机器上运行
- 生成后尽快转移资产
- 使用 `-o` 输出到加密存储
- 考虑离线生成

## 许可证

MIT License - 详见 LICENSE 文件
