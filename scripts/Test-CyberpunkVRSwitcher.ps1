#requires -Version 5.1
[CmdletBinding()]
param([switch]$Integration, [switch]$ForkIntegration, [string]$ReleaseZip, [string]$CodewareDll, [string]$DependencyGameRoot)
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
function Invoke-FixtureCli {
    # Only disposable fake-game tests may mock the process guard. The shipped
    # switcher has no bypass flag and still refuses a live game/mod manager.
    $cliArgs = @($args)
    $index = [Array]::IndexOf($cliArgs, '-GameRoot')
    if ($index -lt 0) { throw 'Fixture CLI requires an explicit GameRoot' }
    $fixtureRoot = [IO.Path]::GetFullPath($cliArgs[$index + 1])
    if (-not $fixtureRoot.StartsWith($base.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
        (Get-Content -LiteralPath (Join-Path $fixtureRoot 'bin\x64\Cyberpunk2077.exe') -Raw) -notin @('fixture, not executable','base game untouched')) {
        throw 'Refusing process-guard mock outside the disposable test fixture'
    }
    $parameters = @{}
    for ($i = 0; $i -lt $cliArgs.Count; $i++) {
        $name = ([string]$cliArgs[$i]).TrimStart('-')
        if ($name -in @('Yes','WhatIf')) { $parameters[$name] = $true }
        else { $i++; $parameters[$name] = $cliArgs[$i] }
    }
    $payload = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes((ConvertTo-Json -InputObject $parameters -Compress)))
    $quotedCli = $cli.Replace("'", "''")
    $command = "`$ProgressPreference='SilentlyContinue'; function global:Get-Process { param(`$Name, `$ErrorAction) }; " +
        "function global:Get-Item { param(`$LiteralPath) if (`$LiteralPath -eq '$($fixtureRoot.Replace("'","''"))\bin\x64\Cyberpunk2077.exe') { [pscustomobject]@{Attributes=[IO.FileAttributes]::Normal;PSIsContainer=`$false;VersionInfo=[pscustomobject]@{ProductVersion='2.5'}} } else { Microsoft.PowerShell.Management\Get-Item -LiteralPath `$LiteralPath -Force } }; " +
        "`$decoded = ConvertFrom-Json ([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('$payload'))); " +
        "`$fixtureParams=@{}; foreach (`$p in `$decoded.PSObject.Properties) { `$fixtureParams[`$p.Name]=`$p.Value }; " +
        "& '$quotedCli' @fixtureParams"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -InputFormat Text -OutputFormat Text -EncodedCommand ([Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command)))
    $script:LASTEXITCODE = $LASTEXITCODE
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
    $retired = @('red4ext\plugins\CyberpunkVR_Hands\CyberpunkVR_Hands.dll',
        'red4ext\plugins\CyberpunkVR_Stereo\renamed-backup.dll',
        'red4ext\plugins\CyberpunkVR_Stereo\view_dir\retired.dxil',
        'bin\x64\plugins\cyber_engine_tweaks\mods\CyberpunkVRPort_QuickBoot\init.lua',
        'r6\scripts\CyberpunkVRPort_LootUi\old.reds',
        'engine\config\platform\pc\vrcam_cpu_tweaks.ini')
    foreach ($rel in $retired) { Put (Join-Path $game $rel) 'retired port file' }
    Put (Join-Path $game $mod) 'nexus stays'
    Put (Join-Path $game 'bin\x64\dxgi.dll') 'reshade'
    Put (Join-Path $game 'bin\x64\version.dll') 'cheeky alternate loader'
    Put (Join-Path $game 'bin\x64\CheekyFoveatedDLSS\CheekyOverlay.ini') "[Overlay]`nMenuKey=122"
    foreach ($name in @('VRCigarette.archive.xl','vrport_basketball.archive','vrport_mag.archive')) { Put (Join-Path $game "archive\pc\mod\$name") 'old port asset' }
    Put (Join-Path $game 'bin\x64\ReShade.ini') 'settings'
    Put (Join-Path $game 'bin\x64\vrport.ini') "xr_world_scale=1.1`nxr_runtime=1`nxr_force_fov=104.5`nxr_fsr3=1`nxr_mono_submit=1`nxr_deep_diag=1`nfirst_launch=1"
    Add-Content -LiteralPath (Join-Path $game 'bin\x64\vrport.ini') -Value "`nxr_bike_head_offset_z=0.12`nxr_veh_head_offset_z=0.03"
    Put (Join-Path $stage $port) 'upstream'
    Put (Join-Path $stage 'r6\scripts\CyberpunkVRPort_New\new.reds') 'new script'
    Set-MigratedSettings $game $stage
    $ini = Get-Content -LiteralPath (Join-Path $stage 'bin\x64\vrport.ini') -Raw
    Assert ($ini -match 'xr_world_scale=1.1' -and $ini -match 'xr_runtime=1' -and $ini -match 'xr_force_fov=104.5') 'Keep personal fit and FOV override'
    Assert ($ini -match 'xr_bike_head_offset_z=0.12' -and $ini -match 'xr_veh_head_offset_z=0.03') 'Keep independent bike and car fit preferences'
    Assert ($ini -notmatch 'xr_fsr3|xr_mono_submit|xr_deep_diag' -and $ini -match 'first_launch=0' -and $ini -match 'xr_framegen=0') 'Reset experimental tuning, protect game settings, FG off'
    $remove = @(Get-PortFiles $game)
    Assert ($remove -contains $old -and $remove -notcontains 'bin\x64\dxgi.dll' -and $remove -notcontains $mod -and $remove -notcontains 'bin\x64\ReShade.ini') 'Default removal excludes proxy and unrelated mods/settings'
    Assert ($remove -notcontains 'bin\x64\version.dll' -and $remove -notcontains 'bin\x64\CheekyFoveatedDLSS\CheekyOverlay.ini') 'Alternate loader and Cheeky hotkey settings preserved'
    foreach ($name in @('VRCigarette.archive.xl','vrport_basketball.archive','vrport_mag.archive')) { Assert ($remove -contains "archive\pc\mod\$name") "Retire merged 0.1.8 asset $name" }
    Assert (@(Get-PortFiles $game $true $true) -contains 'bin\x64\dxgi.dll') 'Explicit proxy disable selects dxgi only'
    $proxyStage = Join-Path $base 'proxy-stage'
    Put (Join-Path $proxyStage 'bin\x64\dxgi.dll') 'different loader'
    $rejected=$false; try { Assert-ProxyPreservation $game $proxyStage } catch { $rejected=$true }
    Assert $rejected 'Default rejects package overwriting existing dxgi loader'
    Assert-ProxyPreservation $game $proxyStage $true
    Put (Join-Path $proxyStage 'bin\x64\version.dll') 'different alternate loader'
    $rejected=$false; try { Assert-ProxyPreservation $game $proxyStage $true } catch { $rejected=$true }
    Assert $rejected 'Dxgi opt-out never authorizes replacing version loader'
    $backup = Join-Path $base 'backup'
    $journal = Invoke-PortTransaction $game $stage $remove $backup
    Assert ($journal.Status -eq 'Installed') 'Transaction commits'
    foreach ($rel in $retired) {
        Assert (-not (Test-Path -LiteralPath (Join-Path $game $rel)) -and
            (Get-Content -LiteralPath (Join-Path $backup "original\$rel") -Raw) -eq 'retired port file') "Cleanly retire with recoverable backup: $rel"
    }
    Assert ((Get-Content -LiteralPath (Join-Path $game 'bin\x64\dxgi.dll') -Raw) -eq 'reshade') 'Default transaction keeps loader byte-for-byte'
    Assert ((Get-Content -LiteralPath (Join-Path $game 'bin\x64\CheekyFoveatedDLSS\CheekyOverlay.ini') -Raw) -match 'MenuKey=122') 'Default transaction keeps Cheeky F11 configuration'
    $disableStage=Join-Path $base 'disable-stage'; New-Item -ItemType Directory -Path $disableStage | Out-Null
    $disableBackup=Join-Path $base 'disable-backup'
    $null=Invoke-PortTransaction $game $disableStage @('bin\x64\dxgi.dll') $disableBackup
    Assert (-not (Test-Path -LiteralPath (Join-Path $game 'bin\x64\dxgi.dll')) -and
        (Get-Content -LiteralPath (Join-Path $disableBackup 'original\bin\x64\dxgi.dll') -Raw) -eq 'reshade') 'Explicit disable backs up loader before removal'
    Copy-VerifiedFile (Join-Path $disableBackup 'original\bin\x64\dxgi.dll') (Join-Path $game 'bin\x64\dxgi.dll')
    Assert ((Get-Content -LiteralPath (Join-Path $game $port) -Raw) -eq 'upstream' -and -not (Test-Path -LiteralPath (Join-Path $game $old))) 'Replace plugin and remove retired script'
    Assert ((Get-Content -LiteralPath (Join-Path $backup "original\$port") -Raw) -eq 'old port') 'Recoverable original plugin backup'
    Assert ((Get-Content -LiteralPath (Join-Path $game $mod) -Raw) -eq 'nexus stays' -and (Test-Path -LiteralPath (Join-Path $game 'bin\x64\ReShade.ini'))) 'Preserve unrelated mod and ReShade settings'
    # Inject a one-time write failure AFTER writing a staged file; the rollback must repair it.
    $rollbackGame = Join-Path $base 'rollback-game'; $rollbackStage = Join-Path $base 'rollback-stage'
    Put (Join-Path $rollbackGame 'bin\original.txt') 'original'
    Put (Join-Path $rollbackGame 'bin\retired.txt') 'old retired file'
    Put (Join-Path $rollbackStage 'bin\original.txt') 'replacement'
    Put (Join-Path $rollbackStage 'bin\new.txt') 'new'
    $realCopy = ${function:Copy-VerifiedFile}
    $script:failedOnce = $false
    function Copy-VerifiedFile([string]$Source, [string]$Destination) {
        & $realCopy $Source $Destination
        if (-not $script:failedOnce -and $Source.StartsWith($rollbackStage)) { $script:failedOnce = $true; throw 'Injected copy failure' }
    }
    $failed = $false
    try { $null = Invoke-PortTransaction $rollbackGame $rollbackStage @('bin\original.txt','bin\retired.txt') (Join-Path $base 'rollback-backup') } catch { $failed = $true }
    Set-Item -Path function:Copy-VerifiedFile -Value $realCopy
    Assert ($failed -and (Get-Content -LiteralPath (Join-Path $rollbackGame 'bin\original.txt') -Raw) -eq 'original' -and -not (Test-Path -LiteralPath (Join-Path $rollbackGame 'bin\new.txt'))) 'Injected partial-write failure rolls back all changes'
    Assert ((Get-Content -LiteralPath (Join-Path $rollbackGame 'bin\retired.txt') -Raw) -eq 'old retired file') 'Clean-first failure restores removed retired files too'
    # A copy hook simulates a file being corrupted after a verified copy. The
    # final whole-install verification must reject it and restore the baseline.
    $script:corruptedOnce = $false
    function Copy-VerifiedFile([string]$Source, [string]$Destination) {
        & $realCopy $Source $Destination
        if (-not $script:corruptedOnce -and $Source.StartsWith($rollbackStage)) {
            $script:corruptedOnce = $true
            [IO.File]::WriteAllText($Destination, 'corrupted after copy')
        }
    }
    $failed = $false
    try { $null = Invoke-PortTransaction $rollbackGame $rollbackStage @('bin\original.txt','bin\retired.txt') (Join-Path $base 'verification-backup') } catch { $failed = $_ -match 'Installed file verification failed' }
    Set-Item -Path function:Copy-VerifiedFile -Value $realCopy
    Assert ($failed -and (Get-Content -LiteralPath (Join-Path $rollbackGame 'bin\original.txt') -Raw) -eq 'original' -and
        (Get-Content -LiteralPath (Join-Path $rollbackGame 'bin\retired.txt') -Raw) -eq 'old retired file' -and
        -not (Test-Path -LiteralPath (Join-Path $rollbackGame 'bin\new.txt'))) 'Final hash audit rejects corruption and rolls back clean replacement'
    $cli = Join-Path $PSScriptRoot 'Switch-CyberpunkVRPort.ps1'
    $forkZip = Join-Path $base 'fork.zip'
    New-TestZip $forkZip @('red4ext/plugins/CyberpunkVR_Stereo/CyberpunkVR_Stereo.dll')
    Invoke-FixtureCli -Action Prepare -Edition Fork -ForkArchive $forkZip -ForkSha256 ('0' * 64) -GameRoot $game -CacheRoot (Join-Path $base 'fork-cache') -BackupRoot (Join-Path $base 'fork-backups')
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
        if (-not $DependencyGameRoot) { throw 'Integration requires real game-2.5 dependency binaries via -DependencyGameRoot.' }
        foreach ($rel in @('red4ext\RED4ext.dll','bin\x64\plugins\cyber_engine_tweaks.asi',
            'engine\tools\scc.exe','red4ext\plugins\ArchiveXL\ArchiveXL.dll',
            'red4ext\plugins\TweakXL\TweakXL.dll','red4ext\plugins\Codeware\Codeware.dll')) {
            Copy-VerifiedFile (Join-Path $DependencyGameRoot $rel) (Join-Path $fixture $rel)
        }
        if ($Integration) { Copy-VerifiedFile $ReleaseZip (Join-Path $cache 'CyberpunkVRPort-0.1.9.zip') }
        $originalLocal = $env:LOCALAPPDATA
        try {
            $env:LOCALAPPDATA = Join-Path $base 'localappdata'
            $settings = Join-Path $env:LOCALAPPDATA 'CD Projekt Red\Cyberpunk 2077\UserSettings.json'
            Put $settings '{"keep":"my-settings"}'
            $cli = Join-Path $PSScriptRoot 'Switch-CyberpunkVRPort.ps1'
            if ($ForkIntegration) {
                Invoke-FixtureCli -Action Install -Edition Fork -ForkArchive $ReleaseZip -ForkSha256 (Get-FileHash -LiteralPath $ReleaseZip -Algorithm SHA256).Hash -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            } else {
                Invoke-FixtureCli -Action Install -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            }
            Assert ($LASTEXITCODE -eq 0) 'CLI installs selected real ZIP into fixture'
            Assert ((Get-Content -LiteralPath (Join-Path $fixture 'bin\x64\dxgi.dll') -Raw) -eq 'original') 'CLI -Yes preserves dxgi without explicit disable'
            Assert ((Get-Content -LiteralPath $settings -Raw) -eq '{"keep":"my-settings"}') 'Game UserSettings preserved byte-for-byte'
            Assert (-not (Test-Path -LiteralPath (Join-Path $fixture $old)) -and -not (Test-Path -LiteralPath (Join-Path $fixture 'r6\scripts\HUDitor\test.reds'))) 'Retired script and HUDitor disabled'
            # Upgrade must snapshot the current customized state, not force a restore
            # to an older baseline. Then unwind both snapshots in newest-first order.
            $firstJournal = Get-ChildItem -LiteralPath $backups -Filter transaction.json -Recurse -File | Select-Object -First 1
            $firstHash = (Get-FileHash -LiteralPath $firstJournal.FullName).Hash
            Put (Join-Path $fixture $port) 'customized current port'
            if ($ForkIntegration) {
                Invoke-FixtureCli -Action Install -Edition Fork -ForkArchive $ReleaseZip -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            } else {
                Invoke-FixtureCli -Action Install -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            }
            Assert ($LASTEXITCODE -eq 0) 'Upgrade succeeds over an Installed journal'
            Assert ((Get-FileHash -LiteralPath $firstJournal.FullName).Hash -eq $firstHash) 'Upgrade leaves earlier baseline journal untouched'
            Invoke-FixtureCli -Action Restore -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            Assert ($LASTEXITCODE -eq 0 -and (Get-Content -LiteralPath (Join-Path $fixture $port) -Raw) -eq 'customized current port') 'First restore returns to current pre-upgrade state, not original baseline'
            Put (Join-Path $fixture 'r6\scripts\CyberpunkVRPort_Test\generated.reds') 'generated during upstream use'
            Invoke-FixtureCli -Action Restore -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            Assert ($LASTEXITCODE -eq 0) 'CLI restore succeeds'
            Assert ((Get-Content -LiteralPath (Join-Path $fixture $port) -Raw) -eq 'original' -and (Test-Path -LiteralPath (Join-Path $fixture $old)) -and (Test-Path -LiteralPath (Join-Path $fixture 'bin\x64\dxgi.dll'))) 'Restore reinstates old plugin/scripts and ReShade'
            Assert (-not (Test-Path -LiteralPath (Join-Path $fixture 'r6\scripts\CyberpunkVRPort_Test\generated.reds'))) 'Restore removes upstream-generated owned files'
            Assert ((Get-Content -LiteralPath (Join-Path $fixture $mod) -Raw) -eq 'original') 'Unrelated mod survives round trip'
            # Missing Nexus dependency must fail without removing the active port.
            Remove-Item -LiteralPath (Join-Path $fixture 'bin\x64\plugins\cyber_engine_tweaks\mods\VisualHolster\init.lua')
            if ($ForkIntegration) {
                Invoke-FixtureCli -Action Install -Edition Fork -ForkArchive $ReleaseZip -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -DownloadsDir (Join-Path $base 'empty-downloads') -Yes
            } else {
                Invoke-FixtureCli -Action Install -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -DownloadsDir (Join-Path $base 'empty-downloads') -Yes
            }
            Assert ($LASTEXITCODE -ne 0 -and (Get-Content -LiteralPath (Join-Path $fixture $port) -Raw) -eq 'original') 'Missing Nexus file blocks changes and preserves active port'
            Put (Join-Path $fixture 'bin\x64\plugins\cyber_engine_tweaks\mods\VisualHolster\init.lua') 'original'
            if ($ForkIntegration) {
                Invoke-FixtureCli -Action Install -Edition Fork -ForkArchive $ReleaseZip -ForkSha256 ('0' * 64) -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            } else {
                Put (Join-Path $cache 'CyberpunkVRPort-0.1.9.zip') 'corrupted download'
                Invoke-FixtureCli -Action Install -GameRoot $fixture -CacheRoot $cache -BackupRoot $backups -Yes
            }
            Assert ($LASTEXITCODE -ne 0 -and (Get-Content -LiteralPath (Join-Path $fixture $port) -Raw) -eq 'original') 'Bad release checksum blocks changes and preserves active port'
        } finally { $env:LOCALAPPDATA = $originalLocal }
    }
    Write-Host "All $script:passed assertions passed. Fixture retained: $base" -ForegroundColor Green
} catch { Write-Host "Test fixture retained: $base"; throw }
