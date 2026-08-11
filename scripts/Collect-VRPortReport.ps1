# Collect a CyberpunkVR Port environment spec sheet.
#
# Produces a flat, dotted key = value report describing the machine, XR runtime, game,
# mod stack, every VR port setting (including the F10 live-controls menu), the game's
# graphics options, and a summary of the last run.
#
# The point is COMPARABILITY. Two people on different headsets, GPUs and runtimes can
# each run this and diff the output to find what actually differs. Every key is always
# emitted -- absent things print "(absent)" rather than vanishing -- so a plain text
# diff lines up between machines and an agent can parse it without guessing.
#
# Usage:
#   pwsh scripts\Collect-VRPortReport.ps1 -GameRoot "D:\SteamLibrary\steamapps\common\Cyberpunk 2077"
#   pwsh scripts\Collect-VRPortReport.ps1 -GameRoot <path> -Label "gthom-psvr2" -Json
#
# Nothing is modified. Read-only. No elevation required.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot,

    # Free-text name for this machine/config, so diffs are attributable.
    [string]$Label = $env:COMPUTERNAME,

    # Headset model. Cannot be detected reliably across runtimes -- state it.
    [string]$Hmd = "",

    [string]$OutFile,

    # Also emit <OutFile>.json for programmatic consumption.
    [switch]$Json
)

$ErrorActionPreference = "Continue"
$ProgressPreference    = "SilentlyContinue"

$SchemaVersion = "cyberpunkvrport-env/1"

if (-not (Test-Path -LiteralPath $GameRoot)) { throw "GameRoot not found: $GameRoot" }
$BinX64 = Join-Path $GameRoot "bin\x64"
if (-not (Test-Path -LiteralPath (Join-Path $BinX64 "Cyberpunk2077.exe"))) {
    throw "Not a Cyberpunk 2077 install (no bin\x64\Cyberpunk2077.exe): $GameRoot"
}

if (-not $OutFile) {
    $stamp   = Get-Date -Format "yyyyMMdd-HHmmss"
    $safe    = ($Label -replace '[^A-Za-z0-9_.-]', '-')
    $OutFile = Join-Path $PSScriptRoot "..\build\reports\vrport-env-$safe-$stamp.txt"
}
$outDir = Split-Path -Parent $OutFile
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

# ---------------------------------------------------------------- helpers

$script:Kv = [ordered]@{}

function Add-Kv {
    param([string]$Key, $Value)
    if ($null -eq $Value) { $Value = "(absent)" }
    elseif ($Value -is [bool]) { $Value = if ($Value) { "yes" } else { "no" } }
    else {
        $Value = [string]$Value
        # Keep one key on one line so the report stays diffable.
        $Value = ($Value -replace '\r?\n', ' ').Trim()
        if ($Value -eq "") { $Value = "(absent)" }
    }
    $script:Kv[$Key] = $Value
}

function Add-Section { param([string]$Name) $script:Kv["# $Name"] = $null }

function Get-Sha256 {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    try { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLower() } catch { $null }
}

function Get-FileVersionString {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    try { (Get-Item -LiteralPath $Path).VersionInfo.FileVersion } catch { $null }
}

# vrport.ini and friends are bare key=value with no [section] header, so the Win32
# profile APIs cannot read them.
function Import-FlatIni {
    param([string]$Path, [string]$Prefix)
    if (-not (Test-Path -LiteralPath $Path)) {
        Add-Kv "$Prefix._file" "(absent)"
        return
    }
    Add-Kv "$Prefix._file" $Path
    foreach ($line in (Get-Content -LiteralPath $Path -ErrorAction SilentlyContinue)) {
        $t = $line.Trim()
        if ($t -eq "" -or $t.StartsWith("#") -or $t.StartsWith(";") -or $t.StartsWith("[")) { continue }
        $eq = $t.IndexOf('=')
        if ($eq -lt 1) { continue }
        $k = $t.Substring(0, $eq).Trim()
        $v = $t.Substring($eq + 1).Trim()
        Add-Kv "$Prefix.$k" $v
    }
}

