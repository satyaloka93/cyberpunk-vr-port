---
type: Architecture Constraint
title: VRCAM resolution catalogue
description: Why launcher resolutions must correspond to authored Cyberpunk player-entity camera assets.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/tools/gen_vrcam_assets.py
tags: [vrcam, resolution, launcher, wolvenkit, stereo]
timestamp: 2026-08-08T03:20:00Z
---

# Invariant

A launcher resolution is usable for real stereo only when all of these agree:

1. A `ResolutionPreset` entry in `src/vr/overlay/launcher_dialog.cpp`.
2. An authored `vrcam_<W>x<H>` player-entity component.
3. Its `vrcam_feed_<W>x<H>` virtual camera.
4. A matching `texture_from_camera_<W>x<H>.dtex` dynamic texture.
5. The component name in the shipped `vrcam.json` catalogue.
6. The imported assets packed into `mods/archive/cyberpunkvrport.archive`.

Listing a resolution in C++ or JSON does not create the engine asset. If the component is absent, the second view cannot render.

# Source of truth and generator

`tools/gen_vrcam_assets.py` parses every `ResolutionPreset` array directly from `launcher_dialog.cpp`, authors missing WolvenKit JSON sources, and refreshes the `vrcam.json` catalogue. The launcher is therefore the source of truth for a full asset-generation pass.

The WolvenKit source project is external to this repository. An exact new resolution is not complete until those JSON files are imported and a replacement archive is packed and committed.

# PSVR2 decision

For initial [PSVR2 support](../hardware/psvr2-steamvr.md), the launcher exposes only square resolutions already present in the shipped archive: `1920`, `2048`, `2560`, `3072`, `3584`, and `4096` per eye. This avoids dead dropdown entries while reducing the aspect mismatch from the Pimax choice.

An exact `3400x3468` option is deferred until the matching VRCAM component, dynamic texture, catalogue entry, and packed archive can ship together.

# Validation

After changing a ladder:

```bash
python - <<'PY'
from tools.gen_vrcam_assets import parse_launcher_resolutions, LAUNCHER
print(parse_launcher_resolutions(LAUNCHER))
PY
```

Then run the generator against the actual WolvenKit project, import changed JSON in WolvenKit, pack the archive, and test that the log reports the selected component and that VRCAM produces fresh frames.
