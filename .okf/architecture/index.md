# Architecture

* [VRCAM resolution catalogue](vrcam-resolution-catalog.md) — Launcher resolutions must correspond to authored and packed engine assets.
* [OpenXR controller to XInput pipeline](controller-input-pipeline.md) — Motion-controller actions are merged into Cyberpunk's native gamepad input.
* [Mounted vehicle interaction pipeline](vehicle-interaction-pipeline.md) — Vehicle classification, seated VRIK ownership, wheel grabs, steering, and gun-mode throttle controls.
* [Ray-tracing stereo resource isolation](ray-tracing-stereo-limitations.md) — Ray-traced reflections and lighting are not stereo-safe while their temporal resources remain shared.
* [DLSS 5 Neural Rendering in stereo VR](dlss5-neural-rendering.md) — Native-versus-addon upscaling, proven VRCAM ownership takeover, the return-address-preserving tail-jump correction, the proxy-resource diagnostic gate, measured cost, and unresolved save-load lifecycle.
* [Eye-tracked NR foveation](eye-tracked-foveation.md) — Gaze straight from the OpenXR runtime with no third-party bridge, the FOCUSED precondition that makes a working tracker read as dead, and the deadband that stops a creeping region shimmering DLSS-NR's temporal history.
* [Two eye-tracking paths — ours and Cheeky's bridge](eye-tracking-two-paths.md) — Same eye data, different delivery: the port asks OpenXR directly and needs nothing installed, while Cheeky reaches it through its own API layer with a registry key, an environment variable and a matched DLL pair.
* [Foveated super resolution via CheekyFoveatedDLSS](cheeky-foveated-sr.md) — Gaze-driven DLSS foveation running alongside the port's own NR foveation: why only an addon owning its own DLSS feature can force DLAA in the fovea, what the arrangement costs, and every file, key and setting it needs.
