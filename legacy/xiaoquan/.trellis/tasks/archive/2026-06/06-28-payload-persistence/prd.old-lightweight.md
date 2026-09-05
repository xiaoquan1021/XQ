# PRD — 存档 payload 实体数据持久化

> 任务 id:`06-28-payload-persistence` | 优先级 P1 | package XQ
> 前置:M0~M7 主线已完成。本任务让存档从"只存结构"升级到"存实体数据"。

## 1. 背景与问题

`XQProjectWriter::save` 当前**只序列化 scene 结构**:每个 node 的 id / domain_type / display_name、
derived relations、stale 标记、provenance、diagnostics。**没有写任何 payload 实体数据**:

- 影像 buffer(标量体素)、path 点序列、contour 多边形、表面三角网格、体网格 tets、
  流场结果、AI 分析结果 —— 全都不落盘。
- 现有存档 round-trip 只是**结构级**(`test_new_domain_roundtrip` 验 scene 结构一致),
  payload 实体数据持久化在父任务里**明确记为技术债**(M5/M7 遗留)。

后果:存档读回后,节点在,但 payload 是空的——无法离线复现一个真实工程。本任务补齐实体级 round-trip。

## 2. 范围

### In scope

1. **writer 扩展**:`XQProjectWriter` 落盘每类 payload 的实体数据:
   - 影像(`XQImageVolume`):geometry(dims/spacing/origin/direction/coordSys)+ scalar buffer + window/level。
   - 路径(`XQPathPayload`):点序列 + 标架(若有)。
   - 分割掩膜(`XQSegmentationMaskPayload`):mask buffer + label 信息。
   - 表面模型(`XQSurfaceModelPayload`):points + triangles + per-triangle faceId + ModelFace 元数据。
   - 体网格(`XQMeshPayload`):points + tets + 边界面 faceId/元数据。
   - 仿真案例(`XQSimulationCasePayload`):RCR / 波形 / RomSettings / FluidProperties(M5 新字段,父任务记未序列化)。
   - 流场结果(`XQFlowResultPayload`)、AI 分析(`XQAiAnalysisPayload`):结果标量/指标。
2. **reader 对称读回**:`XQProjectReader` 解析上述实体,重建 payload 挂回 node。
3. **schema 版本递增**:当前 `schemaVersion 1.1`(见 writer)。新增实体段 → 升到 1.2(或按规则),
   **保持向后兼容**:旧档(无实体段)仍能读(payload 为空,不报错,发诊断)。
4. **大二进制处理**:影像/掩膜 buffer 可能很大,纯文本 hex 不现实。定一个存储策略
   (见 design:外部二进制文件 sidecar,或 base64,或紧凑编码)。
5. **测试**:实体级 round-trip——构造含各类 payload 的 scene → save → load → 断言 payload 实体逐字段一致
   (点数/三角/tet/buffer 内容/仿真参数)。+ 向后兼容测试(读旧档不崩)。

### Out of scope(记后续)

- 压缩 / 增量保存 / 二进制格式优化(先正确,后性能)。
- `.svproj` 原生格式回写(本任务做 XQ 原生存档;svproj 是只读导入)。
- 跨 schema 大版本迁移工具。

## 3. 铁律约束(违反即 BLOCKER)

1. **io 零 VTK**:`xq_io` 只 link `xq_core` + tinyxml2(现状)。实体序列化**不得引入 VTK**——
   payload 实体数据从 XQ 句柄/core 类型直接取(points/triangles/tets/buffer 都是 XQ 自有,不需要 VTK)。
   审计:`rg "#include.*vtk" src/io` 必须空。
2. **向后兼容**:旧档(schema 1.1,无实体段)必须仍能 load 成功,payload 为空 + 发诊断,**不得 ParseError**。
   反向:新档被旧 reader 读 → `minimumReaderVersion` 机制已在(writer 写了),按需调。
3. **round-trip 无损**:save→load→save 两次输出一致(或 load 后 payload 逐字段等值)。浮点按定精度/二进制保。
4. **坐标系/单位保留**:影像 geometry 的 coordinateSystem(LPS)、单位不丢、不静默假定(reader 缺失发诊断)。
5. **provenance/stale/relation 不回归**:现有结构序列化逻辑不破坏。

