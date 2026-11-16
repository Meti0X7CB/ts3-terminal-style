# Builds the "terminal" icon pack from TeamSpeak's own default_mono_2014.zip.
# Dot-source this file, then call New-TerminalIconPack -Source <zip> -Destination <zip>.
#
# Neutral colours (greys and TeamSpeak blues) are remapped onto one slate-grey ramp
# that reads on both light and dark backgrounds; semantic colours (red / amber /
# green) collapse onto three fixed accents. Nothing from TeamSpeak is redistributed:
# the pack is generated from the user's own install.

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$script:Shape     = @(0x6c, 0x6f, 0x85)   # what a dark original shape becomes
$script:Highlight = @(0xd0, 0xd2, 0xda)   # what a white highlight becomes
$script:Red       = '#d07070'
$script:Green     = '#90c090'
$script:Amber     = '#e0c070'
# icons whose blue means "talking": tint green instead of grey
$script:TalkingIcons = @('player_on.svg', 'player_commander_on.svg', 'is_talker.svg')
$script:Latin1 = [Text.Encoding]::GetEncoding(28591)

function ConvertTo-Hls([double]$r, [double]$g, [double]$b) {
    # same maths as Python's colorsys.rgb_to_hls
    $max = [Math]::Max($r, [Math]::Max($g, $b)); $min = [Math]::Min($r, [Math]::Min($g, $b))
    $l = ($max + $min) / 2.0
    if ($max -eq $min) { return @(0.0, $l, 0.0) }
    $d = $max - $min
    $s = if ($l -le 0.5) { $d / ($max + $min) } else { $d / (2.0 - $max - $min) }
    $rc = ($max - $r) / $d; $gc = ($max - $g) / $d; $bc = ($max - $b) / $d
    if ($r -eq $max) { $h = $bc - $gc }
    elseif ($g -eq $max) { $h = 2.0 + $rc - $bc }
    else { $h = 4.0 + $gc - $rc }
    $h = ($h / 6.0) % 1.0
    if ($h -lt 0) { $h += 1.0 }
    return @($h, $l, $s)
}

function Convert-IconColor([string]$hex, [bool]$talking) {
    $h = $hex.TrimStart('#')
    if ($h.Length -eq 3) { $h = -join ($h.ToCharArray() | ForEach-Object { "$_$_" }) }
    $r = [Convert]::ToInt32($h.Substring(0, 2), 16)
    $g = [Convert]::ToInt32($h.Substring(2, 2), 16)
    $b = [Convert]::ToInt32($h.Substring(4, 2), 16)
    $hue, $light, $sat = ConvertTo-Hls ($r / 255.0) ($g / 255.0) ($b / 255.0)
    $deg = $hue * 360
    if ($sat -gt 0.35 -and $light -gt 0.15 -and $light -lt 0.9) {
        if ($deg -lt 20 -or $deg -gt 335) { return $script:Red }
        if ($deg -ge 20 -and $deg -lt 65) { return $script:Amber }
        if ($deg -ge 65 -and $deg -lt 170) { return $script:Green }
        if ($talking -and $deg -ge 170 -and $deg -lt 260) { return $script:Green }
    }
    # neutral / blue: dark shapes -> slate, light highlights -> pale grey
    $lum = (0.299 * $r + 0.587 * $g + 0.114 * $b) / 255.0
    $v = 0..2 | ForEach-Object {
        [int][Math]::Round($script:Shape[$_] + ($script:Highlight[$_] - $script:Shape[$_]) * $lum)
    }
    return '#{0:x2}{1:x2}{2:x2}' -f $v[0], $v[1], $v[2]
}

function Convert-IconSvg([string]$svg, [string]$name) {
    $talking = $script:TalkingIcons -contains $name
    $svg = [regex]::Replace($svg, '(fill|stroke|stop-color)\s*([:=]\s*"?)(white|black)\b', {
        param($m)
        $named = if ($m.Groups[3].Value -eq 'white') { '#ffffff' } else { '#000000' }
        $m.Groups[1].Value + $m.Groups[2].Value + $named
    })
    return [regex]::Replace($svg, '#[0-9a-fA-F]{6}\b|#[0-9a-fA-F]{3}\b', {
        param($m) Convert-IconColor $m.Value $talking   # $talking resolves via dynamic scope
    })
}

function New-TerminalIconPack {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination
    )
    $tmp = "$Destination.tmp"
    if (Test-Path $tmp) { Remove-Item $tmp -Force }
    $in = [IO.Compression.ZipFile]::OpenRead($Source)
    $outStream = [IO.File]::Open($tmp, [IO.FileMode]::CreateNew)
    $out = New-Object IO.Compression.ZipArchive($outStream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($entry in $in.Entries) {
            $target = $out.CreateEntry($entry.FullName, [IO.Compression.CompressionLevel]::Optimal)
            $target.LastWriteTime = $entry.LastWriteTime
            if ($entry.FullName.EndsWith('/')) { continue }   # directory entry
            $reader = $entry.Open(); $ms = New-Object IO.MemoryStream
            $reader.CopyTo($ms); $reader.Dispose()
            $bytes = $ms.ToArray()
            if ($entry.FullName.EndsWith('.svg')) {
                $text = Convert-IconSvg ($script:Latin1.GetString($bytes)) $entry.Name
                $bytes = $script:Latin1.GetBytes($text)
            } elseif ($entry.FullName -eq 'settings.ini') {
                $text = $script:Latin1.GetString($bytes).Replace('PLACEHOLDER=2', 'PLACEHOLDER=1')
                $bytes = $script:Latin1.GetBytes($text)
            }
            $w = $target.Open(); $w.Write($bytes, 0, $bytes.Length); $w.Dispose()
        }
    } finally {
        $out.Dispose(); $outStream.Dispose(); $in.Dispose()
    }
    Move-Item $tmp $Destination -Force
}
