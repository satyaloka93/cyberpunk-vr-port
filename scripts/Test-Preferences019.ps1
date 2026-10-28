$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CyberpunkVR-Switcher.Core.ps1')
$root=Join-Path $env:TEMP ('cpvr-preferences-'+[guid]::NewGuid().ToString('N'))
$game=Join-Path $root 'game';$stage=Join-Path $root 'stage'
New-Item -ItemType Directory "$game\bin\x64" -Force|Out-Null
@('xr_psvr2_haptic_backend=0','xr_haptics=0','xr_haptic_scale=0.75','xr_wheel_grab=0','xr_weapon_laser_dot=1','xr_veh_head_offset_y=0.1','xr_bike_head_offset_y=0.2','xr_framegen=1','first_launch=1')|Set-Content "$game\bin\x64\vrport.ini"
Set-MigratedSettings $game $stage
$lines=Get-Content "$stage\bin\x64\vrport.ini"
foreach($v in @('xr_psvr2_haptic_backend=0','xr_haptics=0','xr_haptic_scale=0.75','xr_wheel_grab=0','xr_weapon_laser_dot=1','xr_veh_head_offset_y=0.1','xr_bike_head_offset_y=0.2','xr_framegen=0','first_launch=0')){
 if($lines -notcontains $v){throw "Lost preference/default $v"}
}
if($lines -contains 'xr_framegen=1' -or $lines -contains 'first_launch=1'){throw 'Unsafe old defaults migrated'}
Write-Host 'PASS: haptic route/mute/gain, grip, laser and distinct vehicle offsets preserved; FG and graphics overwrite disabled.'
