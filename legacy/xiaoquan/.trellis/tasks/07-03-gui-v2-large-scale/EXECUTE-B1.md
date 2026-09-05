# EXECUTE-B1 — S0 命令栈修复 + S5a/S5b/S5c 算法热点(纯 core/services)

> **执行者须知**:本文档每处 before 代码均逐字对照过 `feat/gui-v2`(合并提交 5891c07)真实源码。你的任务是**照做**,不需要也不允许做本文之外的设计决策。若某处 before 与实际源码对不上(行号漂移正常,内容对不上才算),**停下报告,不要自行发挥**。

---

## 0. 环境与固定纪律

- 工作树:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/gui-v2`。Git Bash 下路径为 `/c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`。
- 完整构建(configure+build,禁用单 target 增量):
  ```bash
  cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ && cmd //c build_gui_wt.bat
  ```
- 全量测试:
  ```bash
  cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ && cmd //c ctest_merge.bat
  ```
- 单个测试快速回路(先完整 build,再):
  ```bash
  cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui && \
  "C:/software/Visual Studio/Visual Studio2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe" \
    -R '^test_command_stack$' --output-on-failure
  ```
- **绝不使用 `cmake --build --target <单目标>`**(本仓已实测会静默跳过链接、跑旧 exe 假绿)。每次改完源码都跑完整 `build_gui_wt.bat`,并用 `ls -l build_gui/<测试名>.exe` 核对时间戳确实更新后再跑 ctest。
- 测试文件纪律:全部用文件里已有的 `CHECK` 宏 + 显式 `fail()`,**副作用调用(resample/solve/execute 等)必须先存进变量再 CHECK**,绝不把副作用包进宏参数(Release /DNDEBUG 下会被删成假绿)。
- 不新建 .bat;不动本文档未列出的任何文件;commit 一律精确列文件路径,**绝不 `git add -A` / `git add .`**。
- 基线:开工前先跑一次全量 ctest,确认 **65/65 通过**;不通过先停下报告。

本批 4 个 commit,顺序固定:S0 → S5a → S5b → S5c。每个 commit 前:全量 ctest 全绿 + 对应假绿抽查做完并已还原。

---

## 1. S0 — `XQCommandStack::redo()` 失败丢命令

### 1.1 缺陷

`src/core/command/XQCommandStack.cpp` 的 `redo()`:命令已 `pop_back`,`execute()` 失败直接 `return false` —— 命令既不在 redo 栈也不在 undo 栈,**永久丢失**,用户无法重试。

### 1.2 先写测试(先红)

`tests/core/test_command_stack.cpp`:

**(a)** 在文件 include 区(第 8 行 `#include <core/command/XQSceneCommands.h>` 之后)加一行:

```cpp
#include <core/command/XQCommand.h>
```

**(b)** 在第一个匿名 namespace 内(`make_node` 函数之后、`} // namespace` 之前)加:

```cpp
// execute() succeeds on the 1st call, fails on the 2nd, succeeds again from the
// 3rd on; undo() always succeeds. Reproduces a redo() whose execute() fails
// transiently: the command must stay on the redo stack so the user can retry.
class FlakyCommand : public xq::XQCommand {
public:
    int executeCalls = 0;
    bool executed = false;

    bool execute() override
    {
        ++executeCalls;
        if (executeCalls == 2) {
            return false;
        }
        executed = true;
        return true;
    }

    void undo() override
    {
        executed = false;
    }

    std::string label() const override
    {
        return "flaky";
    }
};
```

**(c)** 在 `main()` 末尾 `return 0;` 之前加测试块:

```cpp
    // A redo() whose execute() fails must keep the command on the redo stack
    // (not silently drop it), so a later redo() can retry and succeed.
    {
        xq::XQCommandStack stack;
        auto owned = std::unique_ptr<FlakyCommand>(new FlakyCommand());
        FlakyCommand* flaky = owned.get();

        if (!stack.push(std::move(owned))) {
            return fail("flaky command initial push succeeds", __LINE__);
        }
        if (!stack.undo() || !stack.can_redo()) {
            return fail("flaky command undo leaves a redo entry", __LINE__);
        }

        const bool redo_failed_result = stack.redo(); // 2nd execute() -> false
        if (redo_failed_result) {
            return fail("failed redo reports false", __LINE__);
        }
        if (!stack.can_redo() || stack.redo_count() != 1) {
            return fail("failed redo keeps command on redo stack", __LINE__);
        }
        if (stack.can_undo()) {
            return fail("failed redo puts nothing on undo stack", __LINE__);
        }

        const bool redo_retry_result = stack.redo(); // 3rd execute() -> true
        if (!redo_retry_result) {
            return fail("retried redo succeeds", __LINE__);
        }
        if (!flaky->executed || flaky->executeCalls != 3) {
            return fail("retried redo re-executed the command", __LINE__);
        }
        if (!stack.can_undo() || stack.can_redo()) {
            return fail("retried redo restores normal stack state", __LINE__);
        }
    }
```

