# TRX Vanity Generator v1.1.0 路线图

## 目标

将项目从 Beta 推进到稳定版（v1.0.0），面向更广泛的用户群体。

---

## 阶段一：性能优化（P1）

### 1.1 自适应批大小调优（Adaptive Batch Sizing）
- **现状**: 已有基础框架（`test_cpu_performance` 和 JSON 基准）
- **目标**: 根据 GPU 设备类型和模式复杂度自动选择最优 batch size
- **验收标准**: 
  - 新增 `test_adaptive_batch` 测试
  - 相比固定 batch size，性能提升 ≥10%
  - 支持 `--auto-tune` 和 `--profile` 参数

### 1.2 GPU 内存池管理
- **现状**: 每次 batch 都重新分配/释放 OpenCL buffer
- **目标**: 预分配固定大小的 buffer pool，减少内存分配开销
- **验收标准**:
  - 连续运行 1000 个 batch 无内存泄漏
  - `valgrind` / `leaks` 检查通过

### 1.3 CPU SIMD 优化（可选）
- **目标**: 探索 AVX2/NEON 加速 secp256k1 点乘
- **优先级**: P2（GPU 已足够快，CPU 是 fallback）

---

## 阶段二：跨平台验证（P0）

### 2.1 Windows 构建验证
- **现状**: NSIS 打包配置已存在，但未在真实 Windows 硬件上验证
- **目标**: 
  - 在 Windows 10/11 + Visual Studio 2022 上完整构建
  - 验证 OpenCL 运行时加载（NVIDIA/AMD/Intel）
  - 修复任何路径分隔符、编码、权限问题
- **验收标准**:
  - `cmake -S . -B build -DBUILD_TESTS=ON` 成功
  - `ctest --test-dir build` 全部通过（或明确记录 GPU 测试跳过原因）
  - `.exe` 安装包能正常安装和运行

### 2.2 Linux 构建验证
- **目标**: 在 Ubuntu 22.04/24.04 和 Fedora 上验证
- **验收标准**: 同 Windows

### 2.3 CI/CD 流水线
- **目标**: GitHub Actions 自动构建矩阵（macOS/Windows/Linux）
- **优先级**: P1

---

## 阶段三：功能扩展（P2）

### 3.1 更多匹配模式
- **Regex 模式**: 支持基础正则（如 `T[0-9]{4}`）
- **多模式同时搜索**: `--pattern "suffix:AAA" --pattern "prefix:TTT"`
- **优先级**: P2

### 3.2 结果持久化增强
- **数据库后端**: SQLite 替代 CSV/JSON
- **搜索历史**: 支持按地址、模式、时间范围查询
- **优先级**: P2

### 3.3 多 GPU 支持
- **目标**: 检测并使用多个 OpenCL 设备
- **优先级**: P2（目前大多数用户只有 1 个 GPU）

---

## 阶段四：安全与合规（P0）

### 4.1 代码签名与公证
- **macOS**: Apple Developer ID 签名 + Notarization
- **Windows**: EV 代码签名证书
- **验收标准**: 
  - macOS Gatekeeper 不拦截
  - Windows SmartScreen 不拦截

### 4.2 安全审计
- **目标**: 第三方安全审计或开源社区 review
- **优先级**: P1

### 4.3 依赖漏洞扫描
- **目标**: 集成 Dependabot 或 Snyk
- **优先级**: P2

---

## 阶段五：用户体验（P2）

### 5.1 安装器优化
- **macOS**: 美化 DMG 背景、拖拽安装提示
- **Windows**: 安装向导中文本地化
- **Linux**: `.deb` 和 `.rpm` 包

### 5.2 文档完善
- **视频教程**: 5 分钟快速上手
- **API 文档**: 如果开放库接口
- **FAQ**: 常见问题汇总

### 5.3 社区建设
- **Discord/Telegram 群组**: 用户支持
- **贡献指南**: CONTRIBUTING.md
- **Issue 模板**: bug report、feature request

---

## 里程碑时间表

| 里程碑 | 目标 | 预计时间 |
|---|---|---|
| v1.0.0-beta.2 | Windows 构建验证通过 | 2-3 周 |
| v1.0.0-rc.1 | 性能优化完成 + 全平台 CI | 4-6 周 |
| v1.0.0 | 代码签名 + 安全审计 + 稳定版发布 | 6-8 周 |
| v1.1.0 | 多模式 + 多 GPU + 社区功能 | 8-12 周 |

---

## 当前状态（截至 v1.0.0-beta.1）

- ✅ macOS arm64 构建 + 测试通过
- ✅ 27 项自动化测试全部通过
- ✅ CPU/GPU 交叉验证通过
- ✅ 加密导出、GUI、打包基础完成
- ⏳ Windows 构建验证（待开始）
- ⏳ Linux 构建验证（待开始）
- ⏳ 代码签名（待申请证书）
- ⏳ CI/CD（待配置）
