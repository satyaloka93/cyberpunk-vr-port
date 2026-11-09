#requires -Version 5.1
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if(-not $OutputDirectory){$OutputDirectory=Join-Path $repo 'dist\switcher-rc1-release'}
$folder=Join-Path $OutputDirectory 'CyberpunkVRPort-Switcher'
$zipPath=Join-Path $OutputDirectory 'CyberpunkVRPort-Switcher.zip'
if(Test-Path -LiteralPath $zipPath){throw "Refusing to overwrite $zipPath"}
. (Join-Path $PSScriptRoot 'CyberpunkVR-Switcher.Core.ps1')
$files=@('scripts\Inspect-CheekyLogs.ps1','scripts\Test-CheekyDiagnostics.ps1','docs\CHEEKY-DIAGNOSTICS.md','Switch-CyberpunkVRPort.bat','Install-Upstream-023.bat','Install-PSVR2-Fork.bat','Install-VRCAM-CP25.bat',
 'scripts\Loader-Identity.ps1','scripts\Test-LoaderIdentity.ps1','wabbajack\loader-identities.json','docs\LOADER-IDENTITY.md','scripts\Test-Switcher023.ps1','scripts\Test-CyberpunkVRSwitcher.ps1','scripts\Switch-CyberpunkVRPort.ps1','scripts\CyberpunkVR-Switcher.Core.ps1','scripts\MoreOccluders.Compatibility.ps1','scripts\README-SWITCHER.txt',
 'docs\UPSTREAM-023-INSTALL.md','docs\PSVR2-019-RC.md','docs\PSVR2-023-RC1.md','docs\VRCAM-CP25-COMPARISON.md',
 'wabbajack\switcher-dependencies-023.json','wabbajack\switcher-dependencies-019.json','wabbajack\release-selection.json')
foreach($rel in $files){Copy-VerifiedFile (Get-SafePath $repo $rel) (Get-SafePath $folder $rel)}
Copy-VerifiedFile (Get-SafePath $repo 'scripts\README-SWITCHER.txt') (Join-Path $folder 'START-HERE.txt')
# Explicit slash paths are portable across ZIP extractors, unlike .NET Framework CreateFromDirectory.
$zip=[IO.Compression.ZipFile]::Open($zipPath,[IO.Compression.ZipArchiveMode]::Create)
try {
 foreach($file in Get-ChildItem -LiteralPath $folder -Recurse -File){
  $name=$file.FullName.Substring($folder.Length+1).Replace('\','/')
  [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$name,[IO.Compression.CompressionLevel]::Optimal)|Out-Null
 }
}finally{$zip.Dispose()}
Get-FileHash -LiteralPath $zipPath
Write-Host "Installer ready: $folder"
