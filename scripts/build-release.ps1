$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$dist = Join-Path $root 'dist'

& (Join-Path $PSScriptRoot 'build.ps1')

$cmakeText = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($cmakeText -notmatch 'project\(AudioRoleSync VERSION ([0-9]+\.[0-9]+\.[0-9]+)') {
    throw 'Could not determine the project version from CMakeLists.txt.'
}
$version = $Matches[1]

$portableStage = Join-Path $dist 'portable\AudioRoleSync'
$portableZip = Join-Path $dist "AudioRoleSync-v$version-win-x64-portable.zip"

Remove-Item (Join-Path $dist 'portable') -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $portableZip -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $portableStage | Out-Null

Copy-Item (Join-Path $dist 'win-x64\AudioRoleSync.exe') $portableStage -Force
Copy-Item (Join-Path $root 'LICENSE') $portableStage -Force
Copy-Item (Join-Path $root 'README.md') $portableStage -Force
New-Item -ItemType Directory -Force -Path (Join-Path $portableStage 'resources') | Out-Null
Copy-Item (Join-Path $root 'resources\app.png') (Join-Path $portableStage 'resources\app.png') -Force

Compress-Archive -Path (Join-Path $portableStage '*') -DestinationPath $portableZip -CompressionLevel Optimal
Remove-Item (Join-Path $dist 'portable') -Recurse -Force

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

$installer = Join-Path $dist "AudioRoleSync-v$version-win-x64-setup.exe"
if (-not (Test-Path $installer)) {
    throw "Installer output was not found: $installer"
}

Write-Host 'Release artifacts:'
Write-Host "  $portableZip"
Write-Host "  $installer"
