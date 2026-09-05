# Design：血管剖面装配

## Service boundary

新增纯 C++ `VesselProfileAssembler`：

```text
assemble(pathNodeId, XQPath,
         contourNodeId, XQContourGroup,
         options)
  -> AssemblyResult { status, profile, issues }
```

service 不持有 scene、不依赖 Qt/VTK/ITK，也不提交 command。Controller 负责把已验证结果包装为 scene command。

## Deterministic contour pipeline

1. 验证 Path 已成功 resample，取得总弧长和 frame API。
2. 验证 ContourGroup 的 source Path id 与传入 id 相同。
3. 使用 `orderedByPathPosition()` 稳定排序；相同弧长直接失败。
4. 对每个 contour：
   - 验证 stable contour id、closed、有限点数；
   - 用既有 `projectToFrame()` 投影到 `(u,v)`；
   - 清理首尾完全重复闭合点，但不改原 contour；
   - 检查非相邻边相交；
   - 用 shoelace 计算绝对面积并拒绝低于容差的退化多边形；
   - 通过 `frameAtArcLength()` 取得中心线 position/tangent。
5. `stableSampleId` 使用稳定 contour id 的值或无碰撞编码；禁止依赖 vector 地址、浮点 hash 或运行时顺序分配。
6. 填入 `areaMm2` 和 measurement quality，最后调用共享 Profile validator。

默认策略是 fail-closed：一个必需 contour 不合法则不输出部分 profile。

## Frame consistency

Contour 面积在其记录的 frame 上测量，但需检查该 frame 与 Path frame：

- origin 到中心线 position 的距离在显式容差内；
- contour normal 与 path tangent 的绝对点积达到阈值；
- x/y axis finite、近似正交。

允许方向正反，因为面积不依赖 winding；不允许完全无关的 plane。

## Imported-gold profile path

提供窄型 XQ-owned importer 输入值（version、frame、units、existing sourcePathNode、externalEvidenceId/fingerprint、optional evidenceAssetId、samples），供自动黄金测试和未来受控科研导入器调用。它先显式完成 source-unit→canonical mm/mm² 转换，再调用同一 source-kind-aware validator。Scene 至少链接 Path→Profile；若 evidence asset 存在，project command 同时建立 asset lineage。壳 A 不发明新的公开文本格式或通用 parser；round-trip 由 typed project payload 负责。

## Controller and commands

新增 typed controller intent，包含新 node/asset id、显示名、输入 node ids、值拷贝和 options。worker thread 运行 assembler，GUI/main thread 提交一个支持多 source 的原子 command：

```text
Path ───────┐
            ├─derived→ VesselProfile
ContourGroup┘
```

复用 domain-contracts 已交付的 project-level batch command，一次提交 Profile node、optional asset/binding、全部 Scene relations 与可用 Asset lineage；任一步失败都全回滚，不在本任务再造第二个命令实现。

## Provenance

Profile 内保存科学输入摘要；scene/asset relation 保存可遍历因果图。参数摘要采用稳定字段顺序，禁止依赖 locale。Controller 在主线程 prepare 时拒绝 stale 输入并捕获 payload clone/revision；worker 返回后、commit 前再次核对 node/revision/stale，防止后台期间输入变化。纯 assembler 不直接读取 Scene stale。

ScaleSlot 由 controller intent 显式指定，或仅在 Path 与全部 evidence nodes 都显式且相同的情况下传播；任何 absent/冲突都产生 absent（或显式 conflict 诊断），绝不默认为 Organ。

## Consumer rule

本任务结束后新增代码不得再复制 MainWindow 中 contour-area 组装。旧 flow 路径可暂时保留兼容，但 flow-smoke child 必须切到 Profile assembler/consumer，并随后删除或封闭旧重复逻辑。