function Get-LastMatch {
    param([string]$Path, [string]$Pattern)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    try {
        $m = Select-String -LiteralPath $Path -Pattern $Pattern -ErrorAction SilentlyContinue | Select-Object -Last 1
        if ($m -and $m.Matches.Count -gt 0 -and $m.Matches[0].Groups.Count -gt 1) {
            return $m.Matches[0].Groups[1].Value
        }
    } catch { }
    $null
}

function Get-Median {
    param([double[]]$Values)
    if (-not $Values -or $Values.Count -eq 0) { return $null }
    $s = $Values | Sort-Object
    $n = $s.Count
    if ($n % 2 -eq 1) { return [math]::Round($s[[int](($n - 1) / 2)], 1) }
    return [math]::Round((($s[$n / 2 - 1] + $s[$n / 2]) / 2), 1)
}

# ---------------------------------------------------------------- report

Add-Section "report"
Add-Kv "report.schema"     $SchemaVersion
Add-Kv "report.generated"  (Get-Date -Format "yyyy-MM-ddTHH:mm:sszzz")
Add-Kv "report.label"      $Label
Add-Kv "report.tool"       "Collect-VRPortReport.ps1"

# ---------------------------------------------------------------- system

Add-Section "system"
try {
    $os  = Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue
    $cs  = Get-CimInstance Win32_ComputerSystem -ErrorAction SilentlyContinue
    $cpu = Get-CimInstance Win32_Processor -ErrorAction SilentlyContinue | Select-Object -First 1
    $mem = @(Get-CimInstance Win32_PhysicalMemory -ErrorAction SilentlyContinue)

    Add-Kv "system.os_caption"   $os.Caption
    Add-Kv "system.os_build"     $os.Version
    Add-Kv "system.last_boot"    ($os.LastBootUpTime).ToString("yyyy-MM-ddTHH:mm:ss")
    Add-Kv "system.uptime_min"   ([math]::Round(((Get-Date) - $os.LastBootUpTime).TotalMinutes, 1))
    Add-Kv "system.cpu"          $cpu.Name
    Add-Kv "system.cpu_cores"    $cpu.NumberOfCores
    Add-Kv "system.cpu_threads"  $cpu.NumberOfLogicalProcessors
    $installedGb = if ($mem.Count) { [math]::Round((($mem | Measure-Object -Property Capacity -Sum).Sum) / 1GB, 0) } else { $null }
    Add-Kv "system.ram_installed_gb" $installedGb
    Add-Kv "system.ram_usable_gb"    ([math]::Round($cs.TotalPhysicalMemory / 1GB, 1))
    Add-Kv "system.ram_modules"      $mem.Count
    # The kit's RATED speed is only in the part number (e.g. F5-6000... = DDR5-6000).
    # SMBIOS reports the speed in use, so a part number above these values means
    # EXPO/XMP is off or not fully applied.
    Add-Kv "system.ram_part"           ($mem | Select-Object -First 1).PartNumber
    Add-Kv "system.ram_speed_mhz"      ($mem | Select-Object -First 1).Speed
    Add-Kv "system.ram_configured_mhz" ($mem | Select-Object -First 1).ConfiguredClockSpeed
} catch { }

try {
    $dg = Get-CimInstance -ClassName Win32_DeviceGuard -Namespace root\Microsoft\Windows\DeviceGuard -ErrorAction SilentlyContinue
    Add-Kv "system.vbs_running" ($dg.VirtualizationBasedSecurityStatus -eq 2)
} catch { Add-Kv "system.vbs_running" $null }

# ---------------------------------------------------------------- gpu

