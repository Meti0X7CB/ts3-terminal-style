<#
  Installs the Terminal style for TeamSpeak 3:
    styles\terminalstyle.dll         the Qt style (light/dark, follows Windows)
    styles\tui*.qss, styles\tui\     the companion theme
    gfx\terminal.zip                 icon pack, generated from TeamSpeak's own icons
  Then pick them in TeamSpeak: Options > Design > Style "Terminal", Theme "tui", Icon pack "terminal".
#>
param(
    [string]$TeamSpeakPath,
    [switch]$Elevated
)
. (Join-Path $PSScriptRoot 'Common.ps1')
. (Join-Path $PSScriptRoot 'IconPack.ps1')

try {
    $ts = Find-TeamSpeak $TeamSpeakPath
    Write-Host "TeamSpeak: $ts"
    Assert-Compatible $ts
    if (-not $Elevated -and (Invoke-ElevatedIfNeeded $ts $PSCommandPath)) {
        Write-Host 'Done.'; exit 0
    }

    $files  = Join-Path $PSScriptRoot 'files'
    $styles = Join-Path $ts 'styles'
    $gfx    = Join-Path $ts 'gfx'

    Copy-Item (Join-Path $files 'styles\terminalstyle.dll') $styles -Force
    Copy-Item (Join-Path $files 'styles\tui.qss')          $styles -Force
    Copy-Item (Join-Path $files 'styles\tui_chat.qss')     $styles -Force
    Copy-Item (Join-Path $files 'styles\tui_chat.qss')     (Join-Path $styles 'tui_chat_darkmode.qss') -Force
    Set-Content (Join-Path $styles 'tui_darkmode.qss') '/* tui: intentionally empty, colours follow the Terminal style */'

    # TeamSpeak expects a folder named after the theme (info-pane templates, images):
    # reuse the user's own default one instead of shipping TeamSpeak's files.
    $tuiDir = Join-Path $styles 'tui'
    if (Test-Path $tuiDir) { Remove-Item $tuiDir -Recurse -Force }
    Copy-Item (Join-Path $styles 'default') $tuiDir -Recurse

    $mono = Join-Path $gfx 'default_mono_2014.zip'
    if (Test-Path $mono) {
        New-TerminalIconPack -Source $mono -Destination (Join-Path $gfx 'terminal.zip')
    } else {
        Write-Warning 'default_mono_2014.zip not found; skipping the icon pack.'
    }

    Write-Host ''
    Write-Host 'Installed. Start TeamSpeak, then Options > Design:' -ForegroundColor Green
    Write-Host '  Style = Terminal,  Theme = tui,  Icon pack = terminal   (restart TeamSpeak once)'
    exit 0
} catch {
    Write-Host "Install failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
