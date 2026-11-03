# DLAA right-eye output repair

DLAA renders at its output resolution. Dimension-only classification therefore
mistook ScalingOutputColor for input, copied it into a private right-eye texture
before evaluation, and never copied the evaluated output back to the SBS image.
The right eye displayed artifacts while ordinary DLSS upscaling worked.

The explicit ScalingOutputColor tag now wins this equal-size ambiguity. Other
input tags, 1x1 exposure, and existing upscaling handling are preserved. No quality,
jitter, motion-vector or frame-generation settings are changed.

Validation: test_dlaa_tag_routing.py compiles the production classification and
checks DLAA output/input/depth/MV/mask, exposure and upscaling. MSVC Release build
passed. User confirmed DLAA runs after deployment; its full-resolution GPU cost
remains expected. This does not resolve the separate headset menu cursor lag.
