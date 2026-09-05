# ITK 三维预处理真实数据运行记录

日期：2026-07-12

## 实际功能

生产调用链已经可执行：

```text
derived IRCAD CT DICOM
  -> GdcmItkDicomSeriesReader
  -> XQImageVolume + ResidentVoxelSource
  -> ItkVascularPreprocessor
       -> itk::Image<float,3>
       -> CurvatureAnisotropicDiffusionImageFilter
       -> MultiScaleHessianBasedMeasureImageFilter
       -> HessianToObjectnessMeasureImageFilter
  -> XQ-owned Float32 vesselness volume
```

调用者只传 `XQImageVolume`、`IVoxelSource` 和版本化 profile。输出在 adapter
返回前已经物化到 `XQMemoryImageBufferHandle`，不保留 ITK 对象或借用指针。

## 输入

逻辑数据路径：

```text
<vascular-data-root>/3D-IRCADb-01/derived/frame-v1/
  3Dircadb1.5/patient/PATIENT_DICOM
```

身份和几何：

```text
SeriesInstanceUID:
1.2.826.0.1.3680043.2.1125.3714849404296577478106961719918593568

FrameOfReferenceUID:
2.25.195508161051123547813769883276794292485

DICOM series fingerprint:
dicom-series-v1:sha256:131d498a86598efc6851626937e6db7d8b3a1d13d21787ff76226fb3a950e26f

dimensions = 512,512,139
spacing_mm = 0.78200000524520896,0.78200000524520896,1.600000023841855
origin_lps_mm = 0,0,0
direction = identity
input_range = -1024,1023
voxel_count = 36,438,016
```

## Profile

```text
id = xq.portal-venous-ct.preprocess.v1
schema = 1
diffusion_iterations = 5
diffusion_time_step = 0.03
diffusion_conductance = 3.0
sigma_mm = 0.6..4.0
sigma_steps = 6 logarithmic
object_dimension = 1
alpha = 0.5
beta = 0.5
gamma = 5.0
bright_object = true
scale_objectness = true
```

## 三次真实运行

三次均由 `xq_vascular_preprocess_probe` 执行同一生产 API，未传入 gold、seed、
Path 或 Contour。

```text
run 1 elapsed_ms = 43806.1383
run 2 elapsed_ms = 43690.9025
final build run elapsed_ms = 42014.6107

input fingerprint:
xq-vascular-input-v1:sha256:c129641580b8ffa68c00d648b81f11588c75e5beabaa042b285cac6701eae6df

profile fingerprint:
xq-vascular-profile-v1:sha256:534b66e814d3894ac5b87125960bff3b4e9911d19af34e3bca79b375fba84bfb

output fingerprint, identical all runs:
xq-vesselness-output-v1:sha256:855568f88a7ee5cf26ca238a6f7516167af15017fbfb6068a20bc7fd202defe2

ITK version = 5.4.0
output_range = 0,317.78466796875
positive_voxel_count = 16,455,684
output_voxel_count = 36,438,016
geometry_equal_input = true
diagnostic_count = 0
```

## 功能回归含义

- 倾斜 direction 与各向异性 spacing 输入通过同一 adapter，输出几何逐值相等，
  index -> LPS -> index 回环误差小于 `1e-9`。
- 同一输入/profile 重复运行产生逐字节相同的 Float32 buffer 和相同 fingerprint。
- 同一物理高斯管在 `0.8 mm` 与 `1.0 mm` 横向采样下，峰值响应比为
  `1.06224489`，证明 sigma 走物理 spacing，不是 index-space 常量。
- 预取消、非法 profile、非有限标量和空 vesselness 都返回无 output 的 typed
  failure；生产代码不提交半结果。

## 当前边界

本 child 交付的是可被下一阶段调用的三维预处理 kernel 和 XQ-owned 中间结果。
自动 mask、Scene/GUI 接入、中心线和网格仍由后续 children 完成；这些缺失不由
本 child 的 `99/99` 回归结果掩盖。
