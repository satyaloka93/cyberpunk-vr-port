# Check Cheeky installation and recent errors

Run `Switch-CyberpunkVRPort.bat`, then choose **11 — Check installation and recent errors (read-only)**. It can run while the game is open. No repair, download, DLL removal, settings change, registry change or process termination occurs.

## Concise by default

Option 11 prints a quick summary: loader identities, Cheeky layer state, relevant
layer/environment warnings, startup readiness and at most eight prioritized
unresolved log findings. Paths/hashes/context remain in the detailed report.
Stale logs and absent alternate/legacy log locations are counted instead of dumped.

Only four known optional user32 import-hook messages (MoveWindow, GetCursorInfo,
GetSystemMetrics and GetMessagePos) are hidden from individual error listings.
An NR search failure is summarized as recovered only when a nearby later search
succeeds, with matching thread/PID when those identifiers exist. A later unresolved
repeat stays visible. The first empty mono capture followed immediately by eye
surface creation is a startup observation, not a claim of successful presentation.
Fatal/GPU, loader, OpenXR, script and other unresolved failures remain review items;
shutdown or Host readiness never clears an unrelated error.

Use `-DetailedDiagnostics` with `-Action Diagnose` for all console detail.
`-ReportPath` always exports the summary **and** full detailed evidence, while the
console remains concise unless `-DetailedDiagnostics` is also supplied. No raw
log content is deleted or changed by these display filters.

The report identifies DXGI, VERSION, chained DXGI2 and RED4ext components using exact catalog hashes and embedded versions. **UNKNOWN does not mean broken.** Recognized Cheeky DXGI and VERSION files together are reported as possible duplicate entry points. They are alternatives: never overwrite an ASI Loader `version.dll` or RED4ext `winmm.dll` to make room for Cheeky.

The inspector reads the selected game's Cheeky Loader, Host and Standalone logs, modern/legacy VR port logs, RED4ext logs and redscript log. It shows paths, modification times, latest retained startup boundary, and PID/UTC where present. New Cheeky diagnostic loaders report `PROCESS_ATTACH`, module/build identity, host startup and Win32/export failures. Older loaders may not write that log at all.

Each log is limited to its last 2 MiB. Earlier history may be excluded; line numbers are explicitly tail-relative in that case. UTF-8, UTF-16 and mixed appended text are handled. Up to 24 distinct findings per log are shown with adjacent context and occurrence counts. Repeated messages are grouped. Later startup/fallback success remains visible and does not automatically clear unrelated errors.

- **MISSING / UNREADABLE:** no log available or access/sharing problem; not proof of a mod conflict.
- **STALE:** older than the latest dated loader start (10-minute tolerance), or more than 24 hours old.
- **UNCORRELATED:** no dated session evidence ties this component to the current launch. A recent modification time alone cannot prove correlation.
- **LOADER/HOST:** check loader, Host and Runtime came from the same Cheeky package. A missing host can also mean a missing dependency; retain the Win32 code.
- **FATAL/GPU / OPENXR / DEPENDENCY/SCRIPT:** inspect the named component and same-session logs. The inspector does not attribute a game or GPU failure to Cheeky merely because both are installed.
- **CANDIDATE:** generic error/failed keyword; inspect context. No matching lines does not prove compatibility.

To export a local report, use PowerShell from the extracted switcher directory:

```powershell
.\scripts\Switch-CyberpunkVRPort.ps1 -Action Diagnose -GameRoot 'D:\SteamLibrary\steamapps\common\Cyberpunk 2077' -ReportPath "$env:USERPROFILE\Desktop\Cyberpunk-Cheeky-report.txt"
```

Use your actual game path. The report must be outside the game folder, use a new filename, and have an existing parent directory without junctions/symlinks. Reports contain paths/usernames; review them before sharing. Nothing is uploaded automatically.

## OpenXR layer checks (RC1 switcher update)

Option 11 also reads the **64-bit** HKCU and HKLM registry views at
`SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit` and `...\Explicit`.
It does not require elevation to write anything; unreadable keys are reported as an incomplete scan.
The 32-bit registrations are not used to diagnose this 64-bit game.

For each registration it shows the registry key, DWORD value, manifest path,
manifest-declared layer name, resolved DLL path, embedded file version and SHA-256.
The manifest's `implementation_version` is not treated as a release version.
A zero DWORD enables an implicit registration; explicit registration only makes
that layer available to an application. Nonzero DWORD values disable entries.
Neither registry presence nor a disk hash proves which DLL a running game loaded.

Checks identify missing/malformed manifests, missing/unreadable DLLs, different
enabled manifests declaring the same layer name, repeated registration of the
same manifest, and possible Cheeky layer/Host/Runtime version-label differences.
Different labels are advisory; equal labels do not guarantee matching releases.
Cheeky plus OFXR is reported without assuming incompatibility. Unknown or upstream
files are not automatically classified as broken.

The report shows relevant Process/User/Machine environment overrides, including
`CHEEKY_OPENXR_LAYER_DISABLE`, `XR_API_LAYER_PATH`, `XR_ADD_API_LAYER_PATH`,
`XR_ENABLE_API_LAYERS`, `XR_LOADER_LAYERS_ENABLE/DISABLE/ALLOW`, `XR_RUNTIME_JSON`,
and manifest-declared enable/disable variable names. Process values belong to the
diagnostic process, not the game. Steam may hold older inherited values until
restarted. Environment-specified directories are reported, not recursively scanned.
Only local drive paths are opened; network/device paths are reported uninspected.
Manifest reads are capped at 1 MiB, DLL hashing at 256 MiB, and registrations at
256 per key. Access errors and caps make the scan incomplete rather than proving absence.

### Deconflicting a mismatched installation

1. Close VR games before changing their shared layer.
2. Install the game loader/Host/Runtime and `OpenXR/CheekyOpenXRSetup.exe` from
   the same chosen Cheeky release. Updating game files alone does not update the
   shared layer. The usual manifest is
   `C:\Program Files\CheekyFoveatedDLSS\OpenXR\XR_APILAYER_CHEEKY_foveated_dlss.json`.
3. If the report shows another enabled Cheeky manifest in an old directory,
   confirm its ownership and back up that exact registry value before disabling
   only the stale entry (DWORD 1). Keep the matching layer for stereo calibration
   and eye tracking. Do not delete the shared Khronos registry key.
4. Restart the applications and inspect same-run logs/loaded-module evidence.
   An already running game can retain the old DLL even after the files change.

This switcher performs **none of those repairs automatically**. It does not alter
layers, environment variables, drivers, loaders, game files or framegen settings.
Current option 4 remains our published 0.2.45 PSVR2 RC1; option 1 remains stock
upstream 0.2.45. This diagnostic update does not change the port binaries.
