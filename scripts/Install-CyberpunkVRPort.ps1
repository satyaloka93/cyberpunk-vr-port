# Direct installer for the CyberpunkVR Port dependency stack.
#
# Installs every mod straight into the Cyberpunk 2077 folder, so the game launches normally
# from Steam. This is the alternative to the Wabbajack/MO2 route, which only works when the
# game is launched through Mod Organizer -- MO2's virtual filesystem is what makes CET and
# RED4ext visible to the game there, so those mods are never actually in the game folder.
#
# Source policy matches the Wabbajack definition: nothing is redistributed. The immutable
# GitHub sources are downloaded and hash-verified here. Nexus files cannot be automated --
# Nexus only issues direct download links to premium accounts, and its terms forbid
# redistribution -- so those are matched out of a local folder BY HASH and the script prints
# the exact page links for any that are missing.
#
# Usage:
#   pwsh scripts\Install-CyberpunkVRPort.ps1 -WhatIf          # show the plan, change nothing
#   pwsh scripts\Install-CyberpunkVRPort.ps1
#   pwsh scripts\Install-CyberpunkVRPort.ps1 -GameRoot "D:\...\Cyberpunk 2077" -DownloadDir D:\dl

[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$GameRoot,

    # Where Nexus zips are looked for, and where direct downloads are cached.
    [string]$DownloadDir,

    [string]$ManifestPath,

    # Skip the packaged VR port release, e.g. when deploying it from source with
    # deploy_stereo.ps1 instead.
    [switch]$SkipVRPort,

    # Proceed even when an archive contains unexpected top-level directories.
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest is far slower with a progress bar

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $ManifestPath) { $ManifestPath = Join-Path $RepoRoot 'wabbajack\manifest.json' }
if (-not $DownloadDir)  { $DownloadDir  = Join-Path $RepoRoot 'build\installer-cache' }

# Cyberpunk mods are packaged to extract at the game root. Anything outside this set is either
# a repackaged archive or something that does not belong in the game folder.
$AllowedRoots = @('bin', 'r6', 'red4ext', 'archive', 'engine', 'mods', 'tools', 'plugins')

# Strict mode turns access to an absent property into a terminating error, and both the
# registry values and several manifest fields are legitimately optional.
function Get-Prop {
    param($Object, [string]$Name)
    if ($null -eq $Object) { return $null }
    if ($Object.PSObject.Properties.Name -notcontains $Name) { return $null }
    return $Object.$Name
}

function Find-CyberpunkRoot {
    if ($GameRoot) {
        $resolved = (Resolve-Path -LiteralPath $GameRoot).Path
        if (-not (Test-Path -LiteralPath (Join-Path $resolved 'bin\x64\Cyberpunk2077.exe'))) {
            throw "Not a Cyberpunk 2077 install: $resolved"
        }
        return $resolved
    }

    $candidates = [System.Collections.Generic.List[string]]::new()
    foreach ($key in @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1091500')) {
        $location = Get-Prop (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue) 'InstallLocation'
        if ($location) { $candidates.Add($location) }
    }
    $steamPath = Get-Prop (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue) 'SteamPath'
    if ($steamPath) {
        $steamPath = $steamPath -replace '/', '\'
        $candidates.Add((Join-Path $steamPath 'steamapps\common\Cyberpunk 2077'))
        $libraryFile = Join-Path $steamPath 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $libraryFile) {
            foreach ($match in [regex]::Matches((Get-Content -Raw -LiteralPath $libraryFile), '"path"\s+"([^"]+)"')) {
                $candidates.Add((Join-Path ($match.Groups[1].Value -replace '\\\\', '\') 'steamapps\common\Cyberpunk 2077'))
            }
        }
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'bin\x64\Cyberpunk2077.exe')) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'Cyberpunk 2077 was not found. Pass -GameRoot "D:\path\to\Cyberpunk 2077".'
}

function Get-Sha256 {
    param([string]$Path)
    # Deliberately not Get-FileHash: under -WhatIf it is suppressed, returns nothing, and every
    # verification then fails on a missing .Hash property. Hashing is a pure read and must run
    # in a dry run too, so go straight to .NET where -WhatIf cannot intercept it.
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($stream)) -replace '-', '').ToLowerInvariant()
    } finally {
        $stream.Dispose()
        $sha.Dispose()
    }
}

