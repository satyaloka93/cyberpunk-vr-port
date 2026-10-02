param(
    [Parameter(Mandatory = $true)][string]$ModRoot,
    [switch]$CheckOnly
)

$ErrorActionPreference = 'Stop'
$initPath = Join-Path $ModRoot 'init.lua'
$helperPath = Join-Path $ModRoot 'utils\PSVR2AutoFireGrip.lua'
$sourcePath = Join-Path $PSScriptRoot '..\patches\dualsense\PSVR2AutoFireGrip.lua'
if (-not (Test-Path -LiteralPath $initPath -PathType Leaf)) { throw "Missing mod init.lua: $initPath" }
if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) { throw "Missing helper: $sourcePath" }

$source = [System.IO.File]::ReadAllText($initPath)
if ($source.Contains('PSVR2FastAutoTrigger')) { throw 'Failed Machine-mode patch still active; restore it first' }
$loadAnchor = "GetFrequency = require('utils/GetFrequency')"
$callAnchor = '    local weaponObj = weapon(data, weaponName, isAiming, weaponState, isTimeDilated, triggerType, isWeaponGlitched, attackSpeed, config, isPerfectCharged, usingWeapon, itemName, isSecondaryMode)'
$loadLine = "PSVR2AutoFireGrip = require('utils/PSVR2AutoFireGrip')"
$callLine = '    PSVR2AutoFireGrip(weaponObj, weaponType, weaponState, triggerType, cycleTime > 0 and 1 / cycleTime or 0, weaponName, delta, isMeleeWeapon, isCyberwareWeapon, config.enableMod and config.gunsRT, type(SetVRHapticPulse) == "function" and SetVRHapticPulse or nil)'
$oldCallLine = '    PSVR2AutoFireGrip(weaponObj, weaponType, weaponState, triggerType, cycleTime > 0 and 1 / cycleTime or 0, weaponName, delta, isMeleeWeapon, isCyberwareWeapon, type(SetVRHapticPulse) == "function" and SetVRHapticPulse or nil)'

function Assert-Once([string]$Text, [string]$Needle) {
    $first = $Text.IndexOf($Needle, [StringComparison]::Ordinal)
    if ($first -lt 0 -or $Text.IndexOf($Needle, $first + $Needle.Length, [StringComparison]::Ordinal) -ge 0) {
        throw "Expected exactly one patch anchor: $Needle"
    }
}

if ($source.Contains($oldCallLine)) {
    Assert-Once $source $loadLine
    Assert-Once $source $oldCallLine
    if (-not (Test-Path -LiteralPath $helperPath -PathType Leaf)) { throw 'Existing grip helper missing' }
    if ($CheckOnly) { Write-Output 'Preflight passed for gun-effects setting gate update.'; return }
    if (Get-Process -Name Cyberpunk2077 -ErrorAction SilentlyContinue) {
        throw 'Cyberpunk2077 is running; close it before changing CET files'
    }
    $backup = "$initPath.before-psvr2-grip-gate-$(Get-Date -Format 'yyyyMMdd-HHmmss').bak"
    Copy-Item -LiteralPath $initPath -Destination $backup -ErrorAction Stop
    Copy-Item -LiteralPath $helperPath -Destination "$helperPath.before-gate.bak" -ErrorAction Stop
    $patched = $source.Replace($oldCallLine, $callLine)
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($initPath, $patched, $utf8NoBom)
    Copy-Item -LiteralPath $sourcePath -Destination $helperPath -Force -ErrorAction Stop
    Write-Output "Updated grip-only helper and setting gate; init backup=$backup"
    return
}

if ($source.Contains($loadLine) -or $source.Contains($callLine)) {
    if (-not ($source.Contains($loadLine) -and $source.Contains($callLine))) {
        throw 'Partial grip-pulse installation; refusing overwrite'
    }
    if (-not (Test-Path -LiteralPath $helperPath -PathType Leaf)) { throw 'Loader installed but helper missing' }
    if ((Get-FileHash -LiteralPath $helperPath -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash) {
        throw 'Installed helper differs from source; refusing overwrite'
    }
    Write-Output 'Already installed with matching helper.'
    return
}

Assert-Once $source $loadAnchor
Assert-Once $source $callAnchor
if (Test-Path -LiteralPath $helperPath) { throw "Conflicting helper already exists: $helperPath" }
if ($CheckOnly) { Write-Output 'Preflight passed; no files changed.'; return }
if (Get-Process -Name Cyberpunk2077 -ErrorAction SilentlyContinue) {
    throw 'Cyberpunk2077 is running; close it before changing CET files'
}

$newline = if ($source.Contains("`r`n")) { "`r`n" } else { "`n" }
$patched = $source.Replace($loadAnchor, $loadAnchor + $newline + $loadLine)
$patched = $patched.Replace($callAnchor, $callAnchor + $newline + $callLine)
$backup = "$initPath.before-psvr2-grip-$(Get-Date -Format 'yyyyMMdd-HHmmss').bak"
Copy-Item -LiteralPath $initPath -Destination $backup -ErrorAction Stop
Copy-Item -LiteralPath $sourcePath -Destination $helperPath -ErrorAction Stop
try {
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($initPath, $patched, $utf8NoBom)
} catch {
    Copy-Item -LiteralPath $backup -Destination $initPath -Force
    throw
}
Write-Output "Installed grip-only auto-fire pulses; init backup=$backup"
Write-Output "Helper SHA256=$((Get-FileHash -LiteralPath $helperPath -Algorithm SHA256).Hash)"
