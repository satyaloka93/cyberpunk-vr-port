# CyberpunkVRPort 0.1.7-psvr2.1 release candidate

Based on upstream 0.1.7. This is a local release candidate, not a published
GitHub release; combined headset validation is still pending.

- PSVR2 Sense Triangle touch + right stick provides D-pad shift while
  suppressing turn input; Triangle click remains the normal Y action.
- Left Create/SystemButton gives tap-to-Start and hold-to-Back, with
  Triangle+R3 fallback and unchanged L3+R3 overlay shortcut.
- F10 keeps the explicit FOV override/presets. Experimental performance-test
  controls are removed. A session-only stable empty-overlay pacing checkbox
  defaults on; disabling it saves minor overlay work but can revive bike
  frame-rate instability with external OFXR.
- Dedicated versioned haptic IPC avoids upstream 0.1.7 input-slot collisions.
  The optional PSVR2Toolkit bridge/driver is maintained and installed
  separately; it is not bundled in the game-root ZIP.
- A separate switcher ZIP can select upstream or the fork ZIP, verify
  pinned dependencies, guide Nexus downloads, preserve fit/FOV preferences,
  and restore the previous port from a transaction backup. It pins the fork
  game ZIP hash and planned release URL for download after publication, and displays
  PSVR2Toolkit guidance when PSVR2/Sense hardware is detected.
- The optional rapid-fire grip helper is in the switcher asset, not
  auto-installed. It supplements eligible high-rate weapons without replacing
  their adaptive trigger mode.

See `docs/PSVR2-TWEAKS-017.md` in the switcher asset for setup and test limits.
