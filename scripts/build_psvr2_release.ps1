#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+-[A-Za-z0-9][A-Za-z0-9.-]*$')]
    [string]$Version = '0.1.9-psvr2.1-rc5',
    [string]$BuildDir = 'build-local',
    [string]$ExpectedDllSha256 = '676d9b6b71e7ec40f3918f6b99ef266abdff83a4e2bf9e5fd92c4b3da0a4ef50',
    [string]$ExpectedBackendSha256 = '7af8e14757e7921a301dfd618c1a3fc5a2ca427a444807e1180cb515a66e4cd4'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'CyberpunkVR-Switcher.Core.ps1')
function New-PortableZip([string]$Folder,[string]$Path) {
    $zip=[IO.Compression.ZipFile]::Open($Path,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach($file in Get-ChildItem -LiteralPath $Folder -Recurse -File) {
            $name=$file.FullName.Substring($Folder.Length+1).Replace('\','/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$name,[IO.Compression.CompressionLevel]::Optimal)|Out-Null
        }
    } finally { $zip.Dispose() }
}
$dllRel = 'red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll'
$dll = Join-Path $repo "$BuildDir\bin\red4ext\plugins\CyberpunkVR_Stereo\Release\CyberpunkVR_Stereo.dll"
if ($ExpectedDllSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
    (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ine $ExpectedDllSha256) {
    throw 'Native DLL does not match the explicitly selected tested build.'
}
$backend = Join-Path (Split-Path -Parent $dll) 'ofxr-v557\CyberpunkVR_OFXR.dll'
if ($ExpectedBackendSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
    (Get-FileHash -LiteralPath $backend -Algorithm SHA256).Hash -ine $ExpectedBackendSha256) {
    throw 'Private backend DLL does not match the selected host/backend pair.'
}
$dist = Join-Path $repo 'dist'
$out = Join-Path $dist "CyberpunkVRPort-$Version"
$zipPath = "$out.zip"
$switcher = Join-Path $dist "CyberpunkVRPort-Switcher-$Version"
$switcherZip = Join-Path $dist 'CyberpunkVRPort-Switcher.zip'
foreach ($path in @($out,$zipPath,$switcher,$switcherZip)) {
    if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite release output: $path" }
}
New-Item -ItemType Directory -Path $dist -Force | Out-Null
# Reuse upstream's enumerated shader/archive/mod packaging. Never read from a
# personal game install or bundle account-gated dependencies or GPU vendor DLLs.
& (Join-Path $PSScriptRoot 'build_dist.ps1') -Version $Version -BuildDir $BuildDir -SourceRevision $Version *>&1 |
    Set-Content -LiteralPath (Join-Path $dist "package-$Version.log") -Encoding UTF8
if ((Get-FileHash -LiteralPath (Join-Path $out $dllRel)).Hash -ine $ExpectedDllSha256) { throw 'Packaged DLL mismatch' }

if ((Get-FileHash -LiteralPath (Join-Path $out 'red4ext\plugins\CyberpunkVR_Stereo\ofxr-v557\CyberpunkVR_OFXR.dll')).Hash -ine $ExpectedBackendSha256) { throw 'Packaged backend mismatch' }

# Safe defaults for manual extraction too; switcher replaces this with its
# personal-preference whitelist. Never distribute the maintainer's runtime INI.
@('first_launch=0','show_launcher=1','xr_framegen=0','xr_framegen_backend=1','xr_framegen_quality=2','xr_framegen_flow_scale=50','xr_framegen_logging=1','xr_psvr2_haptic_backend=2','xr_engine_stereo=1') |
    Set-Content -LiteralPath (Join-Path $out 'bin\x64\vrport.ini') -Encoding ASCII
$guide = Join-Path $repo 'docs\PSVR2-019-RC.md'
Copy-VerifiedFile $guide (Join-Path $out 'INSTALL.txt')
Copy-VerifiedFile (Join-Path $repo "docs\RELEASE-$Version.md") (Join-Path $out 'CHANGELOG.txt')
Copy-VerifiedFile (Join-Path $repo 'LICENSE') (Join-Path $out 'LICENSE')
$forbidden = @(Get-ChildItem -LiteralPath $out -Recurse -File | Where-Object {
    $_.Name -match '^(dxgi|version|nvngx.*)\.dll$' -or $_.Extension -in @('.log','.pdb','.dmp','.enable') -or
    $_.FullName -match '\\(HUDitor|CheekyFoveatedDLSS|CyberpunkVR_Hands|CyberpunkVRPort_QuickBoot)\\'
})
if ($forbidden.Count) { throw "Unwanted release files: $($forbidden.FullName -join ', ')" }
New-PortableZip $out $zipPath
$forkHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
$selection = [ordered]@{schemaVersion=1; fork=[ordered]@{
    name="CyberpunkVRPort PSVR2 fork $Version"
    fileName=[IO.Path]::GetFileName($zipPath)
    url="https://github.com/satyaloka93/cyberpunk-vr-port/releases/download/$Version/$([IO.Path]::GetFileName($zipPath))"
    sha256=$forkHash
}}
New-Item -ItemType Directory -Path (Join-Path $repo 'wabbajack') -Force | Out-Null
# Preserve separately pinned comparison releases when regenerating the current fork package.
$oldSelectionPath = Join-Path $repo 'wabbajack\release-selection.json'
if (Test-Path $oldSelectionPath) {
    $oldSelection = Get-Content $oldSelectionPath -Raw | ConvertFrom-Json
    if ($oldSelection.PSObject.Properties.Name -contains 'vrcam') { $selection.vrcam = $oldSelection.vrcam }
}
$selection | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $repo 'wabbajack\release-selection.json') -Encoding UTF8
foreach ($rel in @('Install-Upstream-021.bat','Install-PSVR2-Fork.bat','Install-VRCAM-CP25.bat','Switch-CyberpunkVRPort.bat',
    'scripts\Switch-CyberpunkVRPort.ps1','scripts\CyberpunkVR-Switcher.Core.ps1','scripts\MoreOccluders.Compatibility.ps1',
    'scripts\Test-CyberpunkVRSwitcher.ps1','scripts\README-SWITCHER.txt',
    'docs\PSVR2-019-RC.md','docs\OFXR-V557-INTEGRATION.md','docs\framegen-live-handoff.md','docs\UPSTREAM-021-INSTALL.md','docs\VRCAM-CP25-COMPARISON.md',
    'wabbajack\switcher-dependencies-019.json','wabbajack\switcher-dependencies-021.json','wabbajack\release-selection.json')) {
    Copy-VerifiedFile (Get-SafePath $repo $rel) (Get-SafePath $switcher $rel)
}
Copy-VerifiedFile (Join-Path $repo 'scripts\README-SWITCHER.txt') (Join-Path $switcher 'START-HERE.txt')
New-PortableZip $switcher $switcherZip
Copy-VerifiedFile $guide (Join-Path $dist 'PSVR2-SETUP.md')
$sums = @($zipPath,$switcherZip,(Join-Path $dist 'PSVR2-SETUP.md')) | ForEach-Object {
    "{0}  {1}" -f (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant(),[IO.Path]::GetFileName($_)
}
$sums | Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt') -Encoding ASCII
Write-Host "Game ZIP: $zipPath"
Write-Host "Game SHA-256: $forkHash"
Write-Host "Installer ZIP: $switcherZip"
Write-Host 'The fork download URL is usable after these assets are published; run fixture tests before publishing.'
