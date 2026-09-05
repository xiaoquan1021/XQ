Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-XQManifestEntries {
    param([Parameter(Mandatory)][string]$ManifestPath)

    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        throw "[Externals][ERROR] Manifest not found: $ManifestPath"
    }

    Get-Content -LiteralPath $ManifestPath | ForEach-Object {
        $line = $_.Trim()
        if ($line -and -not $line.StartsWith("#")) {
            $parts = $line -split "\|", 7
            if ($parts.Count -ne 7) {
                throw "[Externals][ERROR] Invalid manifest row: $line"
            }
            [pscustomobject]@{
                Key = $parts[0]
                Name = $parts[1]
                Version = $parts[2]
                Repository = $parts[3]
                OfficialUrl = $parts[4]
                TargetPath = $parts[5]
                Order = [int]$parts[6]
            }
        }
    } | Sort-Object Order
}

function Test-XQSourceTree {
    param(
        [Parameter(Mandatory)][string]$RootDir,
        [Parameter(Mandatory)][string]$ManifestPath
    )

    $missing = @()
    foreach ($entry in Get-XQManifestEntries -ManifestPath $ManifestPath) {
        $fullPath = Join-Path $RootDir $entry.TargetPath
        if (-not (Test-Path -LiteralPath $fullPath -PathType Container)) {
            $missing += $entry.TargetPath
        }
    }

    if ($missing.Count -gt 0) {
        foreach ($path in $missing) {
            Write-Error "[Externals][ERROR] Missing source: $path" -ErrorAction Continue
        }
        Write-Error "[Externals][ERROR] XQ source mirror is incomplete." -ErrorAction Continue
        Write-Error "[Externals][ERROR] Run: pwsh scripts/Fetch-Sources.ps1" -ErrorAction Continue
        return $false
    }

    return $true
}

function Assert-XQCommand {
    param(
        [Parameter(Mandatory)][string]$Name,
        [string]$ExplicitPath
    )

    if ($ExplicitPath -and (Test-Path -LiteralPath $ExplicitPath)) {
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    throw "[Externals][ERROR] Required command not found: $Name"
}

function ConvertTo-XQCMakePath {
    param([Parameter(Mandatory)][string]$Path)

    return $Path.Replace("\", "/")
}

function New-XQShortDirectoryJunction {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$TargetDir,
        [Parameter(Mandatory)][string]$ShortPath,
        [switch]$CreateTarget
    )

    if (-not (Test-Path -LiteralPath $TargetDir -PathType Container)) {
        if ($CreateTarget) {
            New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null
        }
        else {
            throw "[Externals][ERROR] Missing $Name target directory: $TargetDir"
        }
    }

    $resolvedTarget = (Resolve-Path -LiteralPath $TargetDir).Path
    $shortParent = Split-Path -Parent $ShortPath
    New-Item -ItemType Directory -Path $shortParent -Force | Out-Null

    if (Test-Path -LiteralPath $ShortPath) {
        $existing = Get-Item -LiteralPath $ShortPath -Force
        $existingTargets = @($existing.Target) | Where-Object { $_ } | ForEach-Object {
            if (Test-Path -LiteralPath $_) {
                (Resolve-Path -LiteralPath $_).Path
            }
            else {
                $_
            }
        }

        if ($existing.LinkType -eq "Junction" -and ($existingTargets -contains $resolvedTarget)) {
            $resolvedShortPath = (Resolve-Path -LiteralPath $ShortPath).Path
            if ($resolvedShortPath.Length -gt 50) {
                throw "[Externals][ERROR] $Name short path is still too long for ITK: $resolvedShortPath"
            }
            return $resolvedShortPath
        }

        throw "[Externals][ERROR] Short path already exists and does not point to $Name target: $ShortPath"
    }

    New-Item -ItemType Junction -Path $ShortPath -Target $resolvedTarget | Out-Null
    $createdShortPath = (Resolve-Path -LiteralPath $ShortPath).Path
    if ($createdShortPath.Length -gt 50) {
        throw "[Externals][ERROR] $Name short path is still too long for ITK: $createdShortPath"
    }

    return $createdShortPath
}

function Invoke-XQCMakeBuild {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$SourceDir,
        [Parameter(Mandatory)][string]$BuildDir,
        [Parameter(Mandatory)][string]$InstallDir,
        [string[]]$ConfigureArgs = @(),
        [string]$BuildType = "Release",
        [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount - 1),
        [string]$CMake = "cmake",
        [switch]$ConfigureOnly
    )

    if (-not (Test-Path -LiteralPath $SourceDir -PathType Container)) {
        throw "[Externals][ERROR] Missing $Name source: $SourceDir"
    }

    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null

    $args = @(
        "-S", $SourceDir,
        "-B", $BuildDir,
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DCMAKE_INSTALL_PREFIX=$InstallDir"
    ) + $ConfigureArgs

    & $CMake @args
    if ($LASTEXITCODE -ne 0) {
        throw "[Externals][ERROR] CMake configure failed for $Name"
    }

    if ($ConfigureOnly) {
        Write-Host "[Externals] Configure-only requested; skipping build/install for $Name."
        return
    }

    & $CMake --build $BuildDir --config $BuildType --parallel $Jobs
    if ($LASTEXITCODE -ne 0) {
        throw "[Externals][ERROR] Build failed for $Name"
    }

    & $CMake --install $BuildDir --config $BuildType
    if ($LASTEXITCODE -ne 0) {
        throw "[Externals][ERROR] Install failed for $Name"
    }
}

function Add-XQPath {
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $entries = $env:PATH -split ";"
    if ($entries -notcontains $Path) {
        $env:PATH = "$Path;$env:PATH"
    }
}
