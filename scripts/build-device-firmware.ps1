[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Toolchain
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vendor = Join-Path $root 'vendor/m-vave-fm1'
$sdkBuild = Join-Path $vendor 'sdk/Build.ps1'

if (!(Test-Path -LiteralPath $sdkBuild)) {
    throw 'The pinned M-VAVE source submodule is missing. Initialize vendor/m-vave-fm1 first.'
}
if (!(Test-Path -LiteralPath $Toolchain)) {
    throw 'The matching JieLi toolchain directory does not exist.'
}
# The pinned JieLi clang driver does not automatically add its target C
# library headers to the search path. The FM1 SDK Build.ps1 supplies its own
# project includes, so expose the matching PI32 C headers through Clang's
# documented CPATH environment variable without editing upstream SDK files.
$systemInclude = Join-Path $Toolchain 'q32s-include'
if (!(Test-Path -LiteralPath (Join-Path $systemInclude 'ctype.h') -PathType Leaf)) {
    throw "JieLi PI32 C library headers are missing: $systemInclude"
}
if ([string]::IsNullOrWhiteSpace($env:CPATH)) {
    $env:CPATH = $systemInclude
} else {
    $env:CPATH = $systemInclude + ';' + $env:CPATH
}
Write-Host "Clang target C headers: $systemInclude"
# Stage the same FM1-Lab C modules and source transformations used by CI's
# official simulator build. The SDK recursively compiles every app/*.c unit.
$files = @(
    'fm1lab_instrument.h',
    'fm1lab_instrument.c',
    'fm1lab_library.h',
    'fm1lab_library.c',
    'fm1lab_runtime.h',
    'fm1lab_runtime.c',
    'fm1lab_sequencer.h',
    'fm1lab_sequencer.c',
    'fm1lab_voice_editor.h',
    'fm1lab_voice_editor.c',
    'fm1lab_bank_metadata.h',
    'fm1lab_bank_metadata.c',
    'fm1lab_physical_controls.h',
    'fm1lab_audio_engine.h',
    'fm1lab_audio_engine.c',
    'fm1lab_audio_bridge.h',
    'fm1lab_audio_bridge.c'
)
$app = Join-Path $vendor 'app'
foreach ($file in $files) {
    Copy-Item -LiteralPath (Join-Path $root "firmware/$file") -Destination $app -Force
}
node (Join-Path $root 'scripts/patch-m-vave-device-dx7-core.mjs')
if ($LASTEXITCODE -ne 0) { throw 'FM6 source staging failed.' }

Push-Location $root
try {
    node (Join-Path $root 'scripts/patch-m-vave-instrument-storage.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Instrument storage integration patch failed.' }
    node (Join-Path $root 'scripts/patch-m-vave-two-track-sequencer.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Native two-track sequencer engine patch failed.' }
    node (Join-Path $root 'scripts/patch-m-vave-physical-controls.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Physical control naming patch failed.' }
    node (Join-Path $root 'scripts/patch-m-vave-instrument-home.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Instrument home screen patch failed.' }
    node (Join-Path $root 'scripts/patch-m-vave-instrument-sequencer-screen.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Native sequencer screen patch failed.' }
    node (Join-Path $root 'scripts/patch-m-vave-native-patch-editor.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Native patch editor screen patch failed.' }
    node (Join-Path $root 'scripts/patch-m-vave-instrument-macros.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Instrument macro screen patch failed.' }
    node (Join-Path $root 'scripts/patch-m-vave-audio-mixer.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Native FM6 audio integration patch failed.' }
} finally {
    Pop-Location
}

& $sdkBuild -FirmwareOnly -Toolchain $Toolchain
if ($LASTEXITCODE -ne 0) { throw "M-VAVE device firmware build failed ($LASTEXITCODE)." }
