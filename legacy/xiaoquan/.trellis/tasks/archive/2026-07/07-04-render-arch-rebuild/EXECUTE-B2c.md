# EXECUTE-B2c — 模型单色绿 + mesh 默认隐藏 + 切片模型截线(用户 B2a 验收反馈批)

> 活动任务:`07-04-render-arch-rebuild`。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD e91e28b = B2a)。
> 已知教训:前一 worker 多次把工具调用当纯文本输出导致回合中断。**所有工具调用必须真正以工具形式发出;最终完成报告是纯文本消息,不要以工具调用收尾。**

## 背景(主会话已诊断,直接采信)

用户反馈:①3D 模型"显示不完全";②三切片窗没有模型截面轮廓。
主会话诊断(已对真实数据核实):
- `0007_H_AO_H/Models/0090_0001.vtp`:**84542 点 / 169080 三角 / 0 strips**——数据完整;
- `.mdl` 7 个 face:id1=wall(管壁)+ id2-7 caps;
- 现 faceId LUT(makeSurfaceMapper,XQRenderScene.cpp:435-443,HueRange 0.55→0.0)把 wall 涂成**青色**(与 mesh 线框 0.4,0.85,0.95 撞色)、caps 涂成杂色 → "完整模型看着像碎片"。
- 结论:大概率是**配色/遮挡观感问题**,数据管线完好;但需数据锚点断言防真丢面。

## 目标(四条,用户已确认)

1. Surface 模型默认**单色解剖绿**(faceId LUT 路径保留代码、默认不走,后续做成开关);
2. Mesh(体网格线框)**默认隐藏**(B3 勾选框上线后由用户控制);
3. 三切片窗叠加**模型截线**:每个可见 surface 节点在每个切片视图挂常驻 vtkCutter(vtkPlane=当前切片位置),切片变化只改平面位置不重建;
4. **数据锚点断言**:真实工程加载后,模型三角几何 pointCount==84542 且 triangleCount==169080(锚死"读全了")。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/visualization/XQRenderScene.h`(仅追加步骤 D 列出的探针)|
| 修改 | `src/visualization/XQRenderScene.cpp` |
| 修改 | `src/app/XQMainWindow.cpp`(仅当步骤 B 选择 app 层实现时;首选内核层则不动)|
| 修改 | `tests/visualization/test_render_scene.cpp`、`tests/app/test_main_window.cpp` |

## 步骤 A:模型单色绿

- `addSurface`(XQRenderScene.cpp:1075 附近)与 `addSurfaceProgressive`(:1310 附近)、`mountLodChunk`(:1408):mapper 改走 `makeSurfaceMapper(poly, /*faceIdValid=*/false, ...)`(ScalarVisibilityOff 分支)——**cell data 的 faceId 数组保留在 polyData 上**(后续开关要用),只是 mapper 不用它上色;
- actor property 设统一解剖绿 `SetColor(0.25, 0.80, 0.35)`;`entry.faceIdColoured=false`(setNodeColor 由此对模型生效,顺理成章);
- makeSurfaceMapper 本身与 LUT 代码**不删不改**(faceIdValid=true 路径留给未来开关);
- 常量放匿名 ns:`constexpr double kSurfaceColor[3] = {0.25, 0.80, 0.35};`。

## 步骤 B:mesh 默认隐藏(内核层实现)

`upsertNode`/`upsertNodeProgressive`:**首见**(!hadEntry)且 domain==Mesh / kind==TetMesh 时 `entry.visible=false`;re-upsert 沿用既有"保留呈现状态"逻辑不变(用户 B3 勾选后不被 sync 覆盖的伏笔)。头文件注释更新说明此默认。加 `// TODO(B3): 勾选框上线后此默认交 UI 管理`。

## 步骤 C:切片模型截线(常驻切割器)

