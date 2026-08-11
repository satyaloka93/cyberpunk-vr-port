[CmdletBinding()]
param(
    [string]$Workspace,
    [string]$GamePath = "D:\SteamLibrary\steamapps\common\Cyberpunk 2077",
    [string]$ManualArchiveDirectory = "$env:USERPROFILE\Downloads\cyberpunkVR_MOD_reqs",
    [switch]$ExcludeManualMods,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ManifestPath = Join-Path $PSScriptRoot "manifest.json"
$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json

if (-not $Workspace) {
    $Workspace = Join-Path $RepoRoot "build\wabbajack-author\CyberpunkVRPort-PSVR2"
}
$Workspace = [IO.Path]::GetFullPath($Workspace)
$Downloads = Join-Path $Workspace "downloads"
$Mods = Join-Path $Workspace "mods"
$Profile = Join-Path $Workspace "profiles\PSVR2"
$Output = Join-Path (Split-Path $Workspace -Parent) "output"
$GameExe = Join-Path $GamePath "bin\x64\Cyberpunk2077.exe"

function Assert-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Expected.ToLowerInvariant()) {
        throw "SHA-256 mismatch for $Path`nexpected: $Expected`nactual:   $actual"
    }
    return $true
}

function Find-ManualArchive($Entry) {
    if (-not (Test-Path -LiteralPath $ManualArchiveDirectory -PathType Container)) {
        return $null
    }
    foreach ($pattern in $Entry.localPatterns) {
        foreach ($candidate in Get-ChildItem -LiteralPath $ManualArchiveDirectory -File -Recurse -Filter $pattern) {
            $hash = (Get-FileHash -LiteralPath $candidate.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($hash -eq $Entry.sha256.ToLowerInvariant()) { return $candidate.FullName }
        }
    }
    return $null
}

function Acquire-Archive($Entry) {
    $destination = Join-Path $Downloads $Entry.fileName
    if (Test-Path -LiteralPath $destination) {
        [void](Assert-Hash $destination $Entry.sha256)
        return $destination
    }

    # Reuse repository release/cache files when their canonical names and hashes match.
    foreach ($root in @((Join-Path $RepoRoot "build\wabbajack-cache"), (Join-Path $RepoRoot "dist"))) {
        $candidate = Join-Path $root $Entry.fileName
        if ((Test-Path -LiteralPath $candidate -PathType Leaf) -and (Assert-Hash $candidate $Entry.sha256)) {
            Copy-Item -LiteralPath $candidate -Destination $destination
            return $destination
        }
    }
    $authorRoot = Join-Path $RepoRoot "build\wabbajack-author"
    if (Test-Path -LiteralPath $authorRoot -PathType Container) {
        foreach ($candidate in Get-ChildItem -LiteralPath $authorRoot -File -Recurse -Filter $Entry.fileName) {
            if ($candidate.FullName -eq $destination) { continue }
            if (Assert-Hash $candidate.FullName $Entry.sha256) {
                Copy-Item -LiteralPath $candidate.FullName -Destination $destination
                return $destination
            }
        }
    }

    if ($Entry.manualUrl) {
        $source = Find-ManualArchive $Entry
        if (-not $source) {
            throw @"
Manual archive not found for $($Entry.name).
Download: $($Entry.manualUrl)
Expected SHA-256: $($Entry.sha256)
Prompt: $($Entry.prompt)
Then rerun this script with -ManualArchiveDirectory pointing at the download folder.
"@
        }
        Copy-Item -LiteralPath $source -Destination $destination
    } else {
        Write-Host "Downloading $($Entry.name)..."
        & curl.exe -L --fail --retry 3 --output $destination $Entry.url
        if ($LASTEXITCODE -ne 0) { throw "Download failed for $($Entry.url)" }
    }

    [void](Assert-Hash $destination $Entry.sha256)
    return $destination
}

function Write-DownloadMeta($Entry, [string]$Archive) {
    $lines = @("[General]")
    if ($Entry.nexusModId) {
        $lines += "gameName=$($Entry.nexusGame)"
        $lines += "modID=$($Entry.nexusModId)"
        $lines += "fileID=$($Entry.nexusFileId)"
    } elseif ($Entry.manualUrl) {
        $lines += "manualURL=$($Entry.manualUrl)"
        $lines += "prompt=$($Entry.prompt) Expected SHA-256: $($Entry.sha256)"
    } else {
        $lines += "directURL=$($Entry.url)"
    }
    $lines += "installed=true"
    Set-Content -LiteralPath "$Archive.meta" -Value ($lines -join "`r`n") -Encoding utf8
}

function Expand-ZipMod([string]$Archive, [string]$Destination) {
    if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Recurse -Force }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Expand-Archive -LiteralPath $Archive -DestinationPath $Destination -Force
}

