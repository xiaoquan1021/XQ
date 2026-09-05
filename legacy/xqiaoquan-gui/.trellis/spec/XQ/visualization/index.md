# XQ Visualization 层规范(visualization layer)

> 来源:`06-30-m9b-b-lod-decimation`(M9b-B)。
> `visualization` 是 surface/volume/slice 渲染装配层,VTK 只在本层私有实现,io/core 永无 `vtk*`。写渲染/LOD 代码前先读本层。

---

## 本层规范

- [lod-and-upload.md](./lod-and-upload.md) — Surface LOD 多级 decimation + CPU 选级 + `uploadedPointCount` 契约;两条硬约束(headless 取不了 vtkLODActor 级、分区 decimate 碎片 faceId 下膨胀须诚实回退);架构边界(LOD 砍上传量、砍不掉 host 峰值,峰值降级移交几何源)。

> 架构铁律、分层、外部库规则见 `architecture` layer;消费侧 Source 接口见 `core` layer 的 `source-interface.md`。
