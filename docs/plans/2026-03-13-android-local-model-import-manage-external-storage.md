# Android 本地模型正式导入方案（基于 `MANAGE_EXTERNAL_STORAGE`）

## Summary

把当前 debug probe 验证过的链路产品化：用户从主界面的 `Add Local Model` 入口选择一个本地模型目录，应用申请并依赖 `MANAGE_EXTERNAL_STORAGE`，直接使用用户选中的原始目录，不复制文件。

导入成功后将该目录持久注册为 `local/<absolute-path>` 模型，出现在模型列表里，并弹框询问“是否立即开启会话”。

该方案的删除语义也一并明确：删除用户导入的本地模型时，递归删除原始模型目录本身，而不只是从列表移除。

## Key Changes

### 导入入口

- 复用主页面现有 `Add Local Model` FAB 入口，替换现在的旧提示文案/占位行为。
- 点击后先检查 `MANAGE_EXTERNAL_STORAGE`。
- 未授权时显示简短说明并跳转应用级 All files access 设置页；返回后继续允许导入。
- 已授权时直接打开 `ACTION_OPEN_DOCUMENT_TREE`。

### 导入与注册

- 仅接受“所选目录根下存在 `config.json`”的目录作为一个模型。
- 继续使用现有路径模型格式：`modelId = local/<absolute-path>`。
- 解析目录后做导入前校验：
  - `config.json` 可直接按文件路径读取
  - `ModelConfig.loadConfig(modelId)` 成功
  - 必要模型文件存在
  - 可选再做一次轻量 `ensure/load` 预热，失败则不给导入成功
- 导入成功后把该绝对路径持久化到一个用户导入本地模型注册表；`LocalModelsProvider` 不再只扫 `/data/local/tmp/mnn_models/`，而是合并：
  - 旧的 `/data/local/tmp/mnn_models/`
  - 用户导入注册表中的绝对路径

### 列表与聊天

- 导入成功后刷新模型列表，保证新模型立即可见。
- 成功弹框提供两个动作：
  - `立即开启会话`
  - `稍后`
- 选 `立即开启会话` 时直接走现有 `ChatRouter.startRun(...)`，使用该本地模型的真实路径。
- 列表中的展示名默认取目录名；若已有更稳定命名逻辑则沿用现有 `ModelUtils` / `ModelItem.fromLocalModel` 结果。

### 删除行为

- 对“用户导入的本地模型”单独区分来源，避免与普通下载模型混淆。
- 长按删除时弹强确认，明确写清楚会删除原始目录及其全部文件。
- 确认后：
  - 递归删除该绝对路径目录
  - 从用户导入注册表移除
  - 清理相关 session / mmap / 列表缓存
- 若目录已不存在，也应允许仅清理注册表残留并刷新列表。

## Public Interfaces / Behavior Changes

- `Add Local Model` 从说明型入口变成真实导入入口。
- 新增“用户导入本地模型注册表”这一持久化数据源，存储绝对路径列表。
- `LocalModelsProvider` 的可见来源从单一 `/data/local/tmp/mnn_models/` 扩展为“固定开发目录 + 用户导入目录”。
- 删除用户导入本地模型时，行为改为删除原始目录文件，这是显式产品行为，不是仅取消注册。
- 成功导入后新增成功确认弹框，可直接进入聊天。

## Test Plan

### 单测

- `MANAGE_EXTERNAL_STORAGE` helper：授权判定、设置页 intent/uri 构造。
- 用户导入注册表：新增、去重、删除、失效路径清理。
- 本地模型聚合：`LocalModelsProvider` 同时返回 `/data/local/tmp` 和注册表模型。
- 删除语义：用户导入模型删除时调用“删原目录 + 注销注册表”分支。

### 真机场景

- 未授权时点击 `Add Local Model`：
  - 进入 All files access 设置页
  - 授权回来后可继续导入
- 选择有效模型目录：
  - 导入成功
  - 模型列表出现新 local model
  - 选择“立即开启会话”可进入聊天并成功推理
- 选择无效目录：
  - 缺 `config.json`
  - `loadConfig` 失败
  - 文件缺失
  - 都应给明确失败提示，不注册到列表
- 重复导入同一路径：
  - 不产生重复模型项
- 删除已导入模型：
  - 强确认后删除原目录
  - 列表项消失
  - 重新扫描不再出现
- 删除时目录已被外部手动删掉：
  - 应清掉注册表残留并给用户可理解反馈

### 回归

- `./gradlew :app:testStandardDebugUnitTest`
- `./gradlew :app:assembleStandardDebug`
- `./tests/smoke/scripts/08_regress_api_dumpapp.sh`
- `./tests/smoke/scripts/09_regress_api_uiautomator.sh`
- 补一条新的 smoke / dumpapp 验证：对导入后的 `local//storage/...` 模型执行一次 `llm ensure/run`

## Assumptions

- 正式方案接受 `MANAGE_EXTERNAL_STORAGE` 作为前提，不再尝试 SAF-only 直接加载。
- 一个被选择的目录只代表一个模型，`config.json` 必须在目录根下。
- 导入后的本地模型需要长期保留在模型列表，而不是一次性会话。
- 成功后默认弹确认框，而不是自动直接跳聊天。
- 删除用户导入模型时，允许递归删除原始目录，这是有意产品决策。
