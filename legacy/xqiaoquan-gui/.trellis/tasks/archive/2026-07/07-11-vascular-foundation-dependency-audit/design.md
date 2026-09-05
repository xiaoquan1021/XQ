# Design: 依赖 ABI 与许可审计

## 1. Evidence Model

每项依赖用统一记录表示：

```text
DependencyRecord {
  name, version_or_commit, upstream_url, source_hash,
  source_path, install_path, license_id, license_file,
  local_patches[], cmake_package, imported_targets[],
  architecture, compiler, runtime, build_type,
  runtime_dlls[], probes[], decision, limitations
}
```

`decision` 只能是 `accepted`、`accepted-research-only`、`rejected`、`blocked`。不使用“应该可以”。

## 2. Audit Sources and Authority

按以下顺序交叉验证，任何单一来源不够：

1. 官方 release/tag/LICENSE。
2. `Externals/externals.manifest` 或选定依赖 lock。
3. 实际 source tree/version header/local diff。
4. 实际 install tree package config/imported target。
5. XQ CMake cache/build.ninja/link command/runtime dependency。
6. 最小 probe 的真实执行输出。

目录名只作线索，不作最终版本/ABI证据。

## 3. Probe Layout

依赖探针与产品实现分开，建议位于受控的 `XQ/probes/vascular_foundation/` 或 child research build 目录。探针必须：

- 使用与 XQ 相同 CMake toolchain/prefix。
- 每个 probe 只验证一个边界，失败能定位。
- 不被默认 app target 自动编入；审计结束后决定保留为 test 还是删除 throwaway probe。
- 输出模块版本、输入摘要和确定性数值，不输出 PHI。

Planned probes:

| Probe | Proves |
| --- | --- |
| `itk_vascular_filters_probe` | 3D diffusion + multiscale objectness + component/morphology + distance map 可实例化执行 |
| `itk_vtk_bridge_probe` | ITKVtkGlue/VTK 9.3 同链、oblique direction/spacing/origin 守恒 |
| `vtkvmtk_centerline_probe` | 最小 C++ centerline backend 在真实 surface 上稳定运行 |
| `tetgen_mmg_on_probe` | ON/ON build 真实选择 TetGen->MMG，而非 legacy fallback |

## 4. ABI Inspection

需要记录：

- `CMAKE_GENERATOR`、`CMAKE_CXX_COMPILER`、MSVC version/toolset。
- `CMAKE_SIZEOF_VOID_P`/x64。
- `CMAKE_CXX_STANDARD=17`。
- MSVC runtime flag/imported library config。
- Debug/Release imported locations。
- VTK/ITK build shared/static and runtime DLL set。
- `build.ninja`/link command 中的真实 library path。

若第三方 package 未暴露足够 metadata，使用非破坏性二进制检查和最小链接 probe，不猜。

## 5. ITK Component Strategy

宽泛 `find_package(ITK)` 容易引入不需要模块并通过 ITKVtkGlue 二次查找 VTK。审计结果应给出最小 component set；产品代码修改另由后续 child 执行。

Bridge probe 必须使用带非 identity direction 的 3D image，比较：

- dimensions
- spacing
- origin
- 3x3 direction
- selected index -> physical point
- VTK physical point -> original index/point round-trip

## 6. vtkvmtk Decision Gate

Timebox 不是按“花了多久”随意结束，而是按证据步骤：

1. revision/license accepted for probe；
2. minimal source dependency closure；
3. clean compile/link against exact VTK；
4. synthetic smoke；
5. real surface run；
6. sanitizer/debug reproduction for crash if any；
7. accept or reject.

只要 license 不可接受、依赖闭包要求 Python/SuperBuild、或真实 surface 稳定性不能复现，就拒绝生产采用。

## 7. License Handling

本 child 提供工程采用结论和 obligations 清单，不冒充法律意见。

- ITK: Apache-2.0 expected, verify installed/source LICENSE。
- VTK/GDCM: BSD-family expected, verify exact source。
- MMG: LGPL-3.0 expected, record dynamic-link/source obligations。
- TetGen: reconcile 1.5/1.5.1 and bundled AGPLv3 text; classify research-only/accepted/rejected for stated distribution model。
- vtkvmtk: verify exact selected files and transitive bundled code, not generic project homepage label。

## 8. Deliverables

- `research/dependency-lock.md`
- `research/abi-report.md`
- `research/license-report.md`
- `research/probe-results.md`
- reproducible command/script changes only where justified
- accepted/rejected backend decision table consumed by later children

## 9. Rollback

- Probe changes land independently from production CMake changes。
- Failed vtkvmtk sources are removable without changing XQ public contracts。
- No external install tree is overwritten in place; new sources/build/install paths are versioned/isolated。
- No cleanup/reset of user workspace or existing build trees。
