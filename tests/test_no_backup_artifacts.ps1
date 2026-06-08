$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$trackedFiles = & git -C $repoRoot ls-files
if ($LASTEXITCODE -ne 0) {
    throw "Unable to list tracked files"
}

$backupPattern = '(?i)(\.bak$|\.old$|~$|_backup(_|$)|backup_\d+)'
$backupFiles = $trackedFiles | Where-Object {
    $_ -match $backupPattern -and
    (Test-Path -LiteralPath (Join-Path $repoRoot $_))
}

if ($backupFiles) {
    $locations = $backupFiles -join "`n  - "
    throw "Tracked backup artifacts are not allowed:`n  - $locations"
}