**验证红**:完整构建 + `ctest -R '^test_command_stack$'` → 必须失败在 `failed redo keeps command on redo stack`。不红则停下报告。

### 1.3 修复(后绿)

`src/core/command/XQCommandStack.cpp` 的 `redo()`:

before(逐字):
```cpp
    std::unique_ptr<XQCommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!command->execute()) {
        return false;
    }
```

after:
```cpp
    std::unique_ptr<XQCommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!command->execute()) {
        // Put the command back: a failed redo must stay retryable, not vanish.
        redo_stack_.push_back(std::move(command));
        return false;
    }
```

**验证绿**:完整构建 → `test_command_stack` 通过 → 全量 ctest 65/65。

### 1.4 假绿抽查

把 1.3 加的 `redo_stack_.push_back(std::move(command));` 注释掉 → 完整构建 → `test_command_stack` **必红**;还原,完整构建,再确认绿。

### 1.5 Commit 1

```
fix: redo 失败时把命令归还 redo 栈避免永久丢失
```
文件(精确 add):
- `src/core/command/XQCommandStack.cpp`
- `tests/core/test_command_stack.cpp`

---

## 2. S5a — 分割 visited 整卷 uint8 改位图

### 2.1 现状

`src/services/segmentation/SegmentationService.cpp` 两处 `std::vector<std::uint8_t> visited(voxelCount, 0)`(regionGrowMask 与 keepLargestConnectedComponent),512³ 时各 134 MiB。改成 1 bit/voxel(16.8 MiB)。**等价重构,无先红后绿;门禁 = 跨 64 位字边界新测试 + 假绿抽查。**

### 2.2 位图 helper

在该文件匿名 namespace 内(`is_valid_foreground_label` 函数之后、`} // namespace` 之前)加:

```cpp
// Bit-per-voxel visited set: 1/8 the memory of a byte vector, which matters at
// clinical volumes (512^3: 16.8 MiB instead of 134 MiB per pass).
std::vector<std::uint64_t> make_visited_bits(std::size_t bitCount)
{
    return std::vector<std::uint64_t>((bitCount + 63) / 64, 0);
}

bool bit_test(const std::vector<std::uint64_t>& bits, std::size_t index)
{
    return ((bits[index >> 6] >> (index & 63)) & 1u) != 0;
}

void bit_set(std::vector<std::uint64_t>& bits, std::size_t index)
{
    bits[index >> 6] |= (std::uint64_t{1} << (index & 63));
}
```

### 2.3 regionGrowMask 替换(4 处 + 注释)

before(逐字,现 :162-167 一带):
```cpp
    const auto label = static_cast<XQSegmentationMask::LabelType>(params.foregroundLabel);
    // Iterative 6-connected flood fill. visited tracks enqueued voxels so each
    // is processed once; the mask itself records the grown region.
    std::vector<std::uint8_t> visited(buffer.voxelCount(), 0);
    std::vector<std::size_t> stack;
    stack.push_back(seedIndex);
    visited[seedIndex] = 1;
```
after:
```cpp
    const auto label = static_cast<XQSegmentationMask::LabelType>(params.foregroundLabel);
    // Iterative 6-connected flood fill. visited (bit per voxel) tracks examined
    // voxels so each is processed once; the mask itself records the grown region.
    std::vector<std::uint64_t> visited = make_visited_bits(buffer.voxelCount());
    std::vector<std::size_t> stack;
    stack.push_back(seedIndex);
    bit_set(visited, seedIndex);
```

