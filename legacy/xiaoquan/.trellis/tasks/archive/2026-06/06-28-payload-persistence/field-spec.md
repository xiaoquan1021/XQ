# Field Spec — payload 实体落盘字段清单(Step 1 产出)

> 逐读 core 头(只读,不扩 API)后整理。列每类节点 **实际挂载的 payload 类型** 及其可序列化字段、
> getter、主档 / sidecar 归属。**写码前的事实基线**,实现严格据此。

## 0. 关键事实校正(与 design 草案的偏差,已回报)

逐头核查后,**节点 payload 的真实类型分布**如下(grep `make_shared<...Payload>` 全库 + SvProjectReader/各 service 实测):

| domain | 节点实际挂的 payload 类型 | 实体载体 | 备注 |
|---|---|---|---|
| Image | `XQSourcePayload` | `sourcePath()` 相对路径串 | **无 XQImageVolume,无像素 buffer** |
| ContourGroup | `XQSourcePayload` | `sourcePath()` | SV 导入路径绑定 |
| SegmentationMask(SV导入) | `XQSourcePayload` | `sourcePath()` | — |
| Path | `XQPathPayload` | `XQPath` | 控制点 + 采样点(确定性重算) |
| SegmentationMask(分割服务) | `XQSegmentationMaskPayload` | `XQSegmentationMask` | **唯一真实体素 buffer**,走 sidecar |
| SurfaceModel | `XQSurfaceModelPayload` | `XQSurfaceModel` | 三角几何 + ModelFace |
| Mesh | `XQMeshPayload` | `XQMesh` | 三角 + tet + 边界面 |
| SimulationCase | `XQSimulationCasePayload` | `XQSimulationCase` | RCR/波形/Rom/Fluid |
| FlowResult | `XQFlowResultPayload` | `XQFlowResult` | 时序 |
| AiAnalysis | `XQAiAnalysisPayload` | `XQAiAnalysis` | metrics/annotations |

**取舍 1 — image 没有实体 payload**:`XQImageVolume` / `XQMemoryImageBufferHandle` 从不被任何
`XQPayload` 子类包装(全库无 image payload 类,grep 证实)。Image domain 节点挂的是 `XQSourcePayload`,
只有 `sourcePath()`。design §1 草案的 "image: geometry+buffer+window/level" **不适用** —— 节点上没有这些数据。
按"忠实落盘节点实际持有的数据 + 不扩 core API"原则,image 节点按 `source` kind 落盘其 `sourcePath()`。

**取舍 2 — sidecar 只用于 segMask**:唯一带真实大 buffer 的节点 payload 是 `XQSegmentationMaskPayload`
(`XQSegmentationMask.voxels()`,uint8/voxel)。影像像素 buffer 不在任何节点 payload 上,故 sidecar `.bin`
实际只对 segMask 生效。(design 草案"影像/掩膜 buffer 走 sidecar"中影像那半不成立。)

**取舍 3 — kind 集合调整**:design 草案 kind = {image,path,segMask,surface,mesh,simCase,flowResult,aiAnalysis}。
实际加入 `source`(`XQSourcePayload`,承载 image/contourGroup/SV段掩膜)。最终 kind ∈
{**source**, path, segMask, surface, mesh, simCase, flowResult, aiAnalysis}。
`source` 行额外记其 domain token(因为它 domain-parameterized)。

## 1. 各 payload 落盘字段(主档纯文本,double 用 setprecision(17))

### source(`XQSourcePayload`,挂 image/contourGroup/SV段掩膜等)
- `domainType()` → domain token(domainTypeToString)
- `sourcePath()` → 相对路径串(encode_field)
- 布局:`source <domainToken> <encodedSourcePath>`
- 重建:`make_shared<XQSourcePayload>(domain, sourcePath)`(domain 从 token 反解)

### path(`XQPathPayload` → `XQPath`)
- `id()`(PathId=NodeId)
- `interpolation()`(Polyline/Spline)
- `controlPoints()`(vector<PathControlPoint{Point3 position}>)
- `sampleSpacing()`(double;>0 时 load 后 resample 重算 samplePoints)
- `hasSourceImageNode()` / `sourceImageNode()`
- **samplePoints 不落盘**:`XQPath` 无 samplePoints setter,采样点由 `resample(spacing)` 确定性重算。
  落控制点 + spacing + interpolation,load 后 setControlPoints→resample 重建 samples。
- 布局:
  ```
  pathId <id> interp <Polyline|Spline> sampleSpacing <d> sourceImage <id|-> 
  controlPoints <M>
  cp <x> <y> <z>     (×M)
  ```
- 重建:XQPath; setId/setInterpolation/setControlPoints/setSourceImageNode; if spacing>0 resample(spacing)

