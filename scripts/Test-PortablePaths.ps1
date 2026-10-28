$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CyberpunkVR-Switcher.Core.ps1')
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'Switch-CyberpunkVRPort.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw $errors[0]}
$fn=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Find-Game'},$true)
Invoke-Expression $fn.Extent.Text
$base=Join-Path $env:TEMP ('cpvr-path-test-'+[guid]::NewGuid().ToString('N'))
$steam=Join-Path $base 'Steam with spaces';$library=Join-Path $base 'Library with spaces'
$game=Join-Path $library 'steamapps\common\Cyberpunk 2077'
New-Item -ItemType Directory "$steam\steamapps","$game\bin\x64" -Force|Out-Null
Set-Content "$game\bin\x64\Cyberpunk2077.exe" 'fixture'
$missing=@('Z','Y','X','W','V'|Where-Object {-not(Get-PSDrive $_ -ErrorAction SilentlyContinue)})[0]
if(-not $missing){throw 'No unused test drive'}
$escaped=$library.Replace('\','\\')
@('"libraryfolders"','{',('"path" "'+$missing+':\\disconnected\\library"'),('"path" "'+$escaped+'"'),'}')|Set-Content "$steam\steamapps\libraryfolders.vdf"
function Get-ItemProperty {param($Path,$Name,$ErrorAction)
 if($Name -eq 'SteamPath'){return [pscustomobject]@{SteamPath=$steam}}
 if($Name -eq 'InstallLocation'){return [pscustomobject]@{InstallLocation=($missing+':\old-game')}}
}
$GameRoot=$null
if((Find-Game) -ne $game){throw 'Discovery failed with missing drive and spaced library'}
$GameRoot=$game
if((Find-Game) -ne $game){throw 'Explicit game root failed'}
$failed=$false
try {Expand-SafeModZip $base (Join-Path $base 'unused')}catch{$failed=$_.Exception.Message -like 'Selected a folder, not a ZIP:*'}
if(-not $failed){throw 'Folder did not get actionable error'}
$z=Join-Path $base 'download with spaces.zip'
$zip=[IO.Compression.ZipFile]::Open($z,[IO.Compression.ZipArchiveMode]::Create)
$writer=New-Object IO.StreamWriter($zip.CreateEntry('r6/scripts/test.reds').Open());$writer.Write('fixture');$writer.Dispose();$zip.Dispose()
Expand-SafeModZip ('  "'+$z+'"  ') (Join-Path $base 'extracted')
if((Get-Content "$base\extracted\r6\scripts\test.reds") -ne 'fixture'){throw 'Quoted ZIP path failed'}
Write-Host 'PASS: stale drive/registry ignored, VDF separators preserved, paths with spaces, explicit root, folder rejection and quoted ZIP.'
