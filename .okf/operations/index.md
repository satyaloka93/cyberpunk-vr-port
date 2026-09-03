# Operations

* [Direct installation and clean port replacement](direct-install-lifecycle.md) — Remove an old port payload while preserving shared dependencies and backing up user-adjustable VR state.
* [Runtime dependency compatibility gate](runtime-dependency-compatibility.md) — Check CET/game compatibility before diagnosing flat stereo or missing CET-driven features.
* [PSVR2 adaptive triggers and grip haptics](psvr2-adaptive-triggers.md) — Install and operate the optional Enhanced DualSense Support to PSVR2Toolkit bridge.
* [Generic OpenXR controller haptics](generic-openxr-haptics.md) — Route confirmed gun and melee events to Quest/Touch and other OpenXR controllers without competing with the PSVR2 bridge.
* [Optional low-spec engine profile](optional-low-spec-profile.md) — Keep the historical CPU/streaming INI inactive by default because it also reduces visible fidelity; enable only after a matched A/B.
* [HUDitor VR layout workflow](huditor-vr-layout.md) — Install HUDitor, resolve its F7 conflict, align standard widgets, and preserve both independent HUD layout files.
* [Wabbajack installation automation](wabbajack-automation.md) — Build and validate the portable MO2 profile while preserving source attribution and user-owned VR configuration.
* [Diagnostic logging discipline](diagnostic-logging-discipline.md) — Throttle on novelty, never on a session-wide counter, and never discard the field that tells two callers apart; both mistakes have already blinded a crash here.
* [Host machine stability as a crash confound](host-machine-stability-confound.md) — What survives scrutiny about this machine's hardware faults, what was retracted, and how to tell a machine hang from a port defect.
* [OKF maintenance and GitHub links](okf-maintenance.md) — Keep concepts current and repair links that OKF consumers understand but GitHub cannot resolve.
* [Selective upstream import policy](selective-upstream-imports.md) — Import isolated improvements without replacing validated PSVR2, stereo, DLSSNR, or save-load behavior.
