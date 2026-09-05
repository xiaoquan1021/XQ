param(
    [string]$Target = "all",
    [string]$RootDir,
    [string]$ManifestPath,
    [string]$DownloadDir,
    [switch]$Force,
    [switch]$DryRun
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
if (-not $DownloadDir) {
    $DownloadDir = Join-Path $RootDir "downloads"
}
$DownloadDir = [System.IO.Path]::GetFullPath($DownloadDir)

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

function Get-OfficialArchiveName {
    param([Parameter(Mandatory)]$Entry)

    $uri = [Uri]$Entry.OfficialUrl
    $fileName = [System.IO.Path]::GetFileName($uri.AbsolutePath)
    if (-not $fileName) {
        return "$($Entry.Key)-$($Entry.Version).download"
    }

    $suffix = switch -Regex ($fileName) {
        "\.tar\.gz$" { ".tar.gz"; break }
        "\.tar\.xz$" { ".tar.xz"; break }
        "\.zip$" { ".zip"; break }
        default { [System.IO.Path]::GetExtension($fileName) }
    }
    if (-not $suffix) {
        $suffix = ".download"
    }

    return "$($Entry.Key)-$($Entry.Version)$suffix"
}

$entries = Get-XQManifestEntries -ManifestPath $ManifestPath |
    Where-Object { Test-TargetMatch -Entry $_ -RequestedTarget $Target } |
    Sort-Object Order

if (-not $entries) {
    throw "[Externals][ERROR] Dependency not found in manifest: $Target"
}

New-Item -ItemType Directory -Path $DownloadDir -Force | Out-Null
$indexPath = Join-Path $DownloadDir "official-source-index.tsv"
"key`tname`tversion`tofficial_url`tarchive" | Set-Content -LiteralPath $indexPath -Encoding UTF8

foreach ($entry in $entries) {
    $archiveName = Get-OfficialArchiveName -Entry $entry
    $archivePath = Join-Path $DownloadDir $archiveName
    $partialPath = "$archivePath.part"

    "$($entry.Key)`t$($entry.Name)`t$($entry.Version)`t$($entry.OfficialUrl)`t$archivePath" |
        Add-Content -LiteralPath $indexPath -Encoding UTF8

    Write-Host "download $($entry.Key) $($entry.Version)"
    Write-Host "  official: $($entry.OfficialUrl)"
    Write-Host "  archive:  $archivePath"

    if ($DryRun) {
        continue
    }

    if ((Test-Path -LiteralPath $archivePath) -and -not $Force) {
        $existing = Get-Item -LiteralPath $archivePath
        if ($existing.Length -gt 0) {
            Write-Host "  skip: existing archive ($($existing.Length) bytes)"
            continue
        }
    }

    if ($Force) {
        Remove-Item -LiteralPath $archivePath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $partialPath -Force -ErrorAction SilentlyContinue
    }

    & curl.exe -L --fail --retry 5 --retry-delay 5 --connect-timeout 30 --speed-time 120 --speed-limit 1024 -C - -o $partialPath $entry.OfficialUrl
    if ($LASTEXITCODE -ne 0) {
        throw "[Externals][ERROR] Official download failed for $($entry.Key) from $($entry.OfficialUrl)"
    }

    Move-Item -LiteralPath $partialPath -Destination $archivePath -Force
    $downloaded = Get-Item -LiteralPath $archivePath
    Write-Host "  done: $($downloaded.Length) bytes"
}
