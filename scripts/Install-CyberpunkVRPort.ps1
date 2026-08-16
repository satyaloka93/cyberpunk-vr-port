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

# Expected outcomes -- a Nexus file not fetched yet, the game still running, the wrong folder --
# are not faults in this script. Throwing renders them as a PowerShell exception, which buries the
# one line the user needs under a caret diagram, a CategoryInfo block and a FullyQualifiedErrorId.
# Print the instruction and exit; the .cmd wrapper reports the code.
# Whether there is a human who can answer a prompt. Read-Host in a pipeline, a scheduled task or
# a dry run does not wait for someone -- it either blocks forever with nobody watching, or returns
# nothing and spins. Exiting is the better failure there.
function Test-CanPrompt {
    if ($WhatIfPreference) { return $false }
    if (-not [Environment]::UserInteractive) { return $false }
    try { if ([Console]::IsInputRedirected) { return $false } } catch { }
    return $true
}

function Stop-Clean {
    param([string]$Message, [int]$Code = 1)
    Write-Host ''
    Write-Host "  $Message" -ForegroundColor Yellow
    Write-Host ''
    exit $Code
}

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
            Stop-Clean "Not a Cyberpunk 2077 install: $resolved"
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
    Stop-Clean 'Cyberpunk 2077 was not found. Pass -GameRoot "D:\path\to\Cyberpunk 2077".'
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

