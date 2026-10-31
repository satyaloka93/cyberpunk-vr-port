param([Parameter(Mandatory=$true)][string]$OriginalScript,[Parameter(Mandatory=$true)][string]$WorkingScript)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CyberpunkVR-Switcher.Core.ps1')
. (Join-Path $PSScriptRoot 'MoreOccluders.Compatibility.ps1')
$base=Join-Path $env:TEMP ('cpvr-occluders-patch-'+[guid]::NewGuid().ToString('N'))
$game=Join-Path $base 'game';$CacheRoot=Join-Path $base 'cache';$BackupRoot=Join-Path $base 'backups'
$target=Join-Path $game 'r6\scripts\MoreOccluders.reds'
Copy-VerifiedFile $OriginalScript $target
$originalHash=(Get-FileHash $target).Hash
$interactiveMenu=$false;$MoreOccludersArchive=$null;$WhatIf=$true
function Confirm-Change {return $true}
function Assert-GameClosed {}
Install-OptionalOccluders250 $game
if((Get-FileHash $target).Hash -ne $originalHash){throw 'Preview wrote game'}
$WhatIf=$false
Install-OptionalOccluders250 $game
$actual=[IO.File]::ReadAllText($target).Replace("`r`n","`n").TrimEnd([char[]]"`r`n")
$expected=[IO.File]::ReadAllText($WorkingScript).Replace("`r`n","`n").TrimEnd([char[]]"`r`n")
if($actual -cne $expected){throw 'Result differs from tested local script'}
$backup=@(Get-ChildItem $BackupRoot -Filter '*.original' -File -Recurse)
if($backup.Count -ne 1 -or (Get-FileHash $backup[0].FullName).Hash -ne $originalHash){throw 'Backup is not exact'}
Install-OptionalOccluders250 $game
if(@(Get-ChildItem $BackupRoot -Filter receipt.json -File -Recurse).Count -ne 1){throw 'Idempotent patch created a second backup'}
$patchedHash=(Get-FileHash $target).Hash
# Menu with a script installed: Enter keeps it; no download prompt is required.
$interactiveMenu=$true
function Read-Host {param($Prompt) if($Prompt -notlike 'MoreOccluders.reds is installed*'){throw "Unexpected prompt: $Prompt"}; return ''}
Install-OptionalOccluders250 $game
if((Get-FileHash $target).Hash -ne $patchedHash){throw 'Keeping the installed script changed it'}
Remove-Item function:Read-Host;$interactiveMenu=$false
[IO.File]::WriteAllText($target,$expected+' // unknown edit')
$unknownHash=(Get-FileHash $target).Hash;$failed=$false
try {Install-OptionalOccluders250 $game}catch{$failed=$true}
if(-not $failed -or (Get-FileHash $target).Hash -ne $unknownHash){throw 'Unknown script not safely rejected'}
# Real archive validation and explicit archive opt-in.
$zipstage=Join-Path $base 'zipstage';Copy-VerifiedFile $OriginalScript "$zipstage\r6\scripts\MoreOccluders.reds"
$zip=Join-Path $base 'download.zip';[IO.Compression.ZipFile]::CreateFromDirectory($zipstage,$zip)
Stage-MoreOccluders250 $game (Join-Path $base 'zip-result') $zip
if((Get-OccludersTextHash ([IO.File]::ReadAllText("$base\zip-result\r6\scripts\MoreOccluders.reds"))) -ne (Get-OccludersTextHash $expected)){throw 'ZIP patch mismatch'}
# Exercise rollback after a failed write, using the production implementation.
Copy-VerifiedFile $OriginalScript $target
$realCopy=(Get-Command Copy-VerifiedFile).ScriptBlock
function Copy-VerifiedFile {param($Source,$Destination)
 if($Destination -eq $target -and $Source -notlike '*.original'){[IO.File]::WriteAllText($Destination,'partial');throw 'injected failure'}
 & $realCopy $Source $Destination
}
$failed=$false;try{Install-OptionalOccluders250 $game}catch{$failed=$true}
if(-not $failed -or (Get-FileHash $target).Hash -ne $originalHash){throw 'Write failure did not roll back'}
Write-Host 'PASS: exact known fix, preview, backup, idempotence, unknown refusal, explicit ZIP and failed-write rollback.'
