# EXECUTE-P0 — 三个 P0 的可执行指令(所有选择已拍板)

> 执行者:便宜 agent。**所有决策已由主审拍死,不得再选、不得改方案、不得自行加兜底**。
> 每处改动先 `grep` 定位符号再改(行号可能因代码变动漂移,以符号为准)。
> 工作目录:`C:\Users\OCEAN\Desktop\XIAOQUAN\XQ`。commit 精确列文件,禁止 `git add -A`。

---

## P0-1 依赖逆流:port 接口下沉 core 【方案已定 = A 接口下沉,不用改 spec】

### 决策(不可更改)
把三个 port 接口从 services 移到 core;adapter 只 include core、CMake 只链 xq_core。
**已验证前提**:三个 adapter 的 `.cpp` 只用 core 类型,不引用任何 services 实现符号,下沉后编译链接自洽。

### 步骤 1 — 移动 `ITetMesher.h`:`src/services/meshing/` → `src/core/meshing/`

1. `git mv src/services/meshing/ITetMesher.h src/core/meshing/ITetMesher.h`(先 `mkdir -p src/core/meshing`)。
2. 编辑新文件 `src/core/meshing/ITetMesher.h`:
   - include guard `XQ_SERVICES_MESHING_I_TET_MESHER_H` → `XQ_CORE_MESHING_I_TET_MESHER_H`(开头 `#ifndef/#define` 和结尾注释三处)。
   - 内部 include 不变(`core/XQTetVolumeMeshHandle.h`、`core/source/IGeometrySource.h` 已是 core,合法)。
3. 全仓库把 `#include "services/meshing/ITetMesher.h"` 改成 `#include "core/meshing/ITetMesher.h"`。涉及文件(已 grep 确认,共 7 处):
   - `src/adapters/mmg/MmgVolumeRemesher.h`
   - `src/adapters/mmg/TetGenThenMmg.h`
   - `src/adapters/tetgen/TetGenTetMesher.h`
   - `src/app/XQMainWindow.cpp`
   - `src/services/meshing/VolumeMeshService.cpp`
   - `tests/adapters/test_mmg_volume_mesh.cpp`
   - `tests/adapters/test_tetgen_volume_mesh.cpp`
   - 另有 `tests/services/meshing/VolumeMeshServiceTest.cpp` 若 include 了也一并改。

### 步骤 2 — 移动 `XQAiSegmentationRequest.h`:`src/services/segmentation/` → `src/core/`

> 该文件含 `XQAiSegmentationRequest` 结构体 + `XQAiSegmentationBackend` 接口,只依赖 core 类型。

1. `git mv src/services/segmentation/XQAiSegmentationRequest.h src/core/XQAiSegmentationRequest.h`。
2. 编辑新文件:include guard `XQ_SERVICES_SEGMENTATION_XQ_AI_SEGMENTATION_REQUEST_H`
   → `XQ_CORE_XQ_AI_SEGMENTATION_REQUEST_H`(头尾三处)。内部 include(`core/XQImageVolume.h` 等)不变。
3. 全仓库把 `#include "services/segmentation/XQAiSegmentationRequest.h"` → `#include "core/XQAiSegmentationRequest.h"`。涉及(已 grep,共 8 处):
   `src/adapters/onnx/OnnxBackend.h`、`src/services/ai/AiService.h`、`src/services/segmentation/SegmentationService.h`、
   `src/ui/controllers/SegmentationController.h`、`tests/app/WorkflowIntegrationTest.cpp`、
   `tests/services/ai/AiServiceTest.cpp`、`tests/services/segmentation/SegmentationServiceTest.cpp`、
   `tests/ui/controllers/WorkflowControllerTest.cpp`。

### 步骤 3 — 抽出 AI identify/surrogate 接口到 core 新头 `src/core/XQAiBackends.h`

> `AiService.h` 现在混着「纯接口」(`XQAiIdentifyRequest/Backend`、`XQSurrogateRequest/Backend`)和「服务实现类」(`AiService`)。
> adapter 只需要前者。把前者抽到 core 新头,`AiService.h` include 它。