# Nexus filenames carry a download timestamp and differ between users, so the local copy is
# identified by content instead. This also revalidates every cached direct download.
function Build-HashIndex {
    param([string]$Directory)
    $index = @{}
    if (-not (Test-Path -LiteralPath $Directory)) { return $index }
    $files = @(Get-ChildItem -LiteralPath $Directory -File -Recurse -Include *.zip, *.7z, *.rar -ErrorAction SilentlyContinue)
    if ($files.Count -gt 0) {
        Write-Host "[.] Hashing $($files.Count) archive(s) in $Directory ..."
    }
    foreach ($file in $files) {
        $hash = Get-Sha256 $file.FullName
        if (-not $index.ContainsKey($hash)) { $index[$hash] = $file.FullName }
    }
    return $index
}

# curl.exe ships with Windows 10 1803 and later. It is preferred over Invoke-WebRequest for two
# reasons: it handles redirects and TLS without the .NET Framework quirks in Windows PowerShell
# 5.1, and it is a separate process, so it still works on machines that block powershell.exe
# from making outbound connections -- a common hardening rule.
$script:CurlPath = (Get-Command curl.exe -ErrorAction SilentlyContinue).Source
$script:LastDownloadError = $null
$script:PowerShellEgressBlocked = $null

function Test-PowerShellEgressBlocked {
    if ($null -ne $script:PowerShellEgressBlocked) { return $script:PowerShellEgressBlocked }
    $script:PowerShellEgressBlocked = $false
    try {
        $rules = @(Get-NetFirewallRule -Enabled True -Direction Outbound -Action Block -ErrorAction SilentlyContinue |
                   Where-Object { $_.DisplayName -match 'powershell' })
        if ($rules.Count -gt 0) {
            $script:PowerShellEgressBlocked = ($rules | ForEach-Object { $_.DisplayName }) -join ', '
        }
    } catch { }
    return $script:PowerShellEgressBlocked
}

function Invoke-Download {
    param([string]$Uri, [string]$OutFile)

    if ($script:CurlPath) {
        # --fail so an HTTP error page is never written out as if it were the archive.
        $stderr = & $script:CurlPath -sS -L --fail --max-time 600 -o $OutFile $Uri 2>&1
        if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $OutFile)) { return $true }
        $script:LastDownloadError = "curl.exe: $stderr"
        if (Test-Path -LiteralPath $OutFile) { Remove-Item -LiteralPath $OutFile -Force }
    }

    try {
        Invoke-WebRequest -Uri $Uri -OutFile $OutFile -UseBasicParsing -TimeoutSec 600
        return $true
    } catch {
        $script:LastDownloadError = $_.Exception.Message
        if (Test-Path -LiteralPath $OutFile) { Remove-Item -LiteralPath $OutFile -Force }
        return $false
    }
}

function Get-DirectDownload {
    param([hashtable]$Index, $Mod, [string]$Directory)

    $expected = Get-Prop $Mod 'sha256'
    if ($expected -and $Index.ContainsKey($expected.ToLowerInvariant())) {
        return $Index[$expected.ToLowerInvariant()]
    }

    $target = Join-Path $Directory (Get-Prop $Mod 'fileName')
    if ($WhatIfPreference) {
        # A dry run must not reach the network. Report the intent and leave the plan short.
        Write-Host "[>] would download $($Mod.name)"
        return $null
    }
    Write-Host "[>] Downloading $($Mod.name)"
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    if (-not (Invoke-Download -Uri (Get-Prop $Mod 'url') -OutFile $target)) {
        Write-Host "    failed: $($script:LastDownloadError)"
        return $null   # collected into the manual-download list by the caller
    }

    $actual = Get-Sha256 $target
    if ($expected -and $actual -ne $expected.ToLowerInvariant()) {
        Remove-Item -LiteralPath $target -Force
        throw "$($Mod.name): SHA-256 mismatch. Expected $expected, got $actual. Download discarded."
    }
    return $target
}

