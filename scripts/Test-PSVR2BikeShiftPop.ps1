[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ModRoot,
    [Parameter(Mandatory = $true)][string]$OriginalScript
)
$ErrorActionPreference = 'Stop'
$installer = Join-Path $PSScriptRoot 'Install-PSVR2BikeShiftPop.ps1'
$mod = $ModRoot
$root = Join-Path (Split-Path $PSScriptRoot -Parent) ('build\bike-routing-validation\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
# Mock the running-game guard only in this fixture process. All writes target copies.
function Get-Process { param($Name, $ErrorAction) }
foreach ($case in @('fresh', 'upgrade')) {
    $dir = Join-Path $root $case
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $source = if ($case -eq 'fresh') { $OriginalScript } else { Join-Path $mod 'init.lua' }
    $target = Join-Path $dir 'init.lua'
    Copy-Item -LiteralPath $source -Destination $target
    $before = (Get-FileHash -LiteralPath $target).Hash
    & $installer -ModRoot $dir -CheckOnly
    if ((Get-FileHash -LiteralPath $target).Hash -ne $before) { throw "$case CheckOnly mutated source" }
    & $installer -ModRoot $dir
    $patched = [IO.File]::ReadAllText($target)
    foreach ($marker in @('CPVRPORT_PSVR2_BIKE_BASELINE_SWAP_V2', 'CPVRPORT_PSVR2_BIKE_SHIFT_POP')) {
        if ([regex]::Matches($patched, $marker).Count -ne 1) { throw "$case missing/duplicate $marker" }
    }
    $after = (Get-FileHash -LiteralPath $target).Hash
    & $installer -ModRoot $dir
    if ((Get-FileHash -LiteralPath $target).Hash -ne $after) { throw "$case installer is not idempotent" }
    Write-Output "PASS $case : $target"
}
