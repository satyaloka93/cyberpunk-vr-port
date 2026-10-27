#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('Menu','Status','Nexus','Prepare','Install','Restore')][string]$Action = 'Menu',
    [string]$GameRoot,
    [string]$CacheRoot,
    [string]$BackupRoot = (Join-Path $env:LOCALAPPDATA 'CyberpunkVRPort\switcher-backups'),
    [string]$DownloadsDir = (Join-Path $env:USERPROFILE 'Downloads'),
    [string]$RestoreFrom,
    [string]$MoreOccludersArchive,
    [ValidateSet('Upstream','Fork')][string]$Edition = 'Upstream',
    [string]$ForkArchive,
    [string]$ForkSha256,
    [switch]$KeepReShade,
    [switch]$DisableDxgiProxy,
    [switch]$KeepHUDitor,
    [switch]$Yes,
    [switch]$WhatIf
)
$ErrorActionPreference = 'Stop'
if ($KeepReShade -and $DisableDxgiProxy) { throw 'KeepReShade and DisableDxgiProxy are mutually exclusive.' }
. (Join-Path $PSScriptRoot 'CyberpunkVR-Switcher.Core.ps1')
$repo = Split-Path -Parent $PSScriptRoot
if (-not $CacheRoot) { $CacheRoot = Join-Path $env:LOCALAPPDATA 'CyberpunkVRPort\switcher-cache' }
$CacheRoot = [IO.Path]::GetFullPath($CacheRoot)
$BackupRoot = [IO.Path]::GetFullPath($BackupRoot)
$interactiveMenu = $Action -eq 'Menu'
$release = [pscustomobject]@{
    name='CyberpunkVRPort upstream 0.1.9'; fileName='CyberpunkVRPort-0.1.9.zip'
    url='https://github.com/dariulone/cyberpunk-vr-port/releases/download/0.1.9/CyberpunkVRPort-0.1.9.zip'
    sha256='adc255885f24ede400c1510a9dd34ae5e32a3a61b3cd55bef3a017f802d48bf0'
}
function Set-EditionRelease {
    if ($Edition -ne 'Fork') { return }
    $metadataPath = Join-Path $repo 'wabbajack\release-selection.json'
    $forkMetadata = if (Test-Path -LiteralPath $metadataPath -PathType Leaf) { Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json } else { $null }
    if (-not $ForkArchive) {
        if (-not $forkMetadata -or -not $forkMetadata.fork.url -or -not $forkMetadata.fork.sha256) {
            throw 'No published fork release metadata. Pass -ForkArchive with the local game-root ZIP.'
        }
        if ($forkMetadata.fork.sha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'Fork release metadata has an invalid SHA-256.' }
        $script:release = $forkMetadata.fork
        Write-Host "Fork release: $($release.url)"
        Write-Host "Expected SHA-256: $($release.sha256)"
        return
    }
    $path = [IO.Path]::GetFullPath($ForkArchive)
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Fork archive not found: $path" }
    if ([IO.Path]::GetExtension($path) -ine '.zip') { throw 'ForkArchive must be a game-root ZIP.' }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    $expected = $ForkSha256
    if (-not $expected -and $forkMetadata -and $forkMetadata.fork.fileName -ieq [IO.Path]::GetFileName($path)) { $expected = $forkMetadata.fork.sha256 }
    if ($expected -and $actual -ine $expected) { throw "Fork archive checksum mismatch: $path" }
    Write-Host "Fork archive SHA-256: $actual"
    if (-not $expected) { Write-Host 'Local archive selected explicitly; no independently pinned checksum available yet.' -ForegroundColor Yellow }
    $script:release = [pscustomobject]@{ name=('CyberpunkVRPort local PSVR2 fork: ' + [IO.Path]::GetFileName($path)); fileName=[IO.Path]::GetFileName($path); sha256=$actual; localPath=$path }
}

