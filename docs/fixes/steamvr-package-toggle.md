# Separate SteamVR framegen package toggle

F10 > FRAMEGEN > **SteamVR framegen optimizations (restart required)**.
Stored as `xr_framegen_steamvr_optimizations=1` in the port's config/vrport.ini.
Defaults ON for existing installations with no key, preserving the tested build.

The checkbox saves the requested preference immediately. Restart the game to
apply it. F10 shows the package's active state for the current SteamVR session
and a restart notice if the requested state differs. The control is independent
of upstream's **Adaptive real-frame pacing**, which remains live and unchanged.

At XR initialization, RefreshCadence latches `isSteamVR && preference`. Both
presentation-queue creation and existing synthesis/pacing consumers use that
same latched decision, never the live checkbox. With the package OFF:

- Use the application's OpenXR graphics-binding queue and synthesis queue.
- Use the runtime-reported refresh period and upstream adaptive recovery.
- Disable added SteamVR submission-grid feedback, bounded producer GPU gate,
  and synthesis preparation during the existing idle hold.
- Keep native framegen and upstream Adaptive Pacing available.

VDXR and other non-SteamVR runtimes bypass these adaptations in either setting.
Steam Link is SteamVR, so the control applies there too. Existing backend and
external OFXR restrictions remain: DLSS-G does not use our isolated generation
queue; an identified external XRFrameBridge layer prevents private presentation.
General rendering/scope fixes, synchronization correctness and independently
selected diagnostics remain in place; this is not a stock-binary selector.

Restart is deliberate: the runtime's D3D12 queue is bound at session creation.
Hot-editing only pacing or generator queues would create a partial comparison
and could leave resource ownership inconsistent. Editing then reverting the
preference removes the pending-change notice without altering live routing.

Log marker `upstream022-steamvr-package-toggle-v1`; startup logs requested/active
and SteamVR detection. Flight records separate `steamvr_package_requested`
(in settings) and `steamvr_package_active`. Requested changes are not effective
routing changes until restart. The flight recorder can still measure SteamVR
compositor delivery with this package OFF.

Validation: production Release build; 16 Windows framegen/GPU tests plus HUD.
New fixture uses actual settings Parse/Write/Set/Get and RefreshCadence startup
latching; checks both independent checkbox values, roundtrip, default-on, invalid
input, live no-op then recreation, and non-SteamVR exclusion. Worker test verifies
SteamVR-off never starts refresh polling. Existing queue tests cover shared and
private routes. Recorder fixture verifies requested-on/active-off on VDXR.
Manual F10 restart/application check remains a headset test, not claimed here.

No release or push. Full fork haptics, independent dual-wield aiming and driving
grip toggle remain separate required integration work.
