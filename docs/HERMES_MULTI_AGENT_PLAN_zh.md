# TRX Vanity 项目多 Agent 协作规范

本文档定义 TRX Vanity Generator 项目在 Hermes Agent 协作模式下的正式工作规范，覆盖职责边界、交接流程、Issue/PR 规则、分支命名、测试验证、安全红线、优先级和 Telegram 通知边界。

- 项目路径：`/Users/vincen/Vincen/code/trx_addr`
- 远程仓库：`git@github.com:wensenhh/trx-vanity-generator.git`
- 核心产物：`trx_vanity` CLI
- 技术栈：C++17、CMake、OpenCL、CPack
- 主要目录：`main.cpp`、`host/`、`utils/`、`kernel/`、`tests/`、`docs/`

## 1. 协作目标

多 Agent 协作的目标是以可追踪、可验证、可审查的方式推进项目，避免重复修改、覆盖本地改动或引入安全风险。

核心原则：

1. `main` 分支只接受 Pull Request 合并。
2. 所有任务通过 Issue 或明确用户指令驱动。
3. 每个 Agent 在独立分支上工作，提交范围尽量小而清晰。
4. 任何代码、文档、打包或 GUI 变更都必须包含验证记录。
5. 私钥安全优先于功能、性能、包装和市场表达。
6. 不夸大性能或安全性，不把未验证能力写成承诺。

## 2. Agent 角色与职责边界

### 2.1 测试 + 产品 Agent

职责：

- 拉取最新代码并检查仓库状态。
- 构建、运行测试、执行 CLI/GPU smoke 验证。
- 从产品体验角度审查 README、用户指南、错误提示、功能边界和安全提示。
- 对可复现问题创建 GitHub Issue，给出环境、命令、实际结果、期望结果、影响范围和建议优先级。
- 输出中文测试/产品报告。

边界：

- 默认不直接修改代码或文档。
- 不处理私钥、Token、`.env`、真实结果 CSV 或敏感日志。
- 无 OpenCL/GPU 环境时，应记录环境限制，不直接判定代码错误。

### 2.2 全栈开发 Agent

职责：

- 根据 Issue 或明确任务修复 bug、优化性能、补充测试。
- 维护 C++17、CMake、OpenCL 架构一致性。
- 修改代码时同步更新相关测试和必要文档。
- 提交独立分支并创建 PR。

边界：

- 不做与当前 Issue 无关的大范围重构。
- 不改变私钥输出、安全提示或结果保存行为，除非 Issue 明确要求且 PR 中包含安全说明。
- 不提交构建产物、真实生成结果、私钥、Token、`.env` 或敏感日志。

### 2.3 运营 + 市场 + 安全 + 开发 Agent

职责：

- 推进 GUI、打包、发布体验、用户 onboarding、FAQ、发行清单和安全文档。
- 从普通用户视角审查下载、安装、首次运行、生成地址、保存结果、导入钱包前验证等流程。
- 对包装、GUI、安全、发布、市场表达类需求创建 Issue 或设计方案。
- 可执行小范围文档、打包配置或用户体验改进，并通过 PR 交付。

边界：

- 不为营销夸大收益、速度或安全性。
- 不设计默认上传私钥、结果文件或本地敏感日志到服务器的功能。
- 涉及私钥显示、结果导出、远程服务、Telegram 通知的改动必须进行安全审查。

## 3. 标准工作流程

### 3.1 任务进入

1. 总控 Agent 接收用户需求。
2. 判断任务类型：
   - 测试、复现、产品问题：交给测试 + 产品 Agent。
   - bug 修复、性能优化、代码实现：交给全栈开发 Agent。
   - GUI、打包、发布、安全、市场、用户体验：交给运营 + 市场 + 安全 + 开发 Agent。
3. 对复杂任务先拆分，再分派给对应 Agent。
4. 涉及删除文件、强推、发布 Release、处理私钥或上传结果文件时，必须先取得用户明确确认。

### 3.2 本地准备

每个 Agent 在开始修改前必须执行：

```bash
git fetch origin
git status --short --branch
```

要求：

- 不使用 `git reset --hard` 覆盖用户本地改动。
- 不强推 `main` 或覆盖远程分支。
- 不删除用户文件。
- 发现工作区存在无关改动时，只修改任务允许范围内的文件；如存在冲突风险，应停止并汇报。