Add-Section "gpu"
try {
    $adapters = @(Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue)
    # "Primary" = the real render adapter, not a virtual monitor from a streaming app.
    $primary = $adapters |
        Where-Object { $_.Name -notmatch 'Virtual|Meta|Remote|Basic Display|Parsec' } |
        Sort-Object -Property @{ Expression = { $_.AdapterRAM } } -Descending |
        Select-Object -First 1
    if (-not $primary) { $primary = $adapters | Select-Object -First 1 }

    Add-Kv "gpu.name"           $primary.Name
    Add-Kv "gpu.driver_version" $primary.DriverVersion
    Add-Kv "gpu.driver_date"    $(if ($primary.DriverDate) { $primary.DriverDate.ToString("yyyy-MM-dd") } else { $null })
    Add-Kv "gpu.video_processor" $primary.VideoProcessor
    Add-Kv "gpu.adapter_count"  $adapters.Count

    # Virtual display drivers are kernel-mode and have caused trouble here; list them all.
    $i = 0
    foreach ($a in $adapters) {
        Add-Kv "gpu.adapter[$i].name"    $a.Name
        Add-Kv "gpu.adapter[$i].driver"  $a.DriverVersion
        Add-Kv "gpu.adapter[$i].date"    $(if ($a.DriverDate) { $a.DriverDate.ToString("yyyy-MM-dd") } else { $null })
        Add-Kv "gpu.adapter[$i].status"  $a.Status
        Add-Kv "gpu.adapter[$i].cm_error" $a.ConfigManagerErrorCode
        $i++
    }
} catch { }

# ---------------------------------------------------------------- xr runtime

Add-Section "xr"
try {
    $rt = (Get-ItemProperty 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -ErrorAction SilentlyContinue).ActiveRuntime
    Add-Kv "xr.active_runtime_json" $rt
    $rtName = "(absent)"
    if ($rt -and (Test-Path -LiteralPath $rt)) {
        try {
            $rtj = Get-Content -LiteralPath $rt -Raw | ConvertFrom-Json
            $rtName = $rtj.runtime.name
            if (-not $rtName) { $rtName = Split-Path -Leaf $rt }
        } catch { $rtName = Split-Path -Leaf $rt }
    } elseif ($rt) { $rtName = Split-Path -Leaf $rt }
    Add-Kv "xr.runtime_name" $rtName

    # Implicit API layers inject themselves into every OpenXR app. ReShade and the
    # Virtual Desktop / Oculus compatibility layer both show up here and both change
    # behaviour, so they belong on the sheet.
    # Khronos convention: the registry DWORD is a DISABLE flag. 0 = layer is active,
    # non-zero = present but switched off. Spell that out -- the raw "path=0" reads
    # backwards to anyone who has not memorised the spec.
    $layers  = @()
    $enabled = 0
    foreach ($hive in @('HKLM:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit',
                        'HKCU:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit')) {
        $p = Get-ItemProperty $hive -ErrorAction SilentlyContinue
        if (-not $p) { continue }
        foreach ($prop in ($p.PSObject.Properties | Where-Object { $_.Name -notlike 'PS*' })) {
            $isOn = ($prop.Value -eq 0)
            if ($isOn) { $enabled++ }
            $layers += ("{0} [{1}]" -f $prop.Name, $(if ($isOn) { "enabled" } else { "disabled" }))
        }
    }
    Add-Kv "xr.api_layers_implicit_total"   $layers.Count
    Add-Kv "xr.api_layers_implicit_enabled" $enabled
    Add-Kv "xr.api_layers_implicit"         ($layers -join ' ; ')
} catch { }

try {
    $steamVr = "${env:ProgramFiles(x86)}\Steam\steamapps\common\SteamVR"
    Add-Kv "xr.steamvr_version" (Get-FileVersionString (Join-Path $steamVr "bin\win64\vrserver.exe"))
    $vrs = "${env:ProgramFiles(x86)}\Steam\config\steamvr.vrsettings"
    if (Test-Path -LiteralPath $vrs) {
        $v = Get-Content -LiteralPath $vrs -Raw | ConvertFrom-Json
        Add-Kv "xr.steamvr_settings_file"  $vrs
        Add-Kv "xr.steamvr_supersample"    $v.steamvr.supersampleScale
        Add-Kv "xr.steamvr_manual_override" $v.steamvr.supersampleManualOverride
        Add-Kv "xr.steamvr_motion_smoothing" $v.steamvr.motionSmoothing
        Add-Kv "xr.steamvr_async_reprojection" $v.steamvr.enableAsyncReprojection
    } else {
        Add-Kv "xr.steamvr_settings_file" $null
    }
} catch { }

Add-Kv "xr.hmd_declared" $(if ($Hmd) { $Hmd } else { "(state with -Hmd)" })

# ---------------------------------------------------------------- game

