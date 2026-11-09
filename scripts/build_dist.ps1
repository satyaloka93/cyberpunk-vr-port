# Assemble a tester package under dist\, laid out exactly as it must land in the game root.
#
# Everything comes from the repo or from a build output -- nothing is read out of the installed
# game -- so what a tester gets is what is committed. Run scripts\sync_assets.ps1 first if the
# yamls, archives or grip poses have been touched game-side since the last pull.
#
# Usage:
#   pwsh scripts\build_dist.ps1
#   pwsh scripts\build_dist.ps1 -Version 0.2.2 -Zip

param(
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[A-Za-z0-9][A-Za-z0-9.-]*)?$')]
    [string]$Version = "0.2.3-psvr2.1-rc1",
    # The game 2.5 build tree (RED4ext.SDK game-2.5); build\ is the old 2.31 tree and its DLL is stale.
    [string]$BuildDir = "build-local",
    [ValidatePattern('^[0-9a-fA-F]{7,40}$')]
    [string]$SourceCommit = "631a180b65da8c026ce3cf7afe03dfbd48adf484",
    [string]$DxcDirectory = "",
    [switch]$Zip,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$DistRoot = [IO.Path]::GetFullPath((Join-Path $RepoRoot "dist"))
$Out      = [IO.Path]::GetFullPath((Join-Path $DistRoot "CyberpunkVRPort-$Version"))
if ((Split-Path $Out -Parent) -ne $DistRoot) { throw "Package path must be directly inside dist" }

# Folders that exist for development and have no business in a tester's game.
#
# ReloadRecorder is an AUTHORING tool, not a feature: it records the game's own reload
# animation per frame with VRIK off, and every grip pose and wrist placement the reload
# ships was lifted from those takes. A player has nothing to record and nothing to do with
# the takes, and it puts a third onUpdate on a pair of hands that already carry two solvers.
$SkipMods  = @("CyberpunkVRPort_WorldMapDiag", "CyberpunkVRPort_ReloadRecorder", "CyberpunkVRPort_QuickBoot")
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

