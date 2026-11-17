# Native foveated-DLSS hook reliability

Adapted from upstream 0.2.45 commit
`6f33af62c50a85f88eceaa63f776539d8fc6f395`.

The previous installer tried MinHook every two seconds until success. When no
nearby executable memory was available, its address-space search repeated on
Present. This patch bounds installation to three attempts per DLSS module and
adds upstream's absolute-jump fallback for verified movable prologues. Unsupported
instructions are rejected. No foveation shader, resolution, DLSS preset or FG
scheduling change is included. Cheeky's existing backend exclusion remains.

Additional safeguards in the adaptation:

- Trampoline preparation and hook activation are separate states. Failed enable
  retries enable, not creation, and never advertises that hook as ready.
- A successful Evaluate entry does not prevent the other entry from completing
  its bounded retries. Existing live hooks are not recreated or re-enabled.
- Replacement of a module with owned hooks requires restart; old trampoline
  pointers are not applied to its replacement.
- Failed thread enumeration, access, suspension or context inspection aborts
  patching. Suspended threads are resumed, and a thread inside the target's
  prologue yields Busy with no code change.
- Trampoline protection/cache-flush failures abort, and the original prologue
  is compared again while threads are suspended before writing the patch.

Hooks are process-lifetime, as upstream; hot removal is not supported.

Validation: Windows tests execute both a generated native target and its original
trampoline, reject RIP-relative/branch/return prologues, simulate a thread parked
inside the patch span, verify unchanged code and balanced suspension on Busy,
then retry successfully. Portable policy tests cover failed enable, failed/busy
fallback, capped attempts, changed-module retry budgets and partial Evaluate
success. Full Windows Release build passes. This is not a claim of measured FPS
improvement or validation against every DLSS binary. Real hookup status is logged
under `[foveated-native]`. Artifact: build/rc-0245-fixes-20261118 in root.
