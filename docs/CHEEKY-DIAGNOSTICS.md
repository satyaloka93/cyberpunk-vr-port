# Check Cheeky installation and recent errors

Run `Switch-CyberpunkVRPort.bat`, then choose **11 — Check installation and recent errors (read-only)**. It can run while the game is open. No repair, download, DLL removal, settings change, registry change or process termination occurs.

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

## RC1 preparation status

This diagnostic option is implemented for the upcoming 0.2.3 RC1 workflow. The preparation package retains the previously published upstream 0.2.2/fork RC5/VRCAM download targets. It does **not** yet download the locally tested RC1 DLL. Release publication must pin the completed RC1 archive and update its dependency manifest and labels together. The local Cheeky diagnostic-proxy identities identify a debug package, not a bundled/released Cheeky installer.
