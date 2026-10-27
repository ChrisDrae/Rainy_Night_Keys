<#
.SYNOPSIS
    Downloads sampled Rhodes tines for Keys into assets\tines\jrhodes3d (about 22 MB).

.DESCRIPTION
    jRhodes3d by Jeff Learman: a 1977 Rhodes Mark I Stage 73, recorded straight from the harp
    connector (the DI), 15 notes x 5 velocity layers, full length, FLAC. Keys uses the mono,
    no-crossfade set and its SFZ map. These stand in for the Mk II until it's recorded.

    LICENSE: the samples are CC BY-NC 4.0 (credit Jeff Learman; non-commercial use only). Fine
    for developing and playing Keys; shipping them in a commercial product needs the author's
    permission (see assets\tines\jrhodes3d\LICENSE). Music you make with them is yours (CC0).

    Without these files Keys still plays: the tines fall back to the modeled demo tines. With
    them, the menu lets you switch between the two.

    Pinned to one commit, so the samples never change underneath you.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\fetch-rhodes-samples.ps1
#>
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$commit = 'aea5b8d3e11e2f7102593789a4e0a0e41b30271a'
$base = "https://raw.githubusercontent.com/sfzinstruments/jlearman.jRhodes3d/$commit"
$sfzName = 'jRhodes3d-mono-no-xfade.sfz'

$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $repo 'assets\tines\jrhodes3d'
$sampleDir = Join-Path $dir 'jRhodes3d-mono'
New-Item -ItemType Directory -Force -Path $sampleDir | Out-Null

foreach ($name in $sfzName, 'LICENSE', 'README.md') {
    Invoke-WebRequest -Uri "$base/$name" -OutFile (Join-Path $dir $name)
}

# Every sample the map refers to (some are shared between layers: fetch each once).
$sfz = Get-Content (Join-Path $dir $sfzName) -Raw
$samples = [regex]::Matches($sfz, 'sample=(\S+)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
foreach ($name in $samples) {
    $target = Join-Path $sampleDir $name
    if (-not (Test-Path $target)) {
        Invoke-WebRequest -Uri "$base/jRhodes3d-mono/$name" -OutFile $target
    }
}

Write-Host "$($samples.Count) Rhodes samples are in $dir"
