$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CyberpunkVR-Switcher.Core.ps1')
$tokens=$null;$errors=$null
$path=Join-Path $PSScriptRoot 'Switch-CyberpunkVRPort.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw $errors[0]}
$fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'New-ReleaseStage'},$true)
Invoke-Expression $fn.Extent.Text
$base=Join-Path $env:TEMP ('cpvr-optional-occluders-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $base 'game';$CacheRoot=Join-Path $base 'cache'
New-Item -ItemType Directory "$root\r6\scripts" -Force | Out-Null
$dependencies=@();$interactiveMenu=$true;$MoreOccludersArchive=$null;$release=[pscustomobject]@{name='fixture'}
function Read-Host { throw 'Unexpected interactive download prompt' }
function Get-VerifiedArchive { param($Mod) return 'fixture-release' }
function Expand-SafeModZip { param($Archive,$Destination)
 if($Archive -ne 'fixture-release'){throw 'Attempted More Occluders import'}
 New-Item -ItemType Directory "$Destination\red4ext\plugins\CyberpunkVR_Stereo" -Force|Out-Null
 Set-Content "$Destination\red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll" 'fixture'
}
function Set-MigratedSettings { param($Root,$Stage) }
foreach($Edition in @('Upstream','Fork')){
 $stage=New-ReleaseStage $root
 if(-not (Test-Path "$stage\red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll")){throw 'Stage failed without occluders'}
 if(Test-Path "$stage\r6\scripts\MoreOccluders.reds"){throw 'Occluders unexpectedly staged'}
}
Set-Content "$root\r6\scripts\MoreOccluders.reds" 'existing-user-mod'
$MoreOccludersArchive='nonexistent-legacy-argument.zip'
$stage=New-ReleaseStage $root
if((Get-Content "$root\r6\scripts\MoreOccluders.reds") -ne 'existing-user-mod'){throw 'Existing mod modified'}
if(Test-Path "$stage\r6\scripts\MoreOccluders.reds"){throw 'Deprecated argument installed mod'}
if((Get-Content $path -Raw) -match '33136'){throw 'Required Nexus page still includes More Occluders'}
Write-Host 'PASS: both editions stage without More Occluders or prompts; existing mod preserved; deprecated archive ignored; required Nexus link removed.'