1. 新建 `src/core/XQAiBackends.h`,内容:
```cpp
#ifndef XQ_CORE_XQ_AI_BACKENDS_H
#define XQ_CORE_XQ_AI_BACKENDS_H

#include "core/XQAiAnalysis.h"

#include <memory>
#include <string>

namespace xq {

class XQSurfaceModel;
class XQFlowResult;

// Request to identify / annotate geometry features (stenosis, plaque, branch).
struct XQAiIdentifyRequest {
    std::string modelId;
};

// Backend abstraction for AI feature identification. ONNX types never appear
// here -- this header pulls in no external library.
class XQAiIdentifyBackend {
public:
    virtual ~XQAiIdentifyBackend() = default;

    virtual XQAiAnalysis identify(const XQSurfaceModel& model,
                                  const XQAiIdentifyRequest& request) = 0;
};

// Request to predict a flow field with a surrogate model (in place of a solve).
struct XQSurrogateRequest {
    std::string modelId;
};

// Backend abstraction for surrogate flow prediction. Returns an XQ-owned flow
// result; a null return signals failure. Same no-external-dependency rule.
class XQSurrogateBackend {
public:
    virtual ~XQSurrogateBackend() = default;

    virtual std::shared_ptr<XQFlowResult> predict(const XQSurfaceModel& model,
                                                  const XQSurrogateRequest& request) = 0;
};

} // namespace xq

#endif // XQ_CORE_XQ_AI_BACKENDS_H
```
2. 编辑 `src/services/ai/AiService.h`:
   - 删除其中 `XQAiIdentifyRequest`、`XQAiIdentifyBackend`、`XQSurrogateRequest`、`XQSurrogateBackend` 四个定义(22-54 行区块)。
   - 在 include 区加 `#include "core/XQAiBackends.h"`。
   - 保留 `AiService` 类本身及其余 include。`class XQSurfaceModel; class XQFlowResult;` 前向声明可保留(重复无害)。
3. `src/adapters/onnx/OnnxBackend.h`:把 `#include "services/ai/AiService.h"` 换成 `#include "core/XQAiBackends.h"`
   (OnnxBackend 只用三个 backend 接口 + `XQTensor`,不用 `AiService` 类本身——已 grep OnnxBackend.cpp 确认)。

### 步骤 4 — CMake:接口进 xq_core,adapter 去掉 xq_services

1. `XQ/CMakeLists.txt` 的 `add_library(xq_core STATIC ...)`(21-47 行):
   ITetMesher/XQAiSegmentationRequest/XQAiBackends 都是**纯头文件**(无 .cpp),xq_core 是 STATIC 且用
   `target_include_directories(xq_core PUBLIC .../src)`,头文件随 include 路径自动可见,**无需加进源文件列表**。确认无遗漏即可。
2. `xq_adapter_onnx`(554-558 行):`PUBLIC xq_services` 改为删除该行,只留 `PUBLIC xq_core PRIVATE onnxruntime::onnxruntime`。
3. `xq_adapter_tetgen`(593-596 行):`PUBLIC xq_core xq_services` → `PUBLIC xq_core`。
4. `xq_adapter_mmg`(633-636 行):`PUBLIC xq_core xq_services xq_adapter_tetgen` → `PUBLIC xq_core xq_adapter_tetgen`
   (**保留 xq_adapter_tetgen**,这是文档化的两段式设计,不是违规)。
5. 测试目标里若 `test_tetgen_volume_mesh`/`test_mmg_volume_mesh` 显式链了 `xq_services`(598-600、638-640 行),
   保留不动(测试用 VolumeMeshService 等服务,链 services 合法)。

### 步骤 5 — spec 补记(不改铁律,只补两段式说明)

编辑 `.trellis/spec/XQ/architecture/external-libs.md`,在 OCCT/MMG 那行附近补一句:
> MMG adapter 依赖 TetGen adapter(两段式:TetGen 填充 → MMG 优化),属 adapters 层内的合法有向依赖。

### 步骤 6 — 架构边界守护 ctest(把三条铁律从"人查"变"机器查")

> recon 确认仓库已有 `cmake -P` 脚本型测试先例:`CMakeLists.txt:910` 注册的 `test_portable_test_data_root`
> → `tests/cmake/check_portable_test_data_root.cmake`。**照抄这个模式**新增一个纯脚本测试,零编译成本、每次 ctest 自动跑。
> 当前架构清洁度已 recon 预检:core/services 头/io 均无外部库 include,adapter→services 恰好是待治理的 5 个 include(修完应归零)。