内层邻居循环,before(逐字,现 :191-195):
```cpp
            const std::size_t neighbour = buffer.voxelIndex(nx, ny, nz);
            if (visited[neighbour]) {
                continue;
            }
            visited[neighbour] = 1;
```
after:
```cpp
            const std::size_t neighbour = buffer.voxelIndex(nx, ny, nz);
            if (bit_test(visited, neighbour)) {
                continue;
            }
            bit_set(visited, neighbour);
```

### 2.4 keepLargestConnectedComponent 替换(4 处)

before(逐字,现 :225-226):
```cpp
    const std::size_t voxelCount = mask.voxelCount();
    std::vector<std::uint8_t> visited(voxelCount, 0);
```
after:
```cpp
    const std::size_t voxelCount = mask.voxelCount();
    std::vector<std::uint64_t> visited = make_visited_bits(voxelCount);
```

外层循环,before(逐字,现 :235-242):
```cpp
    for (std::size_t start = 0; start < voxelCount; ++start) {
        if (visited[start] || mask.labelAt(start) == 0) {
            continue;
        }
        component.clear();
        stack.clear();
        stack.push_back(start);
        visited[start] = 1;
```
after:
```cpp
    for (std::size_t start = 0; start < voxelCount; ++start) {
        if (bit_test(visited, start) || mask.labelAt(start) == 0) {
            continue;
        }
        component.clear();
        stack.clear();
        stack.push_back(start);
        bit_set(visited, start);
```

内层邻居循环,before(逐字,现 :261-266):
```cpp
                const std::size_t neighbour = mask.voxelIndex(nx, ny, nz);
                if (visited[neighbour] || mask.labelAt(neighbour) == 0) {
                    continue;
                }
                visited[neighbour] = 1;
                stack.push_back(neighbour);
```
after:
```cpp
                const std::size_t neighbour = mask.voxelIndex(nx, ny, nz);
                if (bit_test(visited, neighbour) || mask.labelAt(neighbour) == 0) {
                    continue;
                }
                bit_set(visited, neighbour);
                stack.push_back(neighbour);
```

改完 `rg 'std::vector<std::uint8_t> visited' src/services/segmentation/` 必须零命中。

### 2.5 跨字边界测试(新增,位图特有回归风险)

`tests/services/segmentation/SegmentationServiceTest.cpp`,在 `main()` 末尾 `return 0;`(若末尾有 `std::printf("OK: ...")` 则在其之前)加:

```cpp
    // Bitmap-visited regression: volumes wider than 64 voxels cross uint64 word
    // boundaries (bit 63 -> word 1 bit 0). A wrong shift/mask in the bitset
    // would stop the flood fill at the boundary.
    {
        const int dimX = 130;
        std::vector<std::uint8_t> values(static_cast<std::size_t>(dimX), 200);
        SyntheticImage synth = make_uint8_image(dimX, 1, 1, values);

        xq::XQRegionGrowingParameters params;
        params.lower = 100.0;
        params.upper = 255.0;
        params.foregroundLabel = 1;
        params.seed[0] = 0;
        params.seed[1] = 0;
        params.seed[2] = 0;

        const xq::SegmentationService::Result grown =
            xq::SegmentationService::regionGrowMask(synth.image, *synth.buffer, params,
                                                    xq::NodeId::invalid());
        CHECK(grown.ok());
        std::size_t labelled = 0;
        for (int i = 0; i < dimX; ++i) {
            if (grown.mask->labelAt(static_cast<std::size_t>(i)) == 1) {
                ++labelled;
            }
        }
        CHECK(labelled == static_cast<std::size_t>(dimX));
    }

    // Same word-boundary regression for keepLargestConnectedComponent: two runs
    // (70 vs 58 voxels) separated by a 2-voxel gap, both spanning a word edge.
    {
        const int dimX = 130;
        std::vector<std::uint8_t> values(static_cast<std::size_t>(dimX), 0);
        for (int i = 0; i <= 69; ++i) {
            values[static_cast<std::size_t>(i)] = 200;
        }
        for (int i = 72; i < dimX; ++i) {
            values[static_cast<std::size_t>(i)] = 200;
        }
        SyntheticImage synth = make_uint8_image(dimX, 1, 1, values);

        xq::XQSegmentationThresholdParameters thr;
        thr.lower = 100.0;
        thr.upper = 255.0;
        thr.foregroundLabel = 1;
        const xq::SegmentationService::Result seeded =
            xq::SegmentationService::thresholdMask(synth.image, *synth.buffer, thr,
                                                   xq::NodeId::invalid());
        CHECK(seeded.ok());

        const xq::SegmentationService::Result largest =
            xq::SegmentationService::keepLargestConnectedComponent(*seeded.mask);
        CHECK(largest.ok());
        for (int i = 0; i <= 69; ++i) {
            CHECK(largest.mask->labelAt(static_cast<std::size_t>(i)) == 1);
        }
        for (int i = 70; i < dimX; ++i) {
            CHECK(largest.mask->labelAt(static_cast<std::size_t>(i)) == 0);
        }
    }
```

