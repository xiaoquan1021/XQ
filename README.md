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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\build-xq.ps1 build -ExternalsRoot ..\Externals
```

The Windows preset builds the new monolithic Qt/MITK target by default and disables the legacy BlueBerry application. During migration, the legacy target can still be enabled with `-DXQ_BUILD_LEGACY_BLUEBERRY=ON`.
