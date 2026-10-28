$ErrorActionPreference='Stop'
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'Switch-CyberpunkVRPort.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw $errors[0]}
$fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Test-Dependency'},$true)
Invoke-Expression $fn.Extent.Text
function Get-SafePath($Root,$Relative){return $Root+'\'+$Relative}
function Test-Path {return $true}
function Get-Item {return [pscustomobject]@{PSIsContainer=$false;VersionInfo=[pscustomobject]@{FileVersion=$script:version}}}
function Get-FileHash {return [pscustomobject]@{Hash=$script:hash}}
$probes=@{codeware=@('Codeware.dll');redscript=@('scc.exe')}
$m=[pscustomobject]@{id='codeware';minimumVersion='1.21.0'}
foreach($case in @(@('1.20.3',$false),@('1.21.0',$true),@('1.21.1.202610270000',$true),@('',$false))){
 $script:version=$case[0]
 if((Test-Dependency $m 'C:\fixture') -ne $case[1]){throw "Version check failed: $version"}
}
$m=[pscustomobject]@{id='redscript';binarySha256=('a'*64)}
$script:hash='a'*64;if(-not (Test-Dependency $m 'C:\fixture')){throw 'Pinned compiler rejected'}
$script:hash='b'*64;if(Test-Dependency $m 'C:\fixture'){throw 'Unknown compiler accepted'}
$manifest=Get-Content (Join-Path $PSScriptRoot '..\wabbajack\switcher-dependencies-019.json') -Raw | ConvertFrom-Json
if($manifest.mods.Count -ne 10 -or @($manifest.mods|Where-Object id -eq 'visible-bullets').Count){throw 'Wrong 019 dependency set'}
if(@($manifest.mods|Where-Object {$_.minimumVersion}).Count -ne 6){throw 'Missing framework minimums'}
Write-Host 'PASS: 0.1.9 parser, minimum versions, timestamp suffix, missing versions, compiler hash and dependency manifest.'
