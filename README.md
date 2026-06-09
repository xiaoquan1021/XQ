# XQ

XQ is being migrated to a Windows x64 / MSVC 2022 native Qt + MITK medical imaging application.

## Windows Build

Clone `XQ` and the Externals entry repository side by side:

```powershell
git clone https://github.com/xiaoquan1021/XQ.git
git clone https://github.com/xiaoquan1021/Externals.git
```

Fetch the external source mirrors from `Externals/externals.manifest`:

```powershell
cd Externals
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\Fetch-Sources.ps1
```

Build the dependency stack:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build_all.ps1 -Profile xq -Target all
```

Configure and build XQ:

```powershell
cd ..\XQ
Copy-Item .env.example .env
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-xq.ps1 build -ExternalsRoot ..\Externals
```

The optional `.env` file is the Windows equivalent of an activation file for this
native desktop stack. It keeps local paths for `XQ_EXTERNALS_ROOT`,
`XQ_BUILD_DIR`, `XQ_EXTERNALS_PLATFORM`, `XQ_VS_INSTALL_PATH`, and `XQ_CMAKE`.
It is ignored by git; commit changes to `.env.example` only.

To enter the configured environment in the current PowerShell process:

```powershell
. .\scripts\Enter-XQEnvironment.ps1
```

After activation, `PATH`, `QT_PLUGIN_PATH`, `XQ_PLUGIN_PATH`, and the VS2022 x64
toolchain variables are set for configure, build, tests, and direct `XQ.exe`
smoke runs. `scripts\build-xq.ps1`, `scripts\run-xq.ps1`, and `Start-XQ.cmd`
also load `.env` automatically.

The Windows preset builds the new monolithic Qt/MITK target by default and disables the legacy BlueBerry application. During migration, the legacy target can still be enabled with `-DXQ_BUILD_LEGACY_BLUEBERRY=ON`.
