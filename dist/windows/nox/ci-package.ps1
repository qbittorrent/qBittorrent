# Smoke-tests the single-file qbittorrent-nox.exe and prepares the upload folder.
# Run from the repository root inside an MSVC developer environment (dumpbin must be available).
param(
    [string]$BuildDir = "build",
    [string]$OutDir = "upload"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $BuildDir "qbittorrent-nox.exe"

# 1. List DLL dependencies; fail if anything that should be linked statically is still a DLL
Write-Host "== DLL dependencies"
$deps = & dumpbin /dependents $exe | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '\.dll$' }
$deps | ForEach-Object { Write-Host "  $_" }
$forbidden = $deps | Where-Object { $_ -match '^(Qt6|vcruntime|msvcp|concrt|ucrtbase|libpq|icu|libssl|libcrypto|zlib|torrent)' }
if ($forbidden) {
    throw "Non-system DLL dependencies found: $($forbidden -join ', ')"
}

# 2. Run it alone (no DLLs next to it, only Windows directories in PATH)
Write-Host "== Smoke test"
$smokeDir = Join-Path ([System.IO.Path]::GetTempPath()) "qbt-smoke"
New-Item -ItemType Directory -Force -Path $smokeDir | Out-Null
Copy-Item $exe $smokeDir -Force
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = Join-Path $smokeDir "qbittorrent-nox.exe"
$psi.Arguments = "--version"
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.EnvironmentVariables["PATH"] = "$env:SystemRoot\System32;$env:SystemRoot"
$proc = [System.Diagnostics.Process]::Start($psi)
$output = $proc.StandardOutput.ReadToEnd()
$proc.WaitForExit()
Write-Host "  $output"
if ($proc.ExitCode -ne 0) { throw "qbittorrent-nox.exe --version failed with exit code $($proc.ExitCode)" }

# 3. Package
Write-Host "== Package"
$pkg = Join-Path $OutDir "qbittorrent-nox"
$sym = Join-Path $OutDir "debug-symbols"
New-Item -ItemType Directory -Force -Path $pkg, $sym | Out-Null
Copy-Item $exe $pkg
Copy-Item "dist/windows/nox/run-nox.bat" $pkg
Copy-Item "dist/windows/nox/seed-server-config.ps1" $pkg
Copy-Item "WebAPI_Changelog.md" $pkg
Copy-Item (Join-Path $BuildDir "qbittorrent-nox.pdb") $sym
Get-ChildItem -Recurse $OutDir | ForEach-Object { Write-Host ("  {0}  {1}" -f $_.FullName, $_.Length) }
