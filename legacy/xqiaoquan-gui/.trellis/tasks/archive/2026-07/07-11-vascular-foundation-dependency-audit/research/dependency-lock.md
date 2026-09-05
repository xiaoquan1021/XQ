# Dependency Lock

Audit date: 2026-07-11

This lock describes the files actually inspected, linked, and executed on the
Windows x64 audit host. A directory name or manifest label is not treated as
version proof. Decisions use only `accepted`, `accepted-research-only`,
`rejected`, or `blocked`.

## Locked roots

- XQ source: `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`
- External source/install: `C:\Users\OCEAN\Desktop\XIAOQUAN\Externals`
- Probe build: `XQ/build_vascular_foundation_probes`
- TetGen/MMG ON build: `XQ/build_dependency_audit`
- ITKThickness3D source: `.trellis/workspace/ocean/itk-thickness3d-v5.3.0`
- Real vtkvmtk input: `C:\Users\OCEAN\Desktop\XIAOQUAN\0007_H_AO_H\Models\0090_0001.vtp`

The external repository was inspected read-only and remains dirty from work
outside this task.

## Dependency records

### VTK

- Official identity: VTK `v9.3.0`, peeled tag commit
  `357d9efeed29cba6383ce626575f1a5f1ac1eefb`.
- Mirror source: `src/VTK-9.3.0`, commit
  `318072d49330c8ff6f819df90995408db9385a38`, tree
  `d6e9b17ed4ca19a375a1cda517fa38ba8b016504`, clean.
- Install: `install/windows-x64/vtk-9.3.0`.
- Package: `lib/cmake/vtk-9.3/vtk-config.cmake` and
  `VTK-targets-release.cmake`.
- Representative imported target locations:
  `VTK::CommonCore -> lib/vtkCommonCore-9.3.lib + bin/vtkCommonCore-9.3.dll`;
  the same Release import-library/DLL pattern is present for the other selected
  `VTK::` targets.
- Representative hashes:
  `VTK-targets-release.cmake` =
  `31F713C9167CDFA2AE35ECD8C9E99AA54C808C4BF068F265960AFA2951C81F3B`;
  `vtkCommonCore-9.3.dll` =
  `8CC197D93CBB823DA0B0036B5E33BE252FCBB0CF23456BEDC4B826B688A33A55`;
  `vtkIOXML-9.3.dll` =
  `90B0DF664EAE27D81008F8353879F535CB4752A8C2AA66E3A73578C8F93F3DB2`.
- Decision: `accepted` for the explicit C++ target set. The current product's
  broad transitive link list is `blocked`; it includes unused Python targets
  and a host `python312.lib` even though the final PE import closure is clean.

### ITK and ITKVtkGlue

- Official identity: ITK `v5.4.0`, peeled tag commit
  `311b7060ef39e371f3cd209ec135284ff5fde735`.
- Mirror source: `src/InsightToolkit-5.4.0`, commit
  `c58dcf2d2bd640f49c46e4888d8193d6a956c479`, tracked tree
  `351adb7bd126732c8915d2253331139420190d4a`.
- Additional remote module present in that source tree:
  `Modules/Remote/GrowCut`, ITKGrowCut `v0.2.1`, commit
  `cbf93ab65117abfbf5798745117e34f22ff04728`, tree
  `023c0fc60d08484e88b8a55463d77ecedf5862f7`. It is installed but is not
  part of the vascular probe's explicit component set or runtime closure.
- Install: `install/windows-x64/itk-5.4.0`.
- Package: `lib/cmake/ITK-5.4/ITKConfig.cmake` and
  `ITKTargets-release.cmake`.
- Required vascular components proven by the probe:
  `ITKAnisotropicSmoothing`, `ITKImageFeature`,
  `ITKBinaryMathematicalMorphology`, `ITKConnectedComponents`,
  `ITKDistanceMap`, and `ITKVtkGlue`. Add `ITKIOGDCM` for production DICOM.
