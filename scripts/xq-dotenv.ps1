Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Import-XQDotEnv {
    param(
        [string]$RepoRoot,
        [string]$EnvFile,
        [switch]$Override
    )

    if (-not $RepoRoot) {
        $RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
    }
    else {
        $RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)
    }

    if (-not $EnvFile) {
        $EnvFile = Join-Path $RepoRoot ".env"
    }
    elseif (-not [System.IO.Path]::IsPathRooted($EnvFile)) {
        $EnvFile = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $EnvFile))
    }
    else {
        $EnvFile = [System.IO.Path]::GetFullPath($EnvFile)
    }

    $loaded = $false
    $applied = New-Object System.Collections.Generic.List[string]
    if (Test-Path -LiteralPath $EnvFile) {
        $loaded = $true
        foreach ($rawLine in Get-Content -LiteralPath $EnvFile) {
            $line = $rawLine.Trim()
            if (-not $line -or $line.StartsWith("#")) {
                continue
            }
            if ($line.StartsWith("export ")) {
                $line = $line.Substring(7).Trim()
            }

            $separator = $line.IndexOf("=")
            if ($separator -le 0) {
                continue
            }

            $name = $line.Substring(0, $separator).Trim()
            $value = $line.Substring($separator + 1).Trim()
            if ($name -notmatch "^[A-Za-z_][A-Za-z0-9_]*$") {
                continue
            }

            if ($value.Length -ge 2) {
                $first = $value[0]
                $last = $value[$value.Length - 1]
                if (($first -eq '"' -and $last -eq '"') -or ($first -eq "'" -and $last -eq "'")) {
                    $value = $value.Substring(1, $value.Length - 2)
                }
            }

            $existing = [Environment]::GetEnvironmentVariable($name, "Process")
            if ($Override -or [string]::IsNullOrEmpty($existing)) {
                [Environment]::SetEnvironmentVariable($name, $value, "Process")
                $applied.Add($name) | Out-Null
            }
        }
    }

    [pscustomobject]@{
        EnvFile = $EnvFile
        Loaded = $loaded
        Applied = $applied.ToArray()
    }
}