1. 新建 `tests/cmake/check_arch_boundaries.cmake`,用 `file(GLOB_RECURSE ...)` + `file(READ ...)` + 正则,断言:
   - `src/core/**` 无 `#include <Qt.../vtk.../itk.../gdcm.../onnx...>`(core 零外部依赖);
   - `src/services/**/*.h` 无 `#include <vtk/itk/gdcm/onnx...>`(服务公开头不泄漏外部库类型);
   - `src/io/**` 无 Qt/vtk/itk/gdcm/onnx include;
   - `src/adapters/**` 无 `#include "services/`(P0-1 修完后必须归零——这条同时是 P0-1 的回归护栏)。
   任一命中则 `message(FATAL_ERROR ...)` 列出违规文件+行,`ctest` 判红。
2. `XQ/CMakeLists.txt` 照 `test_portable_test_data_root` 的写法注册:
   `add_test(NAME test_arch_boundaries COMMAND ${CMAKE_COMMAND} "-DXQ_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_arch_boundaries.cmake")`。
3. **先红后绿(提交前的一次性本地验证,不单独提红 commit)**:测试文件与 P0-1 修复最终同入 commit 1,
   但**在做 include/CMake 修复之前**,先只创建 `check_arch_boundaries.cmake` + 注册,在**未改 include 的当前树**上
   跑一次 `ctest -R test_arch_boundaries`,**确认它判红并列出恰好这 5 处违规**(此清单即正则自检基准):
   ```
   src/adapters/mmg/MmgVolumeRemesher.h:4   #include "services/meshing/ITetMesher.h"
   src/adapters/mmg/TetGenThenMmg.h:4       #include "services/meshing/ITetMesher.h"
   src/adapters/tetgen/TetGenTetMesher.h:4  #include "services/meshing/ITetMesher.h"
   src/adapters/onnx/OnnxBackend.h:5        #include "services/ai/AiService.h"
   src/adapters/onnx/OnnxBackend.h:6        #include "services/segmentation/XQAiSegmentationRequest.h"
   ```
   命中数 ≠ 5 → 正则写错(漏了某 include 形态/路径),先修正则再继续。确认红后再做步骤 1-4 的 include/CMake 修复,复跑转绿。
4. **护栏有效性抽查(P0-1 全部改完后)**:临时在任一 adapter 头加回一行 `#include "services/..."`,
   跑 `ctest -R test_arch_boundaries` **确认转红**,再撤销。证明护栏对回归真的敏感(防「正则永远绿=假护栏」,
   参见 memory `fullverify-discarded-digest-no-anchor`「篡改后不转红=假绿」)。这一步不进 commit,仅验证。

> 此测试进 P0-1 的 commit 1(它是 P0-1 的验收与回归护栏),commit 文件清单已含 CMakeLists.txt,另加
> `tests/cmake/check_arch_boundaries.cmake`。

### P0-1 验收
```
# adapter 默认 OFF,必须开 TETGEN+MMG 才真编译到改动:
cmake -B build-audit -DXQ_ENABLE_TETGEN=ON -DXQ_ENABLE_MMG=ON <其余按 build 配方>
cmake --build build-audit            # 完整构建,别用单 --target(见 memory ninja-target 假绿坑)
# 核对 exe 时间戳真的变了,再:
ctest --test-dir build-audit -C Release --output-on-failure
```
> ⚠️ **ONNX 分支必须单独确认能编译**:步骤 2/3 改了 `OnnxBackend.h` 的两处 include,但 onnx adapter 默认 OFF
> (`XQ_ENABLE_ONNX`,CMakeLists:543),上面 TETGEN+MMG 构建**根本不编译它**——若 include 换错导致编不过,ctest 全绿=假绿。
> 故须**额外**开 ONNX 试编一次:`cmake -B build-onnx -DXQ_ENABLE_ONNX=ON <其余按配方>` 后
> `cmake --build build-onnx --target xq_adapter_onnx`(onnxruntime 不可用时至少 configure + 编该 target 到报错为止,
> 确认不是 include 替换引入的编译错)。arch-boundary ctest 只查 include 文本、不编译,抓不到这类错。

全绿(含新增 `test_arch_boundaries`)+ ONNX 分支编译通过 +
`grep -rn "services/meshing/ITetMesher\|services/segmentation/XQAiSegmentationRequest\|services/ai/AiService.h" src/adapters/` **返回空**(adapter 不再 include services)才算过。

