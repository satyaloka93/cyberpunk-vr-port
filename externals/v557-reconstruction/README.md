# V557 reconstruction subset

LGPL-3.0: see ../../licenses/OFXR-LGPL-3.0.txt. Source attribution and original hashes are in provenance.json. Copied from the known V557 Cyberpunk backend checkout; no OpenXR layer, presentation scheduler, injector, or tray is included. Shaders build from source with Windows SDK fxc, using the port's pinned FidelityFX and NVIDIA OFA headers.

Port adaptations: history accepts already-captured COPY_SOURCE resources and a changing source pointer, with identical descriptions/device required. The adapter retains the color lease until its GPU receipt completes; it changes source only after the preceding submission completes. No redundant staging texture is used. Composition is the unmodified NVIDIA-only, unidirectional V557 shader with cost confidence and pose pre-warp. No game motion vectors are supplied to this backend. External OFXR live tuning uses an isolated mapping name.

The port-owned NVIDIA output path additionally supports COPY_SOURCE destinations with transitions inside composition and a deferred display-consumer fence. Allocation reporting excludes opaque driver memory. See ../../docs/fixes/v557-reconstruction.md for tests, contracts and tradeoffs.
