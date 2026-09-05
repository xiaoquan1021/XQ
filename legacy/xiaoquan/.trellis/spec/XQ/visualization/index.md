# XQ Visualization 层规范(visualization layer)

> 来源:`06-30-m9b-b-lod-decimation`(M9b-B);2026-07-04 增补渲染架构定论;2026-07-05 `07-04-render-arch-rebuild` 交付定稿(用户真机签收);2026-07-06 `07-06-pick-interaction-fix` 增补切片拾取交互场景(用户真机签收)。
> `visualization` 是 surface/volume/slice 渲染装配层,VTK 只在本层私有实现,io/core 永无 `vtk*`。写渲染/LOD 代码前先读本层。

---

## 渲染架构定论(2026-07-04,07-04-render-arch-rebuild,最高优先)

- **产品交互渲染必须走常驻 GPU 管线**:常驻 QVTKOpenGLNativeWidget + 常驻 vtkRenderer,
  体数据一次上传,切片=改 mapper/reslice 参数。产品入口是 `XQRenderScene`(见 render-scene.md)。
- **禁止**在产品交互路径上:每帧新建 vtkRenderWindow / 离屏渲染回读贴 QLabel /
  每次切片重建整卷 vtkImageData。此路线已在真机验收中证伪(卡顿+交互残缺);
  其载体 XQMprView/XQRenderWidget 已于 07-04 B5 删除,离屏契约靶只剩 test-only 的
  XQSceneRenderer(产品代码禁止引用)。
- **渲染模型是可见性驱动的场景合成**:数据管理器勾选控制 actor 显隐,切片叠加派生物
  (轮廓/分割),3D 合成显示所有可见节点;不是"选中单节点才渲染"。
- headless 可测性不得反向决定产品架构;流畅度/观感验收只认真机。
- 详见 `.trellis/tasks/07-04-render-arch-rebuild/`(prd + research 两份盘点)。

## 本层规范

- [render-scene.md](./render-scene.md) — **产品渲染入口** XQRenderScene 常驻管线契约(07-04 定稿):四常驻 renderer、一次上传、axis 约定、upsert/探针签名、切片相机满窗、路径细线、十字线交点拖拽、app 层 payload 指纹增量同步、批量解析;XQSceneRenderer 为 test-only。
- [lod-and-upload.md](./lod-and-upload.md) — Surface LOD 多级 decimation + CPU 选级 + `uploadedPointCount` 契约;两条硬约束(headless 取不了 vtkLODActor 级、分区 decimate 碎片 faceId 下膨胀须诚实回退);架构边界(LOD 砍上传量、砍不掉 host 峰值,峰值降级移交几何源)。

> 架构铁律、分层、外部库规则见 `architecture` layer;消费侧 Source 接口见 `core` layer 的 `source-interface.md`。