---

## P0-2 符号链接/junction 逃逸(canonical 路径封闭校验)【方案已定 — 已本机实测校正】

> ⚠️ **实测更正(2026-07-02,本机 MSVC C++17 探针,见 audit-report「P0-2 实测」)**:
> Windows 上 **`std::filesystem::is_symlink` 对 NTFS junction 一律返回 false**(连 junction 目录本身也是 false,探针实测 `is_symlink_junction_dir=0`)。
> 因此**任何基于 `is_symlink` 的检测对 junction 完全无效**;仅对最终组件加 `FILE_FLAG_OPEN_REPARSE_POINT` 也挡不住
> 「把中间目录(如 `blobs/`)换成指向外部的 junction」。实测有效的唯一手段:`fs::weakly_canonical(path)`
> (MSVC 底层走 `GetFinalPathNameByHandle`,穿透 junction 解析到外部真实路径),与 `fs::weakly_canonical(rootDir)`
> 做**路径元素前缀比较**。探针已证:junction 逃逸判 `inside=0`、普通 blob 判 `inside=1`。

### 决策(不可更改)
- **共享工具 `src/core/io/PathSafety.{h,cpp}` 提供两个函数**(合并三份重复,消化 P2 重复代码项):
  1. `bool isConfinedRelativePath(const std::string& relPath)` —— 纯词法(拒空/绝对/root/`..`),不碰文件系统。第一道快速拒绝。
  2. `bool isPathWithinRoot(const std::string& rootDir, const std::string& fullPath)` —— **物理封闭校验(唯一能挡 junction)**:
     对两者取 `fs::weakly_canonical`,`fullPath` 的 canonical 必须以 `rootDir` 的 canonical 为路径元素前缀;
     任一 `weakly_canonical` 失败(error_code 非 0)即判不封闭返回 false。
- **两条读路径(BlobStore ifstream、MmapBlob mmap)在「完整路径拼好、打开之前」都调 `isPathWithinRoot(root, fullPath)`,不通过即拒绝**。
  不重定向、不兜底。词法检查(1)保留作第一道拒绝,物理检查(2)兜住 junction。
- **`MmapBlob.cpp` 的 `FILE_FLAG_OPEN_REPARSE_POINT` 仍加**作为第二道防线(明确"不跟随最终组件的 reparse point"),
  但**验收以 `isPathWithinRoot` 为准**——它才挡得住中间目录 junction。
- ~~在 `readVerified` 加 `is_symlink` 拒绝~~ —— **作废**,实测对 junction 无效。改为下方步骤 4 的 `isPathWithinRoot`。
  （`is_symlink` 对 junction 无效,已作废,见上决策）。

### 步骤 1 — 新建共享工具(词法 + 物理封闭)

`src/core/io/PathSafety.h`:
```cpp
#ifndef XQ_CORE_IO_PATH_SAFETY_H
#define XQ_CORE_IO_PATH_SAFETY_H

#include <string>

namespace xq {

// Lexical confinement: rejects empty, absolute, rooted, and any path containing
// a ".." component. Does NOT touch the filesystem. First, cheap gate.
bool isConfinedRelativePath(const std::string& relPath);

// Physical confinement: canonicalizes both paths (resolving NTFS junctions /
// symlinks via the OS) and returns true only if fullPath resolves to a location
// at or below rootDir. This is the ONLY check that stops a junction whose target
// is outside the asset root. Any canonicalization failure => not confined.
// An empty rootDir returns false (no boundary to enforce; never silently accept).
bool isPathWithinRoot(const std::string& rootDir, const std::string& fullPath);

} // namespace xq

#endif // XQ_CORE_IO_PATH_SAFETY_H
```
`src/core/io/PathSafety.cpp`:
```cpp
#include "core/io/PathSafety.h"

#include <filesystem>
#include <system_error>

namespace xq {

bool isConfinedRelativePath(const std::string& relPath)
{
    if (relPath.empty()) {
        return false;
    }
    const std::filesystem::path rel(relPath);
    if (rel.is_absolute() || rel.has_root_name() || rel.has_root_directory()) {
        return false;
    }
    for (std::filesystem::path::const_iterator it = rel.begin(); it != rel.end(); ++it) {
        if (*it == "..") {
            return false;
        }
    }
    return true;
}

bool isPathWithinRoot(const std::string& rootDir, const std::string& fullPath)
{
    namespace fs = std::filesystem;
    // Empty root has no confinement boundary to enforce. Current production never
    // constructs GRM with an empty root (XQMainWindow gates on !assetRootDir.empty()),
    // and BlobStore always has a real rootDir. Treat empty root as "cannot confine" =>
    // reject, so an empty root can never silently accept an escaping path.
    if (rootDir.empty()) {
        return false;
    }
    std::error_code ec1, ec2;
    // weakly_canonical resolves junctions/symlinks (MSVC: GetFinalPathNameByHandle)
    // and does not require the leaf to exist -- fine for pre-open checks.
    const fs::path canonRoot = fs::weakly_canonical(fs::path(rootDir), ec1);
    const fs::path canonPath = fs::weakly_canonical(fs::path(fullPath), ec2);
    if (ec1 || ec2 || canonRoot.empty()) {
        return false;
    }
    // Element-wise prefix: every component of canonRoot must match canonPath.
    fs::path::const_iterator rb = canonRoot.begin();
    fs::path::const_iterator pb = canonPath.begin();
    for (; rb != canonRoot.end(); ++rb, ++pb) {
        if (pb == canonPath.end() || *pb != *rb) {
            return false;
        }
    }
    return true;
}

} // namespace xq
```
加进 `XQ/CMakeLists.txt` 的 `add_library(xq_core STATIC ...)` 源文件列表:`src/core/io/PathSafety.cpp`。