注意:`XQRegionGrowingParameters` / `XQSegmentationThresholdParameters` 的字段名以 `src/services/segmentation/SegmentationService.h` 实际声明为准(现源码用法:`params.seed[0..2]`、`params.lower`、`params.upper`、`params.foregroundLabel`);若 lower/upper 是 int 型则去掉 `.0`。`NodeId::invalid()` 在本测试文件所含头里已可用(include 了 core/NodeId.h)。

**验证**:完整构建 → `ctest -R '^test_segmentation_service$'` 绿 → 全量 65/65 绿(既有分割/集成测试全绿即等价性证据)。

### 2.6 假绿抽查

把 **`bit_test`** 里的 `(index & 63)` 篡改成 `(index & 31)` → 完整构建 → `test_segmentation_service` **必红**(voxel 32 误读 voxel 0 的位被判"已访问",flood fill 在字边界早停,`labelled == 130` 失败)。**注意只准篡改 bit_test,不要动 bit_set**(篡改 bit_set 会让部分 voxel 永远测不到已访问,flood fill 不终止,测试挂死而不是干净变红)。还原,完整构建,确认绿。

### 2.7 Commit 2

```
refactor: 分割 visited 改位图,整卷内存降为 1/8
```
文件:
- `src/services/segmentation/SegmentationService.cpp`
- `tests/services/segmentation/SegmentationServiceTest.cpp`

---

## 3. S5b — XQPath::resample 游标推进 + framesForAllSamples 整批取帧

### 3.1 现状

- `src/core/XQPath.cpp` `resample()`:循环内每步调 `point_at_arc_length` / `tangent_at_arc_length`,二者内部 `segment_for_arc_length` 从头线性扫段 → O(N·M)。
- `frameAtArcLength()`:每次调用从头扫 samplePoints_;`CenterlineFrameService::computeFrames` 对每个 sample 调一次 → O(M²)。

两处独立优化,均为纯追加(公共 API 只加 `framesForAllSamples`,原方法全保留)。**新 API 无法先红后绿(旧代码不编译);门禁 = 对拍测试 + 假绿抽查。**

### 3.2 游标推进(XQPath.cpp,匿名 namespace 内改 3 个函数)

`segment_for_arc_length` before(逐字):
```cpp
std::size_t segment_for_arc_length(const std::vector<PathControlPoint>& points,
                                   const std::vector<double>& cumulative,
                                   double arcLength)
{
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        if (cumulative[i + 1] >= arcLength && cumulative[i + 1] > cumulative[i]) {
            return i;
        }
    }
```
after(签名加游标;前向循环从 `*cursor` 起;返回前回写;后备回扫循环原样不动):
```cpp
// cursor (optional): index of the segment found for the previous, smaller
// arcLength. resample() queries monotonically increasing arc lengths, so the
// containing segment index never decreases; scanning forward from the cursor
// makes a whole resample O(N + M) instead of O(N * M). Pass nullptr to scan
// from the beginning (single-query callers).
std::size_t segment_for_arc_length(const std::vector<PathControlPoint>& points,
                                   const std::vector<double>& cumulative,
                                   double arcLength,
                                   std::size_t* cursor)
{
    const std::size_t first = cursor != nullptr ? *cursor : 0;
    for (std::size_t i = first; i + 1 < points.size(); ++i) {
        if (cumulative[i + 1] >= arcLength && cumulative[i + 1] > cumulative[i]) {
            if (cursor != nullptr) {
                *cursor = i;
            }
            return i;
        }
    }
```
(该函数其余部分 —— 反向回扫与 `return 0;` —— 逐字保留,不回写 cursor。)