function Show-PSVR2ToolkitGuidance {
    $detected = $false
    try {
        $detected = @(Get-CimInstance Win32_PnPEntity -ErrorAction Stop | Where-Object {
            $_.Name -match '(?i)(playstation\s*vr\s*2|psvr\s*2|sense\s*controller)'
        }).Count -gt 0
    } catch { Write-Verbose "PSVR2 hardware detection unavailable: $_" }
    if ($detected) { Write-Host "`nPSVR2/Sense hardware detected." -ForegroundColor Green }
    if ($detected -or $Edition -eq 'Fork') {
        if ($Edition -eq 'Upstream') {
            Write-Host 'Official upstream 0.1.9 has native controller haptics, not our isolated Toolkit bridge integration. The published 0.1.8 RC1 predates game 2.5; use Restore to return to your local 2.5 checkpoint.' -ForegroundColor Yellow
            Write-Host 'The switcher does not start/stop a bridge or change Toolkit drivers or OpenXR layers.'
            return
        }
        Write-Host 'PSVR2 haptics/adaptive triggers in our bridge-enabled fork require PSVR2Toolkit, installed separately (Steam DSX is NOT required):'
        Write-Host 'https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2'
        Write-Host 'Use its Cyberpunk 0.1.7 protocol bridge, also used by our modified 0.1.8. Audio controls: Mods > DualSense settings > PSVR2 Haptics.'
        Write-Host 'Read docs/PSVR2-018-RC.md for this 0.1.8 PSVR2 release candidate and Toolkit setup.'
        Write-Host 'This switcher does not install drivers, register OpenXR layers, or start a bridge.'
    }
}