function Expand-ModArchive {
    param([string]$ArchivePath, [string]$Destination, [string]$ModName)

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $roots = @($zip.Entries | ForEach-Object { ($_.FullName -split '[\\/]')[0] } |
                   Where-Object { $_ } | Sort-Object -Unique)
        $unexpected = @($roots | Where-Object { $_ -notin $AllowedRoots })
        if ($unexpected.Count -gt 0) {
            $message = "$ModName extracts unexpected top-level entries: $($unexpected -join ', ')"
            if (-not $Force) { throw "$message. Re-run with -Force to install it anyway." }
            Write-Host "[!] $message (continuing: -Force)"
        }

        $written = 0
        $overwritten = 0
        foreach ($entry in $zip.Entries) {
            if (-not $entry.Name) { continue }   # directory entry

            # Reject traversal rather than trusting third-party archive paths.
            $relative = $entry.FullName -replace '/', '\'
            if ($relative -match '(^|\\)\.\.(\\|$)' -or [System.IO.Path]::IsPathRooted($relative)) {
                throw "$ModName contains an unsafe entry path: $($entry.FullName)"
            }

            $outPath = Join-Path $Destination $relative
            $full = [System.IO.Path]::GetFullPath($outPath)
            if (-not $full.StartsWith([System.IO.Path]::GetFullPath($Destination), [StringComparison]::OrdinalIgnoreCase)) {
                throw "$ModName tried to write outside the game folder: $($entry.FullName)"
            }

            if (Test-Path -LiteralPath $full) { $overwritten++ } else { $written++ }
            if ($PSCmdlet.ShouldProcess($full, 'extract')) {
                New-Item -ItemType Directory -Path (Split-Path -Parent $full) -Force | Out-Null
                [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $full, $true)
            }
        }
        return [pscustomobject]@{ New = $written; Overwritten = $overwritten; Roots = $roots }
    } finally {
        $zip.Dispose()
    }
}

# ------------------------------------------------------------------ run

$root = Find-CyberpunkRoot
if (-not (Test-Path -LiteralPath $ManifestPath)) { throw "Manifest not found: $ManifestPath" }
$manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json

if (Get-Process Cyberpunk2077, ModOrganizer, REDlauncher -ErrorAction SilentlyContinue) {
    throw 'Close Cyberpunk 2077, Mod Organizer 2 and REDlauncher first -- files are locked while they run.'
}

Write-Host ''
Write-Host "  $($manifest.name)  ($($manifest.releaseTag))"
Write-Host "  game     : $root"
Write-Host "  target   : Cyberpunk $($manifest.targetGameVersion)"
Write-Host "  downloads: $DownloadDir"
Write-Host ''

$mods = @($manifest.mods | Sort-Object priority)
if ($SkipVRPort) { $mods = @($mods | Where-Object { $_.id -ne 'cyberpunk-vr-port' }) }

if (-not $script:CurlPath) {
    Write-Host '[!] curl.exe was not found. It ships with Windows 10 1803 and later.'
    Write-Host '    Downloads will fall back to PowerShell, which some machines block outbound.'
    Write-Host '    Install it with:  winget install curl.cURL     (or https://curl.se/windows/)'
    Write-Host '    Without it you can still install everything manually -- see the list below.'
    Write-Host ''
}

$index = Build-HashIndex -Directory $DownloadDir

