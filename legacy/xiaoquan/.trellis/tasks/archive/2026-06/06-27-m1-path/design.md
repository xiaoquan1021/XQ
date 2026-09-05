# M1 路径 — 技术设计(design)

> 基于对 M0 实际产出接口的核查(XQPath / XQDataNode / 命令 / XQScene / PTHPathReader),非 plan 文字。
> 遵循 spec/XQ:core 零依赖、命令/undo、坐标 LPS/mm、参考 SimVascular 不看 XQ1。

## M0 现状核查(已读源码)

- `XQPath`(core):有 `id / interpolation / controlPoints / samplePoints / sourceImageNode /
  sampleSpacing / resample(spacing) / frameAtArcLength(arc, out)`。结构体 `PathControlPoint{Point3}`、
  `PathFrame`、`PathSamplePoint{position,tangent,normal,binormal,arcLength}` 已就绪。
- `XQDataNode`:有 payload-aware 构造 `(id, XQDomainType, name, shared_ptr<XQPayload>)` + `setPayload`。
- 命令(M0 已建):`AddNodeWithSourceRelationCommand(scene*, XQDataNode, source, label)`、
  `ReplacePayloadCommand(scene*, id, domain, payload, label)`——M1 PathService 直接复用,不新建命令类型。
- `XQScene`:insert/remove/link_derived/mark_source_changed 等 public(W1 收紧点)。
- `PTHPathReader::read` 产出 `PTHReadResult{XQPath path, pathName, sourceRelativePath, rawSamplePoints}`。
- **缺口**:M0 没有承载 `XQPath` 几何的 payload;SvProjectReader 给 path 节点挂的是 `XQSourcePayload`
  (仅相对路径)。M1 必须补真实 path payload,并让"读入的 path"与"新建的 path"共用它。

## 设计要点

### 1. XQPathPayload(core 新增,零依赖)
- `class XQPathPayload : public XQPayload`,持有一个 `XQPath`;实现 `domainType()==Path`、`clone()`(深拷贝)。
- 让 PathService 创建的 path 与 PTHPathReader 读入的 path **共用同一 payload 类型**(M1 验收硬要求)。
- 放 `src/core/XQPathPayload.h`(payload 属 core 数据,不属 service)。

### 2. PathService(src/services/path,纯领域,返回命令)
按 prd 公开 API,全部返回 M0 既有命令(不新建命令类型):
- `createPathCommand(name, sourceImageNodeId, controlPoints, spacing)`
  → 组装带 `XQPathPayload` 的 `XQDataNode` + `AddNodeWithSourceRelationCommand`(source=影像节点)。
- `moveControlPointCommand / insertControlPointCommand / deleteControlPointCommand / resamplePathCommand`
  → 复制旧 `XQPath` payload、改控制点或重采样、构造新 `XQPathPayload`、返回 `ReplacePayloadCommand`
    (保留原 path id 与 sourceImageNode)。
- service 不持 scene、不改 scene;只读节点当前 payload + 产出命令。命令的 scene* 由调用方(测试/UI)持有。
- 校验(prd):创建≥2 控制点、spacing>0;编辑要求节点 domain==Path 且有 XQPathPayload;
  move/delete 索引存在、insert 索引可==点数、delete 后≥2 点。非法返回带诊断的失败(不抛异常穿 UI)。

### 3. CenterlineFrameService(src/services/path)
- 沿 `XQPath` 算 origin/tangent/normal/binormal/arcLength 标架序列,复用 `resample` + `frameAtArcLength`。
- **法向不翻转**:用平行传输 / 旋转最小化标架(RMF, double-reflection 法,参考 SimVascular 的 path frame 思路)。
- 输出标架序列供 contour/放样消费;唯一来源,不在别处重算。

### 4. SvProjectReader 接真实 path 几何(满足"0007 读成 XQPath 节点")
- 让 path 节点的 payload 从 `XQSourcePayload` 升级为 `XQPathPayload`:在 SvProjectReader 里对 Paths/*.pth
  调用 PTHPathReader 填充真实 `XQPath`,挂 `XQPathPayload`。
- expected-scene 节点数/关系不变,但 path 节点 payload 类型从 source 变 path-typed;更新集成测断言。

### 5. W1 前置:收紧 scene 可变性(M0 复核遗留)
- 目标:让"scene 变更经命令栈"有类型层保护,而非纯纪律。
- **方案(最小侵入)**:把 `XQScene` 的 mutator(insert/remove/link_derived/mark_source_changed)
  改为仅命令类 + reader building 通道可达——具体用 friend 命令类,或把 mutator 移到一个
  `XQSceneMutator`/friend 边界。PathService 只能拿到 const scene 读节点,产出命令;命令持 scene* 执行变更。
- **裁剪**:若 friend 改造牵动面过大(影响 M0 已绿测试),退而求其次:mutator 保留 public 但
  PathService 签名只接受 `const XQScene&` 读、产出命令,**至少 service 层不碰 mutator**;W1 完整收紧
  作为 risk 记录,留到有更多 service 时统一做。以"不破坏 M0 26 测试 + M1 service 不直接改 scene"为底线。

## 风险 / 注意
- W1 收紧可能触及 M0 命令实现(命令需能调 mutator)。改完必须全量 ctest 仍绿。
- RMF 标架正确性需数值测试(法向连续、与 tangent 正交、单位向量)。
- 验收纪律:Release 全量 ctest;副作用调用别进 assert(用 M0 已确立的 CHECK 风格)。
