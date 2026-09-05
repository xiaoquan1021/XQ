# 壳 A：Flow 能力可关

## Goal

用最薄的 typed capability 配置证明现有壳可以装入或关闭 Flow 芯，而影像、Path、Contour、Profile、渲染、项目保存和其它 workflow 不受影响；不建设通用插件平台。

## Dependencies

- Parent: `07-10-google-earth-shell-a`。
- Hard dependency: `07-10-shell-a-flow-smoke` 完成，真实 Flow consumer 已存在后再提取可选边界。

## Requirements

- `XQWorkflowSession` 接受显式 capabilities/factory 配置；Flow enabled 时复用现有 `FlowController`，disabled 时不创建并返回 unavailable。
- UI 根据 capability 状态禁用/标记 Flow stage，不能崩溃、偷偷运行或用 Noop 假装成功。
- 增加默认 ON 的 `XQ_ENABLE_FLOW` 构建选项；OFF 时 app/core/io/非 Flow services 和 shell tests 仍可构建运行。
- FlowResult/SimulationCase 项目数据仍可被读取和显示为历史数据；关闭执行能力不等于删除数据类型。
- smoke protocol stamp、station mapping、conversion record 与 source revision 等持久 DTO 必须属于 core/io；`XQ_ENABLE_FLOW=OFF` 只排除执行 service/controller，不得排除这些 payload 字段或其 reader/writer。
- 禁止 OperationRegistry、万能 Context、动态 DLL、热加载、ABI 市场或 Python runtime。

## Acceptance Criteria

- [ ] runtime Flow disabled 时 `hasFlowCapability()==false`，相关 UI unavailable，非 Flow 主链全部工作。
- [ ] default ON 的现有 Flow tests 与 smoke tests继续通过。
- [ ] 独立 `XQ_ENABLE_FLOW=OFF` Release configure/build/ctest 通过指定 shell gate。
- [ ] 打开含历史 FlowResult 的项目在 OFF build 中不会丢数据或启动 solver。
- [ ] capability 逻辑集中，不在 MainWindow/CMake 散布重复判断。

## Out of Scope

- 通用模块/operation registry、第三方插件发现和版本解析。
- Darcy、CTC、PhysiCell 的实际 capability。
- 改写现有 typed controller/service 架构。