### 3.3 分支命名

推荐分支格式：

- 测试/产品报告：`docs/qa-product-<short-name>` 或 `test/issue-<number>-<short-name>`
- bug 修复：`codex/fix-issue-<number>-<short-name>`
- 功能实现：`codex/feature-issue-<number>-<short-name>`
- 文档改进：`docs/<short-name>`
- 打包改进：`packaging/issue-<number>-<short-name>`
- GUI 相关：`gui/issue-<number>-<short-name>`
- 安全改进：`security/issue-<number>-<short-name>`

分支规则：

- 每个任务使用独立分支。
- 分支名应包含任务类型和简短描述，Issue 驱动任务应包含 Issue 编号。
- 如本地或远程分支已存在，应创建带后缀的新分支，避免覆盖他人工作，例如 `docs/multi-agent-collaboration-spec-2`。

### 3.4 修改范围控制

- 只修改任务明确允许的文件和目录。
- 不把顺手格式化、无关重构、无关文档更新混入同一 PR。
- 涉及安全、私钥、结果保存、远程上传、GUI 导出时，PR 必须单独说明风险和验证方式。

## 4. Issue 规则

### 4.1 Issue 标题

推荐格式：

```text
<area>: <short problem or request>
```

示例：

```text
cli: improve invalid batch size error message
gpu: add OpenCL device selection flags
packaging: document Windows OpenCL and OpenSSL setup
gui: design minimal desktop wrapper
security: hide private key by default in GUI
```

### 4.2 Issue 正文

Issue 应包含：

- 背景：为什么需要处理。
- 范围：涉及的模块、文档或用户流程。
- 复现步骤或需求说明：可执行命令、输入、预期行为。
- 实际结果与期望结果：适用于 bug 或体验问题。
- 验收标准：完成后如何判断通过。
- 安全注意事项：是否涉及私钥、Token、结果文件、远程服务或日志。
- 优先级：P0、P1、P2 或 P3。

### 4.3 标签建议

建议使用以下标签：

- `bug`
- `product`
- `test`
- `security`
- `packaging`
- `gui`
- `performance`
- `docs`
- `agent:qa-product`
- `agent:dev`
- `agent:growth-security`

## 5. PR 规则

### 5.1 PR 创建条件

创建 PR 前必须满足：

1. 工作区只包含当前任务允许的文件变更。
2. 已运行与变更范围匹配的验证命令。
3. 已确认没有提交私钥、Token、`.env`、真实结果 CSV、构建产物或敏感日志。
4. Commit 使用 Conventional Commits。

推荐 Commit 类型：

- `docs`: 文档变更。
- `fix`: bug 修复。
- `feat`: 新功能。
- `test`: 测试变更。
- `build`: 构建或打包配置。
- `chore`: 非功能性维护。

示例：

```text
docs(agent): formalize multi-agent collaboration spec
fix(cli): validate batch size before generation
build(packaging): include OpenCL kernels in release archive
```

### 5.2 PR 正文模板

```markdown
## What

## Why

## Test

## Risk

Closes #<issue-number>
```

要求：

- `What` 描述改了什么。
- `Why` 描述为什么需要改。
- `Test` 列出实际运行的命令和结果。
- `Risk` 说明兼容性、安全性、性能或发布风险。
- 如 PR 对应 Issue，使用 `Closes #<issue-number>`。

## 6. 测试与验证命令

### 6.1 标准构建与测试

代码变更默认运行：

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### 6.2 CLI smoke

```bash
./build/trx_vanity prefix T --max-attempts 64 -t 1
```

### 6.3 GPU smoke

```bash
./build/trx_vanity prefix T --gpu --batch-size 64 --batches 1
```

如果当前机器没有 OpenCL 或 GPU 不可用，应在报告或 PR 中说明环境限制和错误信息摘要。

### 6.4 安装与打包验证

打包或发布相关变更运行：

```bash
cmake --install build --prefix /tmp/trx_vanity_install
cpack --config build/CPackConfig.cmake -G TGZ
```

### 6.5 文档变更轻量验证

纯文档变更至少运行：

```bash
git diff --check
```

建议额外检查：

