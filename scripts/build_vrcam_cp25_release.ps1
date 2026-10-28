# Package the tested checkpoint without rebuilding its native DLL.
param([string]$BuildDir='..\pacing-018-msvc',[switch]$Force)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot
$version='0.1.8-psvr2-cp25-vrcam-rc1'
$expected='eef7696ca6b97285ba50d47e572d5c77ee0d23f5bba5ac3bca57a76d2c103004'
$dll=Join-Path $repo "$BuildDir\bin\red4ext\plugins\CyberpunkVR_Stereo\Release\CyberpunkVR_Stereo.dll"
if((Get-FileHash $dll).Hash -ine $expected){throw 'Build is not the tested game-2.5 VRCAM DLL.'}
& "$PSScriptRoot\build_dist.ps1" -Version $version -BuildDir $BuildDir -SourceRevision '38f5160f game-2.5 checkpoint; release packaging in tag' -Force:$Force
$out=Join-Path $repo "dist\CyberpunkVRPort-$version"
@('first_launch=0','show_launcher=1','xr_framegen=0','xr_psvr2_haptic_backend=2')|Set-Content "$out\bin\x64\vrport.ini" -Encoding ASCII
Copy-Item "$repo\docs\VRCAM-CP25-COMPARISON.md" "$out\INSTALL.txt" -Force
Copy-Item "$repo\docs\VRCAM-CP25-COMPARISON.md" "$out\CHANGELOG.txt" -Force
Copy-Item "$repo\LICENSE" "$out\LICENSE" -Force
$bad=@(Get-ChildItem $out -File -Recurse|Where-Object{
 $_.Name -match '^(dxgi|version|nvngx.*)\.dll$' -or $_.Extension -in @('.log','.pdb','.dmp','.bak') -or
 $_.Name -in @('MoreOccluders.reds','vrik_calibration.ini') -or $_.FullName -match '\\(HUDitor|CheekyFoveatedDLSS|CyberpunkVR_Hands|CyberpunkVRPort_QuickBoot)\\'
})
if($bad.Count){throw "Unwanted package files: $($bad.FullName -join ', ')"}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip="$out.zip"
if(Test-Path $zip){if(-not $Force){throw 'ZIP exists; use Force explicitly'};Remove-Item $zip}
[IO.Compression.ZipFile]::CreateFromDirectory($out,$zip)
Copy-Item "$repo\docs\VRCAM-CP25-COMPARISON.md" "$repo\dist\VRCAM-CP25-SETUP.md" -Force
Write-Host "VRCAM ZIP SHA256: $((Get-FileHash $zip).Hash)"
