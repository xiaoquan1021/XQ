Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Recipe = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "scripts\Build-Dependency.ps1")

if ($Recipe -notmatch 'function\s+Update-XQMitkBlueBerryToolkitSource') {
    throw "MITK BlueBerry recipe must provide a reproducible source patch"
}

foreach ($required in @(
    'Modules\SegmentationUI\files.cmake',
    'Qmitk/QmitkMonaiLabelToolGUI.cpp',
    'Qmitk/QmitkMonaiLabel2DToolGUI.cpp',
    'Qmitk/QmitkMonaiLabel3DToolGUI.cpp',
    'Qmitk/QmitkMonaiLabelToolGUI.h',
    'Qmitk/QmitkMonaiLabel2DToolGUI.h',
    'Qmitk/QmitkMonaiLabel3DToolGUI.h',
    'Qmitk/QmitkMonaiLabelToolGUIControls.ui'
)) {
    if ($Recipe -notmatch [regex]::Escape($required)) {
        throw "MITK BlueBerry recipe must remove MONAI SegmentationUI entry: $required"
    }
}

$blueberryFunction = [regex]::Match(
    $Recipe,
    '(?s)function\s+Update-XQMitkBlueBerryToolkitSource\s*\{(?<body>.*?)\n\}'
)
if (-not $blueberryFunction.Success) {
    throw "Could not inspect Update-XQMitkBlueBerryToolkitSource"
}

$body = $blueberryFunction.Groups["body"].Value
if ($body -notmatch [regex]::Escape('$monaiUiEntries')) {
    throw "MITK BlueBerry recipe must track MONAI SegmentationUI removals as explicit entries"
}

if ($body -notmatch [regex]::Escape('[regex]::Escape($monaiUiEntry)') -or
    $body -notmatch [regex]::Escape('Removing MITK MONAI Label SegmentationUI sources from BlueBerry build')) {
    throw "MITK BlueBerry MONAI SegmentationUI patch must be idempotent and visible in logs"
}