- Markdown 是否有单一一级标题。
- 标题层级是否从一级标题开始。
- 文档中是否误写入私钥、Token、`.env` 内容、真实结果 CSV 或敏感输出。

## 7. 安全红线

以下内容不得提交到 GitHub、Issue、PR、Telegram 或第三方服务：

- 私钥、助记词、keystore、seed phrase。
- Bot Token、API Key、访问令牌、`.env` 内容。
- 包含私钥或敏感字段的真实结果 CSV、日志、截图或压缩包。
- 可用于复原私钥或访问账号的调试输出。
- 未脱敏的用户机器路径以外的个人信息、账号信息或远程服务凭据。

涉及以下行为必须先做安全审查：

- 修改私钥生成、保存、显示、复制或导出逻辑。
- 新增 GUI 结果列表、CSV 导出、自动保存或剪贴板功能。
- 新增 Telegram、HTTP、云同步、崩溃上报或遥测。
- 修改默认输出格式、日志级别或错误上报内容。

安全默认值：

- 私钥默认隐藏，用户主动确认后才显示。
- 结果文件默认保存在本地，导出前提示敏感风险。
- 不默认上传任何生成结果。
- 文档中必须提醒用户离线生成、妥善保管私钥，并在转入资产前先做小额验证。

## 8. 优先级规则

### P0：立即处理

- 私钥、Token、助记词或敏感结果泄露风险。
- 生成错误地址、错误私钥或无法验证的结果。
- 主流程无法构建、无法运行或稳定崩溃。

### P1：高优先级

- CPU/GPU 结果不一致。
- 核心正确性缺少测试保护。
- 打包缺失运行必需文件，导致用户无法使用。
- 安全提示缺失且可能导致用户误操作。

### P2：正常优先级

- 性能明显低于预期但不影响正确性。
- 错误提示不友好。
- 文档不清楚或首次运行步骤不完整。
- 发布流程、安装说明、Issue/PR 模板不完善。

### P3：低优先级

- UI 文案润色。
- 市场页面、截图、FAQ 的非关键优化。
- 不影响核心使用的体验细节。

## 9. Telegram 通知边界

Telegram 只用于发送协作摘要和链接，不作为敏感数据传输通道。

允许发送：

- 测试摘要、构建结果摘要、失败命令摘要。
- Issue 链接、PR 链接、分支名、commit hash。
- 需要人工确认的问题和选项。
- 不含私钥和敏感字段的普通地址示例。

禁止发送：

- 私钥、助记词、Token、API Key、`.env`。
- 真实结果 CSV、包含私钥的日志或截图。
- 未脱敏的完整崩溃日志，如果其中可能包含敏感路径、环境变量或生成结果。

Token 管理要求：

- Telegram Bot Token 只能放在 Hermes/Gateway 配置或宿主机环境变量中。
- 不得写入仓库文件、Issue、PR、日志或聊天记录。
- Token 轮换、权限变更和通知频道变更应由用户或项目负责人确认。

## 10. 发布与 GUI 路线约束

### 10.1 打包

优先确保 CLI 发行包可用：

- macOS/Linux：CPack TGZ 包含 `bin/trx_vanity`、README、中文用户指南和 `kernel/*.cl`。
- Windows：先明确 OpenSSL/OpenCL 依赖和 zip/msi/nsis 方案，再实施。
- 发布前必须运行构建、测试、安装和打包验证。

### 10.2 GUI

GUI 初期建议使用 wrapper 模式调用现有 `trx_vanity` CLI，避免过早重构核心库。

GUI 必须满足：

- 支持模式选择、规则输入、CPU/GPU 切换、线程或 batch 参数、开始/停止、实时速度、命中结果和导出。
- 私钥默认隐藏。
- 导出结果前提示敏感风险。
- 不默认上传私钥、地址结果或日志。

核心库重构应作为后续独立 Issue/PR 处理，并补充测试覆盖。

## 11. Agent 交接清单

交接给下一个 Agent 或总控 Agent 时，应提供：

- 当前任务目标。
- 已修改文件或已创建 Issue/PR。
- 已运行验证命令和结果。
- 未完成事项或阻塞原因。
- 安全审查结论。
- 下一步建议。

最小交接格式：

```text
任务：
分支：
Issue/PR：
修改文件：
验证：
安全审查：
阻塞/下一步：
```