if (Test-Path $Out) {
    if (-not $Force) { throw "Package already exists: $Out. Use -Force to rebuild it." }
    $resolvedOut = (Resolve-Path -LiteralPath $Out).Path
    if ((Split-Path $resolvedOut -Parent) -ne $DistRoot -or
        ((Get-Item -LiteralPath $Out).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing to replace a package outside dist or a linked directory"
    }
    Remove-Item -LiteralPath $resolvedOut -Recurse -Force
}
New-Item -ItemType Directory -Path $Out -Force | Out-Null

$manifest = @()
function Add-File($src, $rel) {
    $dst = Join-Path $Out $rel
    New-Item -ItemType Directory -Path (Split-Path $dst -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $src -Destination $dst -Force
    $script:manifest += [pscustomobject]@{ Path = $rel; Bytes = (Get-Item -LiteralPath $dst).Length }
}

# ---- the plugin -----------------------------------------------------------------------------
# ONE plugin. This used to package CyberpunkVR_Hands from src\red4ext_plugin\ as well, and
# that directory no longer exists: every script-callable native (VRBodyBonePos, VRPalmModelPos,
# SetVRSprintActive, all of them) is compiled into cyberpunkvrport.dll now, out of src\Natives\.
# Shipping both was also the recipe for the read at 0xFFFFFFFFFFFFFFFF -- two RED4ext plugins
# detouring one address -- so a stale CyberpunkVR_Hands.dll left in red4ext\plugins is worth deleting.
$stereoDll = Need (Join-Path $RepoRoot "$BuildDir\bin\red4ext\plugins\cyberpunkvrport\Release\cyberpunkvrport.dll") "cyberpunkvrport.dll"
Add-File $stereoDll "red4ext\plugins\cyberpunkvrport\cyberpunkvrport.dll"
# redscript's compiler, built with the plugin from redscript's sources: what compiles the scripts when redscript is
# not installed (Core/ScriptCompiler.hpp). Its licence goes with it.
foreach ($n in @("scc_lib.dll", "LICENSE.txt")) {
    $f = Need (Join-Path $RepoRoot "$BuildDir\bin\red4ext\plugins\cyberpunkvrport\Release\redscript\$n") "redscript $n"
    Add-File $f "red4ext\plugins\cyberpunkvrport\redscript\$n"
}
# The TweakDB's record names (WolvenKit's, GPL-3.0, with their licence), for the Lua host's TDBID.ToStringDEBUG.
foreach ($n in @("tweakdbstr.kark", "usedhashes.kark", "LICENSE.txt")) {
    $f = Need (Join-Path $RepoRoot "$BuildDir\bin\red4ext\plugins\cyberpunkvrport\Release\config\tweakdb\$n") "TweakDB names $n"
    Add-File $f "red4ext\plugins\cyberpunkvrport\config\tweakdb\$n"
}

# The sight shaders are loaded by name at PSO-replacement time; without both, the replacement is
# skipped and the only symptom is one line in the log.
Add-File (Need (Join-Path $RepoRoot "src\Shaders\sight_reflex_ps.dxil") "sight PS") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_SightPs.dxil"
Add-File (Need (Join-Path $RepoRoot "src\Shaders\sight_reflex_vs.dxil") "sight VS") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_SightVs.dxil"
Add-File (Need (Join-Path $RepoRoot "src\Shaders\sight_reflex_ps_scope.dxil") "sight PS scope variant") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_SightPsScope.dxil"
foreach ($variant in @('649567EB5A039A01', '372EEC30C414B67A')) {
    Add-File (Need (Join-Path $RepoRoot "src\Shaders\scope_glass_$variant.dxil") "scope glass PS $variant") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_ScopeGlass_$variant.dxil"
}
Add-File (Need (Join-Path $RepoRoot "src\Shaders\lens_velocity_ps.dxil") "lens velocity PS") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_LensVelocityPs.dxil"
Add-File (Need (Join-Path $RepoRoot "src\Shaders\lens_mask_ps.dxil") "lens history mask PS") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_LensMaskPs.dxil"
Add-File (Need (Join-Path $RepoRoot "src\Shaders\sight_mask_ps.dxil") "camera surface history mask PS") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_SightMaskPs.dxil"
Add-File (Need (Join-Path $RepoRoot "src\Shaders\lens_source_resolve_cs.dxil") "lens source resolve CS") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_LensSourceResolveCs.dxil"
Add-File (Need (Join-Path $RepoRoot "src\Shaders\camera_source_resolve_cs.dxil") "camera source resolve CS") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_CameraSourceResolveCs.dxil"
# The outline shader. Without it the port still runs and the engine draws its own thin line, so it
# is not fatal to omit -- which is exactly why it has to be listed: a missing file here looks like a
# feature that was never built.
Add-File (Need (Join-Path $RepoRoot "src\Shaders\vision_highlight_cs.dxil") "outline CS") "red4ext\plugins\cyberpunkvrport\shaders\CyberpunkVR_VisionCs.dxil"
# The volumetric-fog twins for the asymmetric projection, by engine shader hash. Missing ones leave the
# fog ignoring the projection's shift -- shafts in the wrong place, blocky haze -- so all three are required.
foreach ($hash in @('3bec9b299e5c0610', '601954c7c7c00b49', 'b098a198a7565e1d', 'f60d19af775ca034')) {
    Add-File (Need (Join-Path $RepoRoot "src\Shaders\asym_fog\$hash.dxil") "volumetric fog shift twin $hash") "red4ext\plugins\cyberpunkvrport\shaders\asym\$hash.dxil"
}
# The screen-space and planar reflection twins, same reason: without them reflections are traced from the
# wrong points. One per SSR quality's trace (tools/asym_tests/asym_ssr_shaders.py); the traces and the clear of
# the tiles without coarse hits (c36da...) also merge the two eyes' reflections (Render/SsrWideMerge.cpp).
foreach ($hash in @('842ab722bfe403cd', 'b86f8871454997e6', 'c827b32cf54f25fb', '8661cc86756e2ecc', '07c867b70fba5da4', 'acaedc11005a92d0', '5e01aaacc7c617d2', 'e5741046a837b9cc')) {
    Add-File (Need (Join-Path $RepoRoot "src\Shaders\asym_ssr\$hash.dxil") "SSR shift twin $hash") "red4ext\plugins\cyberpunkvrport\shaders\asym_ssr\$hash.dxil"
}
# The shadow-mask twin: merges the two eyes' sun contact shadows as one wide display (Render/SsrWideMerge.cpp).
Add-File (Need (Join-Path $RepoRoot "src\Shaders\asym_shadow\753b3bed8df87d53.dxil") "shadow-mask wide-merge twin") "red4ext\plugins\cyberpunkvrport\shaders\asym_shadow\753b3bed8df87d53.dxil"
# The material view-direction twins (tools/asym_tests/view_dir_shaders.py): screens and holograms whose parallax
# layers and facing terms used the camera's forward axis take the pixel's own view ray (Render/HiddenAreaShaders.cpp).
Get-ChildItem (Join-Path $RepoRoot "src\Shaders\view_dir") -Filter *.dxil | ForEach-Object { Add-File $_.FullName "red4ext\plugins\cyberpunkvrport\shaders\view_dir\$($_.Name)" }
# The cone-AO twins: without them the occlusion in corners differs between the eyes; the traces also merge the
# two eyes' AO at their inner edges (Render/SsrWideMerge.cpp, kConeAo).
foreach ($hash in @('33f28d9a0e7a8352', '1cbc87916ec8e03c', '1381eb9e608485d7', '059246fd5e64ea98')) {
    Add-File (Need (Join-Path $RepoRoot "src\Shaders\asym_ao\$hash.dxil") "cone-AO shift twin $hash") "red4ext\plugins\cyberpunkvrport\shaders\asym_ao\$hash.dxil"
}
# The particle-facing vertex twins, one per particle vertex shader in the game's cache that lays sprites out
# along the camera's axes (tools/asym_tests/particle_facing_shaders.py): without them smoke and steam turn
# with the head. Too many to list; the count is checked instead.
$particleTwins = @(Get-ChildItem -LiteralPath (Need (Join-Path $RepoRoot "src\Shaders\asym_particles") "particle-facing twins") -Filter *.dxil)
if ($particleTwins.Count -lt 106) { throw "particle-facing twins: $($particleTwins.Count) of 106 in src\Shaders\asym_particles" }
foreach ($twin in $particleTwins) {
    Add-File $twin.FullName "red4ext\plugins\cyberpunkvrport\shaders\asym_particles\$($twin.Name)"
}
# The auto-exposure twins (tools/asym_tests/asym_expo_shaders.py): the luminance image metered around the view
# centre, and the slower adaptation.
foreach ($name in @("1a6a7c32da1959d6.dxil", "d1b593e255bfcb11.dxil")) {
    Add-File (Need (Join-Path $RepoRoot "src\Shaders\asym_expo\$name") "auto-exposure twin $name") "red4ext\plugins\cyberpunkvrport\shaders\asym_expo\$name"
}
# The screen-effect twin (tools/asym_tests/screen_fx_shaders.py): the Sandevistan's look without its radial smear
# and block tearing around each eye's screen middle.
Add-File (Need (Join-Path $RepoRoot "src\Shaders\screen_fx\6924aa01ad878186.dxil") "Sandevistan screen-effect twin") "red4ext\plugins\cyberpunkvrport\shaders\screen_fx\6924aa01ad878186.dxil"
# The merged tiled-light twins (tools/asym_tests/stereo_light_merge_shaders.py): both eyes' tiles lit in the left
# eye's dispatch (CyberpunkVR_StereoLightMergePatch).
Get-ChildItem -LiteralPath (Need (Join-Path $RepoRoot "src\Shaders\stereo_light_merge") "merged tiled-light twins") -Filter *.dxil |
    ForEach-Object { Add-File $_.FullName "red4ext\plugins\cyberpunkvrport\shaders\stereo_light_merge\$($_.Name)" }
# Single-pass stereo rewrites the engine's shaders in process with the DirectX shader compiler
# (src/Render/ViewTwinCompiler.cpp): dxcompiler.dll and dxil.dll beside the plugin (dxc\), from the Windows SDK.
$DxcDir = if ($DxcDirectory) { $DxcDirectory } else { "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64" }
foreach ($n in @("dxcompiler.dll", "dxil.dll")) {
    Add-File (Need (Join-Path $DxcDir $n) "DirectX shader compiler $n") "red4ext\plugins\cyberpunkvrport\dxc\$n"
}
# The engine-stereo merge twins (tools/asym_tests/stereo_ssr_merge_shaders.py): under 2.5 engine stereo the ambient
# pass (all its permutations) takes the reflections an eye lost at its inner edge from the other eye's half of the frame,
# and the ambient pass, the tiled lights and the forward-lit materials (glass, clear coat) take the view direction from
# each eye instead of the stereo centre (docs/engine-stereo-reflections-20261102.md). Too many to list; the count is
# checked instead (98 from staticshader_final.cache, 234 from shader_final.cache with --forward).
$mergeTwins = @(Get-ChildItem -LiteralPath (Need (Join-Path $RepoRoot "src\Shaders\stereo_merge") "engine-stereo merge twins") -Filter *.dxil)
if ($mergeTwins.Count -lt 332) { throw "engine-stereo merge twins: $($mergeTwins.Count) of 332 in src\Shaders\stereo_merge" }
foreach ($twin in $mergeTwins) {
    Add-File $twin.FullName "red4ext\plugins\cyberpunkvrport\shaders\stereo_merge\$($twin.Name)"
}
# The ray-traced reflection twins (tools/asym_tests/stereo_rt_shaders.py): the reflection and path-tracing raygens
# (DXIL libraries) take the view direction from each eye, swapped when the state object is created
# (Stereo/DeviceHooks.cpp CreateStateObject). 16a3f056d8ec9845 is the transparent raygen as ReFRACT 1.5.1 rewrites it.
$rtTwins = @(Get-ChildItem -LiteralPath (Need (Join-Path $RepoRoot "src\Shaders\stereo_rt") "ray-traced reflection twins") -Filter *.dxil)
if ($rtTwins.Count -lt 38) { throw "ray-traced reflection twins: $($rtTwins.Count) of 38 in src\Shaders\stereo_rt" }
foreach ($twin in $rtTwins) {
    Add-File $twin.FullName "red4ext\plugins\cyberpunkvrport\shaders\stereo_rt\$($twin.Name)"
}
# The engine-stereo eye twins (tools/asym_tests/stereo_eye_shaders.py): the volumetric clouds (both qualities of the
# march) look up the fog in front of a cloud in the pixel's own eye, not the dispatch camera's.
foreach ($hash in @('8a6c46c528ce5160', 'c944329b340f33c2', '4145873fbf34bf87')) {
    Add-File (Need (Join-Path $RepoRoot "src\Shaders\stereo_eye\$hash.dxil") "engine-stereo eye twin $hash") "red4ext\plugins\cyberpunkvrport\shaders\stereo_eye\$hash.dxil"
}
# First stereo bloom downsample; enabled separately from the other stereo patches.
Add-File (Need (Join-Path $RepoRoot "src\Shaders\stereo_bloom\83848f96b80814d5.dxil") "stereo bloom prefilter") "red4ext\plugins\cyberpunkvrport\shaders\stereo_bloom\83848f96b80814d5.dxil"
# Shared particle-lighting allocation: visibility in either eye is sufficient.
Add-File (Need (Join-Path $RepoRoot "src\Shaders\stereo_particle_lighting\e4c84ae0e5616f91.dxil") "stereo particle-lighting allocator") "red4ext\plugins\cyberpunkvrport\shaders\stereo_particle_lighting\e4c84ae0e5616f91.dxil"
# Clustered-light bounds agree with the per-eye projection used by their planes.
Add-File (Need (Join-Path $RepoRoot "src\Shaders\stereo_light_cull\37851ec24d58e673.dxil") "stereo light-culling twin") "red4ext\plugins\cyberpunkvrport\shaders\stereo_light_cull\37851ec24d58e673.dxil"
# Distant lights query each eye's HiZ rectangle with its own projection.
Add-File (Need (Join-Path $RepoRoot "src\Shaders\stereo_distant_light\6a1544708bf0067e.dxil") "stereo distant-light culler") "red4ext\plugins\cyberpunkvrport\shaders\stereo_distant_light\6a1544708bf0067e.dxil"
# Clustered probe/decal bounds and the ambient pass's tile probe mask, per-eye like the lights.
Add-File (Need (Join-Path $RepoRoot "src\Shaders\stereo_probe_cull\355ba40185eeae65.dxil") "stereo probe-culling twin") "red4ext\plugins\cyberpunkvrport\shaders\stereo_probe_cull\355ba40185eeae65.dxil"

# Merged into the player's settings ONCE, preserving personal preferences, when first_launch=1
# in vrport.ini. FirstLaunch clears it to 0 only after the merge succeeds. A timestamped backup
# of the original file is kept. See INSTALL.txt.
Add-File (Need (Join-Path $RepoRoot "mods\config\UserSettings.json") "UserSettings.json") "red4ext\plugins\cyberpunkvrport\config\UserSettings.json"

# Release defaults avoid the first-launch graphics preset and opt in to diagnostic recording only on request.
Add-File (Need (Join-Path $RepoRoot "mods\config\vrport-release.ini") "RC defaults") "red4ext\plugins\cyberpunkvrport\config\vrport.ini"
foreach ($license in @('OFXR-LGPL-3.0.txt','OFXR-GPL-3.0.txt','OpenVR-LICENSE')) {
    Add-File (Need (Join-Path $RepoRoot ("licenses\"+$license)) $license) ("red4ext\plugins\cyberpunkvrport\licenses\"+$license)
}
Add-File (Join-Path $RepoRoot 'LICENSE') 'red4ext\plugins\cyberpunkvrport\licenses\PORT-LICENSE.txt'
Add-File (Join-Path $RepoRoot 'externals\v557-reconstruction\provenance.json') 'red4ext\plugins\cyberpunkvrport\licenses\V557-provenance.json'
Add-File (Join-Path $RepoRoot 'docs\PSVR2-023-RC1.md') 'red4ext\plugins\cyberpunkvrport\PSVR2-SETUP.md'
Add-File (Join-Path $RepoRoot 'docs\REBUILD-0.2.3-RC1.md') 'red4ext\plugins\cyberpunkvrport\licenses\SOURCE-AND-REBUILD.md'
$releaseInfo=[ordered]@{version=$Version;runtimeCommit='631a180b65da8c026ce3cf7afe03dfbd48adf484';dllSha256=(Get-FileHash -LiteralPath $stereoDll).Hash;source='https://github.com/satyaloka93/cyberpunk-vr-port';sdkCommit='5246e659d783e5834cc08a90e56c8932c9ed3449'}
$releaseInfo | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Out 'red4ext\plugins\cyberpunkvrport\release.json') -Encoding UTF8

# ---- captured grip poses, read from beside the exe -------------------------------------------
# ENUMERATED, never listed. This was a hardcoded three (both smoke grips and the lighter) while
# mods\config\ holds fourteen: it also has RestGrip_Left -- the relaxed left hand worn
# while a weapon is out -- and one TwoHandGrip_<weapon> per weapon anyone has recorded a hold for.
# Both absences are silent by design (an uncaptured weapon just has no two-hand hold), so the
# package quietly shipped no two-handed grip at all. Enumerating means a newly captured pose ships
# by existing, without anyone remembering to add it here.
foreach ($g in (Get-ChildItem (Join-Path $RepoRoot "mods\config") -Filter "CyberpunkVR_*.ini" -File | Sort-Object Name)) {
    Add-File $g.FullName "red4ext\plugins\cyberpunkvrport\grips\$($g.Name)"
}

# ---- the OpenVR shim ---------------------------------------------------------------------------
# engine\config\platform\pc\vrcam_cpu_tweaks.ini (streaming and culling for the second VRCAM view) is not packaged
# since 0.2.0; the uninstaller still removes a copy an earlier release left (see $retiredFiles).
Add-File (Need (Join-Path $RepoRoot "mods\config\openvr_api.dll") "openvr_api.dll") "bin\x64\openvr_api.dll"

# ---- Lua mods (run by the plugin's host, src/Lua), redscript, tweaks -----------------------------
foreach ($d in (Get-ChildItem (Join-Path $RepoRoot "mods\cet") -Directory)) {
    if ($SkipMods -contains $d.Name) { continue }
    $n = Copy-Tree $d.FullName (Join-Path $Out "red4ext\plugins\cyberpunkvrport\lua\$($d.Name)")
    $manifest += [pscustomobject]@{ Path = "red4ext\plugins\cyberpunkvrport\lua\$($d.Name)\  ($n files)"; Bytes = 0 }
}
foreach ($d in (Get-ChildItem (Join-Path $RepoRoot "mods\redscript") -Directory)) {
    # Only our installed modules belong in the package/uninstaller, never a
    # shared script cache, authoring folder or an empty retired module.
    if ($d.Name -notlike "CyberpunkVRPort_*" -or $SkipMods -contains $d.Name -or
        -not (Get-ChildItem -LiteralPath $d.FullName -Recurse -File -Filter '*.reds' | Select-Object -First 1)) { continue }
    # The plugin's scripts folder: compiled with the game's scripts, by redscript's compiler or the plugin's own.
    $n = Copy-Tree $d.FullName (Join-Path $Out "red4ext\plugins\cyberpunkvrport\scripts\$($d.Name)")
    $manifest += [pscustomobject]@{ Path = "red4ext\plugins\cyberpunkvrport\scripts\$($d.Name)\  ($n files)"; Bytes = 0 }
}
# The port's TweakDB records: the plugin applies them itself as the TweakDB loads (Core/TweakRecords.hpp).
$tweaksRoot = Join-Path $RepoRoot "mods\tweaks"
$n = Copy-Tree $tweaksRoot (Join-Path $Out "red4ext\plugins\cyberpunkvrport\config\tweaks")
$manifest += [pscustomobject]@{ Path = "red4ext\plugins\cyberpunkvrport\config\tweaks\  ($n files)"; Bytes = 0 }

# ---- packed archives ---------------------------------------------------------------------------
# ENUMERATED, never listed -- the same rule the grip poses above already follow, and for the same
# reason. The hardcoded three shipped a package with no vrport_mag.archive in it, while that archive
# sat both in mods\archive\ and in the installed game: a file present everywhere except in the
# tester's copy, and nothing said so. Only exact *.archive names match, so the .bak_*, .holdcomp,
# .nocollision and .nophys variants beside them stay out by shape rather than by anyone remembering
# to exclude them. The plugin mounts every archive of its archive folder (Core/ArchiveExtensions.hpp)
# and applies ONE manifest, config\archive.yaml -- kept out of the archive folder, where ArchiveXL
# would apply it a second time.
$archiveDir = Join-Path $RepoRoot "mods\archive"
$packed = @(Get-ChildItem -LiteralPath $archiveDir -File | Where-Object { $_.Name -like "*.archive" })
if ($packed.Count -eq 0) { Write-Host "[!] no archives in mods\archive -- run sync_assets.ps1 first" }
foreach ($f in $packed) { Add-File $f.FullName "red4ext\plugins\cyberpunkvrport\archive\$($f.Name)" }
$manifests = @(Get-ChildItem -LiteralPath $archiveDir -File | Where-Object { $_.Name -like "*.archive.xl" })
if ($manifests.Count -ne 1 -or $manifests[0].Name -ne "cyberpunkvrport.archive.xl") {
    throw "mods\archive must hold exactly one manifest, cyberpunkvrport.archive.xl (found: $($manifests.Name -join ', '))"
}
Add-File $manifests[0].FullName "red4ext\plugins\cyberpunkvrport\config\archive.yaml"

# ---- the port's input bindings -------------------------------------------------------------------
# The plugin merges these into the game's input configuration itself (Core/InputConfig.hpp), or hands them
# to Input Loader when another mod installed it.
$inputDir = Join-Path $RepoRoot "mods\config\input"
if (Test-Path $inputDir) {
    foreach ($binding in @("CyberpunkVRPort_ScannerHud.xml", "CyberpunkVRPort_PhotoMode.xml")) {
        Add-File (Need (Join-Path $inputDir $binding) $binding) "red4ext\plugins\cyberpunkvrport\config\input\$binding"
    }
}

# ---- the OpenXR probe is NOT packaged ---------------------------------------------------------
# It stays in tools\xr_probe\ and goes to a tester by hand, when there is something to measure.
# Registering a MACHINE-WIDE OpenXR API layer is not a thing to ship to everyone who installs a
# mod: it is not dropped in a folder, it is written into a registry key, and one left unregistered
# records every VR application on the box. Build it with the xr_probe_layer target and hand over
# that folder when it is actually needed.

# ---- the note a tester actually reads ----------------------------------------------------------
$readme = @"
CyberpunkVRPort $Version - PSVR2 / Quest release candidate

Use CyberpunkVRPort-Switcher.zip from this release for a clean backed-up install.
Run Switch-CyberpunkVRPort.bat; option4 installs this fork, option1 installs stock
upstream0.2.3, option5 restores the previous port, option11 inspects loader/log errors.
Extract the entire switcher outside the game. Never copy only its main script.

Requires Cyberpunk2077 2.5, RED4ext1.31.0 and your OpenXR runtime. The switcher
downloads/verifies RED4ext. CET/ArchiveXL/TweakXL/InputLoader/Codeware/redscript
are not required for the port, but keep them for other mods that need them.
More Occluders and HUDitor are NOT required. Cheeky, OFXR and Toolkit are optional.

This fork uses our tested0.2.2 base with selected0.2.3 backports, not every upstream
0.2.3 feature. No automatic graphics overwrite: first_launch=0. Personal game
settings and DLSS replacements are preserved by the switcher.

F10 opens the port menu. Framegen starts OFF. To use the improved bike-image
reconstruction select NVIDIA OFA + V557 (experimental), Medium/50%, then enable FG.
It works on PSVR2/SteamVR, Quest/SteamLink and Quest/VDXR. The separate SteamVR
optimization package defaults ON; it also applies to SteamLink, automatically
bypasses VDXR, and needs a game restart when changed. Adaptive pacing defaults ON.
Close external OFXR framegen before enabling built-in FG. Diagnostic flight
recording and the performance overlay default OFF and can be enabled inF10.

Standard vibration uses OpenXR. Advanced PSVR2 adaptive triggers/audio require
our optional PSVR2Toolkit fork bridge; Steam DSX is NOT required. See
red4ext\plugins\cyberpunkvrport\PSVR2-SETUP.md and
https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2
Haptics backend and road/audio settings are inF10. Restart game and bridge after
changing the haptic route. Quest touch thumbrest shifts the D-pad; left thumbrest
plus right-stick click opens inventory.

Hardware-accelerated GPU scheduling (HAGS) improved some local runs; benefit is
system/driver dependent, not a guaranteed FPS gain. Reboot after changing HAGS.

Do not remove dxgi.dll or version.dll merely because they exist. Cheeky/ReShade/
ASI loaders may own them. Switcher option11 identifies known files and extracts
recent errors with recovery context. Unknown hashes need review, not automatic
deletion. Cheeky diagnostic proxies are not bundled or installed by this package.

Manual install: close the game, back up the existing port, remove only its old
files, then extract the game-root paths. The switcher is strongly preferred for
upgrades. Config is red4ext\plugins\cyberpunkvrport\config\vrport.ini.
Logs are under red4ext\plugins\cyberpunkvrport\log and red4ext\logs.
Do not reinstall RED4ext.SDK headers into the game; the compatible SDK is compiled
into the supplied DLL. Its pinned source revision is recorded inrelease.json.

V557 includes LGPL-3.0 code; licenses and provenance are in the plugin licenses
folder. Corresponding source and rebuild instructions accompany this GitHub
release. No game logs, captures or personal calibration are distributed.
"@
Set-Content (Join-Path $Out "INSTALL.txt") $readme -Encoding utf8

# ---- the changelog, beside INSTALL.txt --------------------------------------------------------
# NOT added through Add-File, and that is the point: the manifest drives the generated uninstaller,
# so anything registered there is a path this package puts INTO the game and later deletes from it.
# This is documentation that lives inside the zip, like INSTALL.txt itself.
$changelog = Join-Path $RepoRoot "CHANGELOG.md"
if (Test-Path $changelog) {
    $text = (Get-Content -LiteralPath $changelog -Raw)
    Set-Content (Join-Path $Out "CHANGELOG.txt") $text -Encoding utf8
    Write-Host ("  {0,-64}{1,10:N0}" -f "CHANGELOG.txt", (Get-Item (Join-Path $Out "CHANGELOG.txt")).Length)
} else {
    Write-Host "[!] CHANGELOG.md not found -- the package ships without one"
}

# ---- the uninstaller, GENERATED from the manifest ----------------------------------------------
# Both the runnable UNINSTALL.bat and the UNINSTALL.txt beside it are derived from what this run
# actually copied. Typed by hand they go stale on the first mod added -- exactly the way the grip
# list and the CET folder list in INSTALL.txt both did, each of which shipped wrong for a while.
#
# The folders are the ones this run copied whole, the loose files are the ones it added into
# folders that are NOT ours, and a file inside a listed folder is left out rather than named twice.
# That last part is why the .bat can use rmdir /s on the folder list without ever aiming it at a
# directory the game or another mod owns.
#
# Restoring the first-launch UserSettings.json backup is separate from deleting
# the package files. The generated uninstaller offers to restore that backup.
$ownDirs = @("red4ext\plugins\cyberpunkvrport", "red4ext\plugins\CyberpunkVR_Stereo")
foreach ($e in $manifest) {
    if ($e.Bytes -eq 0 -and $e.Path -match '^(.*?)\\\s+\(\d+ files\)$') { $ownDirs += $Matches[1] }
}
$ownDirs = $ownDirs | Sort-Object -Unique

$loose = @()
foreach ($e in $manifest) {
    if ($e.Bytes -le 0) { continue }
    $inDir = $false
    foreach ($d in $ownDirs) { if ($e.Path.StartsWith($d + "\")) { $inDir = $true; break } }
    if (-not $inDir) { $loose += $e.Path }
}
# Files earlier releases installed and this one no longer ships, removed like the package's own.
$retiredFiles = @("engine\config\platform\pc\vrcam_cpu_tweaks.ini")
$loose = ($loose + $retiredFiles) | Sort-Object -Unique

$dirLines   = ($ownDirs | ForEach-Object { "    " + $_ + "\" }) -join "`r`n"
$looseLines = ($loose   | ForEach-Object { "    " + $_        }) -join "`r`n"

$uninstall = @"
CyberpunkVRPort $Version -- UNINSTALL
=====================================

Run UNINSTALL.bat in this folder and it does all of part 1, 2 and 3 below for you, then offers to
put your own Cyberpunk settings back. This file is the same thing written out, for anyone who
would rather delete by hand or wants to know what the .bat touches before running it.

Nothing here installs to Windows. There is no installer, no registry key and no service: every
file this mod adds is inside the Cyberpunk 2077 game folder, and deleting them IS the uninstall.
The two lists below are generated from the package itself, so they name exactly what it added.

If you only want to TURN IT OFF for a session you need none of this. Delete
red4ext\plugins\cyberpunkvrport\cyberpunkvrport.dll and the game starts flat, with the Lua
mods not run (the plugin runs them) and the port's scripts not compiled.

1. DELETE THESE FOLDERS -- every one of them is ours, nothing else lives in them

$dirLines

2. DELETE THESE FILES -- these sit in folders that are NOT ours, so delete the file, not the folder

$looseLines

3. AND THESE -- the plugin writes them at runtime, so they are not in the package

    bin\x64\vrport.ini                     every setting you changed in the F10 overlay
    bin\x64\vrport-launcher.ini            the launcher's headset, resolution and DEBUG choice
    red4ext\plugins\cyberpunkvrport\log\cyberpunkvrport.log   the plugin's log
    r6\input\CyberpunkVRPort_ScannerHud.xml   the port's bindings, handed to Input Loader when it is installed
    r6\input\CyberpunkVRPort_PhotoMode.xml    (the same)

4. RESTORE YOUR GAME SETTINGS IF NEEDED

    YOUR GAME SETTINGS. On its first launch the plugin merged the VR preset into

        %LOCALAPPDATA%\CD Projekt Red\Cyberpunk 2077\UserSettings.json

    while preserving personal preferences, and copied the original to
    UserSettings.pre-vr-<date>-<time>-<id>.json in the same folder. Removing the mod does not put it
    back. UNINSTALL.bat offers to; by hand, rename that backup over UserSettings.json yourself
    with the game closed. Skip it and you keep the applied VR graphics, input and comfort settings
    for good.

5. IF YOU MOVED A dxgi.dll ASIDE FOR THIS MOD

    Identify the file and its owner before changing it. Cheeky/ReShade/ASI loaders
    are optional independent components. This release does not disable them.
    Use switcher option11 for read-only identities and recent error inspection.

6. WHAT IS NOT TOUCHED, so you do not go looking for it

    Your saves. Nothing here reads or writes them, and nothing this mod does is persisted into a
    save: no custom quest facts, no items that stop existing, no scripted entities parked in the
    world. A save made with the mod loads without it.

    The other mods this one uses -- RED4ext,
    Visual Holsters, Equipment-EX, Nova Optics.
    Each has its own uninstall; the lists above remove only what carries the
    CyberpunkVRPort name.

Generated from commit $SourceCommit on $(Get-Date -Format "yyyy-MM-dd").
"@
Set-Content (Join-Path $Out "UNINSTALL.txt") $uninstall -Encoding utf8

# ---- and the same lists as a batch file anyone can double-click --------------------------------
# A .bat rather than a .ps1 on purpose: no ExecutionPolicy to explain, no "right-click, Run with
# PowerShell", and the player can read every line of it in Notepad before running it. Written
# ASCII, NOT utf8: Set-Content -Encoding utf8 puts a BOM on it, and cmd.exe reads that BOM as part
# of the first command, so `@echo off` becomes an unknown command and the whole window fills up.
$batDirs  = ($ownDirs | ForEach-Object { 'call :killdir "' + $_ + '"' }) -join "`r`n"
$batFiles = ($loose   | ForEach-Object { 'call :killfile "' + $_ + '"' }) -join "`r`n"

$bat = @"
@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo.
echo   CyberpunkVRPort $Version  --  uninstall
echo   =======================================
echo.

rem ---- refuse to run anywhere but the game root ----------------------------------------------
rem Every path below is relative, so in the wrong folder this would delete nothing rather than
rem something -- but saying so beats a run that reports "already gone" 45 times and looks done.
if not exist "bin\x64\Cyberpunk2077.exe" goto :notgameroot

rem ---- and not while the game is running ------------------------------------------------------
rem A locked DLL fails silently enough to leave half a mod behind, and the settings restore below
rem would be overwritten by the running game on exit.
rem Windows' own find.exe by its path: a Unix find earlier on PATH (Git's, MSYS2's) takes /i for a folder,
rem fails, and the check passes with the game up.
tasklist /fi "imagename eq Cyberpunk2077.exe" 2>nul | "%SystemRoot%\System32\find.exe" /i "Cyberpunk2077.exe" >nul
if not errorlevel 1 goto :stillrunning

echo   Game folder:  %CD%
echo.
echo   This removes every file CyberpunkVRPort installed -- $($ownDirs.Count) folders, $($loose.Count) loose files,
echo   plus the files the plugin writes at runtime.
echo.
echo   See UNINSTALL.txt for the complete removal and settings-restoration instructions.
echo   Your Cyberpunk settings are offered back at the end.
echo.
set "GO="
set /p "GO=  Remove CyberpunkVRPort now? [y/N] "
rem First character only, so y / Y / yes / a stray trailing space all mean yes, and an empty
rem answer (just Enter) does not.
if /i not "!GO:~0,1!"=="y" goto :nothingdone
echo.

set /a GONE=0
set /a ABSENT=0
set /a STUCK=0

$batDirs

$batFiles

call :killfile "bin\x64\vrport.ini"
call :killfile "bin\x64\vrport-launcher.ini"
call :killfile "bin\x64\cyberpunkvrport.log"
rem Written by the plugin for Input Loader (Core/InputConfig.cpp), when that mod is installed.
call :killfile "r6\input\CyberpunkVRPort_ScannerHud.xml"
call :killfile "r6\input\CyberpunkVRPort_PhotoMode.xml"

echo.
echo   Removed !GONE!, already gone !ABSENT!, could not remove !STUCK!.
if !STUCK! GTR 0 (
  echo.
  echo   Something is holding those files. Close the game and Steam and run this again.
)

rem ---- offer the player's own settings back ---------------------------------------------------
rem The newest backup, because the plugin only ever writes one and only on a first launch, but a
rem reinstall makes a second and the newest is the one from the install being removed.
set "CFG=%LOCALAPPDATA%\CD Projekt Red\Cyberpunk 2077"
set "BAK="
for /f "delims=" %%F in ('dir /b /o-d "%CFG%\UserSettings.pre-vr-*.json" 2^>nul') do (
  if not defined BAK set "BAK=%%F"
)
if not defined BAK goto :nobackup

echo.
echo   Your own Cyberpunk settings were saved before this mod applied its VR preset:
echo     !BAK!
set "GO2="
set /p "GO2=  Put them back now? [y/N] "
if /i not "!GO2:~0,1!"=="y" (
  echo   Left alone. It is still there whenever you want it.
  goto :done
)
copy /y "%CFG%\!BAK!" "%CFG%\UserSettings.json" >nul
if errorlevel 1 (
  echo   FAILED. Do it by hand: rename
  echo       %CFG%\!BAK!
  echo       over UserSettings.json
) else (
  echo   Restored. Your graphics, comfort and language settings are yours again.
)
goto :done

:nobackup
echo.
echo   No UserSettings.pre-vr-*.json found in
echo     %CFG%
echo   So either the VR preset was never applied, or the backup has been moved.
goto :done

:done
echo.
echo   Done. UNINSTALL.txt is still here if you want to check anything by hand --
echo   delete it, INSTALL.txt and this file whenever you like.
echo.
pause
exit /b 0

:notgameroot
echo   This is not the Cyberpunk 2077 folder:
echo     %CD%
echo.
echo   bin\x64\Cyberpunk2077.exe is not here. UNINSTALL.bat belongs in the game root --
echo   the folder that holds bin\, r6\, red4ext\ and archive\ -- and has to run there.
echo.
pause
exit /b 1

:stillrunning
echo   Cyberpunk 2077 is running.
echo.
echo   Close it first: the plugin DLL is locked while the game is up, and the game
echo   rewrites its settings file on exit, which would undo the restore below.
echo.
pause
exit /b 1

:nothingdone
echo.
echo   Nothing was deleted.
echo.
pause
exit /b 0

rem ---- helpers, past every exit so nothing falls into them ------------------------------------
:killdir
if not exist "%~1\" (
  set /a ABSENT+=1
  exit /b 0
)
rmdir /s /q "%~1" 2>nul
if exist "%~1\" (
  echo   LOCKED      %~1\
  set /a STUCK+=1
) else (
  echo   removed     %~1\
  set /a GONE+=1
)
exit /b 0

:killfile
if not exist "%~1" (
  set /a ABSENT+=1
  exit /b 0
)
del /f /q "%~1" 2>nul
if exist "%~1" (
  echo   LOCKED      %~1
  set /a STUCK+=1
) else (
  echo   removed     %~1
  set /a GONE+=1
)
exit /b 0
"@
Set-Content (Join-Path $Out "UNINSTALL.bat") $bat -Encoding ascii

# ---- report -------------------------------------------------------------------------------------
Write-Host "dist\CyberpunkVRPort-$Version"
foreach ($m in $manifest) {
    if ($m.Bytes -gt 0) { Write-Host ("  {0,-62} {1,10:N0}" -f $m.Path, $m.Bytes) }
    else                { Write-Host ("  {0}" -f $m.Path) }
}
$all = Get-ChildItem $Out -Recurse -File
if ($all.FullName -match '[\\/]CyberpunkVRPort_QuickBoot[\\/]') {
    throw "QuickBoot must not be included in a release package"
}
if (Test-Path -LiteralPath (Join-Path $Out 'r6\scripts\cache')) {
    throw "Shared script cache must not be owned by the release package"
}
if (Test-Path -LiteralPath (Join-Path $Out 'r6\scripts\MoreOccluders.reds')) {
    throw "More Occluders must be installed separately from its author's page"
}
Write-Host ""
Write-Host ("  {0} files, {1:N0} bytes total" -f $all.Count, ($all | Measure-Object Length -Sum).Sum)

if ($Zip) {
    # Not $zip: PowerShell variable names are case-insensitive, so that would be the -Zip switch.
    $archivePath = "$Out.zip"
    if (Test-Path $archivePath) { Remove-Item $archivePath -Force }
    # THE ZIP SPEC'S PATHS: forward slashes, no drive letter, no leading slash (APPNOTE 4.4.17, issue #145).
    # Compress-Archive in Windows PowerShell 5.1 stores "bin\x64\..." -- one long file name with backslashes in it
    # for every unpacker that follows the spec. So each entry is written here under its relative path with "/",
    # empty folders as "folder/" entries, and the finished archive is read back and refused if any name breaks it.
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $root = (Resolve-Path -LiteralPath $Out).ProviderPath.TrimEnd('\') + '\'
    $archive = [System.IO.Compression.ZipFile]::Open($archivePath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem -LiteralPath $Out -Recurse -File | Sort-Object FullName) {
            $name = $file.FullName.Substring($root.Length).Replace('\', '/')
            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $name,
                [System.IO.Compression.CompressionLevel]::Optimal)
        }
        foreach ($dir in Get-ChildItem -LiteralPath $Out -Recurse -Directory | Sort-Object FullName) {
            if (-not (Get-ChildItem -LiteralPath $dir.FullName -Force | Select-Object -First 1)) {
                [void]$archive.CreateEntry($dir.FullName.Substring($root.Length).Replace('\', '/') + '/')
            }
        }
    } finally {
        $archive.Dispose()
    }
    $check = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $bad = @($check.Entries | Where-Object { $_.FullName -match '\\|^/|^[A-Za-z]:' } | Select-Object -First 3)
        $entries = $check.Entries.Count
    } finally {
        $check.Dispose()
    }
    if ($bad.Count) { throw ("Zip entry names break the zip spec: " + (($bad | ForEach-Object FullName) -join ', ')) }
    Write-Host ("  packaged -> {0}  ({1:N0} bytes, {2} entries, '/' paths)" -f $archivePath, (Get-Item $archivePath).Length, $entries)
}
