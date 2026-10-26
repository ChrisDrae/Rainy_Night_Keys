<#
.SYNOPSIS
    Validates the built VST3 with pluginval (https://github.com/Tracktion/pluginval).

.DESCRIPTION
    pluginval is the industry-standard "torture test" for plugins: it loads the plugin the way a
    host would, then calls prepareToPlay with many sample rates / block sizes, fuzzes every
    parameter, saves & restores state, opens the editor, and checks for crashes and timing issues.
    Passing strictness level 5 is the usual bar for "safe to load in a DAW".

    The first run downloads pluginval into <repo>\tools (git-ignored).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\run-pluginval.ps1
    powershell -ExecutionPolicy Bypass -File scripts\run-pluginval.ps1 -Strictness 8
#>
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [ValidateRange(1, 10)]
    [int]$Strictness = 5
)

$ErrorActionPreference = 'Stop'

$repo  = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $repo 'tools'
$exe   = Join-Path $tools 'pluginval.exe'

if (-not (Test-Path $exe)) {
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    $zip = Join-Path $tools 'pluginval_Windows.zip'
    Write-Host 'Downloading pluginval...'
    Invoke-WebRequest -Uri 'https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip' -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $tools -Force
    Remove-Item $zip
}

$plugin = Join-Path $repo "build\vs2026\Keys_artefacts\$Configuration\VST3\Keys.vst3"
if (-not (Test-Path $plugin)) {
    throw "Plugin not found: $plugin`nBuild first:  cmake --build --preset $($Configuration.ToLower())"
}

$logs = Join-Path $tools 'pluginval-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null

& $exe --strictness-level $Strictness --output-dir $logs --validate $plugin
exit $LASTEXITCODE
