param([switch]$NoBrowser)
$ErrorActionPreference = 'Stop'
$webLauncher = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\WEB\launch.ps1'))
if (-not (Test-Path -LiteralPath $webLauncher)) { throw "Shared WEB workspace missing: $webLauncher" }
$release = Split-Path -Leaf $PSScriptRoot
& $webLauncher -Release $release -NoBrowser:$NoBrowser
