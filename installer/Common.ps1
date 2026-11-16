# Shared helpers for install.ps1 / uninstall.ps1

$ErrorActionPreference = 'Stop'

function Find-TeamSpeak {
    param([string]$Path)
    $candidates = @()
    if ($Path) { $candidates += $Path }
    foreach ($key in 'HKLM:\SOFTWARE\TeamSpeak 3 Client', 'HKCU:\SOFTWARE\TeamSpeak 3 Client',
                     'HKLM:\SOFTWARE\WOW6432Node\TeamSpeak 3 Client') {
        try { $v = (Get-ItemProperty $key -ErrorAction Stop).'(default)'; if ($v) { $candidates += $v } } catch {}
    }
    $candidates += "$env:ProgramFiles\TeamSpeak 3 Client", "$env:LOCALAPPDATA\TeamSpeak 3 Client"
    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c 'ts3client_win64.exe'))) { return (Resolve-Path $c).Path }
    }
    throw "TeamSpeak 3 (64-bit) not found. Re-run with -TeamSpeakPath 'C:\path\to\TeamSpeak 3 Client'."
}

function Assert-Compatible([string]$ts) {
    $qt = (Get-Item (Join-Path $ts 'Qt5Core.dll')).VersionInfo.ProductVersion
    if (-not $qt.StartsWith('5.15.')) {
        throw "This TeamSpeak uses Qt $qt; the Terminal style needs Qt 5.15 (TeamSpeak 3.5.x / 3.6.x)."
    }
    if (Get-Process ts3client_win64 -ErrorAction SilentlyContinue) {
        throw 'TeamSpeak is running. Close it (including the tray icon) and run this again.'
    }
}

function Test-Writable([string]$dir) {
    $probe = Join-Path $dir ".write-test-$PID"
    try { [IO.File]::WriteAllText($probe, ''); Remove-Item $probe -Force; return $true } catch { return $false }
}

# Re-runs the calling script elevated when the TeamSpeak folder needs admin rights.
# Returns $true if the caller should stop (the elevated copy did the work).
function Invoke-ElevatedIfNeeded([string]$ts, [string]$script) {
    if (Test-Writable (Join-Path $ts 'styles')) { return $false }
    Write-Host 'TeamSpeak is installed in a protected folder; asking for administrator rights...'
    $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$script`"",
                 '-TeamSpeakPath', "`"$ts`"", '-Elevated')
    $p = Start-Process powershell.exe -Verb RunAs -ArgumentList $argList -Wait -PassThru
    if ($p.ExitCode -ne 0) { throw "The elevated step failed (exit code $($p.ExitCode))." }
    return $true
}
