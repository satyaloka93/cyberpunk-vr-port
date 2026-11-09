# Optional V557 optical reconstruction

The saved November 9 bike capture shows a double upper windscreen edge with the
current NVIDIA OFA + FidelityFX reconstruction, while the old V557 NVIDIA-only
reconstruction produces a cleaner edge from the same endpoints. This is a
reconstruction comparison, not proof of a timing regression in the recent body
updates, nor proof that V557 improves every scene.

F10 > FRAMEGEN > Frame generator adds **NVIDIA OFA + V557 (experimental)**,
persisted as `xr_framegen_backend=3`. Existing values 0–2 and the default 1 are
unchanged. NVIDIA quality and flow resolution apply. Optical flow cannot be off
for this backend; selecting it with scale 0 normalizes to 50%. A non-NVIDIA GPU
falls back to FidelityFX, as with the existing NVIDIA choice.

The option uses V557's current-camera pre-warp, optical-flow cost confidence and
unidirectional midpoint composition. It does not use the engine vectors/depth
for interpolation. Input matching/capture is still retained by the port. The
shader is unchanged from the pinned reference. The historical edge-ghosting
switch remains compiled off; this is not a new edge-ghosting filter.

Only reconstruction is imported. The port's Presenter, real/synthetic cadence,
SteamVR timing package, private synthesis queue, idle preparation, endpoint
ordering and runtime submission remain in use. VDXR still uses the application
queue; enabling this backend does not activate SteamVR-only optimizations on it.
Generated images carry the current camera pose/FOV, unlike the existing FFX
interpolator's midpoint pose. Both direct and prepared submission paths honor
that distinction. Content time is still the temporal midpoint.

The adapter uses an attributed source subset in `externals/v557-reconstruction`,
built with Windows SDK fxc and the existing pinned FidelityFX/NVIDIA headers.
There is no additional runtime DLL, registry layer, external OFXR launcher,
tray configuration, or private XR instance. External OFXR's live tuning mapping
cannot control this component. The subset is LGPL-3.0; retain its source,
provenance and license when distributing builds. Original port files retain
their existing license.

Port adaptations to the component:

- Consume varying, already-captured COPY_SOURCE textures without another staging
  texture. Require matching resource descriptions/device, and a completed prior
  capture before replacing the source pointer.
- Keep native output textures in COPY_SOURCE between submissions. Transition
  within the synthesis command list, after the display consumer receipt.
- Defer the consumer GPU wait until composition so packing/OFA can overlap the
  previous display. CPU submission does not wait for those fences.
- Retain capture/guide leases through the final GPU receipt; refuse backend
  replacement/reset while the previous display owns output. Unknown GPU failure
  retains resources instead of freeing live allocations.
- Report explicit history/output/flow allocation sizes and the component's GPU
  pack+composition time separately from its total generation span. Driver-owned
  OFA allocation size is not exposed. Native capture-start correlation samples
  are not yet implemented for this adapter; do not interpret their absence as
  zero queue latency.

Tradeoffs to measure in the headset: V557 owns three history images and one
synthetic output destination per eye, plus OFA resources. It therefore has different
VRAM/copy costs. Its rotational camera correction is depth-unaware for
translation. Flow-only reconstruction can improve this windscreen while
performing worse on other effects. No live FPS, memory or overall quality
improvement is claimed from the offline image comparison.

Validation commands (Windows; dependencies from the normal build):

- Build `tools/framegen_replay`; run `framegen_replay.exe --v557-test` with the
  game closed. Uses the production adapter and D3D12 debug validation, checking
  shared/private queues, independent eyes, priming/reset, backend replacement,
  display ownership, deferred capture and input lifetime.
- Run the same tool with `<capture directory> <output directory>` for a saved
  three-frame capture. The adapter's images are compared in the current camera;
  FFX outputs are compared in its midpoint camera. FFX is warmed for eleven
  stationary frames. Uneven capture spacing and synthetic warmup prevent
  treating image-error measurements as an exact quality percentage.
- `tools/flight_recorder_tests`: `prepare_ahead` verifies both camera contracts
  in direct/prepared submission; `package_settings` verifies all backend values
  roundtrip and that V557 requires optical flow.

Local test artifacts and deployment receipts remain in
`integration-notes/bike-ghosting-20261109/` and the build's deployment files.
Headset confirmation is required before recommending this as the default.

## Validation result, November 9

Both 3144x3000 integrated-backend images match the previously archived V557
reference pixel-for-pixel (all RGB pixels, both eyes). The clearer windscreen
edge is preserved. GPU adapter checks pass with D3D12 validation on shared and
private queues; all 16 existing frame-generation regression tests pass.
Live bike performance and headset quality remain unverified.

## Remove redundant real outputs, November 9

The first headset run fixed ghosting but reported a performance loss. Its long
Slow segment had about84.2 accepted real+synthetic frames/s but79.4 compositor
estimate, against81.8 accepted /85.9 estimate in an earlier PSVR2 Slow segment.
These were natural gameplay runs, not identical workloads; submitted and
compositor-estimated rates are not unique displayed FPS. The immediately previous
run was VDXR and is not an appropriate same-runtime baseline.

The initial adapter also copied every real endpoint into a second destination
that native Presenter never reads. Reconstruction-only mode now omits that copy
and its destination allocation, restores the current history state in the
composition list, and signals completion there. Each eye retains one synthetic
output, used for a priming copy when history resets. The default reference API
still produces both outputs; the compact mode accepts only pure NVIDIA flow and
advancing capture tickets. No shader, optical-flow quality, or pacing changes.

Two per-eye3144x3000RGBA8 destinations represent approximately72MiB of image
storage; two copy payloads per pair are eliminated. GPU lifetime/state tests
pass on shared/private queues, and saved output pixels remain identical in
both eyes. Live FPS recovery remains pending; the removed copies are a proven
inefficiency, not a proven explanation for the entire regression.