if (-not (Test-Path -LiteralPath $GameExe -PathType Leaf)) {
    throw "Cyberpunk2077.exe not found at $GameExe"
}
if (Test-Path -LiteralPath $Workspace) {
    if (-not $Force) { throw "Workspace already exists: $Workspace (use -Force to rebuild it)" }
    Remove-Item -LiteralPath $Workspace -Recurse -Force
}
New-Item -ItemType Directory -Path $Downloads, $Mods, $Profile, $Output -Force | Out-Null

$sevenZip = @(
    "$env:ProgramFiles\7-Zip\7z.exe",
    "${env:ProgramFiles(x86)}\7-Zip\7z.exe"
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
if (-not $sevenZip) { throw "7-Zip is required to extract the portable MO2 archive." }

# Portable MO2 is itself a Wabbajack source archive.
$mo2 = $Manifest.tools | Where-Object id -eq "mo2"
$mo2Archive = Acquire-Archive $mo2
Write-DownloadMeta $mo2 $mo2Archive
Write-Host "Extracting portable MO2..."
& $sevenZip x -y "-o$Workspace" $mo2Archive | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $Workspace "ModOrganizer.exe"))) {
    throw "Portable MO2 extraction failed."
}

# MO2 2.5.2 bundles an older Basic Games package whose Cyberpunk plugin expects Root Builder.
# Update the complete Python package from one pinned commit: mixing only the new game file with
# the old support classes fails during plugin initialization. Keep MO2's bundled native `lib`.
$basic = $Manifest.tools | Where-Object id -eq "basic-games"
$basicArchive = Acquire-Archive $basic
Write-DownloadMeta $basic $basicArchive
$basicTemp = Join-Path $Workspace ".basic-games-extract"
Expand-ZipMod $basicArchive $basicTemp
$cyberpunkPlugin = Get-ChildItem -LiteralPath $basicTemp -Recurse -File -Filter "game_cyberpunk2077.py" | Select-Object -First 1
if (-not $cyberpunkPlugin) { throw "Pinned Basic Games archive did not contain game_cyberpunk2077.py" }
$basicSource = $cyberpunkPlugin.Directory.Parent.FullName
$basicDestination = Join-Path $Workspace "plugins\basic_games"
foreach ($file in Get-ChildItem -LiteralPath $basicSource -File -Filter "*.py") {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $basicDestination $file.Name) -Force
}
foreach ($folder in @("basic_features", "games")) {
    $destination = Join-Path $basicDestination $folder
    if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
    Copy-Item -LiteralPath (Join-Path $basicSource $folder) -Destination $destination -Recurse -Force
}
Remove-Item -LiteralPath $basicTemp -Recurse -Force

$enabledMods = @()
$modsToInstall = $Manifest.mods | Sort-Object priority
if ($ExcludeManualMods) {
    $modsToInstall = $modsToInstall | Where-Object { -not $_.manualUrl }
    Write-Warning "Building an incomplete core proof: Nexus-only required mods are excluded."
}
foreach ($mod in $modsToInstall) {
    $archive = Acquire-Archive $mod
    Write-DownloadMeta $mod $archive
    $folderName = "{0:D3} - {1}" -f [int]$mod.priority, $mod.name
    $destination = Join-Path $Mods $folderName
    Write-Host "Installing $($mod.name) into the authoring profile..."
    Expand-ZipMod $archive $destination
    $metaIni = @"
[General]
gameName=cyberpunk2077
installationFile=$($mod.fileName)
version=$($mod.version)
notes=Wabbajack source id: $($mod.id)
"@
    Set-Content -LiteralPath (Join-Path $destination "meta.ini") -Value $metaIni -Encoding utf8
    $enabledMods += $folderName
}

# Runtime-generated calibration and layout files belong to the user. Some upstream archives
# ship a default copy (currently HUDitor's persistency.json), so strip every protected path
# from every staged mod before Wabbajack indexes the profile.
$userOwnedPaths = @(
    "red4ext\plugins\CyberpunkVR_Stereo\vrik_calibration.ini",
    "bin\x64\plugins\cyber_engine_tweaks\mods\CyberpunkVRPort_HUD\hud_layout.ini",
    "bin\x64\plugins\cyber_engine_tweaks\mods\HUDitor\persistency.json",
    "bin\x64\vrport.ini"
)
foreach ($folder in $enabledMods) {
    foreach ($relativePath in $userOwnedPaths) {
        $ownedPath = Join-Path (Join-Path $Mods $folder) $relativePath
        if (Test-Path -LiteralPath $ownedPath) {
            Remove-Item -LiteralPath $ownedPath -Force
        }
    }
}