`point_at_arc_length` / `tangent_at_arc_length` 各加尾参转传,before(逐字,以 point 为例):
```cpp
Point3 point_at_arc_length(const std::vector<PathControlPoint>& points,
                           const std::vector<double>& cumulative,
                           double arcLength)
{
    const std::size_t segment = segment_for_arc_length(points, cumulative, arcLength);
```
after:
```cpp
Point3 point_at_arc_length(const std::vector<PathControlPoint>& points,
                           const std::vector<double>& cumulative,
                           double arcLength,
                           std::size_t* cursor = nullptr)
{
    const std::size_t segment = segment_for_arc_length(points, cumulative, arcLength, cursor);
```
`tangent_at_arc_length` 同构(加 `std::size_t* cursor = nullptr`,内部转传)。

`resample()` 主循环与端点补录改传游标,before(逐字,:233-248):
```cpp
    std::vector<PathSamplePoint> samples;
    for (double arcLength = 0.0; arcLength < totalLength; arcLength += sampleSpacing) {
        PathSamplePoint sample = {};
        sample.position = point_at_arc_length(controlPoints_, cumulative, arcLength);
        sample.tangent = tangent_at_arc_length(controlPoints_, cumulative, arcLength);
        sample.arcLength = arcLength;
        samples.push_back(sample);
    }

    if (samples.empty() || samples.back().arcLength < totalLength) {
        PathSamplePoint endpoint = {};
        endpoint.position = point_at_arc_length(controlPoints_, cumulative, totalLength);
        endpoint.tangent = tangent_at_arc_length(controlPoints_, cumulative, totalLength);
        endpoint.arcLength = totalLength;
        samples.push_back(endpoint);
    }
```
after:
```cpp
    std::vector<PathSamplePoint> samples;
    std::size_t segmentCursor = 0; // advances monotonically with arcLength
    for (double arcLength = 0.0; arcLength < totalLength; arcLength += sampleSpacing) {
        PathSamplePoint sample = {};
        sample.position = point_at_arc_length(controlPoints_, cumulative, arcLength, &segmentCursor);
        sample.tangent = tangent_at_arc_length(controlPoints_, cumulative, arcLength, &segmentCursor);
        sample.arcLength = arcLength;
        samples.push_back(sample);
    }

    if (samples.empty() || samples.back().arcLength < totalLength) {
        PathSamplePoint endpoint = {};
        endpoint.position = point_at_arc_length(controlPoints_, cumulative, totalLength, &segmentCursor);
        endpoint.tangent = tangent_at_arc_length(controlPoints_, cumulative, totalLength, &segmentCursor);
        endpoint.arcLength = totalLength;
        samples.push_back(endpoint);
    }
```

### 3.3 framesForAllSamples(纯追加)

`src/core/XQPath.h`,在 `FrameStatus frameAtArcLength(double arcLength, PathFrame* out) const;` 之后加:

```cpp
    // Copies the frame stored at every sample point (one PathFrame per
    // samplePoints() entry, same order) in a single O(M) pass. Requires a prior
    // successful resample(); returns NotResampled otherwise (out untouched).
    // Equivalent to calling frameAtArcLength at each sample's arc length,
    // without the per-call linear scan (O(M^2) -> O(M) for whole-path callers).
    FrameStatus framesForAllSamples(std::vector<PathFrame>* out) const;
```

`src/core/XQPath.cpp`,在 `frameAtArcLength` 实现之后加:

```cpp
XQPath::FrameStatus XQPath::framesForAllSamples(std::vector<PathFrame>* out) const
{
    if (samplePoints_.empty() || out == 0) {
        return FrameStatus::NotResampled;
    }

    out->clear();
    out->reserve(samplePoints_.size());
    for (std::size_t i = 0; i < samplePoints_.size(); ++i) {
        const PathSamplePoint& sample = samplePoints_[i];
        out->push_back(
            {sample.position, sample.tangent, sample.normal, sample.binormal, sample.arcLength});
    }
    return FrameStatus::Ok;
}
```

### 3.4 CenterlineFrameService 改用整批取帧

