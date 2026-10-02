#requires -Version 5.1
[CmdletBinding()]
param([switch]$Integration, [switch]$ForkIntegration, [string]$ReleaseZip, [string]$CodewareDll)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CyberpunkVR-Switcher.Core.ps1')
$base = Join-Path ([IO.Path]::GetTempPath()) ('cpvr-switch-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $base | Out-Null
$script:passed = 0
function Assert($Condition, [string]$Message) {
    if (-not $Condition) { throw "FAIL: $Message" }
    $script:passed++; Write-Host "PASS: $Message"
}
function Put([string]$Path, [string]$Value) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    [IO.File]::WriteAllText($Path, $Value)
}
function New-TestZip([string]$Path, [string[]]$Entries) {
    $zip = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $Entries) {
            $entry = $zip.CreateEntry($name)
            $writer = New-Object IO.StreamWriter($entry.Open())
            try { $writer.Write('test') } finally { $writer.Dispose() }
        }
    } finally { $zip.Dispose() }
}
try {
    $game = Join-Path $base 'game'; $stage = Join-Path $base 'stage'
    Put (Join-Path $game 'bin\x64\Cyberpunk2077.exe') 'base game untouched'
    foreach ($bad in @('..\escape','bin\..\escape','C:\escape','bin\x.dll:evil','\escape','bin\bad.\x','bin\NUL.txt')) {
        $rejected = $false
        try { $null = Get-SafePath $game $bad } catch { $rejected = $true }
        Assert $rejected "Reject unsafe path $bad"
    }
    foreach ($bad in @('../escape','bin/x64/Cyberpunk2077.exe','evil/payload','bin/x.dll:evil')) {
        $zip = Join-Path $base ([guid]::NewGuid().ToString('N') + '.zip')
        New-TestZip $zip @('bin/good.txt',$bad)
        $dest = Join-Path $base ([guid]::NewGuid().ToString('N'))
        $rejected = $false
        try { Expand-SafeModZip $zip $dest } catch { $rejected = $true }
        Assert ($rejected -and -not (Test-Path -LiteralPath (Join-Path $dest 'bin/good.txt'))) "Whole-archive prevalidation: $bad"
    }
    $zip = Join-Path $base 'duplicate.zip'
    New-TestZip $zip @('bin/Same.txt','bin/same.txt')
    $rejected = $false
    try { Expand-SafeModZip $zip (Join-Path $base 'duplicate-out') } catch { $rejected = $true }
    Assert $rejected 'Reject case-insensitive duplicate ZIP paths'
    $port = 'red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll'
    $old = 'r6\scripts\CyberpunkVRPort_SettingsGuard\old.reds'
    $mod = 'r6\scripts\UnrelatedMod\keep.reds'
    Put (Join-Path $game $port) 'old port'
    Put (Join-Path $game $old) 'old script'
    Put (Join-Path $game $mod) 'nexus stays'
    Put (Join-Path $game 'bin\x64\dxgi.dll') 'reshade'
    Put (Join-Path $game 'bin\x64\ReShade.ini') 'settings'
    Put (Join-Path $game 'bin\x64\vrport.ini') "xr_world_scale=1.1`nxr_runtime=1`nxr_force_fov=104.5`nxr_fsr3=1`nxr_mono_submit=1`nxr_deep_diag=1`nfirst_launch=1"
    Put (Join-Path $stage $port) 'upstream'
    Put (Join-Path $stage 'r6\scripts\CyberpunkVRPort_New\new.reds') 'new script'
    Set-MigratedSettings $game $stage
    $ini = Get-Content -LiteralPath (Join-Path $stage 'bin\x64\vrport.ini') -Raw
    Assert ($ini -match 'xr_world_scale=1.1' -and $ini -match 'xr_runtime=1' -and $ini -match 'xr_force_fov=104.5') 'Keep personal fit and FOV override'
    Assert ($ini -notmatch 'xr_fsr3|xr_mono_submit|xr_deep_diag' -and $ini -match 'first_launch=0' -and $ini -match 'xr_framegen=0') 'Reset experimental tuning, protect game settings, FG off'
    $remove = @(Get-PortFiles $game)
    Assert ($remove -contains $old -and $remove -contains 'bin\x64\dxgi.dll' -and $remove -notcontains $mod -and $remove -notcontains 'bin\x64\ReShade.ini') 'Ownership includes retired scripts and proxy, excludes unrelated mods/settings'
    $backup = Join-Path $base 'backup'
    $journal = Invoke-PortTransaction $game $stage $remove $backup
    Assert ($journal.Status -eq 'Installed') 'Transaction commits'
    Assert ((Get-Content -LiteralPath (Join-Path $game $port) -Raw) -eq 'upstream' -and -not (Test-Path -LiteralPath (Join-Path $game $old))) 'Replace plugin and remove retired script'
    Assert ((Get-Content -LiteralPath (Join-Path $backup "original\$port") -Raw) -eq 'old port') 'Recoverable original plugin backup'
    Assert ((Get-Content -LiteralPath (Join-Path $game $mod) -Raw) -eq 'nexus stays' -and (Test-Path -LiteralPath (Join-Path $game 'bin\x64\ReShade.ini'))) 'Preserve unrelated mod and ReShade settings'
    # Inject a one-time write failure AFTER writing a staged file; the rollback must repair it.
    $rollbackGame = Join-Path $base 'rollback-game'; $rollbackStage = Join-Path $base 'rollback-stage'
    Put (Join-Path $rollbackGame 'bin\original.txt') 'original'
    Put (Join-Path $rollbackStage 'bin\original.txt') 'replacement'
    Put (Join-Path $rollbackStage 'bin\new.txt') 'new'
    $realCopy = ${function:Copy-VerifiedFile}
    $script:failedOnce = $false
    function Copy-VerifiedFile([string]$Source, [string]$Destination) {
        & $realCopy $Source $Destination
        if (-not $script:failedOnce -and $Source.StartsWith($rollbackStage)) { $script:failedOnce = $true; throw 'Injected copy failure' }
    }
    $failed = $false
    try { $null = Invoke-PortTransaction $rollbackGame $rollbackStage @() (Join-Path $base 'rollback-backup') } catch { $failed = $true }
    Set-Item -Path function:Copy-VerifiedFile -Value $realCopy
    Assert ($failed -and (Get-Content -LiteralPath (Join-Path $rollbackGame 'bin\original.txt') -Raw) -eq 'original' -and -not (Test-Path -LiteralPath (Join-Path $rollbackGame 'bin\new.txt'))) 'Injected partial-write failure rolls back all changes'
    $cli = Join-Path $PSScriptRoot 'Switch-CyberpunkVRPort.ps1'
    $forkZip = Join-Path $base 'fork.zip'
    New-TestZip $forkZip @('red4ext/plugins/CyberpunkVR_Stereo/CyberpunkVR_Stereo.dll')
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cli -Action Prepare -Edition Fork -ForkArchive $forkZip -ForkSha256 ('0' * 64) -GameRoot $game -CacheRoot (Join-Path $base 'fork-cache') -BackupRoot (Join-Path $base 'fork-backups')
    Assert ($LASTEXITCODE -ne 0 -and (Get-Content -LiteralPath (Join-Path $game $port) -Raw) -eq 'upstream') 'Fork checksum mismatch blocks before staging or game writes'
    if ($Integration -or $ForkIntegration) {
        if (-not $ReleaseZip -or -not $CodewareDll) { throw 'Integration needs -ReleaseZip and a real -CodewareDll for version checking.' }
        $fixture = Join-Path $base 'integration-game'
        $cache = Join-Path $base 'integration-cache'; $backups = Join-Path $base 'integration-backups'
        Put (Join-Path $fixture 'bin\x64\Cyberpunk2077.exe') 'fixture, not executable'
        foreach ($rel in @('red4ext\RED4ext.dll','bin\x64\winmm.dll','bin\x64\plugins\cyber_engine_tweaks.asi',
            'engine\tools\scc.exe','engine\config\base\scripts.ini','red4ext\plugins\ArchiveXL\ArchiveXL.dll',
            'red4ext\plugins\TweakXL\TweakXL.dll','red4ext\plugins\input_loader\input_loader.dll',
            'r6\scripts\EquipmentEx\test.reds','archive\pc\mod\EquipmentEx.archive',
            'bin\x64\plugins\cyber_engine_tweaks\mods\VisualHolster\init.lua',
            'r6\tweaks\BulletProjectileOverhaul\Items.Preset_Ajax_Default.yaml','r6\scripts\NovaOptics\test.reds',
            'bin\x64\plugins\cyber_engine_tweaks\mods\NovaOptics\init.lua','r6\scripts\MoreOccluders.reds',
            'bin\x64\dxgi.dll','bin\x64\ReShade.ini','r6\scripts\HUDitor\test.reds',$port,$old,$mod)) { Put (Join-Path $fixture $rel) 'original' }
        Copy-VerifiedFile $CodewareDll (Join-Path $fixture 'red4ext\plugins\Codeware\Codeware.dll')
        if ($Integration) { Copy-VerifiedFile $ReleaseZip (Join-Path $cache 'CyberpunkVRPort-0.1.7.zip') }
        $originalLocal = $env:LOCALAPPDATA
        try {
            $env:LOCALAPPDATA = Join-Path $base 'localappdata'
            $settings = Join-Path $env:LOCALAPPDATA 'CD Projekt Red\Cyberpunk 2077\UserSettings.json'
            Put $settings '{"keep":"my-settings"}'
            $cli = Join-Path $PSScriptRoot 'Switch-CyberpunkVRPort.ps1'
            if ($ForkIntegration) {
                & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cli -Action Install -Edition Fork -ForkArchive $ReleaseZip -ForkSha256 (Get-FileHash -LiteralPath $ReleaseZip -Algorithm SHA256).Hash -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            } else {
                & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cli -Action Install -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            }
            Assert ($LASTEXITCODE -eq 0) 'CLI installs selected real ZIP into fixture'
            Assert ((Get-Content -LiteralPath $settings -Raw) -eq '{"keep":"my-settings"}') 'Game UserSettings preserved byte-for-byte'
            Assert (-not (Test-Path -LiteralPath (Join-Path $fixture $old)) -and -not (Test-Path -LiteralPath (Join-Path $fixture 'r6\scripts\HUDitor\test.reds'))) 'Retired script and HUDitor disabled'
            Put (Join-Path $fixture 'r6\scripts\CyberpunkVRPort_Test\generated.reds') 'generated during upstream use'
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cli -Action Restore -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            Assert ($LASTEXITCODE -eq 0) 'CLI restore succeeds'
            Assert ((Get-Content -LiteralPath (Join-Path $fixture $port) -Raw) -eq 'original' -and (Test-Path -LiteralPath (Join-Path $fixture $old)) -and (Test-Path -LiteralPath (Join-Path $fixture 'bin\x64\dxgi.dll'))) 'Restore reinstates old plugin/scripts and ReShade'
            Assert (-not (Test-Path -LiteralPath (Join-Path $fixture 'r6\scripts\CyberpunkVRPort_Test\generated.reds'))) 'Restore removes upstream-generated owned files'
            Assert ((Get-Content -LiteralPath (Join-Path $fixture $mod) -Raw) -eq 'original') 'Unrelated mod survives round trip'
            # Missing Nexus dependency must fail without removing the active port.
            Remove-Item -LiteralPath (Join-Path $fixture 'bin\x64\plugins\cyber_engine_tweaks\mods\VisualHolster\init.lua')
            if ($ForkIntegration) {
                & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cli -Action Install -Edition Fork -ForkArchive $ReleaseZip -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -DownloadsDir (Join-Path $base 'empty-downloads') -Yes
            } else {
                & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cli -Action Install -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -DownloadsDir (Join-Path $base 'empty-downloads') -Yes
            }
            Assert ($LASTEXITCODE -ne 0 -and (Get-Content -LiteralPath (Join-Path $fixture $port) -Raw) -eq 'original') 'Missing Nexus file blocks changes and preserves active port'
            Put (Join-Path $fixture 'bin\x64\plugins\cyber_engine_tweaks\mods\VisualHolster\init.lua') 'original'
            if ($ForkIntegration) {
                & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cli -Action Install -Edition Fork -ForkArchive $ReleaseZip -ForkSha256 ('0' * 64) -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            } else {
                Put (Join-Path $cache 'CyberpunkVRPort-0.1.7.zip') 'corrupted download'
                & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cli -Action Install -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            }
            Assert ($LASTEXITCODE -ne 0 -and (Get-Content -LiteralPath (Join-Path $fixture $port) -Raw) -eq 'original') 'Bad release checksum blocks changes and preserves active port'
        } finally { $env:LOCALAPPDATA = $originalLocal }
    }
    Write-Host "All $script:passed assertions passed. Fixture retained: $base" -ForegroundColor Green
} catch { Write-Host "Test fixture retained: $base"; throw }