# Minimal portable profile. QSettings ByteArray paths require escaped backslashes. Wabbajack
# recognizes this standard MO2 representation and remaps both paths during installation.
New-Item -ItemType File -Path (Join-Path $Workspace "portable.txt") -Force | Out-Null
$downloadsIni = ([IO.Path]::GetFullPath($Downloads)).Replace("\", "\\")
$gameIni = ([IO.Path]::GetFullPath($GamePath)).Replace("\", "\\")
$moIni = @"
[General]
first_start=false
gameName=Cyberpunk 2077
gamePath=@ByteArray($gameIni)
selected_profile=PSVR2

[Settings]
download_directory=@ByteArray($downloadsIni)
overwrite_directory=%BASE_DIR%/../CyberpunkVRPort-PSVR2-UserData/overwrite
"@
Set-Content -LiteralPath (Join-Path $Workspace "ModOrganizer.ini") -Value $moIni -Encoding utf8
# MO2 assigns the first modlist.txt line the highest numeric priority. Put the VR port first so
# its tested integration files win intentional conflicts while foundational frameworks stay low.
$profileMods = @($enabledMods)
[array]::Reverse($profileMods)
Set-Content -LiteralPath (Join-Path $Profile "modlist.txt") -Value (($profileMods | ForEach-Object { "+$_" }) -join "`r`n") -Encoding utf8
Set-Content -LiteralPath (Join-Path $Profile "plugins.txt") -Value "" -Encoding utf8
Set-Content -LiteralPath (Join-Path $Profile "loadorder.txt") -Value "" -Encoding utf8

# Generated files have no third-party archive. NoMatchInclude only inlines unmatched files;
# downloaded mod payloads remain sourced from their original direct/Nexus archives.
$noMatch = @(
    "portable.txt",
    "ModOrganizer.ini",
    "modlist.png",
    "profiles"
)
$noMatch += $enabledMods | ForEach-Object { "mods\$_\meta.ini" }
Set-Content -LiteralPath (Join-Path $Workspace "WABBAJACK_NOMATCH_INCLUDE_FILES.txt") -Value ($noMatch -join "`r`n") -Encoding ascii

$imageSource = Join-Path $PSScriptRoot "assets\modlist.png"
$imageDestination = Join-Path $Workspace "modlist.png"
Copy-Item -LiteralPath $imageSource -Destination $imageDestination -Force

$compilerSettings = [ordered]@{
    ModlistIsNSFW = $false
    Source = $Workspace
    Downloads = $Downloads
    Game = "Cyberpunk2077"
    OutputFile = (Join-Path $Output "CyberpunkVR-Port-PSVR2.wabbajack")
    ModListImage = $imageDestination
    UseGamePaths = $true
    UseTextureRecompression = $false
    OtherGames = @()
    MaxVerificationTime = "00:05:00"
    ModListName = $Manifest.name
    ModListAuthor = "satyaloka93"
    ModListDescription = if ($ExcludeManualMods) { "Core compilation proof only; Nexus-only required mods must still be installed manually." } else { "CyberpunkVR Port with the tested PS VR2 runtime, controller, HUD, and safe overlay-pacing configuration." }
    ModListReadme = "https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/wabbajack/README.md"
    ModListWebsite = "https://github.com/satyaloka93/cyberpunk-vr-port"
    ModListCommunity = ""
    ModlistVersion = $Manifest.wabbajackVersion
    PublishUpdate = $false
    MachineUrl = ""
    AutoGenerateReport = $true
    Profile = "PSVR2"
    AdditionalProfiles = @()
    NoMatchInclude = $noMatch
    Include = @()
    Ignore = @()
    AlwaysEnabled = @()
    Version = $Manifest.wabbajackVersion
    Description = "CyberpunkVR Port PS VR2 Wabbajack proof of concept"
}
$settingsPath = Join-Path $Workspace "CyberpunkVR-Port-PSVR2.compiler_settings"
$compilerSettings | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $settingsPath -Encoding utf8

Write-Host ""
Write-Host "Author workspace created: $Workspace"
Write-Host "Profile:                  $Profile\modlist.txt"
Write-Host "Compiler settings:        $settingsPath"
Write-Host "Output directory:         $Output"
if ($ExcludeManualMods) { Write-Warning "This workspace omits Visual Holsters, Visible Bullets, Nova Optics, and HUDitor." }
Write-Host ""
Write-Host "Open ModOrganizer.exe once and confirm the Cyberpunk 2077 plugin/profile."
Write-Host "Then load the .compiler_settings file in Wabbajack's Create a Modlist UI."
Write-Host "For a metadata-light proof compile, run wabbajack-cli compile -i `"$Workspace`" -o `"$Output`"."