`src/services/path/CenterlineFrameService.cpp`,before(逐字,:33-48):
```cpp
    // One frame per sample point, taken at the sample's arc length. Reusing
    // frameAtArcLength keeps frame construction (orthonormalization) in one place
    // and inherits the resample's rotation-minimizing normal transport, so the
    // normal stays continuous between adjacent frames.
    result.frames.reserve(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        PathFrame frame = {};
        const XQPath::FrameStatus frame_status =
            working.frameAtArcLength(samples[i].arcLength, &frame);
        if (frame_status != XQPath::FrameStatus::Ok) {
            result.status = Status::ResampleFailed;
            result.frames.clear();
            return result;
        }
        result.frames.push_back(frame);
    }
```
after:
```cpp
    // One frame per sample point, copied straight from the resample's
    // rotation-minimizing transport in a single O(M) pass (the per-sample
    // frameAtArcLength loop was O(M^2): each call scans the sample list).
    const XQPath::FrameStatus frame_status = working.framesForAllSamples(&result.frames);
    if (frame_status != XQPath::FrameStatus::Ok) {
        result.status = Status::ResampleFailed;
        result.frames.clear();
        return result;
    }
```
(`samples.empty()` 的前置检查与函数其余部分不动。)

### 3.5 对拍测试

`tests/core/test_path.cpp`,在 `main()` 末尾 `return 0;` 之前加:

```cpp
    // framesForAllSamples: NotResampled before resample / with a null out.
    {
        xq::XQPath unsampled;
        unsampled.setControlPoints({{{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}});
        std::vector<xq::PathFrame> frames;
        const xq::XQPath::FrameStatus status = unsampled.framesForAllSamples(&frames);
        CHECK(status == xq::XQPath::FrameStatus::NotResampled);
        CHECK(frames.empty());

        const xq::XQPath::ResampleStatus rs = unsampled.resample(0.5);
        CHECK(rs == xq::XQPath::ResampleStatus::Ok);
        const xq::XQPath::FrameStatus null_status = unsampled.framesForAllSamples(0);
        CHECK(null_status == xq::XQPath::FrameStatus::NotResampled);
    }

    // Cross-check on a long 3D zig-zag: framesForAllSamples must agree with
    // frameAtArcLength at every sample (this also exercises the cursor-based
    // segment lookup in resample against the cursor-free frameAtArcLength path).
    {
        xq::XQPath dense;
        std::vector<xq::PathControlPoint> points;
        for (int i = 0; i < 1000; ++i) {
            xq::PathControlPoint cp;
            cp.position.x = 0.1 * static_cast<double>(i);
            cp.position.y = (i % 2) == 0 ? 0.0 : 0.05;
            cp.position.z = 0.02 * static_cast<double>(i);
            points.push_back(cp);
        }
        dense.setControlPoints(points);

        const xq::XQPath::ResampleStatus rs = dense.resample(0.25);
        CHECK(rs == xq::XQPath::ResampleStatus::Ok);

        std::vector<xq::PathFrame> frames;
        const xq::XQPath::FrameStatus fs = dense.framesForAllSamples(&frames);
        CHECK(fs == xq::XQPath::FrameStatus::Ok);
        CHECK(frames.size() == dense.samplePoints().size());

        for (std::size_t i = 0; i < frames.size(); ++i) {
            xq::PathFrame reference = {};
            const xq::XQPath::FrameStatus ref_status =
                dense.frameAtArcLength(dense.samplePoints()[i].arcLength, &reference);
            CHECK(ref_status == xq::XQPath::FrameStatus::Ok);
            CHECK(close(frames[i].arcLength, reference.arcLength, 1e-9));
            CHECK(close(frames[i].position.x, reference.position.x, 1e-9));
            CHECK(close(frames[i].position.y, reference.position.y, 1e-9));
            CHECK(close(frames[i].position.z, reference.position.z, 1e-9));
            CHECK(close(frames[i].tangent.x, reference.tangent.x, 1e-9));
            CHECK(close(frames[i].tangent.y, reference.tangent.y, 1e-9));
            CHECK(close(frames[i].tangent.z, reference.tangent.z, 1e-9));
            CHECK(close(frames[i].normal.x, reference.normal.x, 1e-9));
            CHECK(close(frames[i].normal.y, reference.normal.y, 1e-9));
            CHECK(close(frames[i].normal.z, reference.normal.z, 1e-9));
            CHECK(close(frames[i].binormal.x, reference.binormal.x, 1e-9));
            CHECK(close(frames[i].binormal.y, reference.binormal.y, 1e-9));
            CHECK(close(frames[i].binormal.z, reference.binormal.z, 1e-9));
        }
    }
```

