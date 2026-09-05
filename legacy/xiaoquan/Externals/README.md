# Externals

[![Ubuntu](https://github.com/xiaoquan1021/Externals/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/xiaoquan1021/Externals/actions/workflows/ubuntu.yml)

Source mirror entrypoint and local build scripts for the external dependencies used by XQ.

This repository does not store compiled dependencies or the external source trees themselves. It keeps the manifest, fetch scripts, platform build entrypoints, and per-library recipes needed to restore `src/...` and generate `install/...` locally.

## XQ Dependency Stack

XQ expects this repository at `~/Externals` on Linux or beside the XQ checkout on Windows by default. The path can be overridden from XQ with `XQ_EXTERNALS_ROOT`.

The current XQ build uses:

- Qt `6.7.0`
- Python `3.11.0`
- HDF5 `1.14.3`
- TinyXML2 `8.0.0`
- FreeType `2.13.0`
- SWIG `3.0.12`
- MMG `5.3.9`
- GDCM `3.0.10`
- VTK `9.3.0`
- ITK `5.4.0`
- OpenCascade `7.6.0`
- MITK `2024.06`

Important Linux XQ paths:

- dependency installs: `~/Externals/install`
- MITK build tree: `~/Externals/src/MITK-2024.06/build/MITK-build`
- MITK install tree: `~/Externals/install/mitk-2024.06`

Important Windows XQ paths:

- dependency installs: `Externals/install/windows-x64/<dependency-version>`
- dependency builds: `Externals/build/windows-x64/<Dependency>`
- MITK build tree: `Externals/build/windows-x64/MITK/MITK-build`
- MITK install tree: `Externals/install/windows-x64/mitk-2024.06`

## Source Mirrors

Official clean dependency sources are mirrored into public GitHub repositories under `xiaoquan1021`. The source list, official upstream URLs, target paths, and build order are defined in `externals.manifest`.

The mirrored repositories are:

- `xiaoquan1021/Qt`
- `xiaoquan1021/Python`
- `xiaoquan1021/HDF5`
- `xiaoquan1021/TinyXML2`
- `xiaoquan1021/FreeType`
- `xiaoquan1021/SWIG`
- `xiaoquan1021/MMG`
- `xiaoquan1021/GDCM`
- `xiaoquan1021/VTK`
- `xiaoquan1021/ITK`
- `xiaoquan1021/OpenCascade`
- `xiaoquan1021/MITK`

These repositories contain source only. Local build output, install trees, generated CMake files, object files, shared libraries, precompiled headers, and packaged build output are not stored in git.

## Repository Layout

- `build_all.sh`: main Ubuntu build entrypoint for the XQ dependency stack.
- `externals.manifest`: source mirror URLs, versions, target paths, and build order.
- `scripts/fetch_sources.sh`: clone or update source mirrors into `src/...`.
- `scripts/install_dependencies_ubuntu20.sh`: Ubuntu 20 system package installer.
- `scripts/build_*.sh`: per-library build scripts.
- `tests/`: lightweight script and preflight tests.
- `install/`, `src/`, and `output/`: local build directories ignored by git.
- `.github/workflows/ubuntu.yml`: lightweight Ubuntu validation workflow.

## Usage

Clone the repository:

```bash
git clone https://github.com/xiaoquan1021/Externals.git
cd Externals
```

Install the required system packages on Ubuntu:

```bash
bash scripts/install_dependencies_ubuntu20.sh
```

Fetch the mirrored source trees:

```bash
bash scripts/fetch_sources.sh
```

Build the XQ dependency stack. `xq` is also the default profile:

```bash
bash build_all.sh xq
```

The build creates:

```text
Externals/src/...
Externals/install/...
```

If a source directory is missing, `build_all.sh` stops before creating build output and prints:

```bash
bash scripts/fetch_sources.sh
```

To fetch or update one dependency:

```bash
bash scripts/fetch_sources.sh Qt
```

Existing git checkouts are updated with `git pull --ff-only`. Existing non-git source directories are left untouched unless `--force` is passed.

## Windows x64 / Visual Studio 2022

Windows builds use the same `externals.manifest`; this repository is only the entrypoint. Fetching sources clones the mirrored repositories listed in the manifest into `src/...`.

From a normal PowerShell prompt:

```powershell
git clone https://github.com/xiaoquan1021/Externals.git
cd Externals
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\Fetch-Sources.ps1
```

Build one small dependency smoke first:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build_all.ps1 -Profile xq -Target TinyXML2
```

Then build the full Windows stack:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build_all.ps1 -Profile xq -Target all
```

`build_all.ps1` imports the Visual Studio 2022 x64 developer environment, prefers the CMake/Ninja bundled with Visual Studio when present, and installs outputs under `install/windows-x64`. The fetch script uses `git -c core.longpaths=true` because Qt and QtWebEngine source paths can exceed the default Windows path limit.

SWIG `3.0.12` is mirrored from the official release archive, not the generated-file-free git tag, so the Windows recipe can build from `Source/CParse/parser.c` without requiring bison/flex.

## Validation

GitHub Actions runs lightweight Ubuntu checks for script syntax, source fetch dry-runs, and build preflight behavior. Full dependency compilation is expected to run locally because Qt, MITK, VTK, ITK, and related libraries are too large for a simple source-only validation workflow.

## Prerequisites

- `bash`
- `git`
- `cmake`
- `make`
- `gcc`/`g++`
- Ubuntu development packages installed by `scripts/install_dependencies_ubuntu20.sh`

Additional packages may be required if your Ubuntu image is customized or missing optional Qt WebEngine build dependencies.