### 步骤 2 — 三处删本地词法定义,改调共享工具(已 grep 核实调用点)

> 全 src/ 的 confinement 出现点(recon 已核实,一个不漏):
> - `BlobStore.cpp:119`【定义】、`:301`【调用,readVerified 入口】
> - `XQProjectReader.cpp:979`【定义】、`:1011`【调用,checked_ref_byte_count,reader 内唯一调用点】
> - `GeometryResourceManager.cpp:80`【定义】、`:103`【调用,validateGeometryBlobRef,仅 geometry 链路】

- `src/io/blob/BlobStore.cpp`:删匿名 namespace 的 `isConfinedRelativePath`(119-136 行),顶部加
  `#include "core/io/PathSafety.h"`;调用点 `:301` 不变(代码在 `namespace xq` 内直接可见 `xq::isConfinedRelativePath`)。
- `src/services/resource/GeometryResourceManager.cpp`:删本地 `isConfinedRelativePath`(80-95 行),include 同上,`:103` 调用点不变。
- `src/io/project/XQProjectReader.cpp`:删本地 `is_confined_relative_path`(979-993 行),include 同上;
  `:1011` 调用点 `is_confined_relative_path(ref.relPath)` 改名为 `isConfinedRelativePath(ref.relPath)`。

### 步骤 3 — MmapBlob 加 reparse 拒绝(第二道防线)+ 封闭校验

> 已核实 `MmapBlob.cpp:16-35` 的 `MappingControl` **是 RAII**(析构里 `CloseHandle(hFile)`/`CloseHandle(hMap)`/`UnmapViewOfFile`)。
> 下方 early return 前**无需手动关句柄**——原文档"若无 RAII 手动关"的分支已删除,不要再自行加。

`src/io/source/MmapBlob.cpp` 的 `map_file_readonly`(约 :56),把 `CreateFileW`(约 66-68 行):
```cpp
// before:
ctl->hFile = ::CreateFileW(widePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
```
改为(加 flag + reparse 拒绝;`ctl` 析构自动关句柄,直接 return 即可):
```cpp
// after: do not follow a reparse point at the leaf; reject it outright.
ctl->hFile = ::CreateFileW(widePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                           nullptr);
if (ctl->hFile == INVALID_HANDLE_VALUE) {
    result.lastError = ::GetLastError();
    return result;   // ctl 析构关句柄
}
BY_HANDLE_FILE_INFORMATION info;
if (!::GetFileInformationByHandle(ctl->hFile, &info)) {
    result.lastError = ::GetLastError();
    return result;
}
if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    result.lastError = ERROR_ACCESS_DENIED;
    return result;   // 拒绝最终组件是 symlink/junction 的情况
}
```
> **注意**:`FILE_FLAG_OPEN_REPARSE_POINT` 只挡"最终组件本身是 reparse point";中间目录 junction 靠**调用方**的
> `isPathWithinRoot` 挡(见步骤 4 GRM 两条链路)。`map_file_readonly` 本身拿不到 rootDir,所以封闭校验放在 GRM 调用点。

