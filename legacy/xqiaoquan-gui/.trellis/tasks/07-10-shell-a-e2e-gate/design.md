# Design：壳 A 端到端验收

## Shared-path rule

Headless test 与 GUI 必须调用相同 DICOM reader/import service、Profile assembler、FlowInputAssembler、smoke service、commands 和 project IO。测试可以提供固定 ids/fixture，但不能重写生产算法。

## Success fixture

- 最小去标识多切片 DICOM series；
- 与其 FrameOfReferenceUID/LPS geometry 对齐的金 Path 和三个以上合法 contour；
- 已知 Profile area/arc 期望值；
- fixed FlowSmokeProtocolV1。

测试顺序：导入 DICOM、提交 Path/Contour、装配 Profile、运行 smoke、保存、销毁全部运行时 cache、重开、重新 acquire voxel 并比较 checksum/metadata/lineage。

## GUI acceptance

GUI 只负责编排同一 controller：明确 series 选择、后台进度、scene tree、MPR/3D overlay、Profile/Flow smoke 状态和 unavailable state。真机检查记录操作步骤、结果和诊断，不以截图替代 headless assertions。

## Failure matrix

每个失败点断言：状态码正确；scene node/asset/relation 数不变；undo count 不变；已有 active dataset 不被替换。Flow OFF 作为独立 build/runtime matrix。

## Completion record

在 task implement 文件记录 fixture 来源、build commit、focused/full tests、真实 series 检查、GUI checklist、已知非阻塞限制和 stop-line 声明。
