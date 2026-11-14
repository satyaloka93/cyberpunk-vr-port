param([Parameter(Mandatory=$true)][string]$ReleaseZip, [ValidateSet('Upstream','Fork')][string]$Target='Upstream')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CyberpunkVR-Switcher.Core.ps1')
. (Join-Path $PSScriptRoot 'Loader-Identity.ps1')
$repo=Split-Path -Parent $PSScriptRoot
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'Switch-CyberpunkVRPort.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw $errors[0]}
foreach($name in @('Get-EditionDependencies','New-ReleaseStage','Restore-Previous')) {
 $fn=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true)
 Invoke-Expression $fn.Extent.Text
}
function Assert($value,$label){if(-not $value){throw "FAIL: $label"};Write-Host "PASS: $label"}
function Put($path,$text){New-Item -ItemType Directory (Split-Path -Parent $path) -Force|Out-Null;[IO.File]::WriteAllText($path,$text)}
$base=Join-Path $env:TEMP ('cpvr-0245-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $base 'game';$CacheRoot=Join-Path $base 'cache';$BackupRoot=Join-Path $base 'backups'
$modern='red4ext\plugins\cyberpunkvrport';$legacy='red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll'
$Edition='Upstream';$dependencies=@(Get-EditionDependencies)
Assert ($dependencies.Count -eq 1 -and $dependencies[0].id -eq 'red4ext' -and $dependencies[0].minimumVersion -eq '1.31.0') 'Upstream requires only RED4ext 1.31.0'
$Edition='Fork';Assert (@(Get-EditionDependencies).Count -eq 1) 'Modern fork requires RED4ext only'
$Edition='Vrcam';Assert (@(Get-EditionDependencies).Count -gt 1) 'VRCAM retains independent dependencies'
$Edition=$Target;$PatchMoreOccluders=$false;$MoreOccludersArchive=$null;$release=[pscustomobject]@{name='fixture'}
if($Target -eq 'Fork') { $expected=(Get-Content (Join-Path $repo 'wabbajack\release-selection.json') -Raw|ConvertFrom-Json).fork.sha256 } else { $expected='64b23fb8f211cb5c3df896676a7d9c8ebf6e70d3affaa4f280ffd39d9cfab6cf' }
Assert ((Get-FileHash $ReleaseZip).Hash -eq $expected) 'Exact published-candidate archive hash'
function Test-Dependency {param($Mod,$Root) return $true}
function Get-VerifiedArchive {param($Mod) return $ReleaseZip}
function Confirm-Change {param($Prompt) return $true}
function Assert-GameClosed {} # Fixture-only, never call the main script against a real game.
Put "$root\$legacy" 'old-port'
Put "$root\red4ext\plugins\CyberpunkVR_Stereo\ofxr-v557\CyberpunkVR_OFXR.dll" 'patched-scanner-backend'
Put "$root\bin\x64\vrport.ini" "xr_haptics=0`nxr_world_scale=1.1`nxr_framegen=1`nfirst_launch=1"
Put "$root\bin\x64\vrport-launcher.ini" 'launcher-personal'
Put "$root\red4ext\plugins\CyberpunkVR_Stereo\vrik_calibration.ini" 'calibration-personal'
Put "$root\bin\x64\plugins\cyber_engine_tweaks\mods\CyberpunkVRPort_VRIK\vrik_settings.ini" 'ik-personal'
Put "$root\bin\x64\plugins\cyber_engine_tweaks\mods\CyberpunkVRPort_Weapon\init.lua" 'old-lua-never-migrate'
Put "$root\r6\scripts\CyberpunkVRPort_Test\old.reds" 'retired'
Put "$root\engine\config\platform\pc\vrcam_cpu_tweaks.ini" 'retired-tuning'
foreach($path in @('bin\x64\dxgi.dll','bin\x64\version.dll','bin\x64\ReShade.ini','red4ext\plugins\Codeware\Codeware.dll','r6\scripts\OtherMod\keep.reds','r6\scripts\MoreOccluders.reds')){Put "$root\$path" 'keep-user-file'}
$stage=New-ReleaseStage $root
Assert (-not(Test-Path "$stage\bin\x64\vrport.ini")) 'No obsolete config created'
foreach($part in @('dxc\dxcompiler.dll','dxc\dxil.dll','redscript\scc_lib.dll')){Assert (Test-Path "$stage\$modern\$part") "Required bundled component $part"}
$ini=Get-Content "$stage\$modern\config\vrport.ini"
Assert ($ini -contains 'xr_haptics=0' -and $ini -contains 'xr_world_scale=1.1' -and $ini -contains 'xr_framegen=0' -and $ini -contains 'first_launch=0') 'Settings migrate and unsafe defaults reset'
if($Target -eq 'Fork') { foreach($setting in @('xr_framegen_backend=3','xr_framegen_quality=1','xr_framegen_flow_scale=50','xr_framegen_pace=1','xr_framegen_steamvr_optimizations=1','xr_framegen_flight_recorder=0','xr_framegen_overlay=0')) { Assert ($ini -contains $setting) "RC1 default $setting" } }
Assert ((Get-Content "$stage\$modern\config\vrik_calibration.ini") -eq 'calibration-personal') 'Calibration moves to new config'
Assert ((Get-Content "$stage\$modern\lua\CyberpunkVRPort_VRIK\vrik_settings.ini") -eq 'ik-personal') 'Lua user data moves to new host'
Assert ((Get-Content "$stage\$modern\lua\CyberpunkVRPort_Weapon\init.lua" -Raw) -ne 'old-lua-never-migrate') 'New Lua code retained'
$remove=@(Get-PortFiles $root $true $false);$backup=Join-Path $BackupRoot 'switch-upstream-fixture'
$null=Invoke-PortTransaction $root $stage $remove $backup
Assert ((Test-Path "$root\$modern\cyberpunkvrport.dll") -and -not(Test-Path "$root\$legacy") -and -not(Test-Path "$root\engine\config\platform\pc\vrcam_cpu_tweaks.ini")) 'Clean old-to-new install'
foreach($path in @('bin\x64\dxgi.dll','bin\x64\version.dll','bin\x64\ReShade.ini','red4ext\plugins\Codeware\Codeware.dll','r6\scripts\OtherMod\keep.reds','r6\scripts\MoreOccluders.reds')){Assert ((Get-Content "$root\$path") -eq 'keep-user-file') "Preserved $path"}
# Reverse edition migration stages only a legacy DLL plus migrated preferences.
$backStage=Join-Path $base 'back-stage';Put "$backStage\$legacy" 'fork'
Set-MigratedSettings $root $backStage
Assert ((Get-Content "$backStage\bin\x64\vrport.ini") -contains 'xr_world_scale=1.1') '0.2.45 settings migrate back to old layout'
Assert ((Get-Content "$backStage\red4ext\plugins\CyberpunkVR_Stereo\vrik_calibration.ini") -eq 'calibration-personal') 'Calibration migrates back'
Assert (@(Get-PortFiles $root $false $false) -contains "$modern\cyberpunkvrport.dll") 'New plugin is retired when switching back'
Put "$root\$modern\log\generated.log" 'generated'
$RestoreFrom=$backup;$WhatIf=$false;$settingsFile=''
Restore-Previous $root
Assert ((Get-Content "$root\$legacy") -eq 'old-port' -and -not(Test-Path "$root\$modern\cyberpunkvrport.dll") -and -not(Test-Path "$root\$modern\log\generated.log")) 'Restore returns exact previous plugin and removes generated new-layout files'
Assert ((Get-Content "$root\red4ext\plugins\CyberpunkVR_Stereo\ofxr-v557\CyberpunkVR_OFXR.dll") -eq 'patched-scanner-backend') 'Restore recovers locally patched OFXR backend'
Assert ((Get-Content "$root\engine\config\platform\pc\vrcam_cpu_tweaks.ini") -eq 'retired-tuning') 'Restore recovers old tuning'
# Current RC1 uses the same layout as upstream. Preserve a local hotfix exactly on restore.
$root=Join-Path $base 'modern-game'
Put "$root\$modern\cyberpunkvrport.dll" 'locally-patched-scanner-dll'
Put "$root\$modern\config\vrport.ini" "xr_haptics=1`nxr_world_scale=1.15`nxr_framegen=1`nfirst_launch=1"
Put "$root\$modern\config\vrik_calibration.ini" 'modern-calibration'
Put "$root\$modern\lua\CyberpunkVRPort_Weapon\init.lua" 'old-modern-code'
Put "$root\$modern\obsolete-local-file.txt" 'old-private-file'
$stage=New-ReleaseStage $root
Assert ((Get-Content "$stage\$modern\config\vrport.ini") -contains 'xr_world_scale=1.15') 'Modern-to-modern preferences migrate'
$remove=@(Get-PortFiles $root $true $false);$backup=Join-Path $BackupRoot 'switch-upstream-modern-fixture'
$null=Invoke-PortTransaction $root $stage $remove $backup
Assert (-not(Test-Path "$root\$modern\obsolete-local-file.txt")) 'Modern install removes stale port files'
$RestoreFrom=$backup
Restore-Previous $root
Assert ((Get-Content "$root\$modern\cyberpunkvrport.dll") -eq 'locally-patched-scanner-dll') 'Restore recovers exact local scanner hotfix'
Assert ((Get-Content "$root\$modern\config\vrik_calibration.ini") -eq 'modern-calibration') 'Restore recovers modern calibration'
Assert ((Get-Content "$root\$modern\config\vrport.ini") -contains 'xr_framegen=1') 'Restore recovers exact previous FG setting'
Write-Host "All 0.2.45 tests passed; fixture: $base"