Add-Section "game"
$exe = Join-Path $BinX64 "Cyberpunk2077.exe"
Add-Kv "game.path"             $GameRoot
Add-Kv "game.exe_file_version" (Get-FileVersionString $exe)
Add-Kv "game.exe_size"         (Get-Item -LiteralPath $exe).Length
Add-Kv "game.store" $(
    if (Test-Path -LiteralPath (Join-Path $BinX64 "steam_api64.dll")) { "steam" }
    elseif (Test-Path -LiteralPath (Join-Path $GameRoot "goggame-*.info")) { "gog" }
    else { "unknown" }
)

$red4extLog = Get-ChildItem (Join-Path $GameRoot "red4ext\logs\red4ext-*.log") -ErrorAction SilentlyContinue |
              Sort-Object LastWriteTime | Select-Object -Last 1
$red4extVersion = $null
if ($red4extLog) {
    Add-Kv "game.product_version" (Get-LastMatch $red4extLog.FullName 'Product version:\s*(\S+)')
    $red4extVersion = Get-LastMatch $red4extLog.FullName 'RED4ext \(v([^)]+)\)'
} else {
    Add-Kv "game.product_version" $null
}

# ---------------------------------------------------------------- mod stack

Add-Section "mods"
Add-Kv "mods.red4ext_version" $red4extVersion
$cetLog = Join-Path $BinX64 "plugins\cyber_engine_tweaks\cyber_engine_tweaks.log"
Add-Kv "mods.cet_version" (Get-LastMatch $cetLog 'CET version\s+(\S+)')

