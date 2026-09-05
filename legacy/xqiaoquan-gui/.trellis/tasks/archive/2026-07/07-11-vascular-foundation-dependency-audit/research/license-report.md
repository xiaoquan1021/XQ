# License Report

Audit date: 2026-07-11

This is an engineering adoption and notice audit, not legal advice. Product
distribution still requires review against the intended distribution model.

## Decisions

| Dependency | Exact evidence | Engineering decision | Distribution obligations / limits |
| --- | --- | --- | --- |
| VTK 9.3.0 | `Copyright.txt`, SHA-256 `11232448BE82E0EA2C2C66219C2E36389F42249894070EE10C549FF182FC08B6` | `accepted` | BSD-3-Clause: retain copyright, conditions, disclaimer, and no-endorsement clause; carry applicable bundled third-party notices. |
| ITK 5.4.0 | `LICENSE`, SHA-256 `AAC73B3148F6D1D7111DBCA32099F68D26C644C6813AE1E4F05F6579AA2663FE`; `NOTICE`, SHA-256 `3BB4F458523EF5A5DF0DEDF461638CAB5D4416F3B91837B2E81C4067EA628807` | `accepted` | Apache-2.0: include license and NOTICE, preserve notices, mark modified files, observe patent terms. |
| ITKThickness3D v5.3.0 | `LICENSE`, same Apache-2.0 hash as ITK | `accepted` | Same Apache-2.0 obligations. Keep the source tag/commit and license with any vendored header integration. |
| GDCM 3.0.10 | `Copyright.txt`, SHA-256 `31A64B1BC4F367401FDD689DAEE273AF8195C24EB5D8DCD5B694F201B06721ED` | `accepted` | BSD-3-Clause-style notice/disclaimer and no endorsement; include codec/third-party notices used by the binary closure. |
| vtkvmtk selected C++ files | Official vmtk `LICENSE`, SHA-256 `962339B7FAEF43248493108C9646DFC00A5872AD8E56DE5E5EED68015F1AF137`; selected file headers name Luca Antiga/David Steinman and refer to `LICENCE` | `accepted` candidate | VMTK BSD-3-Clause notice plus VTK notice. The SimVascular subtree lacks the referenced `LICENCE` file, so product vendoring must add the official VMTK notice instead of relying only on SimVascular's root license. |
| SimVascular source revision | root `LICENSE.md`, SHA-256 `C07DB5E0C3741CEF098AB973E95E6630851CBCD562C8D793436ED46E9F38F564` | `accepted` as source provenance | Retain the SimVascular notice for its changes; it does not replace the VMTK notice named by the third-party files. |
| MMG official tag v5.3.9 | `LICENSE`, SHA-256 `DAD9CD8FB0885A7760760255F1ED63083BB33707E8C0801EDDD598E196B4025D`; `COPYING.LESSER`, SHA-256 `97628AFEBC60F026F5C2B25D7491C46A5C4EE61F693E7CFA07FBD2C03605979B`; `COPYING`, SHA-256 `E7FF0FA81DA220D46FB436142C7FB38BA4E5B3656C24CAAB4D34135CE98A0F07` | `accepted` technical kernel | LGPL-3.0-or-later. Keep MMG dynamically replaceable, ship the LGPL/GPL texts and copyright notices, provide the corresponding source/offer as required, and publish modifications to the LGPL work. Do not statically absorb MMG into a closed executable without a separate compliance plan. |
| TetGen self-identified 1.5 | bundled `LICENSE`, SHA-256 `831E2E642F7F79F3A02A88472EC5344E6441ACFC0529C7E90ABC571FEC226AB9` | `accepted-research-only` | Dual AGPL-3.0-or-later/commercial. The current static linkage is not approved for a proprietary/distributed production build. Choose an AGPL-compatible distribution model with source/network obligations or obtain a commercial license before production acceptance. |
| Shewchuk predicates bundled with TetGen | file says public domain; SHA-256 `5AD5262A393EF27870D09B6849285CE9DC10EEF90C47A7E8EEAB556DA16B9FA5` | `accepted` as part of research build | Preserve the attribution/header even though the file states public domain. This does not relax TetGen's AGPL/commercial terms. |
| Qt 6.7.0 | source `LGPL-3.0-only.txt`, SHA-256 `DA7EABB7BAFDF7D3AE5E9F223AA5BDC1EECE45AC569DC21B3B037520B4464768`, plus GPL/commercial alternatives | `blocked` installed binary | License path is selectable by the product owner, but the installed binary remains blocked until rebuilt without untracked host zstd. Dynamic LGPL use also requires notice/source/relinkability compliance. |
| Host Anaconda zstd | no lock entry; runtime DLL resolved only through inherited host PATH | `rejected` | Do not adopt or redistribute this incidental binary. Rebuild Qt with zstd disabled as the existing external recipe requires. |
| MITK/BlueBerry/Slicer/CTK and Python runtime | installed elsewhere but not selected | `rejected` runtime | They must not be copied, linked, or loaded by XQ. Their presence on disk is not adoption. |

## TetGen conclusion

The vendored XQ `tetgen.h`, `tetgen.cxx`, `predicates.cxx`, and LICENSE are
byte-identical to the SimVascular snapshot at commit
`b8c30d7d6194f16246ae9a435514cc55f6f6ab0b`. Both TetGen code files identify
themselves as version 1.5; there is no evidence for the CMake comment's 1.5.1
claim. The bundled license explicitly offers AGPLv3-or-later or a paid
commercial license.

Therefore:

- Technical probing and internal evaluation may continue under the stated
  research-only classification.
- This audit does not approve proprietary binary distribution.
- A later production decision must record either the commercial license grant,
  an AGPL-compatible distribution design, or a different fill backend.
- MMG acceptance does not make the two-stage TetGen-to-MMG backend distributable
  because the fill stage's license remains controlling for that stage.

## vtkvmtk conclusion

The eight selected SimVascular vtkvmtk sources carry VMTK copyright headers and
refer to a `LICENCE` file that is absent from the imported subtree. The current
official vmtk repository provides the complete BSD-3-Clause VMTK license, and
the SimVascular root is also permissively licensed. The candidate is accepted
provided product integration vendors the VMTK license text and VTK notice with
the exact selected sources. A source-only reference to the missing local file
is not sufficient distribution documentation.

## MMG conclusion

MMG source, headers, import library, and DLL are one technical build. The
official `v5.3.9` tag contains stale embedded `5.3.8` release macros; that
upstream metadata defect does not change the LGPL identity. XQ dynamically
loads `mmg3d.dll` through its import library, which is the intended compliance
shape. Keep MMG replaceable and ship its notices/source obligations.

## Required notice bundle before distribution

At minimum, a production packaging task must assemble and verify:

- VTK and all selected VTK third-party notices.
- ITK LICENSE and NOTICE, plus ITKThickness3D if integrated.
- GDCM and selected codec notices.
- VMTK BSD notice, SimVascular notice for fork changes, and VTK notice.
- MMG LICENSE, COPYING.LESSER, COPYING, corresponding source/offer, and any
  modification notices.
- Qt notices for the selected Qt license route.
- TetGen only after its commercial/AGPL decision is closed.

No notice bundle may include MITK/Python merely to rationalize accidental
runtime or build ingress; those dependencies must instead be removed.
