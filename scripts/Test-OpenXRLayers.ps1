#requires -Version 5.1
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'Inspect-OpenXRLayers.ps1')
$base=Join-Path $env:TEMP ('cpvr-xrlayers-'+[guid]::NewGuid().ToString('N'))
$passed=0
function Assert($v,$message){if(-not $v){throw "FAIL: $message"};$script:passed++;Write-Host "PASS: $message"}
function Put($p,$text){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($p))|Out-Null;[IO.File]::WriteAllText($p,$text)}
function Manifest($dir,$name='XR_APILAYER_CHEEKY_foveated_dlss',$lib='.\layer.dll') {
 $p=Join-Path $base "$dir\layer.json"
 Put $p (@{file_format_version='1.0.0';api_layer=@{name=$name;library_path=$lib;implementation_version='1';disable_environment='CHEEKY_OPENXR_LAYER_DISABLE'}}|ConvertTo-Json)
 $p
}
function Row($p,$hive='HKLM',$kind='Implicit',$value=0,$valueKind='DWord') {
 [pscustomobject]@{Key="$hive\SOFTWARE\Khronos\OpenXR\1\ApiLayers\$kind";View='Registry64';Kind=$kind;Manifest=$p;Value=$value;ValueKind=$valueKind}
}
function Report($rows,$envs=@(),$errors=@()) {Get-CpvrOpenXrDiagnostics -Root (Join-Path $base 'game') -RegistrationSnapshot ([pscustomobject]@{Rows=@($rows);Errors=@($errors)}) -EnvironmentSnapshot $envs}
function Snap {(@(Get-ChildItem $base -File -Recurse|Sort-Object FullName|ForEach-Object {"$($_.FullName):$((Get-FileHash $_.FullName).Hash)"}) -join "`n")}
try {
 $p=Manifest 'current';Put (Join-Path $base 'current\layer.dll') 'fake-dll-never-executed'
 $r=Report @(Row $p)
 Assert ($r.Layers.Count -eq 1 -and $r.Layers[0].State -eq 'READ') 'Relative DLL resolved and read'
 Assert ($r.Layers[0].Binary.Sha256.Length -eq 64) 'Layer hash recorded without loading DLL'
 Assert ($r.Layers[0].Registration -eq 'ENABLED-REGISTRATION') 'Implicit DWORD zero labeled registration only'
 Assert ($r.Layers[0].ImplementationVersion -eq '1' -and -not $r.Layers[0].Binary.Version) 'Manifest implementation number is not invented DLL release version'
 $q=Manifest 'old';Put (Join-Path $base 'old\layer.dll') 'old-fake'
 $r=Report @((Row $p),(Row $q 'HKCU'))
 Assert (($r.Hints -join "`n") -match 'POSSIBLE DUPLICATE LAYER') 'Different enabled manifests across HKCU/HKLM flagged'
 $r=Report @((Row $p),(Row $p 'HKCU'))
 Assert (($r.Hints -join "`n") -match 'REPEATED REGISTRATION' -and ($r.Hints -join "`n") -notmatch 'POSSIBLE DUPLICATE LAYER') 'Same-path registration distinguished from duplicate installation'
 $r=Report @((Row $p),(Row $q 'HKCU' 'Implicit' 1))
 Assert ($r.Layers[1].Registration -eq 'DISABLED' -and ($r.Hints -join "`n") -notmatch 'POSSIBLE DUPLICATE LAYER') 'Disabled stale registration does not count as active duplicate'
 $r=Report @((Row $p),(Row $q 'HKCU' 'Explicit'))
 Assert ($r.Layers[1].Registration -eq 'EXPLICIT-AVAILABLE' -and ($r.Hints -join "`n") -notmatch 'POSSIBLE DUPLICATE LAYER') 'Explicit availability is not automatic activation'
 $r=Report @(Row $p 'HKLM' 'Implicit' '0' 'String')
 Assert ($r.Layers[0].Registration -eq 'INVALID') 'Wrong registry value type not treated as enabled'
 $r=Report @(Row (Join-Path $base 'missing\Cheeky.json'))
 Assert ($r.Layers[0].State -eq 'MISSING-MANIFEST') 'Missing registered manifest identified'
 $bad=Manifest 'bad';Put $bad '{broken'
 $r=Report @((Row $bad),(Row $p))
 Assert ($r.Layers[0].State -eq 'INVALID-MANIFEST' -and $r.Layers[1].State -eq 'READ') 'Malformed manifest does not stop later entries'
 Put $bad '{"api_layer":{"name":"bad"}}';$r=Report @(Row $bad)
 Assert ($r.Layers[0].State -eq 'INVALID-MANIFEST') 'Missing library path rejected'
 $missing=Manifest 'missing-dll';$r=Report @(Row $missing)
 Assert ($r.Layers[0].State -eq 'MISSING-DLL') 'Missing DLL distinguished from missing manifest'
 $network=Manifest 'network' 'XR_APILAYER_CHEEKY_foveated_dlss' '\\unreachable.invalid\share\x.dll'
 $r=Report @(Row $network)
 Assert ($r.Layers[0].Binary.State -eq 'NOT-INSPECTED') 'UNC DLL not opened'
 $r=Report @(Row '\\unreachable.invalid\share\Cheeky.json')
 Assert ($r.Layers[0].State -eq 'NOT-INSPECTED') 'UNC manifest not opened'
 $r=Report @(Row 'relative\Cheeky.json')
 Assert ($r.Layers[0].State -eq 'NOT-INSPECTED') 'Ambiguous relative registered manifest not opened'
 Put $bad ('x' * (1MB+1));$r=Report @(Row $bad)
 Assert ($r.Layers[0].State -eq 'UNREADABLE' -and $r.Layers[0].Error -match '1 MiB') 'Oversized manifest bounded'
 $ofxr=Manifest 'ofxr' 'XR_APILAYER_NOVENDOR_XRFrameBridge';Put (Join-Path $base 'ofxr\layer.dll') 'ofxr-fake'
 $r=Report @((Row $p),(Row $ofxr))
 Assert (($r.Hints -join "`n") -match 'CHEEKY \+ OFXR' -and ($r.Hints -join "`n") -match 'not automatically a conflict') 'OFXR coexistence advisory without invented incompatibility'
 $envs=@([pscustomobject]@{Scope='Process';Name='CHEEKY_OPENXR_LAYER_DISABLE';Value='0'},[pscustomobject]@{Scope='User';Name='XR_API_LAYER_PATH';Value='C:\old\layers'})
 $r=Report @(Row $p) $envs
 $text=@(Format-CpvrOpenXrDiagnostics $r) -join "`n"
 Assert ($text -match 'CHEEKY_OPENXR_LAYER_DISABLE=0' -and $text -match 'XR_API_LAYER_PATH=C:\\old') 'Override values retained verbatim, including zero'
 Assert ($text -match 'not the running game' -and $text -match 'not recursively scanned') 'Environment inheritance and discovery limits stated'
 $cheekyComponents=Join-Path $base 'game\bin\x64\CheekyFoveatedDLSS\CheekyFoveatedDLSSHost.dll';Put $cheekyComponents 'host-fake'
 $r=Report @()
 Assert (($r.Hints -join "`n") -match 'NO READABLE ENABLED CHEEKY') 'Host without registered layer detected without declaring load failure'
 $r=Report @() @() @('HKLM access denied')
 Assert (($r.Hints -join "`n") -match 'INCOMPLETE SCAN' -and $r.Errors.Count -eq 1) 'Permission failure cannot masquerade as complete absence'
 $original=${function:Get-CpvrXrBinary}
 try {
  function Get-CpvrXrBinary([string]$Path){$r=& $original $Path;if($r.State -eq 'READ'){$r.Version=if($Path -like '*Host.dll'){'0.5.4'}else{'0.7.0'}};$r}
  $r=Report @(Row $p)
  Assert (($r.Hints -join "`n") -match 'DIFFERENT CHEEKY VERSION LABELS') 'Different layer/runtime labels flagged as advisory'
 } finally {Set-Item Function:Get-CpvrXrBinary $original}
 $before=Snap;$r=Report @((Row $p),(Row $q)) $envs;$null=@(Format-CpvrOpenXrDiagnostics $r)
 Assert ((Snap) -ceq $before) 'Inspect/format preserves all fixture bytes'
 $source=Get-Content (Join-Path $PSScriptRoot 'Inspect-OpenXRLayers.ps1') -Raw
 Assert ($source -match 'RegistryView\]::Registry64' -and $source -match 'OpenSubKey\(\$sub,\$false\)') 'Registry read explicitly uses 64-bit view and read-only handles'
 Write-Host "$passed OpenXR layer assertions passed. No registry or real game changes."
} finally {Remove-Item -LiteralPath $base -Recurse -Force}