### segMask(`XQSegmentationMaskPayload` → `XQSegmentationMask`)
- dims(`dimensionX/Y/Z`)
- `hasGeometry()` / `geometry()`(ImageGeometry:dims/spacing/origin/direction/coordSys)
- `hasSourceImageNode()` / `sourceImageNode()`
- `labels()`(vector<SegmentationLabel{int value; string name}>)
- `voxels()`(vector<uint8_t>,长度 = voxelCount)→ **sidecar .bin**
- 布局:
  ```
  maskDims <nx> <ny> <nz>
  maskGeometry <has 0|1> [spacing 3d origin 3d direction 9d coordSys LPS|RAS]
  maskSourceImage <id|->
  labels <L>
  label <value> <encodedName>   (×L)
  bufferRef <relpath> <byteCount> <checksum>
  ```
- 坐标系铁律:`hasGeometry()==false` → 发诊断(`PAYLOAD_MASK_GEOMETRY_MISSING`),不默认填。
- 重建:`XQSegmentationMask(dims)`; setGeometry(若 has); setSourceImageNode; setLabels; 逐 voxel setLabelAt(从 .bin)

### surface(`XQSurfaceModelPayload` → `XQSurfaceModel`)
- `id()`
- `source()`(ModelSource:Unknown/Loaded/Generated)
- `hasSourceContourGroupNode()` / `sourceContourGroupNode()`
- `hasTriangleGeometry()` / `triangleGeometry()` → `XQTriangleSurfaceGeometryHandle`
  - `pointCount()` + `point(i)`(Point3)
  - `triangleCount()` + `triangle(i)`(array<int,3>) + `triangleFaceId(i)`
- `faces()`(vector<ModelFace{int faceId; string name; FaceKind kind; optional<int> capId; vector<int> boundaryLoopIds}>)
- `preservedArrays()`(PreservedVtpArrays 4×bool)
- 布局:
  ```
  surfaceId <id> source <Unknown|Loaded|Generated> sourceContour <id|->
  preserved <gNode 0|1> <gElem 0|1> <faceId 0|1> <capId 0|1>
  hasTriGeom <0|1>
  points <P>
  pt <x> <y> <z>            (×P)
  triangles <T>
  tri <a> <b> <c> <faceId>  (×T)
  faces <F>
  face <faceId> <kindInt> <hasCap 0|1> <capId> <encodedName> <nLoops> <loopId...>  (×F)
  ```
- 重建:XQSurfaceModel; setId/setSource/setSourceContourGroupNode/setPreservedArrays;
  若 hasTriGeom → 新建 handle addPoint/addTriangle(a,b,c,faceId) → setTriangleGeometry;
  逐 face addFace

### mesh(`XQMeshPayload` → `XQMesh`)
- `id()`
- `hasSourceModelNode()` / `sourceModelNode()`
- `hasSurfaceTriangles()` / `surfaceTriangles()`(同 surface 的 triangle handle:pt/tri/faceId)
- `hasVolumeTets()` / `volumeTets()`(`XQTetVolumeMeshHandle`:pointCount/point(i)、tetCount/tet(i)=array<int,4>)
- `regions()`(vector<MeshRegion{int regionId; string name}>)
- `boundaryFaces()`(vector<MeshBoundaryFace{int faceId; string name; FaceKind kind; optional<int> capId; vector<int> cellIds; vector<int> localFaces}>)
- `quality()`(MeshQualitySummary:min/max/mean double + elementCount size_t)
- `preservedArrays()`(PreservedMeshArrays 4×bool)
- 布局:
  ```
  meshId <id> sourceModel <id|->
  meshPreserved <gNode> <gElem> <faceId> <capId>
  quality <min> <max> <mean> <elementCount>
  hasSurfTri <0|1>
  surfPoints <P> / surfPt <x><y><z> / surfTris <T> / surfTri <a><b><c><faceId>
  hasVolTet <0|1>
  volPoints <P> / volPt <x><y><z> / tets <T> / tet <a><b><c><d>
  regions <R> / region <regionId> <encodedName>
  meshFaces <F>
  meshFace <faceId> <kindInt> <hasCap><capId> <encodedName> <nCells> <cellId...> <nLocal> <local...>
  ```
- 重建:XQMesh; setId/setSourceModelNode/setPreservedArrays/setQuality;
  surfTri handle→setSurfaceTriangles; volTet handle→setVolumeTets; addRegion; addBoundaryFace

### simCase(`XQSimulationCasePayload` → `XQSimulationCase`)
- `id()`
- `hasSourceMeshNode()` / `sourceMeshNode()`
- `solverParameters()`(SolverParameters{int timeSteps; double timeStepSize})
- `fluidProperties()`(FluidProperties{double density; double viscosity})
- `romSettings()`(RomSettings{NodeId centerlineNode; vector<int> inlet/outletFaceIds; double period; int numTimeSteps; double dt; int numCycles})
- `boundaryConditions()`(vector<BoundaryCondition{int faceId; BoundaryConditionType type; double value; vector<double> rcr; vector<pair<double,double>> flowWaveform; double waveformPeriod}>)
- 布局:
  ```
  caseId <id> sourceMesh <id|->
  solver <timeSteps> <timeStepSize>
  fluid <density> <viscosity>
  rom centerline <id|-> period <d> numTimeSteps <n> dt <d> numCycles <n> inlet <k> <id...> outlet <k> <id...>
  bcs <B>
  bc <faceId> <typeInt> <value> rcr <k> <d...> waveform <m> <t0> <q0> ... waveformPeriod <d>  (×B)
  ```