# The user's real Downloads folder. Read from the registry rather than assuming
# %USERPROFILE%\Downloads, because it is relocatable and frequently relocated to another drive.
function Get-UserDownloadsFolder {
    try {
        $raw = (Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders' -ErrorAction SilentlyContinue).'{374DE290-123F-4565-9164-39C4925E467B}'
        if ($raw) {
            $expanded = [Environment]::ExpandEnvironmentVariables($raw)
            if (Test-Path -LiteralPath $expanded) { return (Resolve-Path -LiteralPath $expanded).Path }
        }
    } catch { }
    $guess = Join-Path $env:USERPROFILE 'Downloads'
    if (Test-Path -LiteralPath $guess) { return (Resolve-Path -LiteralPath $guess).Path }
    return $null
}

# Nexus filenames carry a download timestamp and differ between users, so the local copy is
# identified by content instead. This also revalidates every cached direct download.
#
# Searches the cache first and then the user's Downloads folder, because that is where a browser
# actually puts the four Nexus files -- expecting people to move them into a folder buried inside
# an extracted release is the kind of instruction that gets a mod called broken.
#
# Only the hashes the manifest asks for matter, so hashing stops as soon as all of them are found.
# Downloads folders are big -- this machine has 64 archives in it -- and hashing every one when
# the four wanted files turn up first is pure waiting. Newest first, for the same reason: the file
# just fetched from Nexus is the one being looked for.
function Build-HashIndex {
    param([string[]]$Directories, [string[]]$Wanted, [switch]$TopLevelOnly)

    $index = @{}
    $want = @{}
    foreach ($w in $Wanted) { if ($w) { $want[$w.ToLowerInvariant()] = $true } }

    # How many of the WANTED hashes are in hand. Deliberately not $index.Count: that counts every
    # archive hashed so far, so a cache holding enough unrelated zips would satisfy a naive
    # comparison and stop the search before the Downloads folder was ever looked at.
    $foundCount = {
        $n = 0
        foreach ($k in $want.Keys) { if ($index.ContainsKey($k)) { $n++ } }
        return $n
    }

    foreach ($dir in $Directories) {
        if (-not $dir -or -not (Test-Path -LiteralPath $dir)) { continue }
        if ($want.Count -gt 0 -and (& $foundCount) -ge $want.Count) { break }

        # The cache is ours and small, so recurse it. Downloads is the user's and can be enormous,
        # so only the top level -- which is where a browser drops things.
        $isCache = -not $TopLevelOnly -or $dir -eq $Directories[0]
        $files = @(Get-ChildItem -LiteralPath $dir -File -Recurse:$isCache -ErrorAction SilentlyContinue |
                   Where-Object { $_.Extension -in '.zip', '.7z', '.rar' } |
                   Sort-Object LastWriteTime -Descending)
        if ($files.Count -eq 0) { continue }
        Write-Host "[.] Hashing up to $($files.Count) archive(s) in $dir ..."

        foreach ($file in $files) {
            $hash = Get-Sha256 $file.FullName
            if (-not $index.ContainsKey($hash)) { $index[$hash] = $file.FullName }
            # Everything the manifest wants has been located; nothing left to look for.
            if ($want.Count -gt 0 -and (& $foundCount) -ge $want.Count) { break }
        }
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
        #
        # The $ErrorActionPreference = 'Stop' at the top of this script applies to native commands
        # too: anything curl writes to stderr becomes a TERMINATING NativeCommandError before the
        # exit code below can be looked at. curl reports every failure on stderr, so an ordinary
        # 404 killed the whole install with a PowerShell stack trace instead of being handled here
        # and collected into the manual-download list. Relaxed only around this call.
        $stderr = & {
            $ErrorActionPreference = 'Continue'
            & $script:CurlPath -sS -L --fail --max-time 600 -o $OutFile $Uri 2>&1
        }
        $curlExit = $LASTEXITCODE
        if ($curlExit -eq 0 -and (Test-Path -LiteralPath $OutFile)) { return $true }
        # 2>&1 delivers curl's stderr as ErrorRecord objects, and Out-String renders those as the
        # whole formatted block -- script path, caret diagram, CategoryInfo. Take just the message
        # so the user sees "curl: (22) The requested URL returned error: 404" and nothing else.
        $script:LastDownloadError = "curl: " + ((@($stderr) | ForEach-Object {
            if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.Exception.Message } else { [string]$_ }
        }) -join ' ').Replace('curl.exe : ', '').Replace('curl: ', '').Trim()
        if (Test-Path -LiteralPath $OutFile) { Remove-Item -LiteralPath $OutFile -Force }

        # Exit 22 is --fail firing: the server ANSWERED, with an HTTP error. The transport is
        # fine and the URL is wrong, so the PowerShell fallback cannot do better -- and on a
        # machine where PowerShell egress is blocked it fails for an unrelated reason and
        # overwrites a precise "404" with a misleading "Unable to connect to the remote server".
        # Keep curl's verdict.
        if ($curlExit -eq 22) { return $false }
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
        # A 404 on the port's own release asset almost always means the manifest names a release
        # that is not published (yet, or any more) -- not a broken machine. Say so, and point at
        # the way out: the file is matched by SHA-256 from the downloads folder, so dropping it
        # there works without any download at all. This is also how a release is tested before
        # it is published.
        if ((Get-Prop $Mod 'id') -eq 'cyberpunk-vr-port' -and $script:LastDownloadError -match '40[34]') {
            Write-Host ""
            Write-Host "    $($Mod.name) is not downloadable at that URL." -ForegroundColor Yellow
            Write-Host "    Usually this means the release is not published yet."
            Write-Host "    You can supply it directly instead -- put the file here:"
            Write-Host "      $Directory"
            Write-Host "    with sha256 $($expected)"
            Write-Host "    The name does not matter; it is matched by hash."
            Write-Host ""
        }
        return $null   # collected into the manual-download list by the caller
    }

    $actual = Get-Sha256 $target
    if ($expected -and $actual -ne $expected.ToLowerInvariant()) {
        Remove-Item -LiteralPath $target -Force
        throw "$($Mod.name): SHA-256 mismatch. Expected $expected, got $actual. Download discarded."
    }
    return $target
}