- NodeEntry 增 `std::array<vtkSmartPointer<vtkActor>,3> cutActors`(每切片视图一个)+ 对应 `vtkSmartPointer<vtkPlane> cutPlanes[3]`;
- `addSurface` 构建 actor 后(仅内存 surface 路径;progressive 首块后同理可做,若工作量大则 progressive 留 TODO 并在报告申报——真实 SV 工程走的是 addSurface,必须覆盖):对每 axis 0/1/2:
  - `vtkPlane`:origin=体数据当前切片世界坐标(用 `sliceWorldCoord(axis)`,无卷时先建好、位置延后到 setVolume/setSliceIndex 更新),normal=轴单位向量;
  - `vtkCutter` SetCutFunction(plane),input = 该节点 full polyData(**normals filter 之前的原始 poly**,截线不需要法线);
  - mapper(ScalarVisibilityOff)+ actor:颜色同模型绿、LineWidth 2.0、`GetProperty()->SetLighting(false)`(截线要纯色醒目);
  - AddViewProp 进 `renderer(viewForAxis(axis))`;可见性跟随节点(setNodeVisible/applyPresentation/detachEntry 同步处理 cutActors——**detachEntry 务必从三个切片 renderer 各自移除**,别漏);
- `setSliceIndex(axis,...)`:更新所有 NodeEntry 的 `cutPlanes[axis]->SetOrigin(...)`(只改平面,vtkCutter 管线惰性重算,符合"改参数不重建");
- `setVolume`/`clearVolume`:全部 cutPlanes 重置到当前切片;clearNodes/removeNode 连同 cut actors 摘除;
- mask/path/flow/mesh 不做截线(mesh 默认隐藏;后续需要再说)。

## 步骤 D:XQRenderScene.h 追加探针(仅这两个)

```cpp
// Probe: this node's slice-cut actor count in the given slice view (0 for the
// 3D view / unknown id / a node kind that has no cut overlay).
int nodeSliceCutCount(const NodeId& id, ViewId v) const;
// Probe: the node's current display colour (first actor's property). False for
// an unknown id.
bool nodeColor(const NodeId& id, double rgb[3]) const;
```

## 步骤 E:测试

1. `test_render_scene.cpp`:
   - surface 节点(make_tet_surface)upsert 后:`nodeColor`==(0.25,0.80,0.35)(容差 1e-6);setVolume 后每切片视图 `nodeSliceCutCount(id, Axial/Sagittal/Coronal)==1`、Volume3D==0;setNodeVisible(false) 后截线 actor 也不可见(经 nodeVisible + 新增一个"cut actor GetVisibility"探针?——不加新探针,断言 nodeVisible false 即可,cut 可见性跟随由实现保证并在真机验);removeNode 后 `nodeSliceCutCount`==0;
   - mesh 节点(测试文件既有 tet mesh payload 构造手法)首次 upsert 后 `nodeVisible(id)==false`(新默认);setNodeVisible(true) 后 true;re-upsert 后仍 true(呈现状态保留);
   - 无卷时 upsert surface:nodeSliceCutCount==每视图 1(actor 已建,平面位置在 setVolume 后才有意义)或 0(延后建)——两种实现都可,断言与实现一致即可,报告里写清选了哪种。
2. `test_main_window.cpp`:模型预解析断言(B2a 已有)追加数据锚点:
   `modelPayload->model().triangleGeometry()->pointCount()==84542 && ->triangleCount()==169080`(真实 .vtp 已核对,常量写死+注释来源)。

## 验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
cmd //c build_gui_wt.bat && cmd //c ctest_merge.bat   # 全量必须全绿
cmd //c run_xq.bat   # 冒烟:启动+加载 0007 不崩
```

假绿抽查(必做,附证据):①篡改 kSurfaceColor 为其它值 → nodeColor 断言转红 → 还原绿;②篡改 setSliceIndex 跳过 cutPlanes 更新 → 现有断言可能测不出(平面位置无探针)——如实评估:若测不出,在报告里声明"截线随切片联动只能真机验",不造伪断言。均核对 exe 时间戳。

## 禁做

- 白名单外文件;XQRenderScene.h 除步骤 D 两个探针外零改动;不删 LUT 代码。
- 不动 MDLModelReader/XQSliceViewWidget/拾取/窗宽窗位(B2b);不动 core/services/io/adapters。
- 不 commit;不加未要求兜底;既有断言只增不减。
- 冲突事实停下报告等裁决。

## 完成报告格式(纯文本,不要以工具调用收尾)

1. 文件清单+一句话;2. 构建尾部+ctest 总结行原文;3. 假绿抽查证据(含②的如实评估);4. 冒烟结果;5. progressive 截线是否覆盖或 TODO 申报;6. 偏离与存疑。
