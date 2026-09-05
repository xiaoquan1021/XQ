# M1 路径:PathService + 中心线标架 + 命令/undo

## Goal

血管路径的创建、编辑、加载、重采样,以及沿路径的稳定局部标架(供 contour 放置、斜切、放样)。
纯领域服务,无 UI / VTK / 插件依赖。

## 依赖

- 前置:**M0**(core payload、命令栈、`XQPath`、`SvProjectReader`、PTHPathReader)。
- 后续:M3 建模(放样消费 path 标架)、M5 流体(中心线)。

## Requirements

### 所有权

```
src/services/path/PathService.h / .cpp
src/services/path/CenterlineFrameService.h / .cpp
tests/services/path/PathServiceTest.cpp
tests/services/path/CenterlineFrameServiceTest.cpp
```

### 公开 API(返回命令,不直接改 scene)

```cpp
createPathCommand(name, sourceImageNodeId, controlPoints, spacing) -> AddNodeWithSourceRelationCommand
moveControlPointCommand(node, index, newPos, spacing)   -> ReplacePayloadCommand
insertControlPointCommand(node, index, point, spacing)  -> ReplacePayloadCommand
deleteControlPointCommand(node, index, spacing)         -> ReplacePayloadCommand
resamplePathCommand(node, spacing)                      -> ReplacePayloadCommand
```

编辑命令复制旧 `XQPath` payload,保留原 path id 与 sourceImageNode。

### 局部标架(CenterlineFrameService)

沿 `XQPath` 计算 origin/tangent/normal/binormal/arcLength 标架序列,复用 `XQPath::resample` +
`frameAtArcLength`。**避免法向沿路径突然翻转**(平行传输 / 旋转最小化标架)。不在 contour/viewer/放样里各算一套。

### 校验

- 创建路径至少 2 个控制点;采样间距为正。
- 编辑要求节点为 `XQDomainType::Path` 且带 `XQPath` payload;move/delete 索引存在;insert 索引可等于点数;delete 后至少剩 2 点。

## 约束

- 不引入 Qt/VTK/ITK/插件;不从 service 直接改 scene;不把路径几何存进 UI 状态。
- 参考来源:血管路径语义参考 **SimVascular**(及底层几何库);**XQ1 不参考**。SimVascular 也仅作功能参考,不迁移源码。

## Acceptance Criteria(plan「M1 验收」+ 02-path「验收」)

- [ ] `XQDataNode` 可承载各 payload;`XQScene` 按分组组织(M1 补完 M0 的 payload 机制)。
- [ ] `XQCommandStack` 支持所有 scene 变更的 execute/undo/redo。
- [ ] `0007/Paths/*.pth` 读成 `XQPath` 节点挂到 scene,与新建路径共用同一 payload 类型。
- [ ] 控制点编辑 / 重采样 / undo / redo 测试通过。
- [ ] contour 编辑可直接消费路径,无需读 UI 状态。

## Notes

- 复杂 task,进 Phase 2 前补 design.md / implement.md。

## M0 复核遗留(M1 前置处理)

> 来源:M0 check worker 复核(2026-06-27)。这些在 M0 是合理裁剪/可接受,但 M1 起 service 落地前应处理。
> **M1 处置结论**(2026-06-27 M1 完成时更新)见每条末尾。

- **[前置·重要] 收紧 scene 可变性边界**:当前 `XQScene` 的 insert/remove/link_derived 全 public,
  `XQProject::scene()` 返回非 const 引用——M1 的 PathService 也能绕过命令栈直接改 scene,spec「所有 scene
  变更经 XQCommandStack」目前纯靠纪律、无类型层保护。M1 接入 service 前应收紧(命令类设 friend / reader
  专用 building 通道 / service 拿到只读 scene 之一)。
  **→ M1 处置:采用「底线方案」**。PathService 已做到只读 const 节点 + 只产命令、零 scene mutator 调用,
  `xq_services` 只 link xq_core;但 `XQScene` mutator 仍 public(friend 完整收紧会 break 十几个 M0 fixture 测试)。
  **完整类型层收紧顺延**到 service 更全时(建议 M7 前端接入前)统一做,作为 risk 持续记录。
- **[M1+] stale 多跳传播**:`mark_source_changed` 当前只标记直接 derived,未沿 derived 链递归向下游。
  M1+ stale 真正被消费时补递归 + 测试。**→ M1 未涉及,顺延。**
- **[清理] 测试 fixture 绝对路径**:CMake 里 0007 fixture 用机器绝对路径(`C:/Users/OCEAN/...`),
  换机即 break。M1 起参数化(如 `XQ_FIXTURE_ROOT` cache 变量)。**→ M1 未做(避免扩面),顺延为清理项。**
- **[清理] expected-scene 双真值**:`test_svproject_reader.cpp` 的断言靠 C++ 硬编码,`expected-scene.json`
  只被检查存在、未被解析,两份需手工同步易漂移(M0 已核对一致)。M1 起择一:让测试解析 JSON 驱动断言,
  或把 JSON 降级为文档、C++ 为唯一权威并注明。**→ M1 未做,顺延为清理项。**
- **[清理·M1 新增] CenterlineFrame 法向不翻转测试阈值偏松**:`alignment > 0.5` 能抓翻转但抓不住渐进漂移。
  建议收紧到 >0.9 或补粗采样/急弯用例(M1 check NIT-1)。顺延为清理项。
