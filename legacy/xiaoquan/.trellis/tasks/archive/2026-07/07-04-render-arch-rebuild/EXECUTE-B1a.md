# EXECUTE-B1a — XQRenderScene 渲染内核(xq_visualization,无 Qt)

> 活动任务:`07-04-render-arch-rebuild`。你是 implement worker,只做本简报;读完「必读上下文」再动手。
> 工作目录(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`(git worktree,分支 `feat/render-arch`)。
> 所有行号基于 feat/gui-v2 @ a3715e5(= 本分支起点,无额外提交)。

## 必读上下文(按序)

1. 本简报全文。
2. `design.md`(已随 spawn 注入)——尤其 §2.1 接口、§3.1/3.2/3.5 决策。
3. `research/render-code-inventory.md`——可抄清单。
4. spec `lod-and-upload.md`——LOD/渐进/连通性契约,**原样继承,不重新设计**。

## 目标

新建 `XQRenderScene`:常驻 4 renderer 的场景合成器,体数据一次上传、切片=改参数、节点常驻 actor+显隐。**纯 VTK,零 Qt**。本批不接主窗口、不建 widget、不动旧渲染类(除下述一处 include 搬移)。

## 文件白名单(超出即违规)

| 操作 | 文件 |
|---|---|
| 新建 | `src/visualization/XQRenderTypes.h` |
| 新建 | `src/visualization/XQRenderScene.h` / `XQRenderScene.cpp` |
| 新建 | `tests/visualization/test_render_scene.cpp` |
| 修改 | `CMakeLists.txt`(仅两处:xq_visualization 源列表 + test_render_scene 注册) |
| 修改 | `src/visualization/XQSceneRenderer.h`(仅结构体搬移后的 include,见下) |

## 步骤 1:XQRenderTypes.h(结构体搬移,零语义变化)

把 `XQSceneRenderer.h:24-65` 的 `RenderStats` / `LodOptions` / `ChunkUploadSpec` / `ChunkProgressFn` **原文搬**进新头 `XQRenderTypes.h`(namespace xq,注释一起带走);`XQSceneRenderer.h` 原处删除、改为 `#include "visualization/XQRenderTypes.h"`。旧测试(test_scene_renderer 等)经旧头传递获得类型,**不改旧测试一行**。

## 步骤 2:XQRenderScene.h(公开头,VTK-free,pimpl)

```cpp
#ifndef XQ_VISUALIZATION_RENDER_SCENE_H
#define XQ_VISUALIZATION_RENDER_SCENE_H
#include "visualization/XQRenderTypes.h"
#include "core/NodeId.h"
#include <memory>
namespace xq {
class XQImageVolume; class XQMemoryImageBufferHandle;
class XQDataNode; class IGeometrySource;

enum class ViewId { Axial, Sagittal, Coronal, Volume3D };
enum class GeoKind { Surface, TetMesh };

class XQRenderScene {
public:
    XQRenderScene(); ~XQRenderScene();
    XQRenderScene(const XQRenderScene&) = delete;
    XQRenderScene& operator=(const XQRenderScene&) = delete;

    // 体数据:一次构建常驻 vtkImageData(buffer 为 null → 中性中值填充,同旧
    // build_image 语义);三切片视图各挂 vtkImageSlice(共享该 vtkImageData),
    // 3D 视图挂三张正交切片平面。重复调用 = 替换整卷。失败(几何非法)返回 false。
    bool setVolume(const XQImageVolume& img, const XQMemoryImageBufferHandle* buffer);
    void clearVolume();
    bool hasVolume() const;

    // 切片(axis: 0=x/Sagittal, 1=y/Coronal, 2=z/Axial;沿用旧 XQMprView 映射)。
    // 越界夹紧;同步更新该轴 2D mapper + 3D 对应平面 + 十字线。无卷时空操作。
    void setSliceIndex(int axis, int index);
    int  sliceIndex(int axis) const;   // 无卷返回 -1
    int  sliceCount(int axis) const;   // 无卷返回 0

    // 窗宽窗位:共享 vtkImageProperty(三切片+3D 平面全联动)。
    // setVolume 时初始化为 intensityRange 全窗。
    void setWindowLevel(double window, double level);
    void windowLevel(double* window, double* level) const;

    // 十字线:每切片视图两条正交线 actor,位置由另两轴当前切片索引决定。
    void setCrosshairVisible(bool on);
    bool crosshairVisible() const;     // 默认 true

    // 节点:首见构建 actor 组(payload→actor,B1a 只进 Volume3D 视图),再见先移旧组
    // 重建(payload 可能已变)。不可渲染 payload(Image/SimulationCase/Unknown/空
    // payload)→ ok=false 且不留痕。可见性/透明度/颜色是呈现属性,存本类。
    RenderStats upsertNode(const NodeId& id, const XQDataNode& node);
    // 渐进路径:调用方(app 层)已解析好 IGeometrySource;kind 决定 surface/tet 装配。
    RenderStats upsertNodeProgressive(const NodeId& id, const IGeometrySource& src,
                                      GeoKind kind, const ChunkUploadSpec& spec = {},
                                      const ChunkProgressFn& onChunk = {});
    void removeNode(const NodeId& id);
    void clearNodes();
    void setNodeVisible(const NodeId& id, bool on);   // 未知 id 空操作
    void setNodeOpacity(const NodeId& id, double a);
    void setNodeColor(const NodeId& id, double r, double g, double b);

    // 离散不变量探针(headless 测试用;只读,不触发渲染)。
    int  nodeActorCount(ViewId v) const;  // 只计节点 actor,不含图像切片/十字线
    bool hasNode(const NodeId& id) const;
    bool nodeVisible(const NodeId& id) const;         // 未知 id 返回 false
    long long uploadedPointCount(const NodeId& id) const; // 未知 id 返回 -1

    // widget 挂载(B1b 用):不透明 vtkRenderer*。
    void* vtkRendererHandle(ViewId v) const;
private:
    class Impl; std::unique_ptr<Impl> impl_;
};
} // namespace xq
#endif
```

接口即契约,实现不得增删公开成员;实现里的私有辅助自定。

## 步骤 3:XQRenderScene.cpp 实现要点(可抄源全部在 XQSceneRenderer.cpp)

**体数据**(抄 :70-146 `valid_geometry`/`apply_geometry`/`build_image` 全套,一次构建后常驻):
- 加 memcpy 快路径:`buffer->scalarType()==ScalarType::Float32 && componentCount()==1` 且维度匹配时,整块 `memcpy(base, buffer->bytes().data(), voxelCount*4)`(布局同为 x-fastest,`XQMemoryImageBufferHandle.h:20-21` 已注明);其余类型走旧逐体素循环(只发生在加载时,可接受)。
- 三个 2D 切片:每视图一个 `vtkImageSliceMapper`(SetOrientationToX/Y/Z 按轴)+ `vtkImageSlice`,`SetInputData` 同一 vtkImageData;初始 SliceNumber=各轴中点。
- 3D 三平面:再建三组 mapper/slice 进 Volume3D renderer(数据仍共享;actor/mapper 每 renderer 独立——设计 §3.2)。
- 共享一个 `vtkImageProperty`(SetColorWindow/SetColorLevel),六个 vtkImageSlice 全 SetProperty 同一实例。
- 切片相机默认(B1b 真机对照后允许调,先按此):ParallelProjection ON,ResetCamera 后——Axial 相机 −Z 侧朝 +Z 看、up=−Y;Sagittal 相机 +X 侧朝 −X 看、up=+Z;Coronal 相机 −Y 侧朝 +Y 看、up=+Z(LPS)。
- 十字线:每切片 renderer 两条 `vtkLineSource`+actor,端点取 vtkImageData bounds,位置由另两轴 slice 的世界坐标定;setSliceIndex 时更新。

**节点 actor 构建**(核心抄源,逐函数):
- payload 分发 switch 抄 `XQMainWindow.cpp:2156-2227` 的 domainType 分支结构(SurfaceModel/Mesh/Path/SegmentationMask/FlowResult),但**去掉** lazy-resolve 分支(那是 app 层的事,走 upsertNodeProgressive)和 LOD 策略常量(LodOptions 由内嵌默认,app 层 B1b 再传);Mesh 的内存回退链(volumeTets→surfaceTriangles)保留。
- 各构建器抄 `XQSceneRenderer.cpp`:`addPath` / `addSurface`(含 SurfaceLodBuilder 接入、normals auto-orient——M3 winding 不一致,勿删)/ `addSurfaceProgressive`(plan_chunks + 全局 faceId LUT range + 每块 copy-on-upload + remap scratch)/ `addVolumeMesh` / `addVolumeMeshProgressive` / `addFlowResult` / `addSegmentationMask`,以及 `upload_points` 等批量上传辅助和 **connectivity bounds gate**(`connectivity_in_bounds`,无效连通性 → ok=false、零 actor、零 chunk、不回调 onChunk——spec 场景契约)。
- RenderStats 语义不变:`pointCount` 恒报源点数,`uploadedPointCount` 实际上传,chunk 计数同旧。
- 节点注册表:`std::map<NodeId, NodeEntry>`(NodeEntry 持该节点全部 actor 的 smart pointer + stats + visible/opacity/color);upsert 已存在 id 先 RemoveActor 旧组;setNodeVisible → 每 actor `SetVisibility`;opacity/color → `GetProperty()->SetOpacity/SetColor`(faceId LUT 着色的 surface,color 作为无 LUT 时的回退色,LUT 路径只调 opacity——不确定处从简,别造新机制)。

**明确不做**:renderOffscreenToRgba 不搬;无任何 Qt include;不建 vtkRenderWindow(4 个 renderer 裸持,AddActor/探针都不需要 GL 上下文);不动 SurfaceLodBuilder/ChunkPlan(直接 include 复用)。

## 步骤 4:test_render_scene.cpp

风格照抄 `tests/visualization/test_scene_renderer.cpp`(裸 main + `fail()`,复用其 make_geometry/make_tet_surface 手法;不需要 GL/offscreen 环境)。**铁律:副作用调用(setVolume/upsertNode 等)不进 assert/条件表达式,先调用存结果再断言**(memory 教训:/DNDEBUG 假绿)。

断言清单(全部离散不变量,无像素断言):
1. 无卷初态:hasVolume false、sliceCount==0×3、sliceIndex==-1、setSliceIndex 空操作不崩。
2. setVolume(8×6×4 合成卷,Float32 buffer):返回 true;sliceCount==8/6/4(axis 0/1/2);初始 sliceIndex==各轴中点;windowLevel 读回==intensityRange 全窗。
3. setSliceIndex(2,1)→读回 1;setSliceIndex(2,99)→夹紧 3;setSliceIndex(0,-5)→夹紧 0。
4. memcpy 快路径正确性:Float32 卷 setVolume 后,经探针无法直接读体素——改为**同卷两种类型对比**:同数据分别以 Float32 与 Int16 构 buffer setVolume,两次 sliceCount/windowLevel 行为一致(类型无关契约);(体素级字节正确性由构建器抄旧实现保证,不在此测。)
5. upsertNode(path payload)→ok true、hasNode true、nodeActorCount(Volume3D)==1;再 upsertNode 同 id→actor 数不翻倍。
6. upsertNode(surface payload,make_tet_surface)→ok、pointCount==4;upsertNode(mask)、upsertNode(flow) 各自 ok;nodeActorCount 累计正确。
7. 不可渲染:空 payload 节点→ok false、hasNode false。
8. setNodeVisible(id,false)→nodeVisible false;true→true;未知 id 不崩、nodeVisible false。
9. removeNode→hasNode false、nodeActorCount 回落;clearNodes→归零。
10. 渐进:用 test_scene_renderer_progressive 的 source 构造手法(内存 IGeometrySource stub)upsertNodeProgressive(Surface,maxCellsPerChunk 压小强制多块)→chunkCount>1、completedChunkCount==chunkCount、onChunk 递增回调、uploadedPointCount(id)>0。
11. 无效连通性 source(三角索引==pointCount)→ok false、nodeActorCount 不变、chunkCount==0、onChunk 未被调。
12. setNodeOpacity/setNodeColor 未知 id 不崩;已知 id 调用后 nodeVisible 等其余探针不受扰。

## 步骤 5:CMake

- `xq_visualization` 源列表(CMakeLists.txt:124-129)加 XQRenderScene.cpp/.h、XQRenderTypes.h。
- 照 `test_scene_renderer` 模板(:851-861)注册 `test_render_scene`:add_executable + `target_link_libraries(... PRIVATE xq_visualization)` + `vtk_module_autoinit` + 在 add_test 区(:966 附近)加 `add_test(NAME test_render_scene COMMAND test_render_scene)`。

## 步骤 6:验证(全部必跑,按序)

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
cmd //c build_gui_wt.bat    # 完整构建;绝不用单目标增量(链接静默跳过假绿坑)
cmd //c ctest_merge.bat     # 全量 ctest,必须 100% 绿(含全部旧测试)
```

**自查假绿抽查(必做并写进报告)**:把 XQRenderScene.cpp 里 setNodeVisible 的 SetVisibility 调用注释掉→完整构建→test_render_scene 必须转红;还原→完整构建→全绿。核对 exe 时间戳确实变了(ninja 假绿坑)。

## 禁做

- 白名单外任何文件;尤其不动 XQMprView/XQRenderWidget/XQMainWindow/core/services/io/adapters/旧测试。
- 不 git commit / 不 push(主会话统一提交)。
- 不引 Qt;不做像素断言;不加简报未要求的兜底分支。
- 不重新设计 LOD/chunk/faceId 契约——照抄照用。
- 遇到与本简报冲突的源码事实:停下,在 channel 里报告,等主会话裁决;不自行绕。

## 完成报告格式(channel 里发)

1. 改动文件清单 + 每文件一句话;
2. 构建输出尾部 + ctest 总结行(N/N passed 原文);
3. 假绿抽查过程与结果(转红证据);
4. 偏离简报之处(若无写"无");存疑之处如实标注。