## 4. 验收标准(逐条可证伪)

- [ ] **AC1 各 payload 落盘**:writer 输出含影像/路径/掩膜/表面/体网格/仿真案例/流场/AI 的实体数据段。
- [ ] **AC2 reader 读回**:load 后各 node 的 payload 非空,实体数据重建。
- [ ] **AC3 实体级 round-trip**:含各类 payload 的 scene save→load 后,逐字段断言一致
      (point/triangle/tet 数与内容、buffer、仿真参数 RCR/波形/Rom/Fluid、faceId)。
- [ ] **AC4 向后兼容**:旧 schema 1.1 档(无实体段)load 成功,payload 空 + 发诊断,不 ParseError。
- [ ] **AC5 io 零 VTK**:`rg "#include.*vtk" src/io` 空;`xq_io` link 列表无 VTK。
- [ ] **AC6 坐标系/单位保留**:影像 round-trip 后 coordinateSystem/spacing/origin/direction 一致;
      缺失时发诊断(测一条诊断用例)。
- [ ] **AC7 全量绿 + 假绿抽查**:全新构建 + 全量 ctest(原 44 + 新增持久化测试)全绿;
      假绿抽查:篡改 reader 漏读某字段(如 tet 索引)→ round-trip 测试 Release FAIL → 恢复 → PASS。
- [ ] **AC8 主线不回归**:原 `test_new_domain_roundtrip` / `test_project_roundtrip` / `test_project_versioned_save` 全绿
      (结构级 round-trip 不破坏)。

## 5. 已知现状(实测,供执行者接手)

- `XQProjectWriter::save`(`src/io/project/XQProjectWriter.cpp`,149 行):纯文本格式,
  - header:`XQ_NATIVE_PROJECT schemaVersion 1.1` / writerVersion `XQ-M8-001` / minimumReaderVersion `1.0` /
    createdWith / projectId。
  - body:`scene` → `nodes N` + `node <id> <domain_type> <display_name>`(字段 percent-encode)→
    `relations N` + `derived <src> <derived>` → `stale N` + `staleNode <id> <reason>` → `endScene` →
    `provenance`(record 1 占位)→ `diagnostics 0` → `end`。
  - **完全无 payload 实体**。`encode_field` 做 %HH 编码 unreserved 之外的字符。
- `XQProjectReader::load`(618 行):对称解析上述结构,产 `XQProjectReadResult{project, diagnostics}`;
  `Status`:Ok / FileNotFound / ParseError / UnsupportedVersion。
- payload 类型(`src/core/*Payload.h`):XQImageVolume / XQPathPayload / XQSegmentationMaskPayload /
  XQSurfaceModelPayload / XQMeshPayload / XQSimulationCasePayload / XQFlowResultPayload / XQAiAnalysisPayload。
  几何句柄:points/triangles/faceId/tets 都是 XQ 自有 POD,**不需要 VTK 即可序列化**。
- 现有测试:`test_new_domain_roundtrip`(结构 round-trip)、`test_project_roundtrip`、`test_project_versioned_save`。

## 6. 风险 / 注意

- **大 buffer 存储**:影像/掩膜体素纯文本 hex 会让档案爆大且慢。design 定 sidecar 二进制方案
  (主档引用 + `.bin` 实体),或 base64。**先把方案定清再写**。
- **格式选择**:现存档是自定义纯文本(非 xml)。是延续纯文本还是转 tinyxml2 xml?
  延续纯文本改动小、与现 reader 一致;转 xml 更规整但要重写 reader。**design 定**(倾向延续纯文本 + sidecar)。
- **schema 兼容矩阵**:1.1 旧档 ↔ 1.2 新 reader、1.2 新档 ↔ 旧 reader(minimumReaderVersion 控)。测清。
- **浮点精度**:点坐标/参数序列化精度要够(round-trip 等值);二进制 sidecar 可避精度损失。
