[CmdletBinding()]
param(
    [string]$Workspace,
    [string]$WabbajackFile,
    [string]$InstalledProof
)

$ErrorActionPreference = "Stop"
$Manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot "manifest.json") -Raw | ConvertFrom-Json
$errors = [Collections.Generic.List[string]]::new()

function Fail([string]$Message) { $errors.Add($Message) }
function Check-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "Missing file: $Path"; return }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Expected.ToLowerInvariant()) { Fail "Hash mismatch: $Path" }
}

$all = @($Manifest.tools) + @($Manifest.mods)
$duplicateIds = $all | Group-Object id | Where-Object Count -gt 1
if ($duplicateIds) { Fail "Duplicate manifest IDs: $($duplicateIds.Name -join ', ')" }
$duplicatePriorities = $Manifest.mods | Group-Object priority | Where-Object Count -gt 1
if ($duplicatePriorities) { Fail "Duplicate mod priorities: $($duplicatePriorities.Name -join ', ')" }
if ($Manifest.wabbajackVersion -notmatch '^\d+\.\d+\.\d+\.\d+$') { Fail "Wabbajack version must have four numeric fields." }

foreach ($entry in $all) {
    if ($entry.sha256 -notmatch '^[0-9a-fA-F]{64}$') { Fail "Invalid SHA-256 for $($entry.id)" }
    if (-not $entry.fileName) { Fail "Missing fileName for $($entry.id)" }
    if ($entry.nexusModId) {
        if (-not $entry.nexusFileId -or $entry.nexusGame -ne "cyberpunk2077") {
            Fail "Incomplete Nexus metadata for $($entry.id)"
        }
    } elseif (-not $entry.url) {
        Fail "No direct or Nexus source for $($entry.id)"
    }
}

$required = @("red4ext", "cet", "redscript", "archivexl", "tweakxl", "codeware",
              "input-loader", "equipment-ex", "visual-holsters", "visible-bullets",
              "nova-optics", "huditor", "cyberpunk-vr-port")
foreach ($id in $required) {
    if ($id -notin $Manifest.mods.id) { Fail "Required source missing from manifest: $id" }
}

$initializer = Get-Content -LiteralPath (Join-Path $PSScriptRoot "Initialize-AuthorWorkspace.ps1") -Raw
if ($initializer -match 'NoMatchInclude\s*=.*["'']mods["'']') {
    Fail "Unsafe broad NoMatchInclude for mods would redistribute unmatched third-party files."
}

$ownedNames = @("vrik_calibration.ini", "hud_layout.ini", "persistency.json", "vrport.ini")
$artifactIsFull = $false

if ($Workspace) {
    $Workspace = [IO.Path]::GetFullPath($Workspace)
    foreach ($entry in $all) {
        $archive = Join-Path $Workspace "downloads\$($entry.fileName)"
        if (-not (Test-Path -LiteralPath $archive)) {
            if ($entry.nexusModId) { continue } # core-only proof intentionally omits Nexus entries
            Fail "Workspace archive missing: $($entry.fileName)"
            continue
        }
        Check-Hash $archive $entry.sha256
        $metaPath = "$archive.meta"
        if (-not (Test-Path -LiteralPath $metaPath)) { Fail "Missing Wabbajack meta: $metaPath"; continue }
        $meta = Get-Content -LiteralPath $metaPath -Raw
        if ($entry.nexusModId) {
            if ($meta -notmatch "modID=$($entry.nexusModId)(\r?\n|$)" -or
                $meta -notmatch "fileID=$($entry.nexusFileId)(\r?\n|$)") {
                Fail "Wrong Nexus metadata: $metaPath"
            }
        } elseif ($meta -notmatch [regex]::Escape("directURL=$($entry.url)")) {
            Fail "Wrong directURL: $metaPath"
        }
    }
    foreach ($path in @("ModOrganizer.exe", "portable.txt", "ModOrganizer.ini", "profiles\PSVR2\modlist.txt")) {
        if (-not (Test-Path -LiteralPath (Join-Path $Workspace $path))) { Fail "Workspace path missing: $path" }
    }
    $modlistPath = Join-Path $Workspace "profiles\PSVR2\modlist.txt"
    if ((Test-Path -LiteralPath $modlistPath) -and
        -not ((Get-Content -LiteralPath $modlistPath | Select-Object -First 1) -like "+200 - CyberpunkVR Port*")) {
        Fail "CyberpunkVR Port must be first/highest-priority in modlist.txt."
    }
    $plugin = Join-Path $Workspace "plugins\basic_games\games\game_cyberpunk2077.py"
    if (-not (Test-Path -LiteralPath $plugin)) { Fail "Current Cyberpunk MO2 plugin missing" }
    else {
        $pluginText = Get-Content -LiteralPath $plugin -Raw
        if ($pluginText -notmatch '_forced_libraries\s*=\s*\["version\.dll",\s*"winmm\.dll"\]') {
            Fail "Cyberpunk MO2 plugin does not force-load CET and RED4ext."
        }
    }
    $iniPath = Join-Path $Workspace "ModOrganizer.ini"
    if (Test-Path -LiteralPath $iniPath) {
        $ini = Get-Content -LiteralPath $iniPath -Raw
        if ($ini -notmatch 'download_directory=@ByteArray\(.+\\\\.+\)') {
            Fail "MO2 downloads path must use an escaped QSettings ByteArray."
        }
        if ($ini -notmatch 'overwrite_directory=%BASE_DIR%/\.\./CyberpunkVRPort-PSVR2-UserData/overwrite') {
            Fail "MO2 overwrite must live outside Wabbajack's managed install directory."
        }
    }
    foreach ($owned in $ownedNames) {
        if (Get-ChildItem -LiteralPath (Join-Path $Workspace "mods") -Recurse -File -Filter $owned -ErrorAction SilentlyContinue) {
            Fail "Workspace unexpectedly stages user-owned file: $owned"
        }
    }
}

