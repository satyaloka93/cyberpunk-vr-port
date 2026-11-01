# Live V557 / native frame-generation handoff

RC4 requires the matching private backend pinned in `cmake/EmbeddedOfxr.cmake`.
Its `OFXR_SupportsNativeHandoffV1` export identifies the completed-frame
handoff contract. Older backend DLLs are rejected.

The private V557 layer stays in the loader-managed OpenXR chain even when a native
generator is selected. Settings queue a control request; the host sends it only
after closing the current XR frame. Native generation is gated until a later
successful end-frame observes all sessions at the requested disabled revision,
with no pending/error/ready sessions. Generation then resumes through the existing
native presenter. This prevents enabling V557 on a native-generated frame already
being submitted, and prevents native generation while V557 is draining.

Disabled V557 stops its pipelined presenter, including a presenter promoted during
the same bypass call, and cannot promote while disabled. After the disabled receipt, the host calls OFXR_TrimInactiveSessionV1. It drains
GPU work and releases private synthesis/output pools and capture history before
allowing native generation. History is recreated when V557 is enabled again.
Hooks and game-owned images remain; no submitted GPU work is canceled. A switch can hitch while GPU
work drains or SDK contexts initialize. Actual VRAM savings and performance still require in-game verification.

External OFXR remains a separate launch-time layer. Disarm it before launching a
session intended for built-in switching. Native FidelityFX/DLSS-G selection and
DLSS-G fallback behavior are unchanged. The selected generator and successfully
initialized generator are distinct; DLSS-G availability is still checked.

The port log records `[framegen-switch] requested` and `completed` with backend,
revision and native permission. No RT descriptor/allocation diagnostics are
included in this checkout. The host refuses an older private backend DLL.


## Retirement validation, 2026-11-01

Release build and six local checks pass. The real-loader integration fixture now
allocates real D3D12 images/history and requires flight-log receipts for history
retirement (embedded_configuration880) and reconstruction (882). Active trim is
rejected and repeated inactive trim is idempotent. Session-scoped before/after
VRAM samples have subject880/881; these are process usage, not attributable freed
bytes. The transition fixture does not generate full game-size optical-flow
frames. Existing split-eye, cropped-eye, HUD and SteamVR rendering checks pass.

Native logs record initialization, first generated pair, and context retirement.

RC4 deliberately excludes the experimental stereo RR camera changes.