# The release zip ships this script inside it, so the port's own files are normally sitting right
# beside it already. Copying them is both simpler and more honest than downloading the very
# archive the user just extracted -- and it removes the need for the manifest to record a hash of
# itself, which could never be stable anyway (Compress-Archive stamps timestamps, so every rebuild
# changes it).
function Get-LocalPortPayload {
    param([string]$Base)
    $roots = @('bin', 'r6', 'red4ext', 'archive', 'engine') |
        Where-Object { Test-Path -LiteralPath (Join-Path $Base $_) }
    # The marker file, not just the folders: an empty bin\ beside the script is not a payload.
    if (-not (Test-Path -LiteralPath (Join-Path $Base 'red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll'))) {
        return $null
    }
    return $roots
}

function Copy-LocalPayload {
    param([string]$Base, [string[]]$Roots, [string]$Destination, [string]$ModName)

    $written = 0; $overwritten = 0
    foreach ($r in $Roots) {
        foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Base $r) -Recurse -File)) {
            $relative = $f.FullName.Substring($Base.Length).TrimStart('\')
            $outPath = Join-Path $Destination $relative
            $full = [System.IO.Path]::GetFullPath($outPath)
            if (-not $full.StartsWith([System.IO.Path]::GetFullPath($Destination), [StringComparison]::OrdinalIgnoreCase)) {
                throw "$ModName tried to write outside the game folder: $relative"
            }
            if (Test-Path -LiteralPath $full) { $overwritten++ } else { $written++ }
            if ($PSCmdlet.ShouldProcess($full, 'copy')) {
                New-Item -ItemType Directory -Path (Split-Path -Parent $full) -Force | Out-Null
                Copy-Item -LiteralPath $f.FullName -Destination $full -Force
            }
        }
    }
    return [pscustomobject]@{ New = $written; Overwritten = $overwritten; Roots = $Roots }
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
    Stop-Clean 'Close Cyberpunk 2077, Mod Organizer 2 and REDlauncher first -- files are locked while they run.'
}

Write-Host ''
Write-Host "  $($manifest.name)  ($($manifest.releaseTag))"
Write-Host "  game     : $root"
Write-Host "  target   : Cyberpunk $($manifest.targetGameVersion)"
Write-Host "  auto-cache: $DownloadDir   (automatic downloads only)"
Write-Host ''

$script:LocalPayloadRoots = if ($SkipVRPort) { $null } else { Get-LocalPortPayload -Base $RepoRoot }
if ($script:LocalPayloadRoots) {
    Write-Host "  source   : this folder (the port's files are here -- nothing to download)"
    Write-Host ''
}

$mods = @($manifest.mods | Sort-Object priority)
if ($SkipVRPort) { $mods = @($mods | Where-Object { $_.id -ne 'cyberpunk-vr-port' }) }

if (-not $script:CurlPath) {
    Write-Host '[!] curl.exe was not found. It ships with Windows 10 1803 and later.'
    Write-Host '    Downloads will fall back to PowerShell, which some machines block outbound.'
    Write-Host '    Install it with:  winget install curl.cURL     (or https://curl.se/windows/)'
    Write-Host '    Without it you can still install everything manually -- see the list below.'
    Write-Host ''
}

# Search the cache first, then the user's Downloads folder. Only ever WRITE to the cache --
# Downloads is the user's, and an installer has no business leaving files in it.
$searchDirs = @($DownloadDir)
$userDownloads = Get-UserDownloadsFolder
# The cache folder does not exist until something is downloaded into it, and under Set-StrictMode
# reading .Path off the nothing that Resolve-Path returns is a terminating error.
$cacheResolved = if (Test-Path -LiteralPath $DownloadDir) { (Resolve-Path -LiteralPath $DownloadDir).Path } else { $DownloadDir }
if ($userDownloads -and $userDownloads -ne $cacheResolved) {
    $searchDirs += $userDownloads
    Write-Host "  your downloads: $userDownloads   (put hand-fetched files here)"
    Write-Host ''
}
$index = Build-HashIndex -Directories $searchDirs -Wanted @($mods | ForEach-Object { Get-Prop $_ 'sha256' }) -TopLevelOnly