if ($WabbajackFile) {
    $WabbajackFile = [IO.Path]::GetFullPath($WabbajackFile)
    if (-not (Test-Path -LiteralPath $WabbajackFile -PathType Leaf)) {
        Fail "Wabbajack artifact missing: $WabbajackFile"
    } else {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip = [IO.Compression.ZipFile]::OpenRead($WabbajackFile)
        try {
            $entry = $zip.GetEntry("modlist")
            if (-not $entry) { Fail "Artifact has no modlist manifest." }
            else {
                $reader = [IO.StreamReader]::new($entry.Open())
                try { $compiled = $reader.ReadToEnd() | ConvertFrom-Json }
                finally { $reader.Dispose() }
                if ($compiled.GameType -ne "Cyberpunk2077") { Fail "Artifact targets $($compiled.GameType), not Cyberpunk2077." }
                if (@($compiled.Archives).Count -ne $all.Count) {
                    Fail "Artifact contains $(@($compiled.Archives).Count) archives; expected $($all.Count)."
                } else { $artifactIsFull = $true }
                foreach ($source in $all) {
                    $archive = $compiled.Archives | Where-Object Name -eq $source.fileName
                    if (-not $archive) { Fail "Artifact source missing: $($source.fileName)"; continue }
                    $stateType = $archive.State.'$type'
                    if ($source.nexusModId) {
                        if ($stateType -notlike "NexusDownloader*" -or
                            $archive.State.ModID -ne $source.nexusModId -or
                            $archive.State.FileID -ne $source.nexusFileId) {
                            Fail "Artifact has wrong Nexus state for $($source.id)."
                        }
                    } elseif ($stateType -notlike "HttpDownloader*" -or $archive.State.Url -ne $source.url) {
                        Fail "Artifact has wrong direct source for $($source.id)."
                    }
                }
                foreach ($directive in $compiled.Directives) {
                    if ($directive.'$type' -like "*InlineFile" -and
                        $directive.To -like "mods\*" -and $directive.To -notlike "*\meta.ini") {
                        Fail "Artifact illegally inlines mod payload: $($directive.To)"
                    }
                    foreach ($owned in $ownedNames) {
                        if ($directive.To -like "*\$owned") { Fail "Artifact supplies user-owned file: $($directive.To)" }
                    }
                }
            }
        } finally { $zip.Dispose() }
    }
}

if ($InstalledProof) {
    $InstalledProof = [IO.Path]::GetFullPath($InstalledProof)
    foreach ($path in @("ModOrganizer.exe", "portable.txt", "profiles\PSVR2\modlist.txt")) {
        if (-not (Test-Path -LiteralPath (Join-Path $InstalledProof $path))) { Fail "Installed proof path missing: $path" }
    }
    $stereo = Join-Path $InstalledProof "mods\200 - CyberpunkVR Port 0.1.1-psvr2.2\red4ext\plugins\CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll"
    Check-Hash $stereo "56ad555cfb5fc19f486ac34c54f7c8bbaa3fdaba9357368485b2fda0a1d6c0be"
    $installedIniPath = Join-Path $InstalledProof "ModOrganizer.ini"
    if (Test-Path -LiteralPath $installedIniPath) {
        $installedIni = Get-Content -LiteralPath $installedIniPath -Raw
        $normalizedIni = $installedIni.Replace("\", "/")
        if ($installedIni -notmatch 'download_directory\s*=\s*\S+' -or
            $normalizedIni -match [regex]::Escape("build/wabbajack-author")) {
            Fail "Installed MO2 paths were not remapped away from the author workspace."
        }
        if ($installedIni -notmatch 'overwrite_directory\s*=\s*%BASE_DIR%/\.\./CyberpunkVRPort-PSVR2-UserData/overwrite') {
            Fail "Installed MO2 overwrite path is not update-safe."
        }
    }
    if ($artifactIsFull) {
        foreach ($mod in $Manifest.mods) {
            $folder = "{0:D3} - {1}" -f [int]$mod.priority, $mod.name
            if (-not (Test-Path -LiteralPath (Join-Path $InstalledProof "mods\$folder") -PathType Container)) {
                Fail "Full installation is missing mod folder: $folder"
            }
        }
    }
    foreach ($owned in $ownedNames) {
        if (Get-ChildItem -LiteralPath $InstalledProof -Recurse -File -Filter $owned -ErrorAction SilentlyContinue) {
            Fail "Proof installer unexpectedly supplied user-owned file: $owned"
        }
    }
}

if ($errors.Count) {
    $errors | ForEach-Object { Write-Error $_ }
    throw "Wabbajack definition validation failed with $($errors.Count) error(s)."
}
Write-Host "Wabbajack definition validation passed: $($Manifest.mods.Count) pinned mods, $($Manifest.tools.Count) pinned tools."
