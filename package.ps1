# Builds the shareable zip: dist\ts3-terminal-style-<version>.zip
# Run build.cmd first.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$version = ([regex]::Match((Get-Content "$root\CMakeLists.txt" -Raw), 'VERSION (\d+\.\d+\.\d+)')).Groups[1].Value
$name = "ts3-terminal-style-$version"
$stage = Join-Path $root "dist\$name"

$dll = Join-Path $root 'build\styles\terminalstyle.dll'
if (-not (Test-Path $dll)) { throw 'build\styles\terminalstyle.dll missing - run build.cmd first.' }

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory "$stage\files\styles" -Force | Out-Null

Copy-Item "$root\installer\*" $stage
Copy-Item $dll "$stage\files\styles\"
Copy-Item "$root\theme\tui.qss", "$root\theme\tui_chat.qss" "$stage\files\styles\"
Copy-Item "$root\README.md", "$root\LICENSE" $stage

$zip = Join-Path $root "dist\$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host "Created $zip"
Get-ChildItem $stage -Recurse -File | ForEach-Object { $_.FullName.Substring($stage.Length + 1) }
