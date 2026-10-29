$ErrorActionPreference = 'Stop'

& (Join-Path $PSScriptRoot 'build.ps1')

$root = Resolve-Path (Join-Path $PSScriptRoot '..')

$isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
$candidates = @()
if ($isccCommand) {
    $candidates += $isccCommand.Source
}
$candidates += @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
)

$iscc = $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $iscc) {
    throw 'Inno Setup 6 was not found. Install it and rerun this script.'
}

& $iscc (Join-Path $root 'installer\AudioRoleSync.iss')
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE."
}

$cmakeText = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($cmakeText -notmatch 'project\(AudioRoleSync VERSION ([0-9]+\.[0-9]+\.[0-9]+)') {
    throw 'Could not determine the project version from CMakeLists.txt.'
}
$version = $Matches[1]
$installer = Join-Path $root "dist\AudioRoleSync-v$version-win-x64-setup.exe"

if (-not (Test-Path $installer)) {
    throw "Installer output was not found: $installer"
}

Write-Host "Installer: $installer"