function Find-Game {
    if ($GameRoot) {
        $root = [IO.Path]::GetFullPath($GameRoot).TrimEnd('\')
        if (-not (Test-Path -LiteralPath (Get-SafePath $root 'bin\x64\Cyberpunk2077.exe') -PathType Leaf)) { throw "Not a Cyberpunk game folder: $root" }
        return $root
    }
    $candidates = @('D:\SteamLibrary\steamapps\common\Cyberpunk 2077','F:\SteamLibrary\steamapps\common\Cyberpunk 2077')
    foreach ($key in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500')) {
        $value = Get-ItemProperty -Path $key -Name InstallLocation -ErrorAction SilentlyContinue
        if ($value) { $candidates += $value.InstallLocation }
    }
    $steam = Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -Name SteamPath -ErrorAction SilentlyContinue
    if ($steam) {
        $candidates += Join-Path $steam.SteamPath 'steamapps\common\Cyberpunk 2077'
        $libraries = Join-Path $steam.SteamPath 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $libraries) {
            foreach ($match in [regex]::Matches((Get-Content -LiteralPath $libraries -Raw), '"path"\s+"([^"]+)"')) {
                $candidates += Join-Path ($match.Groups[1].Value.Replace('\\','\')) 'steamapps\common\Cyberpunk 2077'
            }
        }
    }
    $found = @($candidates | Where-Object { $_ -and (Test-Path -LiteralPath (Join-Path $_ 'bin\x64\Cyberpunk2077.exe')) } | ForEach-Object { [IO.Path]::GetFullPath($_).TrimEnd('\') } | Sort-Object -Unique)
    if ($found.Count -ne 1) { throw "Found $($found.Count) game installs. Specify -GameRoot explicitly: $($found -join ', ')" }
    return $found[0]
}

function Assert-GameClosed {
    if (Get-Process Cyberpunk2077,REDlauncher,ModOrganizer,Vortex -ErrorAction SilentlyContinue) {
        throw 'Close Cyberpunk, REDlauncher, Mod Organizer and Vortex before switching. No processes will be killed.'
    }
}

function Get-VerifiedArchive($Mod) {
    if ($Mod.PSObject.Properties.Name -contains 'localPath') {
        if ((Get-FileHash -LiteralPath $Mod.localPath -Algorithm SHA256).Hash -ine $Mod.sha256) { throw 'Selected local release archive changed after selection.' }
        return $Mod.localPath
    }
    $cache = Join-Path $CacheRoot $Mod.fileName
    if (Test-Path -LiteralPath $cache -PathType Leaf) {
        if ((Get-FileHash -LiteralPath $cache -Algorithm SHA256).Hash -ieq $Mod.sha256) { return $cache }
        throw "Cached file checksum mismatch: $cache. Move it aside and retry."
    }
    # Look only at plausible filenames, not every multi-GB game installer in Downloads.
    if (Test-Path -LiteralPath $DownloadsDir) {
        $patterns = @($Mod.fileName)
        if ($Mod.PSObject.Properties.Name -contains 'localPatterns') { $patterns += @($Mod.localPatterns) }
        foreach ($pattern in $patterns) {
            foreach ($file in Get-ChildItem -LiteralPath $DownloadsDir -Filter $pattern -File) {
                if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ieq $Mod.sha256) { return $file.FullName }
            }
        }
    }
    if ($Mod.PSObject.Properties.Name -notcontains 'url' -or -not $Mod.url) { return $null }
    Write-Host "Downloading $($Mod.name) (SHA-256 verified)..."
    New-Item -ItemType Directory -Path $CacheRoot -Force | Out-Null
    $temp = $cache + '.' + [guid]::NewGuid().ToString('N') + '.partial'
    # curl avoids PowerShell 5.1's legacy HTML parser and follows GitHub asset redirects.
    & curl.exe --fail --location --retry 2 --connect-timeout 30 --max-time 600 --output $temp $Mod.url
    if ($LASTEXITCODE -ne 0) { throw "Download failed ($LASTEXITCODE). Partial file: $temp" }
    if ((Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash -ine $Mod.sha256) { throw "Download checksum mismatch; NOT installing: $temp" }
    Move-Item -LiteralPath $temp -Destination $cache
    return $cache
}

$probes = @{
    'red4ext'=@('red4ext\RED4ext.dll','bin\x64\winmm.dll')
    'cet'=@('bin\x64\plugins\cyber_engine_tweaks.asi')
    'redscript'=@('engine\tools\scc.exe','engine\config\base\scripts.ini')
    'archivexl'=@('red4ext\plugins\ArchiveXL\ArchiveXL.dll')
    'tweakxl'=@('red4ext\plugins\TweakXL\TweakXL.dll')
    'codeware'=@('red4ext\plugins\Codeware\Codeware.dll')
    'input-loader'=@('red4ext\plugins\input_loader\input_loader.dll')
    'equipment-ex'=@('r6\scripts\EquipmentEx','archive\pc\mod\EquipmentEx.archive')
    'visual-holsters'=@('bin\x64\plugins\cyber_engine_tweaks\mods\VisualHolster\init.lua')
    'visible-bullets'=@('r6\tweaks\BulletProjectileOverhaul\Items.Preset_Ajax_Default.yaml')
    'nova-optics'=@('r6\scripts\NovaOptics','bin\x64\plugins\cyber_engine_tweaks\mods\NovaOptics\init.lua')
}

function Test-Dependency($Mod, [string]$Root) {
    foreach ($rel in $probes[$Mod.id]) {
        $path = Get-SafePath $Root $rel
        if (-not (Test-Path -LiteralPath $path)) { return $false }
        if ((Get-Item -LiteralPath $path).PSIsContainer -and @(Get-ChildItem -LiteralPath $path -File -Recurse).Count -eq 0) { return $false }
    }
    if ($Mod.PSObject.Properties.Name -contains 'binarySha256') {
        return (Get-FileHash -LiteralPath (Join-Path $Root $probes[$Mod.id][0]) -Algorithm SHA256).Hash -ieq $Mod.binarySha256
    }
    if ($Mod.PSObject.Properties.Name -contains 'minimumVersion') {
        $v = (Get-Item -LiteralPath (Join-Path $Root $probes[$Mod.id][0])).VersionInfo.FileVersion
        if (-not $v -or $v -notmatch '^(\d+)\.(\d+)\.(\d+)') { return $false }
        return [version]$Matches[0] -ge [version]$Mod.minimumVersion
    }
    if ($Mod.id -eq 'codeware') {
        $v = (Get-Item -LiteralPath (Join-Path $Root $probes['codeware'][0])).VersionInfo.FileVersion
        # Codeware's fourth component is a timestamp that can exceed Int32.
        if (-not $v -or $v -notmatch '^(\d+)\.(\d+)\.(\d+)' -or
            [version]$Matches[0] -lt [version]'1.20.0') { return $false }
    }
    return $true
}

function New-ReleaseStage([string]$Root) {
    $stage = Join-Path $CacheRoot ('stage-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    $missing = @()
    foreach ($mod in $dependencies) {
        if (Test-Dependency $mod $Root) { Write-Host "KEEP installed: $($mod.name) [installed requirement satisfied; not a runtime compatibility test]"; continue }
        $archive = Get-VerifiedArchive $mod
        if ($archive) {
            Write-Host "STAGE: $($mod.name)"
            Expand-SafeModZip $archive $stage
        } else {
            $missing += "$($mod.name): $($mod.manualUrl)"
        }
    }
    $occluders = Get-SafePath $Root 'r6\scripts\MoreOccluders.reds'
    if (-not (Test-Path -LiteralPath $occluders) -and -not $MoreOccludersArchive -and $interactiveMenu) {
        Write-Host 'More Occluders (Scripted v0.3) needs your Nexus account:'
        Write-Host 'https://www.nexusmods.com/cyberpunk2077/mods/33136?tab=files'
        $MoreOccludersArchive = (Read-Host 'Paste the downloaded ZIP path, or Enter to get the missing-download report').Trim('"')
    }
    if (Test-Path -LiteralPath $occluders -PathType Leaf) {
        Write-Host 'KEEP MoreOccluders.reds (installed; version is not encoded in this script).'
    } elseif ($MoreOccludersArchive) {
        # User-supplied, account-downloaded ZIP. Only accept the one expected script.
        $check = Join-Path $CacheRoot ('occluders-' + [guid]::NewGuid().ToString('N'))
        Expand-SafeModZip $MoreOccludersArchive $check
        $files = @(Get-ChildItem -LiteralPath $check -Recurse -File)
        if ($files.Count -ne 1 -or $files[0].FullName -ine (Join-Path $check 'r6\scripts\MoreOccluders.reds')) {
            throw 'Select More Occluders (Scripted v0.3), not the ArchiveXL variant.'
        }
        Write-Host "User-provided More Occluders ZIP SHA256: $((Get-FileHash -LiteralPath $MoreOccludersArchive).Hash)"
        Copy-VerifiedFile $files[0].FullName (Get-SafePath $stage 'r6\scripts\MoreOccluders.reds')
    } else {
        $missing += 'More Occluders: https://www.nexusmods.com/cyberpunk2077/mods/33136?tab=files (Optional files: Scripted v0.3; pass -MoreOccludersArchive "path-to.zip")'
    }
    if ($missing.Count) {
        Write-Host "`nNexus sign-in/manual downloads required. Nothing in the game has been changed." -ForegroundColor Yellow
        $missing | ForEach-Object { Write-Host $_ }
        Write-Host "Save ZIPs in $DownloadsDir and rerun. For More Occluders supply its ZIP explicitly."
        $missing | Set-Content -LiteralPath (Join-Path $CacheRoot 'NEXUS-DOWNLOADS.txt') -Encoding UTF8
        throw 'Dependencies incomplete. All game changes blocked.'
    }
    # Missing/partial frameworks may ship default configuration files. Keep existing user
    # configuration, even when repairing a missing dependency binary.
    foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File) {
        if ($file.Extension -in @('.ini','.json','.xml')) {
            $rel = $file.FullName.Substring($stage.Length + 1)
            $existing = Get-SafePath $Root $rel
            if (Test-Path -LiteralPath $existing -PathType Leaf) { Copy-VerifiedFile $existing $file.FullName }
        }
    }
    Expand-SafeModZip (Get-VerifiedArchive $release) $stage
    if (-not (Test-Path -LiteralPath (Get-SafePath $stage 'red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll'))) { throw 'Release payload is incomplete.' }
    Set-MigratedSettings $Root $stage
    return $stage
}

function Show-Plan([string]$Root, [string]$Stage, [string[]]$Remove) {
    $writes = @(Get-ChildItem -LiteralPath $Stage -Recurse -File)
    Write-Host "`nGame: $Root"
    Write-Host "Stage: $Stage"
    Write-Host "$($Remove.Count) existing port/conflict files to back up; $($writes.Count) staged files to install."
    Write-Host 'Clean replacement: back up ALL originals, remove selected old port files, install, then verify hashes and retired-file removal.'
    Write-Host "Backup parent: $BackupRoot"
    Write-Host 'Game files, saves, unrelated mods, DLSS replacements and game UserSettings.json are preserved.'
    Write-Host 'Personal fit/input preferences and calibration migrate; old render/FG/debug tuning does not.'
    Write-Host 'First-launch graphics overwrite is disabled; native FG starts OFF; launcher starts visible.'
    if ($DisableDxgiProxy) {
        Write-Host 'EXPLICIT REQUEST: back up and disable bin\x64\dxgi.dll. This may disable Cheeky, ReShade or another loader.' -ForegroundColor Yellow
    } else {
        Write-Host 'Existing dxgi.dll/version.dll loaders and Cheeky/ReShade settings are preserved. Their compatibility is not guaranteed.'
        Write-Host 'Only use -DisableDxgiProxy if you intend to disable the dxgi loader; -Yes alone never selects this.'
    }
    if (-not $KeepHUDitor) { Write-Host 'HUDitor: disabled into backup; the port native HUD replaces it.' -ForegroundColor Yellow }
    $Remove | Set-Content -LiteralPath (Join-Path $Stage '..\last-plan-remove.txt') -Encoding UTF8
    $writes.FullName | Set-Content -LiteralPath (Join-Path $Stage '..\last-plan-write.txt') -Encoding UTF8
    Write-Host "Full file lists: $CacheRoot\last-plan-{remove,write}.txt"
}

function Confirm-Change([string]$Prompt) {
    if ($Yes) { return $true }
    return (Read-Host "$Prompt Type SWITCH to continue") -ceq 'SWITCH'
}

function Restore-Previous([string]$Root) {
    $selected = $RestoreFrom
    if (-not $selected) {
        $journals = @(Get-ChildItem -LiteralPath $BackupRoot -Filter transaction.json -Recurse -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending)
        foreach ($file in $journals) {
            $j = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
            if ($j.GameRoot -ieq $Root -and $j.Status -in @('Installed','Prepared') -and $file.Directory.Name -match '^(upstream|switch)-') { $selected = $file.Directory.FullName; break }
        }
    }
        if (-not $selected) { throw 'No active switch backup found. Use -RestoreFrom for an explicit backup.' }
    $selected = [IO.Path]::GetFullPath($selected)
    $j = Get-Content -LiteralPath (Join-Path $selected 'transaction.json') -Raw | ConvertFrom-Json
    if ($j.Schema -ne 1 -or $j.GameRoot -ine $Root -or $j.Status -notin @('Installed','Prepared')) { throw 'Backup is not a restorable transaction for this game.' }
    # Verify ALL originals before touching anything. A damaged backup cannot be used.
    foreach ($entry in $j.Entries) {
        $null = Get-SafePath $Root $entry.Path
        if ($entry.Existed) {
            $file = Get-SafePath (Join-Path $selected 'original') $entry.Path
            if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $entry.BeforeHash) { throw "Damaged backup: $file" }
        }
    }
    Write-Host "Restore original port from: $selected"
    Write-Host 'Current port state is backed up first. Current game UserSettings.json and saves remain untouched.'
    if ($WhatIf -or -not (Confirm-Change 'Restore the previous port?')) { return }
    Assert-GameClosed
    $stage = Join-Path $CacheRoot ('restore-stage-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    foreach ($entry in $j.Entries) {
        if ($entry.Existed) { Copy-VerifiedFile (Get-SafePath (Join-Path $selected 'original') $entry.Path) (Get-SafePath $stage $entry.Path) }
    }
    $remove = @(@($j.Entries | ForEach-Object { $_.Path }) + @(Get-PortFiles $Root $false $false) | Sort-Object -Unique)
    $backup = Join-Path $BackupRoot ('before-restore-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
    $null = Invoke-PortTransaction $Root $stage $remove $backup $settingsFile
    $j.Status = 'Restored'; Save-Journal $j $selected
    Write-Host "Previous port restored. State before restore saved in $backup" -ForegroundColor Green
}

try {
    $root = Find-Game
    foreach ($storage in @($CacheRoot,$BackupRoot)) {
        if (($storage.TrimEnd('\') + '\').StartsWith(($root + '\'), [StringComparison]::OrdinalIgnoreCase)) { throw 'Cache/backups must be outside the game folder.' }
    }
    $dependencies = @((Get-Content -LiteralPath (Join-Path $repo 'wabbajack\switcher-dependencies.json') -Raw | ConvertFrom-Json).mods | Sort-Object priority)
    foreach ($mod in $dependencies) { if (-not $probes.ContainsKey($mod.id)) { throw "Unknown dependency probe: $($mod.id)" } }

    $settingsFile = Join-Path $env:LOCALAPPDATA 'CD Projekt Red\Cyberpunk 2077\UserSettings.json'
    if ($Action -eq 'Menu') {
        Write-Host "CyberpunkVR Port switcher -- official upstream 0.1.9 or PSVR2 fork 0.1.8-psvr2.1-rc1`nGame: $root"
        Write-Host "1  Preview / download and stage upstream 0.1.9`n2  Install upstream 0.1.9`n3  Preview / stage PSVR2 fork`n4  Install PSVR2 fork`n5  Restore previous port`n6  Installed dependency status`n7  Open required Nexus download pages`nQ  Quit"
        $choice = Read-Host 'Choose'
        $Action = switch ($choice) { '1' { 'Prepare' }; '2' { 'Install' }; '3' { $Edition='Fork'; 'Prepare' }; '4' { $Edition='Fork'; 'Install' }; '5' { 'Restore' }; '6' { 'Status' }; '7' { 'Nexus' }; default { return } }
        if ($Edition -eq 'Fork' -and -not $ForkArchive) { $ForkArchive = (Read-Host 'Optional path to local fork ZIP (Enter downloads pinned release)').Trim('"') }
    }
    if ($Edition -eq 'Upstream') {
        $dependencies = @((Get-Content -LiteralPath (Join-Path $repo 'wabbajack\switcher-dependencies-019.json') -Raw | ConvertFrom-Json).mods | Sort-Object priority)
        foreach ($mod in $dependencies) { if (-not $probes.ContainsKey($mod.id)) { throw "Unknown dependency probe: $($mod.id)" } }
        if ($Action -in @('Install','Prepare')) {
            $gameVersion = (Get-Item -LiteralPath (Join-Path $root 'bin\x64\Cyberpunk2077.exe')).VersionInfo.ProductVersion
            if ($gameVersion -notmatch '^2\.5(?:\.0)?$') { throw "Upstream 0.1.9 requires game 2.5; detected '$gameVersion'. Nothing changed." }
        }
    }
    if ($Action -eq 'Nexus') {
        $links = @($dependencies | Where-Object { $_.PSObject.Properties.Name -contains 'manualUrl' -and -not (Test-Dependency $_ $root) } | ForEach-Object { $_.manualUrl })
        if (-not (Test-Path -LiteralPath (Get-SafePath $root 'r6\scripts\MoreOccluders.reds'))) { $links += 'https://www.nexusmods.com/cyberpunk2077/mods/33136?tab=files' }
        if (-not $links.Count) { Write-Host 'All Nexus-required mods are already present. No account downloads needed.'; return }
        foreach ($link in $links) { Write-Host $link; if (-not $WhatIf) { Start-Process $link } }
        Write-Host "Use Manual Download while signed in, save ZIPs to $DownloadsDir, then rerun this switcher. More Occluders: select Scripted v0.3."
        return
    }
    if ($Action -eq 'Status') {
        Write-Host "Game: $root"
        foreach ($mod in $dependencies) {
            $label = if (Test-Dependency $mod $root) { 'KEEP (installed)' } else { 'MISSING / REPAIR' }
            Write-Host "$label : $($mod.name)"
        }
        Write-Host "More Occluders script present: $(Test-Path -LiteralPath (Get-SafePath $root 'r6\scripts\MoreOccluders.reds'))"
        Write-Host "dxgi proxy present: $(Test-Path -LiteralPath (Get-SafePath $root 'bin\x64\dxgi.dll'))"
        Write-Host 'Framework versions are checked against the selected manifest. redscript 0.5.32 uses its pinned compiler hash (no file version). Other mods are presence-checked.'
        Show-PSVR2ToolkitGuidance
        return
    }
    Set-EditionRelease
    Assert-GameClosed
    # A per-game mutex prevents simultaneous switch/restore operations from this tool.
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $key = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($root.ToLowerInvariant())))).Replace('-','') } finally { $sha.Dispose() }
    $mutex = New-Object Threading.Mutex($false, ('Local\CyberpunkVR-Switch-' + $key))
    $locked = $false
    try {
        try { $locked = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $locked = $true }
        if (-not $locked) { throw 'Another switcher is active for this game.' }
        if ($Action -eq 'Restore') { Restore-Previous $root; return }
        # Completed installs may be upgraded: each transaction snapshots CURRENT files
        # into a new directory. Older baselines remain immutable and restorable.
        # An interrupted transaction still requires recovery before any new install.
        if (Test-Path -LiteralPath $BackupRoot) {
            foreach ($file in Get-ChildItem -LiteralPath $BackupRoot -Filter transaction.json -Recurse -File) {
                $j = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
                if ($j.GameRoot -ieq $root -and $j.Status -eq 'Prepared') { throw "Interrupted switch. Run Restore -RestoreFrom `"$($file.Directory.FullName)`" first." }
                if ($Action -eq 'Install' -and $j.GameRoot -ieq $root -and $j.Status -eq 'Installed' -and $file.Directory.Name -match '^(upstream|switch)-') {
                    Write-Host "Earlier backup preserved: $($file.Directory.FullName)"
                    Write-Host 'Upgrade will create a NEW backup of the currently installed port. Restore defaults to the newest completed switch.'
                }
            }
        }
        $stage = New-ReleaseStage $root
        Assert-ProxyPreservation $root $stage ([bool]$DisableDxgiProxy)
        $remove = @(Get-PortFiles $root (-not $KeepHUDitor) ([bool]$DisableDxgiProxy) | Sort-Object -Unique)
        Show-Plan $root $stage $remove
        Show-PSVR2ToolkitGuidance
        if ($Action -eq 'Prepare' -or $WhatIf) { Write-Host 'Prepared only. No game files changed.' -ForegroundColor Green; return }
        if (-not (Confirm-Change "Install $($release.name)?")) { Write-Host 'Cancelled. Game unchanged.'; return }
        Assert-GameClosed
        $backup = Join-Path $BackupRoot ('switch-' + $Edition.ToLowerInvariant() + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
        $null = Invoke-PortTransaction $root $stage $remove $backup $settingsFile
        Write-Host "Installed $($release.name). Backup: $backup" -ForegroundColor Green
        Write-Host 'Start your OpenXR runtime, then launch from Steam. F10 opens the port menu. FG starts OFF.'
    } finally { if ($locked) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
} catch {
    Write-Host "`nSTOPPED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
