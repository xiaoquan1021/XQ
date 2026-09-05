# Design — 引入 TetGen/MMG 生产级体网格

> 配合 `prd.md`。讲 kernel 适配方式、依赖边界(谁 link tetgen)、faceId 映射、降级机制。

## 0. 设计原则

- **kernel 包在 adapter,暴露纯 XQ 函数**:`adapters/tetgen` 只导出一个签名全是 XQ 类型的函数,
  内部 .cpp 调 TetGen。这样 `tetgen.h` 不出 adapter。
- **不让 `xq_services` 直接 link tetgen**(保 service 纯 C++ 可无头单测的铁律)。见 §2 的依赖方案。
- **TetGen 路径与星形剖分共存**,kernel 选择运行时/编译期决定,星形是 fallback。

## 1. adapter 接口(`src/adapters/tetgen/TetGenVolumeMesher.{h,cpp}`)

公开头零 tetgen 类型:

```cpp
namespace xq {
class XQTriangleSurfaceGeometryHandle;
class XQTetVolumeMeshHandle;

struct TetGenParams {
    double maxVolume = 0.0;      // -a;0=不限
    double minRadiusEdgeRatio = 2.0; // -q quality
    bool preserveSurface = true; // -Y 不在边界面加点(保 faceId 映射简单)
};

struct TetGenResult {
    bool ok;
    std::string message;
    std::shared_ptr<XQTetVolumeMeshHandle> mesh; // 含 tet + 边界面 faceId
    // 边界三角 → faceId 的映射(若 mesh 句柄本身不带,单独给)
};

// 把闭合表面送 TetGen 生成体网格。surface 的 triangleFaceId 作为 facet marker
// 传入,TetGen 输出的边界面带回该 marker → 回填到结果。纯 XQ 类型进出。
TetGenResult tetgenTetrahedralize(const XQTriangleSurfaceGeometryHandle& surface,
                                  const TetGenParams& params);
} // namespace xq
```

`.cpp` 内:`XQ surface → tetgenio(in)`(pointlist / facetlist,facet marker = triangleFaceId)→
`tetrahedralize("pq1.4YA", &in, &out)` → `tetgenio(out) → XQTetVolumeMeshHandle`
(out.pointlist → addPoint,out.tetrahedronlist → addTet,out.trifacemarkerlist → 边界面 faceId)。

## 2. 依赖边界(关键决策)

**问题**:`VolumeMeshService` 在 `xq_services`,现只 link `xq_core`。要用 TetGen 怎么不破纯净?

**方案 A(推荐):依赖注入 / 函数指针**。
- `VolumeMeshService::buildVolumeMesh` 增一个可选参数:`TetMesher` 回调
  (`std::function<TetGenResult(const XQTriangleSurfaceGeometryHandle&, const TetGenParams&)>`),
  或一个纯虚 `ITetMesher` 接口(定义在 core/services,零 tetgen)。
- 默认无回调 → 走星形剖分;传入回调(由上层 controller 用 `adapters/tetgen` 装配)→ 走 TetGen。
- 这样 `xq_services` **不 link tetgen**,只定义接口;装配在 controller/app 层。与 M6 `AiService` 的
  纯虚 Backend + mock 范式一致。

**方案 B:新建 `xq_meshing_kernel` 库**,link tetgen,`xq_services` 不动,controller 直接调 adapter
  生成 mesh 再交 service 入 scene。更解耦但多一层。

> **定方案 A**(与既有 AiService Backend 范式一致,改动最小,service 仍零外部依赖)。

依赖方向:`app/controllers → adapters/tetgen → core`;`services` 只定义 `ITetMesher` 接口(在 core 或 services)。

## 3. faceId 全链路(铁律 3)

- 输入:每个表面三角的 `triangleFaceId(i)` → TetGen `in.facetmarkerlist[i]`。
- TetGen 用 `-A`/marker 传播到输出边界面;`out.trifacemarkerlist` 给出每个边界三角的 marker。
- 转回:边界三角 faceId = 对应 marker。`-Y`(preserveSurface)保证不在边界面插点,
  输入输出边界三角一一对应,映射简单可靠。
- 与 `transferBoundaryFaces` 的 ModelFace(kind/capId/name)语义对齐:faceId 是 key,元数据从 faces 查。

## 4. CMake(仿 XQ_ENABLE_ONNX)

```cmake
option(XQ_ENABLE_TETGEN "Build TetGen volume mesh adapter" OFF)
if(XQ_ENABLE_TETGEN)
    find_package(tetgen CONFIG REQUIRED)   # 或 find_library,取决于 Externals 怎么装
    add_library(xq_adapter_tetgen STATIC src/adapters/tetgen/TetGenVolumeMesher.cpp)
    target_link_libraries(xq_adapter_tetgen PUBLIC xq_core PRIVATE tetgen)
    target_compile_definitions(xq_adapter_tetgen PRIVATE XQ_ENABLE_TETGEN)

    add_executable(test_tetgen_volume_mesh tests/adapters/test_tetgen_volume_mesh.cpp)
    target_link_libraries(test_tetgen_volume_mesh PRIVATE xq_adapter_tetgen xq_services)
    add_test(NAME test_tetgen_volume_mesh COMMAND test_tetgen_volume_mesh)
endif()
```

- OFF(默认):不编 adapter、不注册 tetgen 测试 → 全量与现状一致(44 绿)。`VolumeMeshService` 只星形。
- ON:多编 adapter + 测试;app 层把 `tetgenTetrahedralize` 注入 service 的 `ITetMesher`。

## 5. 测试设计(AC3/AC4/AC7)

`tests/adapters/test_tetgen_volume_mesh.cpp`(`XQ_ENABLE_TETGEN=ON` 才编):

- **弯曲段用例**:构造一个质心落体外的弯曲管段闭合表面(或从 fixture 读),
  `tetgenTetrahedralize` → 断言 ok、所有 tet 有符号体积同号(无翻转)、min 归一化质量 > 0.1。
- **对照**:同输入 `VolumeMeshService` 星形剖分 → 应有翻转 tet(符号不一致)→ 量化差异,证明 TetGen 更优。
- **faceId**:输入 wall=1/inlet=2/outlet=3,输出边界面 faceId 集合 == 输入集合,各自三角数合理。
- **CHECK 宏式**,副作用先取变量不进 assert(铁律)。
- 假绿点:篡改 facet marker 传入(全置 0)→ faceId 测试 FAIL。

## 6. TetGen 进 Externals(前置,见 prd 风险)

- 优先:TetGen 源(1.6.0)CMake 编译装进 `Externals/install/windows-x64/tetgen-1.6.0`,
  加进 `CMAKE_PREFIX_PATH`,仿现有四根。写进 build 配方。
- 许可:TetGen 自带许可(非商业/AGPL 类),在 design/notes 记录,确认与项目分发兼容。
- 若装不进:本任务拆出"装 TetGen"前置子步骤先做,装好再回来。

## 7. 不做 / 后续

- MMG remeshing / 各向异性 / 边界层 → 后续任务(本任务 TetGen 跑通即可)。
- 表面修复 / 自交处理 → 后续。
- 与原 TetGen .msh/.vtu 逐单元对照 → 可顺带做统计量对照,不强求逐单元。
