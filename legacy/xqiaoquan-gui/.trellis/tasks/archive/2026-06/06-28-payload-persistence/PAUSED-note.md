# 暂停 note — payload-persistence(2026-06-29)

## 为什么暂停
任务的 prd/design 按"轻量化单机原型"基线写(几何写主档文本、影像 buffer 落盘),
与真实目标**重型前处理(高分影像 + 千万级网格)**对不上。在错的基线上做完会返工,故暂停,
到新对话按重型目标**重写 prd/design** 后再实现。

## 已确认的方向性结论(新规划直接用)
1. **大几何必须二进制 sidecar 化**:surface 的 points/triangles/faceId、mesh 的
   surfPoints/surfTris/volPoints/tets、flowResult 的 q/p/a 矩阵——重型场景下写主档文本会让
   `.xqproj` 爆到 GB 级且 reader 逐行解析慢到不可用。`Point3`=3×double POD、Triangle/Tet=array<int,3/4>,
   适合 POD 直写二进制。当前 worker 只对 segMask 做了 sidecar(prd §6 风险已点名,大几何漏了)。
2. **不违背零-VTK**:sidecar 是 XQ 自写裸字节,`xq_io` 仍只 link xq_core+tinyxml2。
   对比 SimVascular:它靠 VTK 写 .vti/.vtp/.vtu 外包实体落盘(依赖 VTK);XQ 因零-VTK 铁律必须自己实现。
3. **目标定位**:重型在前处理/网格端,求解先 1D(FlowSolver1D 够用),3D CFD 通过
   `ICfdSolver` 接口 + adapter(可碰 VTK,在 adapters 层)后续接,本任务不做。
4. **image 无实体 payload**(已查证):全库无 image payload 类,Image 节点挂 XQSourcePayload
   只有 sourcePath;prd 里"影像 geometry+buffer 落盘"不适用。已定:接受现状,按 source kind 落 sourcePath。

## 有价值的产出(留着)
- `field-spec.md` — 8 类 payload 真实字段清单 + 取舍(节点真实 payload 类型分布)。事实基线,新规划直接用。
- stash 里的 reader/writer 改动 — sidecar 框架(segMask 的 bufferRef+byteCount+FNV checksum+相对路径)
  已写好,新方案可把它从 segMask 扩展到所有大几何,是复用不是新造。
- `worker-brief-implement.md` — 实现 brief。

## 代码状态
- 这次 worker 的代码改动(XQProjectReader/Writer.cpp、tests/io/test_payload_roundtrip.cpp、XQ/CMakeLists.txt)
  已 `git stash`,未入库(基于"几何写文本"方案,要按重型返工)。
- 未跑通全量验收:`test_project_versioned_save` FAIL(schema 1.1→1.2 断言 + 旧档 Info 诊断冲突,未决)。
- jsonl(implement/check)已 curate 过真实 spec 条目,保留。

## 新对话接手怎么做
1. 重读本 note + field-spec.md + 上述方向性结论。
2. 按"重型前处理"目标**重写 prd/design**:大几何 sidecar 二进制布局、向后兼容、零-VTK。
   (旧档诊断行为那个卡点也在重写时一并定:倾向"按版本区分,只 1.2+ 缺 payloads 段才发诊断";
    test_project_versioned_save 三处断言里 schema 1.1→1.2 那处必改。)
3. `git stash pop` 复用 sidecar 框架,或参考后重写。
4. 实现 → 全新 build 全量 ctest + 假绿抽查 → commit。
