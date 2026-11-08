# Restore Quest thumbrest controls on the 0.2.2 base

The PSVR2 controls backport omitted the Quest controls from our earlier fork.
Restore the Oculus Touch `thumbrest_touch` action, both per-hand touch bindings,
and the left-hand modifier. Sources: PR extraction 3c406e96 / 0.1.9 fork action
setup and the preserved RC5 Quest inventory change in
`build/build/quest-inventory-isolation/working/src/Runtimes/OpenXRFrameLoop.cpp`
(root workspace path). This is the existing control scheme, not a new mapping.

- Touch left thumbrest + move right stick: D-pad at +/-0.5; suppress look and
  pending snap turns throughout the touch hold, even inside the dead zone.
- Touch left thumbrest + click right stick: XInput Back/Select, inventory/hub.
  Consume ordinary R3 so it does not also crouch or change scanner tab.
- Bare R3 retains its normal action. Release thumbrest to use scanner-tab R3.
- L3+R3 still belongs to the upstream overlay, even with thumbrest touched.
- Existing L3 D-pad fallback and PSVR2 Triangle/Create behavior are retained.

Thumbrest paths are added inside the Touch profile's one binding suggestion,
so gameplay/haptic suggestions are not replaced. PSVR2 does not receive this
action; unsupported profiles remain unbound. This does not promise native Pico
support unless the runtime exposes the same Touch thumbrest input. No headset
model guess or SteamVR-only gate: both VDXR and Steam Link can supply it.

Input must be active and session focused. Shared touch-shift flag carries look
suppression into XInput merge. Action handles reset on shutdown. The system menu
button is unchanged: the restored inventory shortcut uses thumbrest + R3, without
relying on reserved Meta/Steam Link menu-button holds.

Validation: production Release build; controller policy fixture covers existing
PSVR2 tap/hold behavior and D-pad suppression plus inventory vs bare R3, release,
and L3+R3 priority. Headset validation of this restoration is pending. Restart the
game for the added OpenXR action/bindings. Separate from the Steam Link controller
binding reload/hand-tracking incident; this patch does not claim to fix that.
