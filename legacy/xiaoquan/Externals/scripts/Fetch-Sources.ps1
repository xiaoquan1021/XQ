param(
    [string]$Target = "all",
    [switch]$DryRun,
    [switch]$Force,
    [string]$RootDir,
    [string]$ManifestPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DefaultRoot = Resolve-Path (Join-Path $ScriptDir "..")
if (-not $RootDir) {
    $RootDir = if ($env:EXTERNALS_ROOT) { $env:EXTERNALS_ROOT } else { $DefaultRoot.Path }
}
$RootDir = [System.IO.Path]::GetFullPath($RootDir)
if (-not $ManifestPath) {
    $ManifestPath = if ($env:EXTERNALS_MANIFEST) { $env:EXTERNALS_MANIFEST } else { Join-Path $RootDir "externals.manifest" }
}

. (Join-Path $ScriptDir "Build-Helpers.ps1")

function Test-TargetMatch {
    param(
        [Parameter(Mandatory)]$Entry,
        [Parameter(Mandatory)][string]$RequestedTarget
    )

    if ($RequestedTarget -eq "all") {
        return $true
    }

    return $Entry.Key.Equals($RequestedTarget, [StringComparison]::OrdinalIgnoreCase) -or
        $Entry.Name.Equals($RequestedTarget, [StringComparison]::OrdinalIgnoreCase)
}

function Invoke-SourceClone {
    param([Parameter(Mandatory)]$Entry)

    $fullTarget = Join-Path $RootDir $Entry.TargetPath
    if (Test-Path -LiteralPath $fullTarget) {
        $insideGit = $false
        git -C $fullTarget rev-parse --is-inside-work-tree *> $null
        if ($LASTEXITCODE -eq 0) {
            $insideGit = $true
        }

        if ($insideGit -and -not $Force) {
            Write-Host "update $($Entry.Key) -> $($Entry.TargetPath)"
            Write-Host "  repo: $($Entry.Repository)"
            Write-Host "  version: $($Entry.Version)"
            Write-Host "  official: $($Entry.OfficialUrl)"
            if (-not $DryRun) {
                git -c core.longpaths=true -C $fullTarget pull --ff-only
                if ($LASTEXITCODE -ne 0) {
                    throw "[Externals][ERROR] git pull failed for $($Entry.Key)"
                }
            }
            return
        }

        if (-not $Force) {
            throw "[Externals][ERROR] $($Entry.TargetPath) already exists; use -Force to replace it."
        }

        if ($DryRun) {
            Write-Host "remove $($Entry.TargetPath)"
        }
        else {
            Remove-Item -Recurse -Force -LiteralPath $fullTarget
        }
    }

    Write-Host "clone $($Entry.Key) -> $($Entry.TargetPath)"
    Write-Host "  repo: $($Entry.Repository)"
    Write-Host "  version: $($Entry.Version)"
    Write-Host "  official: $($Entry.OfficialUrl)"

    if ($DryRun) {
        return
    }

    New-Item -ItemType Directory -Path (Split-Path -Parent $fullTarget) -Force | Out-Null
    git -c core.longpaths=true clone --depth 1 $Entry.Repository $fullTarget
    if ($LASTEXITCODE -ne 0) {
        throw "[Externals][ERROR] git clone failed for $($Entry.Key)"
    }
}

$matched = $false
foreach ($entry in Get-XQManifestEntries -ManifestPath $ManifestPath) {
    if (Test-TargetMatch -Entry $entry -RequestedTarget $Target) {
        $matched = $true
        Invoke-SourceClone -Entry $entry
    }
}

if (-not $matched) {
    throw "[Externals][ERROR] Dependency not found in manifest: $Target"
}
