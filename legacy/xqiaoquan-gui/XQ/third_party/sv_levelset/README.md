# sv_levelset —— vendored SimVascular 血管两阶段水平集 filter

逐字搬运自 SimVascular,**请勿修改**(改动会使上游对照失效)。

## 溯源
- 上游:SimVascular `Code/Source/sv3/ITKSegmentation`
- 本机来源:`C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/src/SimVascular/Code/Source/sv3/ITKSegmentation`
- 搬运日期:2026-07-07

## 文件(12 个,逐字拷贝,保留上游 CRLF 行尾)
血管水平集 filter 的编译闭包,从两个 PhaseOne/Two `*ImageFilter.h` 出发经 `#include` 拉全:

- `sv3_VascularLevelSetImageFilter.{h}` —— 基类(继承 itk::SegmentationLevelSetImageFilter)
- `sv3_VascularLevelSetFunction.{h,hxx}` —— 基类 function
- `sv3_VascularPhaseOneLevelSetImageFilter.{h,hxx}` + `sv3_VascularPhaseOneLevelSetFunction.{h,hxx}` —— PhaseOne(粗定位,EquilibriumCurvature kc)
- `sv3_VascularPhaseTwoLevelSetImageFilter.{h,hxx}` + `sv3_VascularPhaseTwoLevelSetFunction.{h,hxx}` —— PhaseTwo(曲率上下阈值 kupp/klow 精修)
- `sv3_VascularLevelSetObserver.h` —— 观察器(基类未 include,搬来仅为闭包完整,XQ 不用)

`sv3_VascularLevelSetFunction.hxx` 是孤儿(无人 include),连同 Observer.h 一并搬来无害。只依赖 ITK(itkSegmentationLevelSet* / itkMacro / itkImage / itkExpNegative / itkGradientMagnitudeRecursiveGaussian 等),无 SV_EXPORT / cv* / VTK / Qt 污染。

## 关键使用契约(见 memory sv-vascular-levelset-propagation-zero-required)
- **PropagationScaling 必须 = 0**,否则 `filter->Update()` 段错误(filter 只在 prop==0 时分配 gradient/speed/advection image)。
- SV 默认参数(cvITKLevelSet 源码 sv3_ITKLevelSet.cxx:77-79):`prop=0 / adv=0 / curv=1.0`。
- 种子初值图内负外正(SetInsideIsPositive(false)):phi0 = hypot(dx,dy) - rPx 直接喂 SetInitialImage。

## License
SimVascular BSD-style(见上游 Copyright-SimVascular.txt);各文件头部保留原始 MIT 风格授权声明。
