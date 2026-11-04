[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Toolchain,
    [Parameter(Mandatory = $true)]
    [string]$SourceUrl
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
if ([string]::IsNullOrWhiteSpace($SourceUrl) -or $SourceUrl -notmatch '^https://') {
    throw 'SourceUrl must be the HTTPS URL for the matching M-VAVE source archive.'
}

# Stage the same FM1-Lab C modules and source transformations used by CI's
# official simulator build. The SDK recursively compiles every app/*.c unit.
$files = @(
    'fm1lab_instrument.h',
    'fm1lab_instrument.c',
    'fm1lab_bank_metadata.h',
    'fm1lab_bank_metadata.c',
    'fm1lab_physical_controls.h'
)
$app = Join-Path $vendor 'app'
foreach ($file in $files) {
    Copy-Item -LiteralPath (Join-Path $root "firmware/$file") -Destination $app -Force
}

Push-Location $root
try {
    node (Join-Path $root 'scripts/patch-m-vave-instrument-storage.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Instrument storage integration patch failed.' }
    node (Join-Path $root 'scripts/patch-m-vave-physical-controls.mjs')
    if ($LASTEXITCODE -ne 0) { throw 'Physical control naming patch failed.' }
} finally {
    Pop-Location
}

& $sdkBuild -FirmwareOnly -Toolchain $Toolchain -SourceUrl $SourceUrl
if ($LASTEXITCODE -ne 0) { throw "M-VAVE device firmware build failed ($LASTEXITCODE)." }
