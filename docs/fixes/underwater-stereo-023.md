# Per-eye underwater reconstruction

Source: dariulone/cyberpunk-vr-port 0.2.3, e91394502e (2026-11-08).

Carry the exact upstream signed shader 4145873fbf34bf87.dxil, source recipe and
release packaging inclusion. Existing stereo_eye loader discovers the added
shader. In SBS it selects clip-to-world rows and coordinates from the pixel's
eye rather than the shared frame camera; outside SBS that selection stays native.

This upstream shader ALSO halves underwater optical depth and depth darkening
in all modes (UNDERWATER_CLARITY=0.5). It is not solely an eye-alignment change.
The source generator and deployed binary are kept together unchanged. No RT/RR
feature allocation, DLSS vectors, general lighting or framegen backend changes.

Validation: binary equals the upstream Git blob byte for byte; Windows SDK dxc
successfully disassembles the DXIL container. MSVC Release builds the loader.
Actual GPU replacement and underwater appearance require a game run.