### 步骤 4 — 两条读路径在打开前加 `isPathWithinRoot` 封闭校验

**(a) BlobStore ifstream 路径** —— `src/io/blob/BlobStore.cpp` `readVerified`,
在 `const fs::path path = fs::path(rootDir_) / ref.relPath;`(约 310 行)之后、`fs::exists` 之前插入:
```cpp
if (!isPathWithinRoot(rootDir_, path.string())) {
    return Status::MissingBlob;   // junction/symlink escapes the asset root
}
```

**(b) GRM geometry 链路** —— `src/services/resource/GeometryResourceManager.cpp` 的 `fill` lambda
(**已核实签名 `auto fill = [&](...) -> bool`,:284-304;失败 `return false`,成功 `return true`**),
在 `dst->path = joinPath(assetRootDir_, ref->relPath);`(约 :296)之后、进入 `MappedGeometrySource` 之前加:
```cpp
if (!isPathWithinRoot(assetRootDir_, dst->path)) {
    return false;   // lambda 返回 bool;false 会经 !fill(...) 传播成空 handle,与既有 validateGeometryBlobRef 失败一致
}
```
> ⚠️ **必须写 `return false;`,不是裸 `return;`(bool lambda 里编译不过),更不是 `return GeometrySourceHandle();`
> (那是外层函数返回类型,写进 lambda 会类型错乱)**。`fill` 会被 points/tris/faceId/volPoints/tets 调用最多 5 次
> (:310-312、:320-321),这一处 check 即覆盖所有 blob,无需每次调用点重复加。

**(c) GRM voxel 链路(新发现缺口,必须补)** —— `acquireVoxelSource`(:166-230)对 `voxelsRef->relPath`
**当前只有 nullptr/empty 检查(:188),没有任何 confinement**。在 `:191 const std::string relPath = ...` 后先加词法检查,
在 `:193-195 blobPath` 拼好后加物理检查:
```cpp
// after relPath obtained (:191):
if (!isConfinedRelativePath(relPath)) {
    return VoxelSourceHandle();
}
// after blobPath assembled (:195), before constructing MappedVoxelSource:
if (!isPathWithinRoot(assetRootDir_, blobPath)) {
    return VoxelSourceHandle();
}
```
> voxel 链路顶部加 `#include "core/io/PathSafety.h"`(整个 GRM.cpp 一次 include 即可)。

### 步骤 5 — 负面测试(必须,先红后绿)

框架:`tests/io/test_blob_store.cpp` 与 `GeometryResourceManagerTest.cpp` **均为自制断言**
(`int g_failures` + `check(bool, const char*)`),**没有 gtest**。故不能用 `GTEST_SKIP()`。

- **BlobStore 用例**(`tests/io/test_blob_store.cpp`):在 rootDir 下用
  `system("cmd /c mklink /J \"<root>\\blobs\" \"<外部目录>\"")` 建 junction(`mklink` 是 cmd 内建,**必须 `cmd /c`**,
  不能直接 `system("mklink ...")`)。外部目录里放一个符合某 ref 的文件,断言 `readVerified` 返回 `MissingBlob`
  (不是 Ok、不读到外部内容)。清理时 `cmd /c rmdir "<root>\blobs"` 只删 junction 不动外部目标。
- **GRM voxel 用例**(`GeometryResourceManagerTest.cpp`,利用现成 `freshDir()`/`makeVoxelBlob`/`makeVoxelAsset`):
  voxel 链路有**两道检查**,负面测试要分别覆盖、且都能先红后绿:
  - **词法逃逸(验 `isConfinedRelativePath`)**:注册 voxels blob 的 `relPath = "../outside.bin"`,
    **并在 `<assetRoot>/../outside.bin` 放一个尺寸/内容都合法的真实 voxel blob**(照 `makeVoxelBlob` 的尺寸,
    如 16×16×16×2=8192 字节,否则补丁前就因 `valid_==false` 返回无效 handle=永远绿,测不到 confinement)。
    断言 `acquireVoxelSource` 返回**无效 handle**。补丁前:文件存在→映射成功→**拿到有效 handle(红)**;补丁后:词法拒→无效 handle(绿)。
  - **物理 junction 逃逸(验 `isPathWithinRoot`)**:`relPath` 词法合法(如 `pre/x.bin`),但把 `<assetRoot>/pre`
    建成指向外部目录的 junction,外部放尺寸正确的真实 voxel blob。断言无效 handle。补丁前:经 junction 映射成功→有效 handle(红);补丁后:canonical 前缀比较判逃逸→无效 handle(绿)。
  > 关键:**两个 voxel 用例的外部/逃逸目标都必须是"尺寸正确的真实 blob"**,否则补丁前就因 `MappedVoxelSource valid_==false`
  > (文件不存在/尺寸不符,MappedVoxelSource.cpp:33-40)返回无效 handle,red-before-patch 不可达 = 假绿(篡改 confinement 也测不出)。