- Release locations actually loaded include `ITKCommon-5.4.dll`,
  `ITKSmoothing-5.4.dll`, `ITKImageFeature-5.4.dll`, and `ITKVTK-5.4.dll`.
- Representative hashes:
  `ITKTargets-release.cmake` =
  `6279E3004122DB77109521B3259DFC7467C826734592D16C8B5467D73F9002AD`;
  `ITKCommon-5.4.dll` =
  `8BA4C65A58D3F814EA4F97DC0B5333C613026F342587C4975F4178BB78116399`;
  `ITKVTK-5.4.dll` =
  `D4385E376D624AB6921D0E16A98F0F8F5BB9822A1EFB77E6E05D3644A254AC31`.
- Decision: `accepted` with explicit components and the exact VTK package;
  the current product-wide broad `find_package(ITK)` recipe is `blocked` for
  clean-build reproducibility.

### GDCM

- Official identity: GDCM tag `v3.0.10`, commit
  `b56f671dba79dca00d8cb3ebde83dbd88292e9e1`.
- Mirror source: `src/GDCM-3.0.10`, commit
  `68a6135d9ff4e0eae0713776937d9844ad87bef7`, tree
  `314dfba03a5827771b1a7deac9d61b74c8349a7e`, clean.
- Install: `install/windows-x64/gdcm-3.0.10`.
- Imported targets used by XQ: `gdcmMSFF` and its `gdcmDSED`, `gdcmIOD`,
  `gdcmDICT`, and `gdcmCommon` closure. `ITKIOGDCM` is the ITK-side reader.
- Hashes: `GDCMTargets-release.cmake` =
  `FDDDB6F69D4DD57114F264F010C3CDA60F5A3A672A3443A169EC28FCE46C1CB3`;
  `gdcmMSFF.dll` =
  `33719A125F2187C62807F2C7BF2048B3DA2F19A47CD511B4466C1462B61C17C4`.
- Decision: `accepted`.

### ITKThickness3D fallback

- Official source/tag: ITKThickness3D `v5.3.0`, commit
  `36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb`, tree
  `e03750b011699d2989d28700f0ffba6b61d64979`, clean detached checkout.
- Integration form: header-only probe target; no installed DLL or second ITK.
- Header hashes:
  `itkBinaryThinningImageFilter3D.h` =
  `73E9537C5CB6AFC3EC9ECB809D0DE73A93D7C923D00B938F28A3E83447177EDA`;
  `.hxx` =
  `C7101F1AC5EF309BC60416DCBFE7C5B188CBA323D60BB8017D66C5D065C4BCD6`.
- Decision: `accepted` as the true 3D thinning fallback candidate. This does
  not select fallback B over vtkvmtk or prove clinical centerline quality.

### vtkvmtk centerline candidate

- Source owner/revision: SimVascular commit
  `b8c30d7d6194f16246ae9a435514cc55f6f6ab0b`, tree
  `c0633b06844debb21420efc47fff030741543b1b`, clean.
- Source path:
  `Code/ThirdParty/vmtk/simvascular_vmtk`.
- Compiled closure: eight `.cxx` files listed in the probe CMake; the ordered
  filename/SHA-256 aggregate is
  `15328E8A7A066E3EBD0477488F9AB7F192DE3610CAA4C142625D608CBF26DF25`.
- Build target: local static `vtkvmtk_centerline_minimal`, linked only to the
  explicit VTK 9.3 C++ targets. No Python, SuperBuild, or second VTK/ITK.
- Real input hash: `0090_0001.vtp` =
  `7FA57F163319D43348EDF5B8C52FA0F47231A37A16845FD4FEE3B36592AD372A`.
- Decision: `accepted` as a technically viable candidate. Product integration
  must carry the VMTK BSD notice and still pass the later real CTA quality gate.

### TetGen

- XQ source: `XQ/third_party/tetgen/tetgen.h` and `tetgen.cxx`.
- Provenance: byte-identical to the SimVascular files at commit
  `b8c30d7d6194f16246ae9a435514cc55f6f6ab0b`.
