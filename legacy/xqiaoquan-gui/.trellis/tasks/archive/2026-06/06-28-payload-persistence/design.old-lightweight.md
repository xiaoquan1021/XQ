# Design — 存档 payload 实体数据持久化

> 配合 `prd.md`。讲格式选择、各 payload 的序列化布局、大 buffer 策略、schema 兼容、reader 结构。

## 0. 设计决策(先定,别边写边改)

1. **延续现有自定义纯文本格式**(非转 xml):现 writer/reader 是纯文本逐行(`key value`),
   reader 618 行已稳。在 `endScene` 后、`provenance` 前**新增 `payloads` 段**,改动最小、向后兼容易做。
2. **大二进制走 sidecar**:影像/掩膜 buffer 不进主档,写成同目录 `.bin` 文件(`<projectId>.<nodeId>.image.bin`),
   主档记 `bufferRef <relpath> <byteCount> <checksum>`。小几何(points/tris/tets)直接进主档文本(量可控)。
3. **schema 1.1 → 1.2**:新增 payloads 段。reader 见 1.1 档无 payloads 段 → payload 空 + 诊断,不报错。

## 1. 主档新增段布局(`payloads`)

在 `endScene` 与 `provenance` 之间插:

```
payloads <N>
payload <nodeId> <payloadKind>
  ... kind-specific lines ...
endPayload
... (重复 N 次) ...
endPayloads
```

`payloadKind` ∈ {image, path, segMask, surface, mesh, simCase, flowResult, aiAnalysis}。

### 各 kind 的行布局(草案,执行者按实际 payload 字段细化)

- **image**:
  ```
  geometry dims <nx> <ny> <nz> spacing <sx> <sy> <sz> origin <ox> <oy> <oz>
  direction <9 doubles> coordSys LPS scalarType <type> components <c>
  window center <c> width <w> intensityRange <lo> <hi>
  bufferRef <relpath> <byteCount> <checksum>     # 体素走 sidecar
  ```
- **path**:`points <M>` + 每点 `pt <x> <y> <z>`(+ 标架若有 `frame <...>`)。
- **segMask**:`labels <...>` + `bufferRef <relpath> <byteCount> <checksum>`(mask 走 sidecar)。
- **surface**:`points <P>` + `pt <x> <y> <z>`×P;`triangles <T>` + `tri <a> <b> <c> <faceId>`×T;
  `faces <F>` + `face <faceId> <kind> <capId> <name>`×F(ModelFace 元数据)。
- **mesh**:`points <P>` + `pt`×P;`tets <T>` + `tet <a> <b> <c> <d>`×T;边界面 `faces <F>` + `face ...`。
- **simCase**:RCR(`rcr <Rp> <C> <Rd>` 每出口)、波形(`waveform <M>` + 采样点)、
  RomSettings(inlet/outletFaceIds、segments)、FluidProperties(`fluid density <d> viscosity <v>` CGS)。
- **flowResult**:每段/每点的压力/流量标量数组(若大 → sidecar,否则文本)。
- **aiAnalysis**:指标 kv(FFR/WSS/OSI/dP 等)。

> 字段值沿用 writer 的 `encode_field`(%HH)处理名字等含空格/特殊字符的串;数值直接写,定足够精度
> (如 `setprecision(17)` for double round-trip)。

## 2. sidecar 二进制

- 写:buffer → `<projectFileDir>/<projectId>.<nodeId>.<kind>.bin`,原始字节(scalarType 决定 element 大小)。
- 主档:`bufferRef <relpath> <byteCount> <checksum>`(checksum 可用简单 FNV/CRC,核验完整性)。
- 读:按 relpath 读 .bin,byteCount/checksum 校验;不匹配 → 诊断 + payload buffer 空(不崩)。
- 路径用相对(相对主档目录),换机/移动目录仍可读。

## 3. reader 扩展

- `XQProjectReader::load` 解析到 `payloads` → 逐 `payload` 块按 kind 重建 payload,
  用 node id 找到对应 node,挂 payload。
- **向后兼容**:解析完 `endScene` 后,peek 下一 token:是 `payloads` 则解析,是 `provenance` 则跳过
  (旧档 1.1)→ 不报错。
- 缺字段/坐标系缺失 → push 诊断(`XQProjectReadResult.diagnostics`),不静默假定(铁律)。
- 解析失败局部化:单个 payload 块坏 → 该 node payload 空 + 诊断,不整档 ParseError(尽量鲁棒)。

## 4. schema 兼容矩阵

| 档 schema | reader | 结果 |
|---|---|---|
| 1.1(无 payloads) | 1.2 reader | OK,payload 空 + 诊断"档案不含实体数据" |
| 1.2(有 payloads) | 1.2 reader | OK,实体重建 |
| 1.2 | 旧 1.1 reader | 由 `minimumReaderVersion` 控:若新档把它升到 >1.1,旧 reader 拒(UnsupportedVersion) |

> 决定:1.2 writer 的 `minimumReaderVersion` 是否升?若实体段是**可选增量**(旧 reader 跳过也无害),
> 可不升;若实体段是必读,升到 1.2。**倾向不升**(实体是增量,旧 reader 读结构仍有效)——design 定为不升,
> 让旧 reader 能读新档的结构部分。

## 5. 测试设计(AC3/AC4/AC6/AC7)

`tests/io/test_payload_roundtrip.cpp`(link xq_io,无需 Qt):

- 构造含各 payload 的 scene(小尺寸:影像 4×4×2、表面 4 点 4 三角、体网格几个 tet、一个 simCase 带 RCR/波形)。
- save 到 temp → load → 逐字段断言:
  - 影像 dims/spacing/origin/direction/coordSys/buffer 字节一致。
  - 表面 points/triangles/faceId 一致;mesh tets 一致;simCase RCR/波形/fluid 一致。
- **向后兼容**:写一个手工 1.1 档(无 payloads)→ load → Status::Ok + payload 空 + 有诊断。
- **坐标系诊断**:构造 coordSys 缺失的档 → load 发诊断。
- CHECK 宏式,**reader::load 等副作用调用先取变量再判断,绝不进 assert**(铁律,Release /DNDEBUG)。
- 假绿点:reader 故意漏读 tet 第 4 索引 → round-trip 测试 FAIL。

CMake:`add_executable(test_payload_roundtrip ...)` + `target_link_libraries(... PRIVATE xq_io)` + `add_test`。

## 6. 不做 / 后续

- 压缩 / 增量 / 二进制格式优化 → 后续(先正确)。
- `.svproj` 回写、跨大版本迁移 → 后续。
- sidecar 改 HDF5/单文件容器 → 后续(先 sidecar .bin)。