- 重建:XQSimulationCase; setId/setSourceMeshNode/setSolverParameters/setFluidProperties/setRomSettings; addBoundaryCondition

### flowResult(`XQFlowResultPayload` → `XQFlowResult`)
- `hasSourceCaseNode()` / `sourceCaseNode()`
- `times()`(vector<double>)
- `segments()`(vector<FlowSegment{int segmentId; double arcLengthStart/End; int faceId}>)
- `flowQ()`/`pressureP()`/`areaA()`(vector<vector<double>>,[segment][time])
- `converged()` / `maxCfl()`
- 布局:
  ```
  flowSourceCase <id|-> converged <0|1> maxCfl <d>
  times <K> <t...>
  segments <S>
  seg <segmentId> <arcStart> <arcEnd> <faceId>  (×S)
  series <S> <K>     # 三个矩阵各 S 行 K 列;每行一行 q / 一行 p / 一行 a
  q <d × K> (×S)
  p <d × K> (×S)
  a <d × K> (×S)
  ```
- 重建:XQFlowResult; setSourceCaseNode/setTimes; addSegment×S; setSeries(q,p,a); setConverged/setMaxCfl

### aiAnalysis(`XQAiAnalysisPayload` → `XQAiAnalysis`)
- `kind()`(Identify/FlowMetrics/SurrogatePrediction)
- `modelId()`(string)
- `provenance()`(Computed/ModelInferred/SurrogatePredicted)
- `metrics()`(vector<NamedMetric{string name; double value; string unit}>)
- `annotations()`(vector<Annotation{string label; double arcLength; int faceId; double score}>)
- `hasSourceNode()` / `sourceNode()`
- `diagnostic()`(string)
- 布局:
  ```
  aiKind <int> provenance <int> modelId <encoded|-> source <id|-> diagnostic <encoded|->
  metrics <M>
  metric <encodedName> <value> <encodedUnit>  (×M)
  annotations <A>
  annotation <encodedLabel> <arcLength> <faceId> <score>  (×A)
  ```
- 重建:XQAiAnalysis; setKind/setModelId/setProvenance/setSourceNode/setDiagnostic; addMetric; addAnnotation

## 2. sidecar(只 segMask)
- 路径:`<projectFileStem>.<nodeId>.segMask.bin`,与主档同目录,主档记**相对路径**(filename)。
- 内容:`XQSegmentationMask.voxels()` 原始 uint8 字节。
- checksum:FNV-1a 32bit(io 自实现,零依赖)。
- 读:按 relpath(相对主档目录)读 .bin,校验 byteCount==voxelCount && checksum;不符→诊断+mask 全 0(不崩)。

## 3. 编码/精度约定
- 字符串字段(name/path/modelId/...)用现有 `encode_field`(%HH)。空串落 `-`(sentinel),读回 `-`→空(配 encode 区分:`-` 不在 unreserved? `-` 是 unreserved,会原样;故空串专门落 `%2D`?）。
  → 采用:可选 NodeId 用 `-` 表示无;可选/可空字符串用 encode_field(空串 encode 后仍是空 token,会被 split 吞掉)
  → **空串字符串字段统一落 `-` sentinel,encode_field 实际输出非空时不可能等于裸 `-`**(`-` 是 unreserved 会原样输出,故 name="-" 会与 sentinel 冲突)。
  → 取舍:对可能为空的 name/unit/modelId/diagnostic,落 `<len> <encoded>` 形式或前缀长度?
     **决定**:用 `kEmpty = "%00"`? 不行。改用 **总是 encode,空串落专用 token `\x01`?** 过度。
  → **最终决定**:可空字符串字段一律 `<present 0|1> <encoded>`;present=0 时无 encoded token。简单无歧义。
- double:`std::setprecision(17)` + `std::scientific` 不必,默认 `<<` 配 setprecision(17) 即可 round-trip。
- NodeId:`serialize()`/`deserialize()`;可选 NodeId 落 `-` 表无。

## 4. 向后兼容(reader)
- `endScene` 后 `line_is_single_token(lines,index,"payloads")` peek:
  - 是 `payloads` → 解析 payloads 段(本任务新增)。
  - 否(旧 1.1 档直接 `provenance`,或旧 1.0 无 provenance)→ 跳过,走原有 provenance/legacy 分支。
  - 旧 1.1 档(schema 1.1,无 payloads)load 成功,**发一条 Info 诊断**`PROJECT_NO_PAYLOAD_DATA`,payload 空。
- schema:writer 升 1.1→1.2;`minimumReaderVersion` 维持 `1.0`(payloads 是可选增量,旧 reader 跳过无害)。
  但 reader 当前对 1.1 档「无 payloads 段直接 provenance」是合法的,1.2 档「有 payloads」也合法,二者都 peek 区分。
</content>
</invoke>