- Hashes: header =
  `F654992C828B720E3BC16A281E7137E7FFF97473208350536B1F94EAA4FA2612`;
  source =
  `1254B964D5FB8D17D1CCBB28DD96147AB95CBF5C6362D8C9F393C823E98E1D1D`.
- Exact self-identity: both files say `Version 1.5`; the source prints
  `Version 1.5`. The CMake comment saying `1.5.1` is not version evidence.
- Build target: local static `tetgen` with `TETLIBRARY`, then
  `xq_adapter_tetgen`.
- Decision: `accepted-research-only`; see the AGPL/commercial decision in the
  license report. It is not an accepted distributable production backend.

### MMG

- Official identity: tag object
  `03cdf9348abd3faeb7ea48659002c8692185f573`, peeled tag commit
  `a1dbb17caf812402fec298dbdbdeb9031172ddd9`, official tree
  `719a104dfb54cc4620c33f08390dccde3f520da6`.
- Mirror source: `src/mmg-5.3.9`, commit
  `696f8cefcf20b667498ebe8b3fae4c88f9596b97`, tree
  `b08008cf6fa9e854ab58b9050951bf0e17114257`.
- Comparison: all 265 official tracked files are byte-identical. The mirror
  adds `XQ-MIRROR.md` and extends `.gitignore`; there are no code changes.
- Version reconciliation: the official `v5.3.9` tag itself defines
  `CMAKE_RELEASE_VERSION=5.3.8` and date `Apr. 10, 2017`. The generated
  `mmgcommon.h` and `mmg3d_O3.exe -h` therefore print `Release 5.3.8`.
  The correct compound identity is "official tag v5.3.9, embedded runtime
  version 5.3.8", not a mixed installation.
- Install: `install/windows-x64/mmg-5.3.9`; manually located header/import
  library/runtime trio `include/mmg/mmg3d/libmmg3d.h`, `lib/mmg3d.lib`, and
  `bin/mmg3d.dll`.
- The build and installed `mmg3d.dll` are byte-identical; installed hash =
  `5EFF43154A8ADF683F4639450A73206E9EE5A7CDAB4581B2D48495154DA6BFA5`.
- Decision: `accepted` as a technical dynamic remeshing kernel. Production use
  remains conditional on an approved fill backend and LGPL compliance.

### Forbidden or blocked transitive packages

- MITK/BlueBerry/Slicer/CTK: `rejected` as XQ runtime dependencies. They are
  installed elsewhere but absent from all scanned executable import closures.
- Python: `rejected` as an XQ runtime dependency. It is absent from the final
  executable import closure, but host Anaconda `python312.lib` is still named
  by the current broad product link command, so that build recipe is `blocked`.
- Qt 6.7.0 install: `blocked` until rebuilt. Its current `Qt6Core.dll` imports
  host `zstd.dll`; the build cache records `zstd_DIR=C:/software/anaconda/...`
  and `FEATURE_zstd=ON`. The current external recipe explicitly disables zstd,
  but this installed binary predates that correction.

## Explicit vascular package recipe

The accepted probe recipe, to be consumed by a later product-integration child,
is:

```cmake
find_package(VTK 9.3.0 EXACT CONFIG REQUIRED COMPONENTS
    CommonCore CommonDataModel CommonExecutionModel CommonMath
    FiltersCore FiltersGeneral FiltersGeometry IOImage IOXML)

find_package(ITK 5.4.0 EXACT REQUIRED COMPONENTS
    ITKAnisotropicSmoothing ITKImageFeature
    ITKBinaryMathematicalMorphology ITKConnectedComponents
    ITKDistanceMap ITKVtkGlue ITKIOGDCM)
```

The UI/render target may add its own explicit Qt/VTK rendering components. It
must not recover the current broad `${ITK_LIBRARIES}`/all-VTK link graph.