# Resolve every source before writing anything, so a missing Nexus file cannot leave the game
# folder half-installed.
$plan = [System.Collections.Generic.List[object]]::new()
$missing = [System.Collections.Generic.List[object]]::new()

foreach ($mod in $mods) {
    $isNexus = [bool]($mod.PSObject.Properties.Name -contains 'nexusModId')
    if ($isNexus) {
        $modHash = Get-Prop $mod 'sha256'
        $hash = if ($modHash) { $modHash.ToLowerInvariant() } else { $null }
        if ($hash -and $index.ContainsKey($hash)) {
            $plan.Add([pscustomobject]@{ Mod = $mod; Path = $index[$hash] })
        } else {
            $missing.Add($mod)
        }
    } else {
        $resolved = Get-DirectDownload -Index $index -Mod $mod -Directory $DownloadDir
        if ($resolved) {
            $plan.Add([pscustomobject]@{ Mod = $mod; Path = $resolved })
        } elseif (-not $WhatIfPreference) {
            $missing.Add($mod)   # last resort: the user fetches it in a browser
        }
    }
}

if ($missing.Count -gt 0) {
    $nexusCount = @($missing | Where-Object { $_.PSObject.Properties.Name -contains 'nexusModId' }).Count
    $directCount = $missing.Count - $nexusCount

    Write-Host ''
    Write-Host "[X] $($missing.Count) file(s) must be downloaded by hand." -ForegroundColor Yellow
    Write-Host "    Save them anywhere under: $DownloadDir"
    Write-Host '    Filenames do not matter -- they are matched by SHA-256, then verified.'
    if ($nexusCount -gt 0) {
        Write-Host ''
        Write-Host '    Nexus files can never be automated: Nexus issues direct links only to'
        Write-Host '    premium accounts and its terms forbid redistribution.'
    }
    if ($directCount -gt 0) {
        Write-Host ''
        Write-Host "    $directCount of these are normally downloaded automatically, so something"
        Write-Host '    blocked network access.'
        $blocked = Test-PowerShellEgressBlocked
        if ($blocked) {
            Write-Host "    This machine has firewall rules blocking PowerShell outbound: $blocked"
            if (-not $script:CurlPath) {
                Write-Host '    Installing curl.exe would work around it -- curl is a separate process.'
            }
        } elseif (-not $script:CurlPath) {
            Write-Host '    Installing curl.exe may help: winget install curl.cURL'
        }
        if ($script:LastDownloadError) { Write-Host "    Last error: $($script:LastDownloadError)" }
    }
    Write-Host ''
    foreach ($mod in $missing) {
        Write-Host "    $($mod.name)"
        $link = Get-Prop $mod 'manualUrl'
        if (-not $link) { $link = Get-Prop $mod 'url' }
        if ($link) { Write-Host "      $link" }
        Write-Host "      sha256 $(Get-Prop $mod 'sha256')"
    }
    Write-Host ''
    throw 'Nothing was installed. Re-run once the files above are in place.'
}

Write-Host "[+] All $($plan.Count) sources present and hash-verified."
Write-Host ''

$totalNew = 0
$totalOver = 0
foreach ($item in $plan) {
    $result = Expand-ModArchive -ArchivePath $item.Path -Destination $root -ModName $item.Mod.name
    $totalNew += $result.New
    $totalOver += $result.Overwritten
    Write-Host ("[+] {0,-34} {1,5} new {2,5} replaced   [{3}]" -f `
        $item.Mod.name, $result.New, $result.Overwritten, ($result.Roots -join ' '))
}

Write-Host ''
Write-Host "[ok] $($plan.Count) mods installed -- $totalNew files added, $totalOver replaced."
Write-Host '     Launch Cyberpunk normally from Steam. Mod Organizer is not used.'
Write-Host "     Log after first launch: $root\bin\x64\cyberpunkvrport.log"
