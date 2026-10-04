# Update Log

## 2026-10-04

* **Confirmed bike haptics**: User confirmed bilateral road feedback and the corrected R2 clutch feel. Transfer the full original L2 resistance/shift/resistance sequence to R2, with continuous engine trigger feedback on L2. Installer supports clean setup and the earlier patch, with backups and regression coverage. The bridge audio change is committed separately in PSVR2Toolkit as `c080162`. See [bike haptics](bike-haptics.md).

## 2026-10-03

* **Vehicle grip toggle**: Restored the fork's per-hand driving grip toggle on the 0.1.7 branch: first click near the animated wheel/handlebar latches the arm; the next click releases it anywhere. Camera-loss maintenance preserves a latched grab until tracking, vehicle state, or hand ownership is lost. Updated the existing driving regression and F10 documentation. See [vehicle grip toggle](vehicle-grip-toggle.md).

## 2026-10-02

* **0.1.7 PSVR2 prerelease published**: Published tag `0.1.7-psvr2.1` on the fork with both the game ZIP and switcher ZIP; current asset hashes and headset status are in [release changes](release-017-changes.md). Opened focused upstream PR [#96](https://github.com/dariulone/cyberpunk-vr-port/pull/96) for the controls, haptics, FOV and pacing changes.

* **Plugin-only headset candidate deployed**: After matching backups, copied only the clean-built DLL/PDB into the closed game and verified installed hashes. Full switcher was not applied because it would disable ReShade/HUDitor. See [0.1.7 release changes](release-017-changes.md); headset outcome pending.

* **Clean release candidate verified**: Commit d57af2c built in an isolated checkout, two local ZIPs packaged, and the switcher passed 30 assertions against the real fork ZIP in a temporary game fixture. The exact ZIP hashes and remaining headset gate are in [0.1.7 release changes](release-017-changes.md).

* **Release scope isolated**: PSVR2 haptics IPC and the session-only empty-overlay pacing toggle were separated from experimental GPU/HUD probes. MSVC Release build and five grip-only Lua tests passed. See [0.1.7 release changes](release-017-changes.md).

* **Switcher prepared**: Added separate upstream/fork switcher asset, pinned dependency acquisition, Nexus handoff, backup/restore and optional PSVR2Toolkit guidance. The 21 non-integration fixture assertions pass; no live game installation or public Cyberpunk release was performed. See [release switcher](release-switcher.md).

## 2026-09-29

* **FOV control**: Restored the user's 104.5-degree override; added explicit override/preset controls and camera FOV readout. Corrected the misleading projection-only label. See [FOV override](fov-override.md); headset edge/clarity validation pending.
* **Creation**: Recorded [PSVR2 input and bridge integration](psvr2-input-bridge.md), upstream base, one-branch policy, separate future general-haptics commit and the legacy shared-slot collision.
* **Deployment**: MSVC Release build and 24 policy checks passed; deployed DLL/PDB and safe bridge launcher with verified backups and unchanged user settings. Physical headset test pending.
