$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'Switch-CyberpunkVRPort.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw $errors[0]}
$fn=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Set-EditionRelease'},$true)
Invoke-Expression $fn.Extent.Text
$Edition='Vrcam';$ForkArchive=$null;Set-EditionRelease
if($release.fileName -ne 'CyberpunkVRPort-0.1.8-psvr2-cp25-vrcam-rc1.zip'){throw 'Wrong VRCAM payload selected'}
$metadata=Get-Content "$repo\wabbajack\release-selection.json" -Raw|ConvertFrom-Json
if($metadata.fork.fileName -ne 'CyberpunkVRPort-0.1.9-psvr2.1-rc2.zip'){throw 'Current fork selection overwritten'}
if($release.sha256 -eq $metadata.fork.sha256){throw 'Comparison and current fork hashes equal'}
$Edition='Fork';Set-EditionRelease
if($release.fileName -ne $metadata.fork.fileName){throw 'Return-to-current selection broken'}
Write-Host 'PASS: VRCAM selects separate pinned 2.5 archive; current fork remains 0.1.9.'
