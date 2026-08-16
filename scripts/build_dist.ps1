# Assemble a tester package under dist\, laid out exactly as it must land in the game root.
#
# Everything comes from the repo or from a build output -- nothing is read out of the installed
# game -- so what a tester gets is what is committed. Run scripts\sync_assets.ps1 first if the
# yamls, archives or grip poses have been touched game-side since the last pull.
#
# Usage:
#   pwsh scripts\build_dist.ps1
#   pwsh scripts\build_dist.ps1 -Version 0.1.1-psvr2.2 -Zip

param(
    [string]$Version = "0.1.1-psvr2.2",
    [string]$BuildDir = "build",
    # ONE config for both plugins. These used to be hardcoded separately -- Stereo from Release,
    # Hands from RelWithDebInfo -- so building Release left the old RelWithDebInfo Hands DLL in
    # place and packaging silently picked it up. That shipped a two-day-old plugin once, with no
    # error and no visible symptom until it was installed.
    [ValidateSet("Release", "RelWithDebInfo", "Debug")]
    [string]$Config = "Release",
    # Escape hatch for the case where a binary really is older than a source that cannot affect
    # it (a comment-only edit, say). Off by default: staleness is a hard failure.
    [switch]$AllowStale,
    [switch]$Zip,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$Out      = Join-Path $RepoRoot "dist\CyberpunkVRPort-$Version"

# Folders that exist for development and have no business in a tester's game.
$SkipMods  = @("CyberpunkVRPort_WorldMapDiag")
# *.md too: the notes beside a script are for whoever maintains it, not for a tester's game folder.
$SkipFiles = @("db.sqlite3", "*.log", "*.bak", "*.orig", "*.rar", "*.zip", "*.md")

function Copy-Tree($src, $dst) {
    New-Item -ItemType Directory -Path $dst -Force | Out-Null
    $n = 0
    foreach ($f in (Get-ChildItem -LiteralPath $src -File)) {
        $skip = $false
        foreach ($p in $SkipFiles) { if ($f.Name -like $p) { $skip = $true; break } }
        if ($skip) { continue }
        Copy-Item $f.FullName (Join-Path $dst $f.Name) -Force
        $n++
    }
    foreach ($d in (Get-ChildItem -LiteralPath $src -Directory)) {
        $n += Copy-Tree $d.FullName (Join-Path $dst $d.Name)
    }
    return $n
}

function Need($p, $what) {
    if (-not (Test-Path -LiteralPath $p)) { throw "$what not found: $p" }
    return $p
}

# Newest source file feeding a binary. Build trees are excluded -- src\red4ext_plugin\build sits
# INSIDE the Hands source tree, and its own outputs would otherwise always look newer.
function Newest-Source([string[]]$roots) {
    $newest = $null
    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $files = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -in '.cpp', '.h', '.hpp', '.inl' -and $_.FullName -notmatch '\\build\\' }
        foreach ($f in $files) {
            if (-not $newest -or $f.LastWriteTimeUtc -gt $newest.LastWriteTimeUtc) { $newest = $f }
        }
    }
    return $newest
}

# A packaged plugin must be newer than every source that feeds it. Packaging a stale binary
# produces a zip that looks correct, installs cleanly, and behaves like an older build -- there is
# no symptom until someone tests it, and then the symptom points at the wrong thing entirely.
function Need-Fresh($p, $what, [string[]]$srcRoots) {
    Need $p $what | Out-Null
    $bin = Get-Item -LiteralPath $p
    $src = Newest-Source $srcRoots
    if ($src -and $src.LastWriteTimeUtc -gt $bin.LastWriteTimeUtc) {
        $msg = @"

$what is STALE -- it was built before its own sources.

  binary   $p
           $($bin.LastWriteTime)
  source   $($src.FullName)
           $($src.LastWriteTime)

Rebuild it for -Config $Config and run this again, or pass -AllowStale if the newer
source genuinely cannot affect this binary.
"@
        if ($AllowStale) { Write-Warning $msg } else { throw $msg }
    }
    Write-Host ("  {0,-24} {1:yyyy-MM-dd HH:mm}  [{2}]" -f $what, $bin.LastWriteTime, $Config)
    return $p
}