**验证**:完整构建 → `test_path`、`test_centerline_frame_service`、`test_path_service` 绿 → 全量 65/65(既有 helix 连续性测试是转移不变量的对照)。

### 3.6 假绿抽查(两处各一)

1. 游标:把 `segment_for_arc_length` 里 `const std::size_t first = cursor != nullptr ? *cursor : 0;` 篡改为 `... ? *cursor + 1 : 0;` → 完整构建 → `test_path` 对拍块**必红**(重采样位置错段);还原并确认绿。
2. 整批取帧:把 `framesForAllSamples` push 的 `sample.normal` 篡改为 `scale(sample.normal, -1.0)` → `test_path` 对拍 normal 比对**必红**(`test_centerline_frame_service` 的 alignment>0.5 也应红);还原并确认绿。

### 3.7 Commit 3

```
refactor: 路径重采样游标推进 O(N+M),整批取帧替代逐点扫描
```
文件:
- `src/core/XQPath.h`
- `src/core/XQPath.cpp`
- `src/services/path/CenterlineFrameService.cpp`
- `tests/core/test_path.cpp`

---

## 4. S5c — FlowSolver1D 最后周期抽帧上限

### 4.1 现状与决策

`FlowSolver1D::solve` 对最后周期每步录一帧(`FlowSolver1D.cpp` `if (step >= recordStart)` 块)。20000 步 × 每段 3 序列 → 帧数组是大网格下的内存/序列化大头。
**决策:`SolverInput` 纯追加 `int maxRecordedFrames = 0;`,0/负 = 不限 = 旧行为逐字节等价。** 既有断言 `times().size() == numTimeSteps`(FlowSolver1DTest、FlowIntegrationTest、AiIntegrationTest 的 20000 步档)因默认 0 **零触碰**——这些测试文件本批**一个字都不改**(FlowSolver1DTest 追加新块除外)。GUI 侧把 `maxRecordedFrames` 设为 2000 的那一行**不在本批**(归 B5,避免与 B2 的 XQMainWindow.cpp 大改冲突)。

### 4.2 SolverInput 追加字段

`src/services/flow/FlowSolver1D.h`,`struct SolverInput` 内 `int numCycles = 1;` 行之后追加:

```cpp
        // Maximum number of recorded last-cycle frames. 0 (or negative) keeps
        // the historical behavior of recording every step. When positive and
        // numTimeSteps exceeds it, frames are recorded every
        // ceil(numTimeSteps / maxRecordedFrames) steps, and the final step is
        // always recorded so the series still ends at the end of the cycle.
        int maxRecordedFrames = 0;
```

### 4.3 solve() 抽帧

`src/services/flow/FlowSolver1D.cpp`,在 `const int totalSteps = ...` / `const int recordStart = ...` 两行(逐字定位:`const int recordStart = input.numTimeSteps * ((input.numCycles > 0 ? input.numCycles : 1) - 1);`)之后追加:

```cpp
    // Frame-recording stride for the last cycle (see SolverInput::maxRecordedFrames).
    int recordStride = 1;
    if (input.maxRecordedFrames > 0 && input.numTimeSteps > input.maxRecordedFrames) {
        recordStride =
            (input.numTimeSteps + input.maxRecordedFrames - 1) / input.maxRecordedFrames;
    }
```

录制条件 before(逐字):
```cpp
        // Record the last cycle.
        if (step >= recordStart) {
```
after:
```cpp
        // Record the last cycle (decimated by recordStride; final step always kept).
        if (step >= recordStart
            && (((step - recordStart) % recordStride) == 0 || step == totalSteps - 1)) {
```
(块体其余内容不动。)

### 4.4 测试(FlowSolver1DTest.cpp 追加两块)

在 `main()` 末尾 `std::printf("OK: ...")` 之前加:

