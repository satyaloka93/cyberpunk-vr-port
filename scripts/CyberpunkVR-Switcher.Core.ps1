# Shared by the Windows PowerShell 5.1 switcher and isolated fixture tests.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression

function Get-SafePath([string]$Root, [string]$Relative) {
    $relative = $Relative.Replace('/', '\')
    if (-not $relative -or $relative -match '(^|\\)\.\.?($|\\)|[:*?"<>|\x00-\x1f]' -or
        [IO.Path]::IsPathRooted($relative) -or $relative -match '(^|\\)[^\\]*[. ]($|\\)' -or
        $relative -match '(^|\\)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.[^\\]*)?($|\\)') {
        throw "Unsafe relative path: $Relative"
    }
    $base = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $full = [IO.Path]::GetFullPath((Join-Path $base $relative))
    if (-not $full.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) { throw "Path escapes root: $Relative" }
    $check = $full
    while ($check) {
        if (Test-Path -LiteralPath $check) {
            $item = Get-Item -LiteralPath $check -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing junction/symlink: $check" }
        }
        $parent = Split-Path -Parent $check
        if ($parent -eq $check) { break }
        $check = $parent
    }
    return $full
}

function Get-RelativeFiles([string]$Root, [string]$Relative) {
    $path = Get-SafePath $Root $Relative
    if (-not (Test-Path -LiteralPath $path)) { return }
    $item = Get-Item -LiteralPath $path -Force
    if (-not $item.PSIsContainer) { return $Relative }
    # Walk one level at a time; never follow a reparse point during enumeration.
    foreach ($child in Get-ChildItem -LiteralPath $path -Force) {
        Get-RelativeFiles $Root ($Relative.TrimEnd('\') + '\' + $child.Name)
    }
}

function Copy-VerifiedFile([string]$Source, [string]$Destination) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
    if ((Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash) { throw "Copy verification failed: $Destination" }
}

function Expand-SafeModZip([string]$Archive, [string]$Destination) {
    $Archive = $Archive.Trim().Trim([char[]]'"').Trim([char[]]"'").Trim()
    if (-not $Archive) { throw 'No ZIP file selected. Paste the full path to the downloaded .zip file.' }
    if (Test-Path -LiteralPath $Archive -PathType Container) {
        throw "Selected a folder, not a ZIP: $Archive. Use Copy as path on the downloaded .zip file, not its folder."
    }
    if ([IO.Path]::GetExtension($Archive) -ine '.zip') { throw "Select the downloaded .zip file: $Archive" }
    if (-not (Test-Path -LiteralPath $Archive -PathType Leaf)) { throw "ZIP file not found or inaccessible: $Archive" }
    try { $zip = [IO.Compression.ZipFile]::OpenRead($Archive) }
    catch { throw "Cannot read ZIP '$Archive': $($_.Exception.GetBaseException().Message)" }
    try {
        $seen = @{}; [long]$total = 0
        # Validate the entire archive before extracting anything.
        foreach ($entry in $zip.Entries) {
            $rel = $entry.FullName.Replace('/', '\').TrimEnd('\')
            if (-not $rel) { continue }
            $null = Get-SafePath $Destination $rel
            $top = ($rel -split '\\')[0]
            if ($top -notin @('bin','r6','red4ext','archive','engine','INSTALL.txt','UNINSTALL.txt','UNINSTALL.bat','CHANGELOG.txt','LICENSE','LICENSE.txt','README.md','README.txt')) {
                throw "Unexpected archive root: $rel. Supply the game-root ZIP, not a source archive."
            }
            if ($rel -ieq 'bin\x64\Cyberpunk2077.exe') { throw 'A mod archive must not replace the game executable.' }
            if ($entry.FullName -match '[\\/]$') { continue }
            if ($seen.ContainsKey($rel)) { throw "Duplicate archive path: $rel" }
            $seen[$rel] = $true
            $total += $entry.Length
            if ($entry.Length -gt 512MB -or $total -gt 2GB) { throw 'Archive exceeds the safety size limit.' }
        }
        foreach ($entry in $zip.Entries) {
            if ($entry.FullName -match '[\\/]$') { continue }
            $rel = $entry.FullName.Replace('/', '\')
            # Dependency licensing/readmes stay in the cache, not over the game's root files.
            if ($rel -in @('LICENSE','LICENSE.txt','README.md','README.txt')) { continue }
            $out = Get-SafePath $Destination $rel
            if (Test-Path -LiteralPath $out) { throw "Package overlap requires review: $rel" }
            New-Item -ItemType Directory -Path (Split-Path -Parent $out) -Force | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $out, $false)
        }
    } finally { $zip.Dispose() }
}

function Assert-ProxyPreservation([string]$Root, [string]$Stage, [bool]$DisableDxgiProxy = $false) {
    foreach ($name in @('dxgi.dll','version.dll')) {
        $existing = Get-SafePath $Root "bin\x64\$name"
        $incoming = Get-SafePath $Stage "bin\x64\$name"
        if ($name -eq 'dxgi.dll' -and $DisableDxgiProxy) { continue }
        if ((Test-Path -LiteralPath $existing -PathType Leaf) -and (Test-Path -LiteralPath $incoming -PathType Leaf) -and
            (Get-FileHash -LiteralPath $existing).Hash -ne (Get-FileHash -LiteralPath $incoming).Hash) {
            throw "Package would replace existing $name loader. Nothing installed; review the loader conflict explicitly."
        }
    }
}

function Get-PortFiles([string]$Root, [bool]$DisableLegacyHud = $true, [bool]$DisableProxy = $false) {
    foreach ($parent in @('bin\x64\plugins\cyber_engine_tweaks\mods','r6\scripts','red4ext\plugins')) {
        $path = Get-SafePath $Root $parent
        if (Test-Path -LiteralPath $path) {
            foreach ($child in Get-ChildItem -LiteralPath $path -Force) {
                if ($child.Name -like 'CyberpunkVRPort_*' -or $child.Name -like 'CyberpunkVR_*') {
                    Get-RelativeFiles $Root "$parent\$($child.Name)"
                }
            }
        }
    }
    foreach ($rel in @('r6\tweaks\vrport','r6\tweaks\vrcigarette','bin\x64\vrport.ini','bin\x64\vrport-launcher.ini',
        'bin\x64\openvr_api.dll','engine\config\platform\pc\vrcam_cpu_tweaks.ini','INSTALL.txt','UNINSTALL.txt','UNINSTALL.bat','CHANGELOG.txt')) {
        Get-RelativeFiles $Root $rel
    }
    foreach ($spec in @(@('bin\x64','CyberpunkVR_*Grip*.ini'), @('archive\pc\mod','cyberpunkvrport*'),
        @('archive\pc\mod','vrport_*'), @('archive\pc\mod','VRCigarette.archive.xl'), @('r6\input','CyberpunkVRPort_*'))) {
        $path = Get-SafePath $Root $spec[0]
        if (Test-Path -LiteralPath $path) {
            foreach ($file in Get-ChildItem -LiteralPath $path -Filter $spec[1] -File) { Get-RelativeFiles $Root "$($spec[0])\$($file.Name)" }
        }
    }
    if ($DisableLegacyHud) {
        foreach ($rel in @('bin\x64\plugins\cyber_engine_tweaks\mods\HUDitor','r6\scripts\HUDitor',
            'r6\input\HUDitor.xml','archive\pc\mod\HUDitor.archive','archive\pc\mod\HUDitor.archive.xl')) { Get-RelativeFiles $Root $rel }
    }
    if ($DisableProxy) { Get-RelativeFiles $Root 'bin\x64\dxgi.dll' }
}

function Save-Journal($Journal, [string]$Directory) {
    $temp = Join-Path $Directory 'transaction.next.json'
    $Journal | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $temp -Encoding UTF8
    Move-Item -LiteralPath $temp -Destination (Join-Path $Directory 'transaction.json') -Force
}

function Invoke-PortTransaction([string]$Root, [string]$Stage, [string[]]$Remove, [string]$Backup, [string]$SettingsFile = '') {
    if (Test-Path -LiteralPath $Backup) { throw "Backup already exists: $Backup" }
    $writes = @(Get-ChildItem -LiteralPath $Stage -Recurse -File | ForEach-Object { $_.FullName.Substring($Stage.TrimEnd('\').Length + 1) })
    $paths = @(@($Remove) + $writes | Sort-Object -Unique)
    $entries = @()
    foreach ($rel in $paths) {
        $target = Get-SafePath $Root $rel
        $source = Get-SafePath $Stage $rel
        if (Test-Path -LiteralPath $target -PathType Container) { throw "File/directory collision: $target" }
        $entries += [pscustomobject]@{ Path=$rel; Existed=(Test-Path -LiteralPath $target -PathType Leaf); BeforeHash=''; AfterHash=''; Write=(Test-Path -LiteralPath $source -PathType Leaf) }
    }
    New-Item -ItemType Directory -Path $Backup | Out-Null
    $journal = [pscustomobject]@{ Schema=1; GameRoot=$Root; Status='BackingUp'; Created=(Get-Date -Format o); SettingsFile=$SettingsFile; Entries=$entries }
    Save-Journal $journal $Backup
    # ALL originals copied and verified before the first game-file change.
    foreach ($entry in $entries) {
        if ($entry.Existed) {
            $target = Get-SafePath $Root $entry.Path
            Copy-VerifiedFile $target (Get-SafePath (Join-Path $Backup 'original') $entry.Path)
            $entry.BeforeHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        }
        if ($entry.Write) { $entry.AfterHash = (Get-FileHash -LiteralPath (Get-SafePath $Stage $entry.Path) -Algorithm SHA256).Hash }
    }
    if ($SettingsFile -and (Test-Path -LiteralPath $SettingsFile)) { Copy-VerifiedFile $SettingsFile (Join-Path $Backup 'UserSettings.before.json') }
    $journal.Status = 'Prepared'; Save-Journal $journal $Backup
    try {
        # Clean replacement, not an overlay extraction. ALL originals are already
        # backed up above. Only the explicitly selected old port/conflict files
        # are removed; dependencies and other mods are never swept by directory.
        foreach ($rel in @($Remove | Sort-Object -Unique)) {
            $target = Get-SafePath $Root $rel
            if (Test-Path -LiteralPath $target -PathType Leaf) { Remove-Item -LiteralPath $target -Force }
        }
        foreach ($entry in $entries) {
            $target = Get-SafePath $Root $entry.Path
            if ($entry.Write) { Copy-VerifiedFile (Get-SafePath $Stage $entry.Path) $target }
        }
        # Check against the pre-write stage hashes, not just source/destination
        # equality during each copy. Failure must use the same rollback path.
        foreach ($entry in $entries) {
            $target = Get-SafePath $Root $entry.Path
            if ($entry.Write) {
                if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $entry.AfterHash) {
                    throw "Installed file verification failed: $($entry.Path)"
                }
            } elseif (Test-Path -LiteralPath $target) {
                throw "Retired port file remains: $($entry.Path)"
            }
        }
        $journal.Status = 'Installed'; Save-Journal $journal $Backup
    } catch {
        $cause = $_
        try {
            foreach ($entry in $entries) {
                $target = Get-SafePath $Root $entry.Path
                if ($entry.Existed) { Copy-VerifiedFile (Get-SafePath (Join-Path $Backup 'original') $entry.Path) $target }
                elseif (Test-Path -LiteralPath $target -PathType Leaf) { Remove-Item -LiteralPath $target -Force }
            }
            $journal.Status = 'RolledBack'; Save-Journal $journal $Backup
        } catch { throw "Install AND rollback failed. Keep $Backup and run Restore. Recovery error: $_; install error: $cause" }
        throw "Install failed; original files restored. $cause"
    }
    return $journal
}

function Set-MigratedSettings([string]$Root, [string]$Stage) {
    $ini = Get-SafePath $Root 'bin\x64\vrport.ini'
    # Personal fit/input preferences, not experimental render/FG/diagnostic settings.
    $keys = @('xr_head_offset_x','xr_head_offset_y','xr_head_offset_z','xr_world_scale','xr_ipd_scale','xr_runtime','xr_force_fov',
        'xr_hmd_smooth','xr_hand_lerp_speed','xr_snap_turn','xr_snap_turn_angle_deg','xr_movement_source',
        'xr_movement_control','xr_physical_body_rotation','xr_immersive_holsters','xr_disable_mouse_y',
        'xr_haptics','xr_haptic_scale','xr_psvr2_haptic_backend','xr_weapon_laser_dot','xr_wheel_grab','xr_openxr_haptic_gain','xr_openxr_vehicle_haptic_gain','xr_two_hand_radius','xr_3dof_movement')
    $lines = @('# Migrated personal preferences; selected release supplies rendering defaults.', 'first_launch=0','show_launcher=1','xr_framegen=0')
    if (Test-Path -LiteralPath $ini) {
        foreach ($line in Get-Content -LiteralPath $ini) {
            if ($line -match '^\s*([a-zA-Z0-9_]+)\s*=') {
                if ($Matches[1] -in $keys -or $Matches[1] -match '^xr_(veh|bike)_head_offset_[xyz]$') { $lines += $line }
            }
        }
    }
    $dest = Get-SafePath $Stage 'bin\x64\vrport.ini'
    New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
    $lines | Set-Content -LiteralPath $dest -Encoding ASCII
    foreach ($rel in @('bin\x64\vrport-launcher.ini','red4ext\plugins\CyberpunkVR_Stereo\vrik_calibration.ini',
        'bin\x64\plugins\cyber_engine_tweaks\mods\CyberpunkVRPort_VRIK\vrik_settings.ini')) {
        $source = Get-SafePath $Root $rel
        if (Test-Path -LiteralPath $source -PathType Leaf) { Copy-VerifiedFile $source (Get-SafePath $Stage $rel) }
    }
    foreach ($file in Get-ChildItem -LiteralPath (Get-SafePath $Root 'bin\x64') -Filter 'CyberpunkVR_*Grip*.ini' -File) {
        $rel = 'bin\x64\' + $file.Name
        # Only keep hand poses still shipped by the selected release.
        if (Test-Path -LiteralPath (Get-SafePath $Stage $rel)) { Copy-VerifiedFile $file.FullName (Get-SafePath $Stage $rel) }
    }
}