- **建 junction 失败的处理**:junction 目录在 Windows 无需管理员(探针已证退出码 0)。万一 `system` 返回非 0,
  **不得静默当通过**——用 `check(false, "cannot create junction: environment unsupported")` 显式记一条失败,
  或打印醒目告警并让该用例的 `g_failures` 保持可见(注释写明原因)。宁可红,不可假绿。
- **验证顺序**:先不打 P0-2 补丁跑 → 确认用例**红**(证明抓得到逃逸);再打补丁 → 确认**绿**。

### P0-2 验收
默认档全量 ctest 全绿 + 两个新负面用例绿;
`grep -rn "isConfinedRelativePath\|is_confined_relative_path" src/` 只应出现在 `PathSafety.*` 定义 + 各调用点(无第二份定义体);
`grep -rn "isPathWithinRoot" src/` 出现在 PathSafety 定义 + BlobStore/GRM 两处(geometry)+ GRM voxel 处。

---

## P0-3 恶意 path payload 致 resample 死循环(设采样点上限)【方案已定】

### 决策(不可更改)
在 **reader 端**对 `sampleSpacing` 相对总弧长设下限,使采样点数有界;并**检查 `resample` 返回值**,
非 Ok 则整个加载报 `ParseError`(与现有 blob 校验失败一致的处理,不静默、不兜底)。
下限规则:`spacing` 必须 ≥ `totalLength / kMaxPathSamples`,其中 `kMaxPathSamples = 1'000'000`(一百万点,足够任何真实血管中心线)。

### 步骤 1 — XQPath::resample 增加点数上限保护(core 侧,自我保护)

`src/core/XQPath.cpp` `resample`(209 行起),在 `for` 循环前计算并加保护:
```cpp
// after totalLength computed, before the sample loop:
// Guard against a hostile (spacing, length) combo that would allocate an
// unbounded number of samples (a malicious project file). 1e6 samples covers
// any real centerline; beyond that, reject rather than hang/OOM.
constexpr std::size_t kMaxSamples = 1'000'000;
if (totalLength / sampleSpacing > static_cast<double>(kMaxSamples)) {
    return ResampleStatus::InvalidSpacing;
}
```
> 放在 `sampleSpacing <= 0.0` 检查之后(此时 spacing > 0,除法安全)、`cumulative`/`totalLength` 求出之后。

### 步骤 2 — reader 检查返回值,失败即报错

`src/io/project/XQProjectReader.cpp` `rebuild_path`(约 1298 行):
```cpp
// before:
if (spacing > 0.0) {
    path.resample(spacing);
}
```
改为:
```cpp
// after: a hostile spacing (tiny value vs huge arc length) must fail the load,
// not silently hang. resample enforces a sample-count ceiling.
if (spacing > 0.0) {
    if (path.resample(spacing) != XQPath::ResampleStatus::Ok) {
        return PayloadRebuild::TextError;
    }
}
```
> 确认 `rebuild_path` 的返回类型是 `PayloadRebuild` 且 `TextError` 是其失败值(已核实上下文 1298 行属于返回 `PayloadRebuild::Ok` 的函数)。确认 `XQPath::ResampleStatus` 在该 .cpp 可见(XQPath.h 已 include)。

### 步骤 3 — 负面测试(必须,文件已点名)

> **不要手拼 xqproj 文本**(段头/token 格式易错,见 memory `xq-section-header-peek`,拼错会让 load 因别的原因失败→假绿)。
> 用现成入口构造合法 payload 再改 spacing。

