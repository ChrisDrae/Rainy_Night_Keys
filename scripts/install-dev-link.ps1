<#
.SYNOPSIS
    Makes the freshly built "Keys.vst3" visible to Ableton Live (and every other VST3 host).

.DESCRIPTION
    Hosts on Windows look for VST3 plugins in  C:\Program Files\Common Files\VST3 .
    That folder is write-protected, so instead of copying after every build (which would need
    admin rights every time) this script creates a *directory junction* once:

        C:\Program Files\Common Files\VST3\Keys.vst3
            -> <repo>\build\vs2026\Keys_artefacts\<Configuration>\VST3\Keys.vst3

    A junction is a folder-level redirect. After this, every rebuild is picked up automatically:
    rebuild, then in Live: Preferences > Plug-Ins > Rescan (hold Alt while clicking for a deep rescan).

    IMPORTANT: Windows locks a DLL while a program has it loaded. Close Live (or at least remove
    every Keys instance and wait a moment) before rebuilding, or the linker will fail
    with "cannot open file ... Keys.vst3".

.EXAMPLE
    # From an *elevated* PowerShell (right-click > Run as Administrator), in the repo folder:
    powershell -ExecutionPolicy Bypass -File scripts\install-dev-link.ps1
    powershell -ExecutionPolicy Bypass -File scripts\install-dev-link.ps1 -Configuration Debug
#>
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$repo    = Split-Path -Parent $PSScriptRoot
$source  = Join-Path $repo "build\vs2026\Keys_artefacts\$Configuration\VST3\Keys.vst3"
$vst3Dir = Join-Path $env:CommonProgramW6432 'VST3'    # always the 64-bit "C:\Program Files\Common Files"
$link    = Join-Path $vst3Dir 'Keys.vst3'

$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Please run this script from an elevated PowerShell (Run as Administrator).'
}

if (-not (Test-Path $source)) {
    throw "Build output not found:`n  $source`nBuild first:  cmake --build --preset $($Configuration.ToLower())"
}

if (-not (Test-Path $vst3Dir)) {
    New-Item -ItemType Directory -Path $vst3Dir | Out-Null
}

if (Test-Path $link) {
    $item = Get-Item $link -Force
    if ($item.LinkType -eq 'Junction') {
        # Delete only the junction itself (non-recursive) - never the build output it points to.
        [System.IO.Directory]::Delete($link, $false)
        Write-Host "Removed previous junction: $link"
    }
    else {
        throw "$link exists and is a real folder (an old copy?). Delete it manually, then re-run."
    }
}

New-Item -ItemType Junction -Path $link -Target $source | Out-Null
Write-Host "Linked:`n  $link`n  -> $source"
Write-Host 'Now rescan plug-ins in Ableton Live (Preferences > Plug-Ins > Rescan).'