if (Test-Path $Out) {
    # Running the installer from the staging folder puts its download cache INSIDE $Out, and this
    # wipe would take the hand-fetched Nexus archives with it -- files that cannot be re-downloaded
    # automatically and cost real effort to collect. Refuse rather than destroy them silently.
    $cache = Join-Path $Out 'build\installer-cache'
    if ((Test-Path $cache) -and -not $Force) {
        $n = @(Get-ChildItem -LiteralPath $cache -File -Recurse -ErrorAction SilentlyContinue).Count
        if ($n -gt 0) {
            throw @"

$Out contains a download cache with $n archive(s):
  $cache

Rebuilding would delete them, and the Nexus ones cannot be fetched automatically. Move them
somewhere outside dist\CyberpunkVRPort-$Version first, then point the installer at that folder:

    scripts\Install-CyberpunkVRPort.ps1 -DownloadDir "<that folder>"

Pass -Force to delete the cache and rebuild anyway.
"@
        }
    }
    Remove-Item $Out -Recurse -Force
}
New-Item -ItemType Directory -Path $Out -Force | Out-Null

$manifest = @()
function Add-File($src, $rel) {
    $dst = Join-Path $Out $rel
    New-Item -ItemType Directory -Path (Split-Path $dst -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $src -Destination $dst -Force
    $script:manifest += [pscustomobject]@{ Path = $rel; Bytes = (Get-Item -LiteralPath $dst).Length }
}

# ---- the two plugins ------------------------------------------------------------------------
Write-Host "Plugins (config $Config):"
$stereoDll = Need-Fresh (Join-Path $RepoRoot "$BuildDir\bin\red4ext\plugins\CyberpunkVR_Stereo\$Config\CyberpunkVR_Stereo.dll") "CyberpunkVR_Stereo.dll" `
    @((Join-Path $RepoRoot "src\vr"), (Join-Path $RepoRoot "src\common"))
$handsDll  = Need-Fresh (Join-Path $RepoRoot "src\red4ext_plugin\build\$Config\CyberpunkVR_Hands.dll") "CyberpunkVR_Hands.dll" `
    @((Join-Path $RepoRoot "src\red4ext_plugin"), (Join-Path $RepoRoot "src\common"))
Add-File $stereoDll "red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll"
Add-File $handsDll  "red4ext\plugins\CyberpunkVR_Hands\CyberpunkVR_Hands.dll"

# The sight shaders are loaded by name at PSO-replacement time; without both, the replacement is
# skipped and the only symptom is one line in the log.
Add-File (Need (Join-Path $RepoRoot "src\vr\shaders\sight_reflex_ps.dxil") "sight PS") "red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_SightPs.dxil"
Add-File (Need (Join-Path $RepoRoot "src\vr\shaders\sight_reflex_vs.dxil") "sight VS") "red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_SightVs.dxil"

# Installed over the player's own settings ONCE, on the first launch with first_launch=0 in
# vrport.ini, keeping a timestamped copy of what was there. See INSTALL.txt.
Add-File (Need (Join-Path $RepoRoot "mods\config\UserSettings.json") "UserSettings.json") "red4ext\plugins\CyberpunkVR_Stereo\UserSettings.json"

# ---- captured grip poses, read from beside the exe -------------------------------------------
foreach ($g in @("CyberpunkVR_SmokeGrip_right.ini","CyberpunkVR_SmokeGrip_Left.ini","CyberpunkVR_LighterGrip_Left.ini")) {
    Add-File (Need (Join-Path $RepoRoot "mods\config\$g") $g) "bin\x64\$g"
}

# ---- HUDitor's HUD layout, arranged for a headset --------------------------------------------
# HUDitor ships its own persistency.json laid out for a flat screen, and installs BEFORE the port
# (priority 120 vs 200), so this one lands on top. It is the user's own widget placement data, not
# HUDitor's code -- nothing of theirs is redistributed by shipping it.
Add-File (Need (Join-Path $RepoRoot "mods\config\HUDitor-persistency.json") "HUDitor VR layout") "bin\x64\plugins\cyber_engine_tweaks\mods\HUDitor\persistency.json"

# ---- engine-side tuning + the OpenVR shim ------------------------------------------------------
Add-File (Need (Join-Path $RepoRoot "mods\config\vrcam_cpu_tweaks.ini") "vrcam_cpu_tweaks.ini") "engine\config\platform\pc\vrcam_cpu_tweaks.ini"
Add-File (Need (Join-Path $RepoRoot "mods\config\openvr_api.dll") "openvr_api.dll") "bin\x64\openvr_api.dll"

# ---- CET mods, redscript, tweaks --------------------------------------------------------------
foreach ($d in (Get-ChildItem (Join-Path $RepoRoot "mods\cet") -Directory)) {
    if ($SkipMods -contains $d.Name) { continue }
    $n = Copy-Tree $d.FullName (Join-Path $Out "bin\x64\plugins\cyber_engine_tweaks\mods\$($d.Name)")
    $manifest += [pscustomobject]@{ Path = "bin\x64\plugins\cyber_engine_tweaks\mods\$($d.Name)\  ($n files)"; Bytes = 0 }
}
foreach ($d in (Get-ChildItem (Join-Path $RepoRoot "mods\redscript") -Directory)) {
    if ($d.Name -eq "logs" -or $SkipMods -contains $d.Name) { continue }
    $n = Copy-Tree $d.FullName (Join-Path $Out "r6\scripts\$($d.Name)")
    $manifest += [pscustomobject]@{ Path = "r6\scripts\$($d.Name)\  ($n files)"; Bytes = 0 }
}
$tw = Join-Path $RepoRoot "mods\tweaks\vrcigarette"
if (Test-Path $tw) {
    $n = Copy-Tree $tw (Join-Path $Out "r6\tweaks\vrcigarette")
    $manifest += [pscustomobject]@{ Path = "r6\tweaks\vrcigarette\  ($n files)"; Bytes = 0 }
}

# ---- packed archives ---------------------------------------------------------------------------
foreach ($a in @("cyberpunkvrport.archive","VRCigarette.archive.xl")) {
    $p = Join-Path $RepoRoot "mods\archive\$a"
    if (Test-Path $p) { Add-File $p "archive\pc\mod\$a" }
    else { Write-Host "[!] $a is not in the repo -- run sync_assets.ps1 first" }
}

# ---- the OpenXR probe is NOT packaged ---------------------------------------------------------
# It stays in tools\xr_probe\ and goes to a tester by hand, when there is something to measure.
# Registering a MACHINE-WIDE OpenXR API layer is not a thing to ship to everyone who installs a
# mod: it is not dropped in a folder, it is written into a registry key, and one left unregistered
# records every VR application on the box. Build it with the xr_probe_layer target and hand over
# that folder when it is actually needed.

# ---- bundled user documentation ---------------------------------------------------------------
Add-File (Need (Join-Path $RepoRoot "docs\PSVR2-CONTROLS.txt") "PSVR2 controls") "PSVR2-CONTROLS.txt"
Add-File (Need (Join-Path $RepoRoot "docs\PSVR2-ADAPTIVE-TRIGGERS.md") "PSVR2 adaptive-trigger guide") "PSVR2-ADAPTIVE-TRIGGERS.md"
Add-File (Need (Join-Path $RepoRoot "docs\HUDITOR-VR-SETUP.md") "HUDitor VR setup guide") "HUDITOR-VR-SETUP.md"
# ---- the installer, IN the release ------------------------------------------------------------
# Shipped inside the main zip so there is exactly one download. The port's own files sit beside
# the script once extracted, and the installer detects that and copies them instead of fetching
# the archive it is already inside -- so the manifest never needs a hash of itself.
foreach ($s in @('Install-CyberpunkVRPort.ps1', 'Install-CyberpunkVRPort.cmd', 'Collect-VRPortReport.ps1')) {
    Add-File (Need (Join-Path $RepoRoot "scripts\$s") $s) "scripts\$s"
}
Add-File (Need (Join-Path $RepoRoot "wabbajack\manifest.json") "manifest.json") "wabbajack\manifest.json"

# A file cannot contain its own hash. This manifest ships INSIDE the release zip, so any sha256 it
# claims for that zip is stale the moment the zip is built -- and editing it to fix that changes
# the zip again. The value is unused on this path (the port's files are beside the script, so it is
# copied rather than downloaded), so the honest thing is to not claim one at all rather than ship a
# number that is guaranteed wrong. The repo's copy keeps its pin, for running from a checkout.
$stagedManifest = Join-Path $Out "wabbajack\manifest.json"
$sm = Get-Content -LiteralPath $stagedManifest -Raw
$smMatch = [regex]::Match($sm, '(?s)\{[^{}]*"id":\s*"cyberpunk-vr-port".*?\}')
if ($smMatch.Success) {
    $blk = $smMatch.Value
    # Drop the sha256 member and its trailing comma/newline, and mark why.
    $blk2 = [regex]::Replace($blk, ',\s*"sha256":\s*"[^"]*"', '')
    $blk2 = [regex]::Replace($blk2, '"url":\s*"([^"]*)"', '"url": "$1",' + "`r`n      `"localOnly`": true")
    $sm = $sm.Remove($smMatch.Index, $smMatch.Length).Insert($smMatch.Index, $blk2)
    [System.IO.File]::WriteAllText($stagedManifest, $sm, (New-Object System.Text.UTF8Encoding $false))
    $manifest = @($manifest | Where-Object { $_.Path -ne "wabbajack\manifest.json" })
    $manifest += [pscustomobject]@{ Path = "wabbajack\manifest.json"; Bytes = (Get-Item -LiteralPath $stagedManifest).Length }
}

$releaseNotes = Join-Path $RepoRoot "docs\RELEASE-$Version.txt"
if (Test-Path -LiteralPath $releaseNotes) {
    Add-File $releaseNotes "RELEASE-NOTES.txt"
} else {
    # Optional on purpose -- test builds do not need notes -- but silence here means a release
    # zip ships without them and nobody notices until it is published. Say so, loudly.
    Write-Warning "No release notes for $Version -- package will NOT contain RELEASE-NOTES.txt.`n         Expected: $releaseNotes"
}

# ---- the note a tester actually reads ----------------------------------------------------------
$readme = @"
CyberpunkVRPort $Version
========================

WHAT THIS IS
    A VR mod for Cyberpunk 2077: stereo rendering through OpenXR, 6DoF head tracking, motion
    controllers merged into the game's own gamepad input, VRIK arms, and a set of gameplay mods
    (HUD placement, holsters, melee, weapon handling, smoking).

BEFORE YOU INSTALL -- READ THIS ONE
    The first time the plugin starts it REPLACES your Cyberpunk settings with the ones this mod
    was tuned against:

        %LOCALAPPDATA%\CD Projekt Red\Cyberpunk 2077\UserSettings.json

    Your own file is copied to UserSettings.pre-vr-<date>-<time>.json in the same folder first,
    and if that copy fails the install is abandoned rather than forced. It happens exactly once:
    afterwards the file is yours and nothing here looks at it again. Everything you change in the
    game's own menus sticks.

    To skip it entirely: after the first launch creates bin\x64\vrport.ini, set first_launch=1 in
    it BEFORE starting the game a second time. To ask for it again later, set first_launch=0.

REQUIREMENTS
    Cyberpunk 2077 2.31 (this build's engine offsets are matched to it)
    RED4ext, Cyber Engine Tweaks, redscript, TweakXL, ArchiveXL, Codeware
    HUDitor v1.1.0 and Input Loader for the tested standard-widget VR HUD layout
    An OpenXR runtime, started BEFORE the game

    HUDitor defaults to F7, which this port uses for HMD recenter. Rebind HUDitor to another
    unused keyboard key. Read HUDITOR-VR-SETUP.md before arranging the HUD; Mod Settings is
    recommended for the hotkey UI.

    Nothing else may proxy dxgi. If bin\x64\dxgi.dll exists (R.E.A.L. VR installs one), move it
    out of the folder -- two VR paths in one process fight over the same engine hooks.

    Optional PS VR2 adaptive triggers/grip haptics additionally require Enhanced DualSense
    Support, Native Settings UI, and the matching PSVR2Toolkit Cyberpunk DSX Bridge release.
    DSX and Enhanced DualSense Support's UDPClient.exe are not used. Read
    PSVR2-ADAPTIVE-TRIGGERS.md before installing that optional driver-level add-on.

INSTALL
    Extract the contents of this folder into your Cyberpunk 2077 game root -- the folder that
    contains bin\, r6\, red4ext\ and archive\. The paths inside already match.

    Then start your OpenXR runtime, then the game. A small launcher window appears first: pick
    your headset and per-eye render resolution there.

PLAYSTATION VR2 QUICK START
    Use the normal Cyberpunk2077.exe with the PS VR2 PC adapter and SteamVR set as the active
    OpenXR runtime. Start SteamVR first, select PlayStation VR2 in the mod launcher, and begin
    with 3072 x 3072 (or 2560 x 2560 for more performance).

    SteamVR may omit Create/Menu when converting the Oculus Touch profile to Sense. In SteamVR's
    Manage Controller Bindings for the CyberpunkVR OpenXR application, manually map LEFT CREATE
    to the app action "Sense Create / Options" (SystemButton):

        quick press/release under 0.5 s       Start -> Cyberpunk system/pause menu
        hold for at least 0.5 s              Back  -> Cyberpunk in-game menu

    Fallback: touch Triangle without clicking and use R3 with the same quick/hold timing. Bare
    R3 remains crouch. For D-pad input, touch Triangle without clicking and move the RIGHT stick;
    turning is suppressed during the shift. Read PSVR2-CONTROLS.txt for the full control map,
    binding steps, and troubleshooting. Read HUDITOR-VR-SETUP.md to install HUDitor, resolve
    its F7 conflict, align standard HUD widgets, and preserve the layout. Read
    PSVR2-ADAPTIVE-TRIGGERS.md to add weapon/vehicle trigger profiles and synthesized grip
    haptics through the PSVR2Toolkit bridge.

WHAT LANDS WHERE
    red4ext\plugins\CyberpunkVR_Stereo\   the VR plugin, its shaders, the settings template
    red4ext\plugins\CyberpunkVR_Hands\    avatar / VRIK / weapon / smoking natives
    bin\x64\CyberpunkVR_*Grip*.ini        captured hand poses for holding a cigarette and lighter
    bin\x64\plugins\cyber_engine_tweaks\mods\CyberpunkVRPort_*\
    r6\scripts\CyberpunkVRPort_*\
    r6\tweaks\vrcigarette\
    archive\pc\mod\                       packed assets + the ArchiveXL manifest

    The player entity assets in cyberpunkvrport.archive carry one render-to-texture camera per
    supported resolution. The launcher offers exactly the ones that exist.

IF SOMETHING IS WRONG
    bin\x64\cyberpunkvrport.log            the plugin's own log, start here
    red4ext\logs\                          script validation errors land here
    bin\x64\plugins\cyber_engine_tweaks\   per-mod CET logs

    Uninstall: delete the files listed above. Nothing is written outside the game folder except
    the settings file named at the top, and its backup sits next to it.

Built from commit $(git -C $RepoRoot rev-parse --short HEAD 2>$null) on $(Get-Date -Format "yyyy-MM-dd").
"@
Set-Content (Join-Path $Out "INSTALL.txt") $readme -Encoding utf8

# ---- report -------------------------------------------------------------------------------------
Write-Host "dist\CyberpunkVRPort-$Version"
foreach ($m in $manifest) {
    if ($m.Bytes -gt 0) { Write-Host ("  {0,-62} {1,10:N0}" -f $m.Path, $m.Bytes) }
    else                { Write-Host ("  {0}" -f $m.Path) }
}
$all = Get-ChildItem $Out -Recurse -File
Write-Host ""
Write-Host ("  {0} files, {1:N0} bytes total" -f $all.Count, ($all | Measure-Object Length -Sum).Sum)

if ($Zip) {
    # Not $zip: PowerShell variable names are case-insensitive, so that would be the -Zip switch.
    $archivePath = "$Out.zip"
    if (Test-Path $archivePath) { Remove-Item $archivePath -Force }
    Compress-Archive -Path (Join-Path $Out "*") -DestinationPath $archivePath
    Write-Host ("  packaged -> {0}  ({1:N0} bytes)" -f $archivePath, (Get-Item $archivePath).Length)

    # Checksums, generated here rather than by hand. The 0.1.1-psvr2.3 sums file was produced
    # manually, which is a step that gets skipped or done against the wrong build exactly once and
    # then ships. The two plugin hashes are listed separately so a tester can verify the binaries
    # after extracting, without re-hashing the zip.
    $sumsPath = "$Out-SHA256SUMS.txt"
    $lines = @("$((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLower()) *$(Split-Path $archivePath -Leaf)", "", "Bundled binaries:")
    foreach ($rel in @("red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll",
                       "red4ext\plugins\CyberpunkVR_Hands\CyberpunkVR_Hands.dll")) {
        $f = Join-Path $Out $rel
        if (Test-Path -LiteralPath $f) {
            $lines += "$((Get-FileHash -LiteralPath $f -Algorithm SHA256).Hash.ToLower()) *$rel"
        }
    }
    [System.IO.File]::WriteAllText($sumsPath, ($lines -join "`r`n") + "`r`n", (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("  checksums -> {0}" -f $sumsPath)

    # The manifest pins the release for anyone running this script from a git checkout, where the
    # port's files are NOT beside it. Inside the release zip they are, so that path never fetches
    # and the hash below is irrelevant to it -- but it must still be right for the checkout case.
    $zipHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLower()
    $manifestSrc = Join-Path $RepoRoot 'wabbajack\manifest.json'
    $wantFile = "CyberpunkVRPort-$Version.zip"
    $wantUrl  = "https://github.com/satyaloka93/cyberpunk-vr-port/releases/download/$Version/$wantFile"

    $raw = Get-Content -LiteralPath $manifestSrc -Raw
    $before = $raw
    $raw = [regex]::Replace($raw, '"releaseTag":\s*"[^"]*"', "`"releaseTag`": `"$Version`"", 1)
    # (?s) so . spans newlines -- without it this silently never matches a pretty-printed manifest.
    $m = [regex]::Match($raw, '(?s)\{[^{}]*"id":\s*"cyberpunk-vr-port".*?\}')
    if (-not $m.Success) { throw "Could not locate the cyberpunk-vr-port block in $manifestSrc" }
    $new = $m.Value
    $new = [regex]::Replace($new, '"name":\s*"[^"]*"',     "`"name`": `"CyberpunkVR Port $Version`"")
    $new = [regex]::Replace($new, '"version":\s*"[^"]*"',  "`"version`": `"$Version`"")
    $new = [regex]::Replace($new, '"fileName":\s*"[^"]*"', "`"fileName`": `"$wantFile`"")
    $new = [regex]::Replace($new, '"url":\s*"[^"]*"',      "`"url`": `"$wantUrl`"")
    $new = [regex]::Replace($new, '"sha256":\s*"[^"]*"',   "`"sha256`": `"$zipHash`"")
    $raw = $raw.Remove($m.Index, $m.Length).Insert($m.Index, $new)

    if ($raw -ne $before) {
        [System.IO.File]::WriteAllText($manifestSrc, $raw, (New-Object System.Text.UTF8Encoding $false))
        Write-Host "  manifest  -> pinned to $Version / $($zipHash.Substring(0,16))...  (COMMIT THIS)"
    }
}