- **io 用例 → `tests/io/test_payload_roundtrip.cpp`**(该文件已有 `build_path(project)` 造 path、writer `save`、
  `set_blob_token(...)` 改单个 token、`expect_load_fails(...)` 断言加载失败的辅助):
  用 `build_path` 造一条控制点相距极大(如两点 x 相差 1e9)的 path → writer 存成合法工程 → 用 `set_blob_token`
  把 path payload 里的 `sampleSpacing` 改成极小值(如 1e-9)→ `expect_load_fails` 断言 `XQProjectReader::load`
  返回 `ParseError` 且**有限时间内返回**(不 hang)。若 `set_blob_token` 改不到 path payload 内的 spacing token,
  就用同文件既有的「先 build 后改 payload 文本单 token」模式,只替换 spacing 那一个数值,别整段重写。
- **core 用例 → `tests/core/test_path.cpp`**(该文件已存在):直接调 `XQPath::resample(极小 spacing)` 断言返回
  `InvalidSpacing`(命中 kMaxSamples 上限);正常 spacing 断言 `Ok`;`spacing<=0` 断言 `InvalidSpacing`(既有行为不回归)。
- **先红后绿(本地一次性验证)**:core 用例可直接先跑确认红(补丁前 resample 无上限,极小 spacing 会 hang——
  **仅本地手工验证、别进 CI 跑未打补丁版**);更稳的先红做法是打补丁后临时把 `kMaxSamples` 改超大值确认用例转红,再改回。
  io 用例同理:补丁后确认 1 秒内返回 `ParseError`。

### P0-3 验收
默认档全量 ctest 全绿 + 两个新用例绿;恶意工程加载在 1 秒内返回 `ParseError`。

---

## 三个 P0 的 commit 划分(精确列文件)

```
# commit 1 (P0-1):feat: adapter 依赖逆流治理 — port 接口下沉 core
git add src/core/meshing/ITetMesher.h src/core/XQAiSegmentationRequest.h src/core/XQAiBackends.h \
        src/services/ai/AiService.h src/adapters/onnx/OnnxBackend.h \
        src/adapters/tetgen/TetGenTetMesher.h src/adapters/mmg/MmgVolumeRemesher.h \
        src/adapters/mmg/TetGenThenMmg.h src/app/XQMainWindow.cpp \
        src/services/meshing/VolumeMeshService.cpp src/services/segmentation/SegmentationService.h \
        src/ui/controllers/SegmentationController.h CMakeLists.txt \
        .trellis/spec/XQ/architecture/external-libs.md \
        tests/adapters/test_mmg_volume_mesh.cpp tests/adapters/test_tetgen_volume_mesh.cpp \
        tests/services/meshing/VolumeMeshServiceTest.cpp tests/app/WorkflowIntegrationTest.cpp \
        tests/services/ai/AiServiceTest.cpp tests/services/segmentation/SegmentationServiceTest.cpp \
        tests/ui/controllers/WorkflowControllerTest.cpp tests/adapters/onnx/OnnxBackendTest.cpp \
        tests/cmake/check_arch_boundaries.cmake
# (git mv 会自动 stage 删除的旧路径;确认 git status 里旧 ITetMesher.h / XQAiSegmentationRequest.h 是 renamed)

# commit 2 (P0-2):fix: 资产 blob 拒绝符号链接/junction 逃逸 + 合并词法校验 + voxel 链路补 confinement
git add src/core/io/PathSafety.h src/core/io/PathSafety.cpp CMakeLists.txt \
        src/io/blob/BlobStore.cpp src/services/resource/GeometryResourceManager.cpp \
        src/io/project/XQProjectReader.cpp src/io/source/MmapBlob.cpp \
        tests/io/test_blob_store.cpp tests/services/resource/GeometryResourceManagerTest.cpp

# commit 3 (P0-3):fix: 恶意 path payload 致 resample 死循环 — 采样点上限 + 校验返回值
git add src/core/XQPath.cpp src/io/project/XQProjectReader.cpp \
        tests/core/test_path.cpp tests/io/test_payload_roundtrip.cpp
```
> **注意**:P0-1 和 P0-2 都动 CMakeLists.txt 和 XQProjectReader.cpp,分两 commit 时确保各自只含本步改动;
> 若嫌拆分易错,可合并为一个 commit,但 commit message 要覆盖三项。**由执行 agent 视实际 diff 决定,但绝不 `git add -A`**。