```cpp
    // ===================================================================
    // maxRecordedFrames decimates the recorded last cycle: frame count is
    // bounded, the series still starts at 0 and ends at the cycle end, and
    // the decimated result stays consistent and analyzable.
    // ===================================================================
    {
        const std::size_t n = 11;
        std::vector<double> x(n, 0.0);
        std::vector<double> area(n, 0.5);
        for (std::size_t i = 0; i < n; ++i) {
            x[i] = 5.0 * static_cast<double>(i) / static_cast<double>(n - 1);
        }

        Solver::SolverInput in;
        in.arcLength = x;
        in.area0 = area;
        in.fluid = xq::FluidProperties{};
        in.inletWaveform.push_back(std::make_pair(0.0, 3.0));
        in.inletWaveform.push_back(std::make_pair(0.5, 8.0));
        in.inletWaveform.push_back(std::make_pair(1.0, 3.0));
        in.period = 1.0;
        in.rcr = {106.0, 0.00068483, 1784.0};
        in.numTimeSteps = 10000;
        in.dt = 1.0e-4; // 10000 * 1e-4 = one full period
        in.numCycles = 1;
        in.maxRecordedFrames = 100;

        const Solver::Result result = Solver::solve(in);
        CHECK(result.ok());
        const xq::XQFlowResult& flow = result.flow;
        CHECK(flow.isConsistent());
        CHECK(flow.times().size() >= 100);
        CHECK(flow.times().size() <= 101);
        CHECK(flow.times().front() == 0.0);
        CHECK(flow.times().back() >= in.period - 2.0 * in.dt);
        CHECK(flow.converged());

        // Decimated frames must still feed the metrics stage.
        xq::FlowMetricsService::Request request;
        request.referencePressure = 1.0e5; // absolute baseline: FFR needs Pa > 0
        const xq::FlowMetricsService::Result metrics =
            xq::FlowMetricsService::analyzeFlow(flow, request);
        CHECK(metrics.ok());
    }

    // maxRecordedFrames = 0 (the default) records every step: the historical
    // behavior, asserted explicitly so the default can never silently change.
    {
        const std::size_t n = 11;
        std::vector<double> x(n, 0.0);
        std::vector<double> area(n, 0.5);
        for (std::size_t i = 0; i < n; ++i) {
            x[i] = 5.0 * static_cast<double>(i) / static_cast<double>(n - 1);
        }

        Solver::SolverInput in;
        in.arcLength = x;
        in.area0 = area;
        in.fluid = xq::FluidProperties{};
        in.inletWaveform.push_back(std::make_pair(0.0, 3.0));
        in.inletWaveform.push_back(std::make_pair(0.5, 8.0));
        in.inletWaveform.push_back(std::make_pair(1.0, 3.0));
        in.period = 1.0;
        in.rcr = {106.0, 0.00068483, 1784.0};
        in.numTimeSteps = 200;
        in.dt = 1.0e-4;
        in.numCycles = 2;
        in.maxRecordedFrames = 0;

        const Solver::Result result = Solver::solve(in);
        CHECK(result.ok());
        CHECK(result.flow.times().size() == static_cast<std::size_t>(in.numTimeSteps));
    }
```

文件顶部 include 区追加:
```cpp
#include <services/ai/FlowMetricsService.h>
```
(`test_flow_solver_1d` 链接 `xq_services`,已含 ai 服务,无 CMake 改动。)

**验证**:完整构建 → `test_flow_solver_1d` 绿 → 全量 ctest 65/65。**特别确认** `test_flow_integration`、`test_ai_integration` 绿(默认 0 的零触碰证据)。

### 4.5 假绿抽查

把 4.3 的 `(((step - recordStart) % recordStride) == 0` 篡改为 `== 1` → 完整构建 → `test_flow_solver_1d` **必红**(`times().front() == 0.0` 失败:step 0 不再录制);还原,完整构建,确认绿。

### 4.6 Commit 4

```
feat: 流场求解最后周期抽帧上限 maxRecordedFrames(默认 0 保持旧行为)
```
文件:
- `src/services/flow/FlowSolver1D.h`
- `src/services/flow/FlowSolver1D.cpp`
- `tests/services/flow/FlowSolver1DTest.cpp`

---

## 5. 批次收尾(完成定义)

1. 4 个 commit 全部落盘后,再跑一次**全新配置的完整构建 + 全量 ctest**(如怀疑缓存,删 `build_gui` 重来),65/65 绿。
2. `git log --oneline -4` 确认提交信息与文件清单同本文档一致;`git status` 干净(无未跟踪的意外文件;若见大数据目录变 `??` 立即停下)。
3. 汇报格式:每个 commit 的 hash、先红后绿/假绿抽查各自的实际输出摘录(红时的 FAIL 行 + 绿时的 ctest 计数)。**没跑过的步骤不许写"通过"。**
