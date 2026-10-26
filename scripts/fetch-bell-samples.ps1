<#
.SYNOPSIS
    Downloads the CC0 bell samples Keys uses into assets\bell (about 46 MB).

.DESCRIPTION
    * Celeste: Sonatina Symphonic Orchestra's celeste (originally by stamperadam on freesound.org,
      CC0), every major third C4–G#7, soft and hard, plus a mechanical-noise sample.
    * Vibraphone: Versilian Community Sample Library (CC0), soft mallets, F2–E5, two dynamics.

    Without these files Keys still plays: the bell layer falls back to a modeled bell.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\fetch-bell-samples.ps1
#>
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$repo = Split-Path -Parent $PSScriptRoot
$celesteDir = Join-Path $repo 'assets\bell\celeste'
$vibesDir = Join-Path $repo 'assets\bell\vibes'
New-Item -ItemType Directory -Force -Path $celesteDir, $vibesDir | Out-Null

$celesteBase = 'https://raw.githubusercontent.com/peastman/sso/master/Sonatina%20Symphonic%20Orchestra/Samples/Celeste'
$notes = 'c4', 'e4', 'g#4', 'c5', 'e5', 'g#5', 'c6', 'e6', 'g#6', 'c7', 'e7', 'g#7'
foreach ($note in $notes) {
    foreach ($dynamic in 'soft', 'hard') {
        $name = "celeste-$note-$dynamic.flac"
        $target = Join-Path $celesteDir $name
        if (-not (Test-Path $target)) {
            Invoke-WebRequest -Uri "$celesteBase/$($name.Replace('#', '%23'))" -OutFile $target
        }
    }
}
Invoke-WebRequest -Uri "$celesteBase/celeste-mech.flac" -OutFile (Join-Path $celesteDir 'celeste-mech.flac')
Invoke-WebRequest -Uri "$celesteBase/README.md" -OutFile (Join-Path $celesteDir 'README.md')

$vibesBase = 'https://raw.githubusercontent.com/sgossner/VCSL/master/Idiophones/Struck%20Idiophones/Vibraphone/Soft%20Mallets'
$vibes = @(
    'F2_v1_rr1', 'F2_v2_rr1', 'A2_v1_rr1', 'A2_v2_rr1', 'C3_v1_rr2', 'C3_v2_rr1', 'E3_v1_rr2', 'E3_v2_rr1',
    'G3_v1_rr1', 'G3_v2_rr1', 'B3_v1_rr1', 'B3_v2_rr1', 'D4_v1_rr1', 'D4_v2_rr1', 'F4_v1_rr1', 'F4_v2_rr1',
    'A4_v1_rr1', 'A4_v2_rr1', 'C5_v1_rr1', 'C5_v2_rr1', 'E5_v1_rr1', 'E5_v2_rr1')
foreach ($v in $vibes) {
    $name = "Vibes_soft_${v}_Main.wav"
    $target = Join-Path $vibesDir $name
    if (-not (Test-Path $target)) {
        Invoke-WebRequest -Uri "$vibesBase/$name" -OutFile $target
    }
}
Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/sgossner/VCSL/master/LICENSE' -OutFile (Join-Path $vibesDir 'LICENSE.txt')

Write-Host "Bell samples are in $repo\assets\bell"