# Where a person puts a file they fetched by hand: their Downloads folder. The cache is for
# automatic downloads and is not somewhere anyone should be told to go and put things.
$manualDir = if ($userDownloads) { $userDownloads } else { $DownloadDir }

# Prints only -- it does NOT exit. Pass 1 wants to report and then wait for the user to go and
# fetch the files; pass 2 wants to report and give up. Deciding that here would deny pass 1 the
# choice.
function Write-MissingReport {
    param(
        [System.Collections.Generic.List[object]]$Missing,
        [string]$Directory,
        [switch]$TriedDownloading
    )
    if ($Missing.Count -eq 0) { return }
    $missing = $Missing
    $nexusCount = @($missing | Where-Object { $_.PSObject.Properties.Name -contains 'nexusModId' }).Count
    $directCount = $missing.Count - $nexusCount

    Write-Host ''
    Write-Host "[X] $($missing.Count) file(s) must be downloaded by hand." -ForegroundColor Yellow
    Write-Host "    Save them here -- your Downloads folder:"
    Write-Host "      $Directory"
    Write-Host '    Filenames do not matter -- they are matched by SHA-256, then verified.'
    if ($nexusCount -gt 0) {
        Write-Host ''
        Write-Host '    Nexus files can never be automated: Nexus issues direct links only to'
        Write-Host '    premium accounts and its terms forbid redistribution.'
    }
    if ($directCount -gt 0 -and $TriedDownloading) {
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
    # Every entry here has to be fetchable by hand, so every entry gets a URL. Printing a name on
    # its own, or a bare "sha256" with nothing after it, leaves the user with no way to act.
    foreach ($mod in $missing) {
        Write-Host "    $($mod.name)"
        $link = Get-Prop $mod 'manualUrl'
        if (-not $link) { $link = Get-Prop $mod 'url' }
        if ($link) {
            Write-Host "      $link"
        } else {
            # No link anywhere in the manifest: say that plainly rather than printing nothing.
            Write-Host "      (no download URL in the manifest -- see the release page)"
            Write-Host "      https://github.com/satyaloka93/cyberpunk-vr-port/releases"
        }
        $sha = Get-Prop $mod 'sha256'
        if ($sha) {
            Write-Host "      sha256 $sha"
        } else {
            Write-Host "      (no hash recorded -- it will be installed without verification)"
        }
        # The port's own entry inside a release zip is marked local-only: its files are supposed to
        # be beside this script. Reaching this list means they were not, so name the real problem
        # instead of asking the user to download something they already have.
        if (Get-Prop $mod 'localOnly') {
            Write-Host "      This should have been installed from the folder containing this script."
            Write-Host "      Extract the whole release zip and run scripts\Install-CyberpunkVRPort.cmd"
            Write-Host "      from inside it, so bin\, r6\, red4ext\, archive\ and engine\ sit beside scripts\."
        }
    }
    Write-Host ''
}

# HUDitor ships two things that are wrong for VR, and both are in ITS files, not ours:
#
#   * its editor is bound to F7, which this port already polls for HMD recenter;
#   * it refuses to open unless the quest fact "unlock_car_hud_dpad" is set, and reports that
#     by borrowing the combat-restriction toast -- a 2 s message that reads as unrelated and is
#     effectively invisible in a headset. On a save without the fact, HUDitor looks broken.
#
# These are patched IN PLACE after HUDitor is installed rather than by shipping modified copies
# of its files. Redistributing another author's mod, even patched, is not something this
# installer does -- and it would go stale against every HUDitor release. Editing what the user
# just installed keeps the source policy intact.
#
# Idempotent, and never fatal: a HUDitor that fails to patch is a HUDitor with its stock
# behaviour, which is worth a warning and nothing more.
function Update-HUDitorForVR {
    param([string]$GameRoot)

    $xml   = Join-Path $GameRoot 'r6\input\HUDitor.xml'
    $cfg   = Join-Path $GameRoot 'r6\scripts\HUDitor\config.reds'
    $ctrl  = Join-Path $GameRoot 'r6\scripts\HUDitor\inkHUDGameController.reds'
    $cache = Join-Path $GameRoot 'r6\cache\inputUserMappings.xml'
    if (-not (Test-Path -LiteralPath $xml)) { return }

    $done = @()
    try {
        # 1. F7 -> Delete, in the binding and in the config default it is tied to by overridableUI.
        foreach ($f in @($xml, $cfg)) {
            if (-not (Test-Path -LiteralPath $f)) { continue }
            $t = Get-Content -LiteralPath $f -Raw
            if ($t -match 'IK_F7') {
                [System.IO.File]::WriteAllText($f, ($t -replace 'IK_F7', 'IK_Delete'))
                $done += (Split-Path $f -Leaf)
            }
        }

        # Input Loader rebuilds this from r6\input when a source changes, but it does NOT recreate
        # it if absent, so patch the merged copy directly rather than deleting it and hoping.
        if (Test-Path -LiteralPath $cache) {
            $t = Get-Content -LiteralPath $cache -Raw
            $m = [regex]::Match($t, '(?s)<mapping name="HUDitor_Editor_Button".*?</mapping>')
            if ($m.Success -and $m.Value -match 'IK_F7') {
                $t = $t.Remove($m.Index, $m.Length).Insert($m.Index, ($m.Value -replace 'IK_F7', 'IK_Delete'))
                [System.IO.File]::WriteAllText($cache, $t)
                $done += 'inputUserMappings.xml'
            }
        }

        # 2. Drop the progression gate.
        if (Test-Path -LiteralPath $ctrl) {
            $t = Get-Content -LiteralPath $ctrl -Raw
            if ($t -match 'unlock_car_hud_dpad') {
                $t = [regex]::Replace($t, '(?s)\s*huditorAvailable\s*=\s*this\.questSystem\.GetFact\(n"unlock_car_hud_dpad"\)\s*>\s*0;\s*if\s*!huditorAvailable\s*\{.*?\};', '')
                $t = [regex]::Replace($t, '\r?\n\s*let huditorAvailable: Bool;', '')
                [System.IO.File]::WriteAllText($ctrl, $t)
                $done += 'inkHUDGameController.reds'
            }
        }
    } catch {
        Write-Host "[!] HUDitor VR patch failed: $($_.Exception.Message)"
        Write-Host '    HUDitor keeps its stock behaviour: editor on F7, and blocked on saves'
        Write-Host '    where the HUD-customisation unlock has not happened yet.'
        return
    }

    if ($done.Count -gt 0) {
        Write-Host "[+] HUDitor patched for VR -- editor on Delete (F7 is HMD recenter), unlock gate removed."
    } else {
        Write-Host '[.] HUDitor already patched for VR.'
    }
}

# Resolve every source before writing anything, so a missing Nexus file cannot leave the game
# folder half-installed.
$resolved = @{}                                     # id -> plan item, assembled in priority order below
$missing  = [System.Collections.Generic.List[object]]::new()

# PASS 1 -- everything that can ONLY come from disk: the Nexus files, and the port itself when its
# payload is beside this script. No network at all.
#
# This runs FIRST on purpose. It used to be one pass, so the eight automatic downloads ran, and
# only then did the script announce that four Nexus files were missing and exit -- making the user
# wait through several minutes of downloading to be told to go do something by hand, then run the
# whole thing again. Now a missing manual file costs nothing and is reported immediately.
#
# And rather than exiting, it WAITS. Fetching four files from Nexus takes a couple of minutes, and
# telling someone to run the whole installer again afterwards -- re-detecting the game, re-hashing
# every archive -- is pointless when the script can simply look again. Ctrl+C still quits.
while ($true) {
    $resolved = @{}
    $missing  = [System.Collections.Generic.List[object]]::new()

    foreach ($mod in $mods) {
        $id = Get-Prop $mod 'id'
        if ($id -eq 'cyberpunk-vr-port' -and $script:LocalPayloadRoots) {
            $resolved[$id] = [pscustomobject]@{ Mod = $mod; Path = $null; LocalBase = $RepoRoot; LocalRoots = $script:LocalPayloadRoots }
            continue
        }
        if ($mod.PSObject.Properties.Name -contains 'nexusModId') {
            $modHash = Get-Prop $mod 'sha256'
            $hash = if ($modHash) { $modHash.ToLowerInvariant() } else { $null }
            if ($hash -and $index.ContainsKey($hash)) {
                $resolved[$id] = [pscustomobject]@{ Mod = $mod; Path = $index[$hash] }
            } else {
                $missing.Add($mod)
            }
        }
    }
    if ($missing.Count -eq 0) { break }

    Write-MissingReport -Missing $missing -Directory $manualDir -TriedDownloading:$false

    # Nothing can answer a prompt in a dry run, a pipeline or a scheduled task, and hanging there
    # forever is worse than exiting. Only wait when there is a person to wait for.
    if (-not (Test-CanPrompt)) {
        Stop-Clean 'Nothing was installed. Re-run once the files above are in place.'
    }

    Write-Host '    Leave this window open and fetch them now, into the folder above.'
    Write-Host ''
    Read-Host '  Press Enter to look again (Ctrl+C to quit)' | Out-Null
    Write-Host ''
    $index = Build-HashIndex -Directories $searchDirs -Wanted @($mods | ForEach-Object { Get-Prop $_ 'sha256' }) -TopLevelOnly
}

# PASS 2 -- the automatic downloads, now that we know the manual files are all present.
foreach ($mod in $mods) {
    $id = Get-Prop $mod 'id'
    if ($resolved.ContainsKey($id)) { continue }
    $got = Get-DirectDownload -Index $index -Mod $mod -Directory $DownloadDir
    if ($got) {
        $resolved[$id] = [pscustomobject]@{ Mod = $mod; Path = $got }
    } elseif (-not $WhatIfPreference) {
        $missing.Add($mod)   # last resort: the user fetches it in a browser
    }
}
if ($missing.Count -gt 0) {
    Write-MissingReport -Missing $missing -Directory $manualDir -TriedDownloading:$true
    Stop-Clean 'Nothing was installed. Re-run once the files above are in place.'
}

# Install order is manifest priority, not resolution order.
$plan = [System.Collections.Generic.List[object]]::new()
foreach ($mod in $mods) {
    $id = Get-Prop $mod 'id'
    if ($resolved.ContainsKey($id)) { $plan.Add($resolved[$id]) }
}


Write-Host "[+] All $($plan.Count) sources present and hash-verified."
Write-Host ''

$totalNew = 0
$totalOver = 0
foreach ($item in $plan) {
    $result = if (Get-Prop $item 'LocalRoots') {
        Copy-LocalPayload -Base $item.LocalBase -Roots $item.LocalRoots -Destination $root -ModName $item.Mod.name
    } else {
        Expand-ModArchive -ArchivePath $item.Path -Destination $root -ModName $item.Mod.name
    }
    $totalNew += $result.New
    $totalOver += $result.Overwritten
    Write-Host ("[+] {0,-34} {1,5} new {2,5} replaced   [{3}]" -f `
        $item.Mod.name, $result.New, $result.Overwritten, ($result.Roots -join ' '))
}

Write-Host ''
Update-HUDitorForVR -GameRoot $root

Write-Host ''
Write-Host "[ok] $($plan.Count) mods installed -- $totalNew files added, $totalOver replaced."
Write-Host '     Launch Cyberpunk normally from Steam. Mod Organizer is not used.'
Write-Host "     Log after first launch: $root\bin\x64\cyberpunkvrport.log"