$plugDir = Join-Path $GameRoot "red4ext\plugins"
$plugins = @(Get-ChildItem $plugDir -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
Add-Kv "mods.red4ext_plugin_count" $plugins.Count
Add-Kv "mods.red4ext_plugins"      ($plugins -join ' ; ')

$cetModDir = Join-Path $BinX64 "plugins\cyber_engine_tweaks\mods"
$cetMods = @(Get-ChildItem $cetModDir -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
Add-Kv "mods.cet_mod_count" $cetMods.Count
Add-Kv "mods.cet_mods"      ($cetMods -join ' ; ')

# ReShade injects globally on this setup rather than as a local dxgi.dll, so check both.
$reshadeLog = Join-Path $BinX64 "ReShade.log"
Add-Kv "mods.reshade_version" (Get-LastMatch $reshadeLog "ReShade version '([^']+)'")
Add-Kv "mods.reshade_source"  (Get-LastMatch $reshadeLog "loaded from '([^']+)'")
Add-Kv "mods.local_dxgi_proxy" (Test-Path -LiteralPath (Join-Path $BinX64 "dxgi.dll"))

# Streamline / upscaler DLLs materially change the Present path.
foreach ($d in @("nvngx_dlss.dll", "nvngx_dlssg.dll", "nvngx_dlssd.dll",
                 "sl.interposer.dll", "sl.dlss.dll", "sl.dlss_g.dll", "sl.reflex.dll")) {
    Add-Kv "mods.upscaler.$d" (Get-FileVersionString (Join-Path $BinX64 $d))
}

# ---------------------------------------------------------------- vr port build

Add-Section "vrport_build"
$stereoDll = Join-Path $plugDir "CyberpunkVR_Stereo\CyberpunkVR_Stereo.dll"
$handsDll  = Join-Path $plugDir "CyberpunkVR_Hands\CyberpunkVR_Hands.dll"
Add-Kv "vrport_build.stereo_dll_sha256" (Get-Sha256 $stereoDll)
Add-Kv "vrport_build.stereo_dll_size"   $(if (Test-Path -LiteralPath $stereoDll) { (Get-Item -LiteralPath $stereoDll).Length } else { $null })
Add-Kv "vrport_build.stereo_dll_mtime"  $(if (Test-Path -LiteralPath $stereoDll) { (Get-Item -LiteralPath $stereoDll).LastWriteTime.ToString("yyyy-MM-ddTHH:mm:ss") } else { $null })
Add-Kv "vrport_build.hands_dll_sha256"  (Get-Sha256 $handsDll)

# ---------------------------------------------------------------- vr port settings

# vrport.ini is where the F10 live-controls menu persists: stereo scale, world/IPD
# scale, sharpness, smoothing, prediction, snap turn, movement source, menu FOV and
# the rest. These change image and behaviour directly and are the first thing to
# compare between two machines that "look different".
Add-Section "vrport (F10 live controls)"
Import-FlatIni (Join-Path $BinX64 "vrport.ini")          "vrport"
Import-FlatIni (Join-Path $BinX64 "vrport-launcher.ini") "vrport_launcher"

Add-Section "vrport_hud (F10 HUD layout)"
Import-FlatIni (Join-Path $cetModDir "CyberpunkVRPort_HUD\hud_layout.ini") "vrport_hud"

Add-Section "vrport_vrik"
Import-FlatIni (Join-Path $cetModDir "CyberpunkVRPort_VRIK\vrik_settings.ini") "vrport_vrik"
Import-FlatIni (Join-Path $plugDir "CyberpunkVR_Stereo\vrik_calibration.ini")  "vrport_vrik_calib"

Add-Section "vrport_vrcam"
$vrcamJson = Join-Path $cetModDir "CyberpunkVRPort_Stereo\vrcam.json"
if (Test-Path -LiteralPath $vrcamJson) {
    try {
        $vc = Get-Content -LiteralPath $vrcamJson -Raw | ConvertFrom-Json
        Add-Kv "vrport_vrcam.component"      $vc.component
        Add-Kv "vrport_vrcam.virtual_camera" $vc.virtualCamera
    } catch { }
} else {
    Add-Kv "vrport_vrcam.component" $null
    Add-Kv "vrport_vrcam.virtual_camera" $null
}

# ---------------------------------------------------------------- graphics

# Every /graphics and /video option, so upscaler mode, ray tracing, frame generation
# and resolution can be compared directly rather than described in prose.
Add-Section "graphics (UserSettings.json)"
$userSettings = Join-Path $env:LOCALAPPDATA "CD Projekt Red\Cyberpunk 2077\UserSettings.json"
Add-Kv "graphics._file" $(if (Test-Path -LiteralPath $userSettings) { $userSettings } else { $null })
if (Test-Path -LiteralPath $userSettings) {
    try {
        $us = Get-Content -LiteralPath $userSettings -Raw | ConvertFrom-Json
        foreach ($group in $us.data) {
            if ($group.group_name -notmatch '^/(graphics|video|display)') { continue }
            $g = ($group.group_name -replace '^/', '' -replace '/', '.')
            foreach ($opt in $group.options) {
                Add-Kv "graphics.$g.$($opt.name)" $opt.value
            }
        }
    } catch {
        Add-Kv "graphics._parse_error" $_.Exception.Message
    }
}

# ---------------------------------------------------------------- last run

# A spec sheet without a behaviour baseline is hard to act on: two identical configs
# performing differently is itself the finding.
Add-Section "lastrun"
$log = Join-Path $BinX64 "cyberpunkvrport.log"
Add-Kv "lastrun.log_file" $(if (Test-Path -LiteralPath $log) { $log } else { $null })
if (Test-Path -LiteralPath $log) {
    Add-Kv "lastrun.log_mtime"    (Get-Item -LiteralPath $log).LastWriteTime.ToString("yyyy-MM-ddTHH:mm:ss")
    Add-Kv "lastrun.pacing_mode"  (Get-LastMatch $log 'injectedDxgiWait=\d+ mode=(\S+)')
    Add-Kv "lastrun.swapchain"    (Get-LastMatch $log 'OVERLAY-PACING\] .*? size=(\S+)')
    Add-Kv "lastrun.xr_cycles"    (Get-LastMatch $log 'XR pacing: cycles=(\d+)')
    Add-Kv "lastrun.xr_missed"    (Get-LastMatch $log 'XR pacing: cycles=\d+ submits=\d+ missed=(\d+)')
    Add-Kv "lastrun.save_loads"   (@(Select-String -LiteralPath $log -Pattern 'save load' -ErrorAction SilentlyContinue)).Count

    $fps = @(Select-String -LiteralPath $log -Pattern '\[PERF\] present=([\d.]+)' -ErrorAction SilentlyContinue |
             ForEach-Object { [double]$_.Matches[0].Groups[1].Value })
    Add-Kv "lastrun.perf_samples"    $fps.Count
    Add-Kv "lastrun.present_fps_median" (Get-Median $fps)
    Add-Kv "lastrun.present_fps_min" $(if ($fps.Count) { [math]::Round(($fps | Measure-Object -Minimum).Minimum, 1) } else { $null })
    Add-Kv "lastrun.present_fps_max" $(if ($fps.Count) { [math]::Round(($fps | Measure-Object -Maximum).Maximum, 1) } else { $null })

    foreach ($sym in @(
        @{ k = "overlay_fence_timeouts"; p = 'fence wait timed out' },
        @{ k = "overlay_disabled";       p = 'overlay disabled' },
        @{ k = "drain_timeouts";         p = 'full drain did not complete' },
        @{ k = "bbidx_reuse";            p = 'recorded backbuffer .* twice' },
        @{ k = "device_removed";         p = 'DXGI_ERROR_DEVICE_HUNG|device removal|887a' })) {
        Add-Kv "lastrun.$($sym.k)" (@(Select-String -LiteralPath $log -Pattern $sym.p -ErrorAction SilentlyContinue)).Count
    }
}

Add-Section "stability (last 90 days)"
try {
    $rq = Join-Path $env:LOCALAPPDATA "REDEngine\ReportQueue"
    $reports = @(Get-ChildItem $rq -Directory -ErrorAction SilentlyContinue |
                 Where-Object { $_.LastWriteTime -gt (Get-Date).AddDays(-90) })
    Add-Kv "stability.game_crash_reports" $reports.Count
    $hangs = @($reports | Where-Object {
        Get-ChildItem (Join-Path $_.FullName "attch") -Filter "gpucrash-*.log" -ErrorAction SilentlyContinue })
    Add-Kv "stability.gpu_crash_reports" $hangs.Count
    Add-Kv "stability.latest_crash" $(if ($reports) { ($reports | Sort-Object LastWriteTime | Select-Object -Last 1).Name } else { $null })
} catch { }
try {
    $bc = @(Get-WinEvent -FilterHashtable @{ LogName = 'System'; Id = 1001; StartTime = (Get-Date).AddDays(-90) } -ErrorAction SilentlyContinue |
            Where-Object { $_.ProviderName -match 'WER-SystemErrorReporting' })
    Add-Kv "stability.bugchecks" $bc.Count
    Add-Kv "stability.bugcheck_codes" (($bc | ForEach-Object { if ($_.Message -match 'bugcheck was: (0x[0-9a-fA-F]+)') { $Matches[1] } } | Sort-Object -Unique) -join ' ; ')
    $nv = @(Get-WinEvent -FilterHashtable @{ LogName = 'System'; StartTime = (Get-Date).AddDays(-90) } -ErrorAction SilentlyContinue |
            Where-Object { $_.ProviderName -match 'nvlddmkm|amdkmdag|igfx' })
    Add-Kv "stability.gpu_driver_events" $nv.Count
} catch { }

# ---------------------------------------------------------------- emit

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# CyberpunkVR Port environment report")
$lines.Add("# schema $SchemaVersion -- flat 'key = value', one per line, every key always present.")
$lines.Add("# Compare two machines with a plain text diff. '(absent)' means not found.")
$lines.Add("")
foreach ($k in $script:Kv.Keys) {
    if ($k.StartsWith("# ")) { $lines.Add(""); $lines.Add("#### $($k.Substring(2))") }
    else { $lines.Add("$k = $($script:Kv[$k])") }
}

Set-Content -LiteralPath $OutFile -Value $lines -Encoding utf8
Write-Host "[ok] wrote $OutFile  ($(($script:Kv.Keys | Where-Object { -not $_.StartsWith('# ') }).Count) keys)"

if ($Json) {
    $jsonPath = [IO.Path]::ChangeExtension($OutFile, ".json")
    $obj = [ordered]@{}
    foreach ($k in $script:Kv.Keys) { if (-not $k.StartsWith("# ")) { $obj[$k] = $script:Kv[$k] } }
    $obj | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $jsonPath -Encoding utf8
    Write-Host "[ok] wrote $jsonPath"
}
