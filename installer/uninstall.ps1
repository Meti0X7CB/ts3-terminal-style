<#
  Removes everything install.ps1 added. TeamSpeak falls back to its default style,
  theme and icon pack the next time it starts.
#>
param(
    [string]$TeamSpeakPath,
    [switch]$Elevated
)
. (Join-Path $PSScriptRoot 'Common.ps1')

try {
    $ts = Find-TeamSpeak $TeamSpeakPath
    Write-Host "TeamSpeak: $ts"
    if (Get-Process ts3client_win64 -ErrorAction SilentlyContinue) {
        throw 'TeamSpeak is running. Close it (including the tray icon) and run this again.'
    }
    if (-not $Elevated -and (Invoke-ElevatedIfNeeded $ts $PSCommandPath)) {
        Write-Host 'Done.'; exit 0
    }

    $targets = @(
        'styles\terminalstyle.dll',
        'styles\tui.qss', 'styles\tui_chat.qss', 'styles\tui_chat_darkmode.qss', 'styles\tui_darkmode.qss',
        'styles\tui',
        'gfx\terminal.zip'
    )
    foreach ($t in $targets) {
        $p = Join-Path $ts $t
        if (Test-Path $p) { Remove-Item $p -Recurse -Force; Write-Host "removed $t" }
    }
    Write-Host 'Uninstalled.' -ForegroundColor Green
    exit 0
} catch {
    Write-Host "Uninstall failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
