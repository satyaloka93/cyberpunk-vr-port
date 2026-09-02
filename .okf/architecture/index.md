# Architecture

* [VRCAM resolution catalogue](vrcam-resolution-catalog.md) — Launcher resolutions must correspond to authored and packed engine assets.
* [OpenXR controller to XInput pipeline](controller-input-pipeline.md) — Motion-controller actions are merged into Cyberpunk's native gamepad input.
* [Mounted vehicle interaction pipeline](vehicle-interaction-pipeline.md) — Vehicle classification, seated VRIK ownership, wheel grabs, steering, and gun-mode throttle controls.
* [Ray-tracing stereo resource isolation](ray-tracing-stereo-limitations.md) — Ray-traced reflections and lighting are not stereo-safe while their temporal resources remain shared.
* [DLSS 5 Neural Rendering in stereo VR](dlss5-neural-rendering.md) — Native-versus-addon upscaling, proven VRCAM ownership takeover, the return-address-preserving tail-jump correction, the proxy-resource diagnostic gate, measured cost, and unresolved save-load lifecycle.
