[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallPath,
    [string]$GamePath = "D:\SteamLibrary\steamapps\common\Cyberpunk 2077",
    [switch]$ReplaceExisting
)

$ErrorActionPreference = "Stop"
$InstallPath = [IO.Path]::GetFullPath($InstallPath)
$GamePath = [IO.Path]::GetFullPath($GamePath)
if (-not (Test-Path -LiteralPath $InstallPath -PathType Container)) { throw "Wabbajack installation not found: $InstallPath" }
if (-not (Test-Path -LiteralPath $GamePath -PathType Container)) { throw "Cyberpunk game directory not found: $GamePath" }

$userData = Join-Path (Split-Path -Parent $InstallPath) "CyberpunkVRPort-PSVR2-UserData"
$destinationRoot = Join-Path $userData "overwrite"
if ($destinationRoot.StartsWith($InstallPath, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to place user state inside Wabbajack's managed installation."
}

$protectedPaths = @(
    "red4ext\plugins\CyberpunkVR_Stereo\vrik_calibration.ini",
    "bin\x64\plugins\cyber_engine_tweaks\mods\CyberpunkVRPort_HUD\hud_layout.ini",
    "bin\x64\plugins\cyber_engine_tweaks\mods\HUDitor\persistency.json",
    "bin\x64\vrport.ini"
)

$copied = 0
$matched = 0
$skipped = 0
foreach ($relativePath in $protectedPaths) {
    $source = Join-Path $GamePath $relativePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        Write-Host "Not present: $relativePath"
        continue
    }

    $destination = Join-Path $destinationRoot $relativePath
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    if (Test-Path -LiteralPath $destination -PathType Leaf) {
        $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        $destinationHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        if ($sourceHash -eq $destinationHash) {
            Write-Host "Already identical: $relativePath"
            $matched++
            continue
        }
        if (-not $ReplaceExisting) {
            Write-Warning "Destination differs; preserving it. Use -ReplaceExisting to copy the manual-game version: $relativePath"
            $skipped++
            continue
        }
        $backup = "$destination.pre-migration-$(Get-Date -Format yyyyMMdd-HHmmss).bak"
        Copy-Item -LiteralPath $destination -Destination $backup
        Write-Host "Backed up destination: $backup"
    }

    Copy-Item -LiteralPath $source -Destination $destination -Force
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $destinationHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    if ($sourceHash -ne $destinationHash) { throw "Post-copy hash mismatch: $relativePath" }
    Write-Host "Copied: $relativePath"
    $copied++
}

Write-Host ""
Write-Host "External user-data root: $userData"
Write-Host "Copied=$copied identical=$matched skipped-conflicts=$skipped"
Write-Host "The source game files were not changed."
