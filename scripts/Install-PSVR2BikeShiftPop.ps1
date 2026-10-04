[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ModRoot,
    [switch]$CheckOnly
)

$ErrorActionPreference = 'Stop'
$initPath = Join-Path $ModRoot 'init.lua'
if (-not (Test-Path -LiteralPath $initPath)) { throw "Enhanced DualSense Support init.lua not found: $initPath" }
if (Get-Process -Name Cyberpunk2077 -ErrorAction SilentlyContinue) {
    throw 'Close Cyberpunk 2077 before changing the CET mod script.'
}

$source = [IO.File]::ReadAllText($initPath)
$updated = $source
$newline = [Environment]::NewLine
$shiftMarker = 'CPVRPORT_PSVR2_BIKE_SHIFT_POP'
$baselineMarker = 'CPVRPORT_PSVR2_BIKE_BASELINE_SWAP_V2'

# Move the entire old L2 resistance -> shift pulse -> resistance sequence to R2.
# Only removing the L2 pulse leaves R2 as Machine -> Machine, hiding the shift.
if (-not $updated.Contains($baselineMarker)) {
    $anchors = [regex]::Matches($updated, '(?m)^(?<indent>[ \t]*)if \(vehData\) then(?<cr>\r?)$')
    if ($anchors.Count -ne 1) { throw "Expected one vehicle baseline branch; found $($anchors.Count). No file changed." }
    $indent = $anchors[0].Groups['indent'].Value
    $replacement = @(
        "${indent}if (vehData) then"
        "${indent}    if (vehicleType == 'vehicleBikeBaseObject') then -- $baselineMarker"
        "${indent}        vehData.leftTriggerType, vehData.rightTriggerType = vehData.rightTriggerType, vehData.leftTriggerType"
        "${indent}        vehData.leftForceTrigger, vehData.rightForceTrigger = vehData.rightForceTrigger, vehData.leftForceTrigger"
        "${indent}    end"
    ) -join $newline
    $replacement += $anchors[0].Groups['cr'].Value
    $updated = $updated.Replace($anchors[0].Value, $replacement)
}

if (-not $updated.Contains($shiftMarker)) {
    $call = 'data = GearChangeModeList[vehicleGearChangeModeList[chosenGearMode]](data, false, config)'
    $anchors = [regex]::Matches($updated, '(?m)^(?<indent>[ \t]*)' + [regex]::Escape($call) + '(?<cr>\r?)$')
    if ($anchors.Count -ne 1) { throw "Expected one gear-change call; found $($anchors.Count). No file changed." }
    $indent = $anchors[0].Groups['indent'].Value
    $replacement = @(
        "${indent}local cpvrportIsBike = vehicleType == 'vehicleBikeBaseObject' -- $shiftMarker"
        "${indent}local cpvrportBikeLeftType = data.leftTriggerType"
        "${indent}local cpvrportBikeLeftForce = data.leftForceTrigger"
        "${indent}$call"
        "${indent}if (cpvrportIsBike) then"
        "${indent}    data.leftTriggerType = cpvrportBikeLeftType"
        "${indent}    data.leftForceTrigger = cpvrportBikeLeftForce"
        "${indent}end"
    ) -join $newline
    $replacement += $anchors[0].Groups['cr'].Value
    $updated = $updated.Replace($anchors[0].Value, $replacement)
}

if ($updated -ceq $source) { Write-Output 'Bike R2 resistance/shift sequence is already installed.'; exit 0 }
if ($CheckOnly) { Write-Output 'Validated bike baseline and shift routing upgrade; no files changed.'; exit 0 }

$backup = "$initPath.before-psvr2-bike-shift-v2-$(Get-Date -Format 'yyyyMMdd-HHmmss-fff').bak"
Copy-Item -LiteralPath $initPath -Destination $backup
try {
    [IO.File]::WriteAllText($initPath, $updated, [Text.UTF8Encoding]::new($false))
} catch {
    Copy-Item -LiteralPath $backup -Destination $initPath -Force
    throw
}
Write-Output "Installed bike R2 resistance/shift sequence; L2 carries the continuous engine profile. Backup: $backup"
