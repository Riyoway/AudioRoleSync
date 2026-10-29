$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$build = Join-Path $root 'build'
$dist = Join-Path $root 'dist\win-x64'

cmake -S $root -B $build -A x64
cmake --build $build --config Release --parallel

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item (Join-Path $build 'Release\AudioRoleSync.exe') (Join-Path $dist 'AudioRoleSync.exe') -Force

Write-Host "Built: $dist\AudioRoleSync.exe"
