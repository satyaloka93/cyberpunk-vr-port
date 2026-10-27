# Cyberpunk 2.50 native stereo: findings and integration boundary

Investigation: 2026-10-26. This document describes static evidence from the
shipped PC executable, not a working native-stereo VR implementation or a
performance measurement.

## What is established

CDPR's [official 2.5 patch notes](https://www.cyberpunk.net/en/news/52120/check-out-whats-coming-to-night-city-in-the-edgerunners-2-update-for-cyberpunk-2077)
announce glasses-free 3D support for compatible Samsung Odyssey 3D monitors,
including a Motion Comfort setting. They do not announce OpenXR, headset
support, or single-pass rendering.

The binary contains genuine two-camera geometry preparation:

* The camera descriptor grew from `0x3A0` to `0x3E0` bytes. The added fields
  include a mode byte at `+0x399`, an eye identifier at `+0x398`, and child
  camera pointers at `+0x3D0/+0x3D8`.
* Function RVA `0x206EC30` creates two child cameras. It derives a separation
  vector from an input scalar and the parent's camera basis. It subtracts
  that vector from the parent's position for one child and adds it for the
  other, then updates each child's pose/matrices through `0x26A7B28`.
* Function `0x206EC10` labels the children 1 and 2, clears the parent-mode
  flag and sets the child-eye projection flag. This is stronger evidence
  than finding strings containing "stereo": there are two distinct native
  camera poses. It does not establish that every rendering pass consumes them.
* Official RED4ext SDK generated types add `stereo` at byte `+0x0A` of
  `ent::RenderToTextureFeatures`. The old 2.31 type was eight bytes; the new
  type is eleven, also adding motion-blur and depth-of-field selections.
* The EXE names `CRenderNode_MapStereo3D` and `MapStereo3D`, has settings
  `Odyssey3D_StereoEnabled`, `Odyssey3D_MotionComfort`, `Odyssey3D_Strength`
  and `Odyssey3D_WeaponStrength`, and loads the Odyssey SDK through the
  installed Samsung Hub path. String references and actual LEA instruction
  boundaries were retained separately from semantic conclusions.

The presence of real child cameras supports a native geometric-stereo route.
Output resources, the complete draw path, temporal histories and sharing of
rendering work still need validation. Do not label this a proven single-pass
or faster implementation.

## How the port could use it

The useful integration point would be the native eye views and their images
before monitor-specific output mapping. The current port already provides
headset tracking, controls, haptics and OpenXR submission; those remain needed.

1. Trace the native stereo enable path into view construction, resource
   allocation, scene rendering and `MapStereo3D`. Establish the exact eye
   resources, layouts and ownership, and whether camera generation can run
   independently of Samsung display discovery.
2. Feed headset eye poses, measured eye separation and each eye's asymmetric
   projection into the native cameras. A monitor's depth-strength or comfort
   setting is not automatically a correct headset IPD/projection.
3. Capture both completed eye images before display mapping and submit them
   through OpenXR, with matching render poses and proper resource/fence lifetime.
4. Verify occlusion, first-person geometry, shadows, reflections, transparencies,
   motion vectors, depth and DLSS histories separately for both eyes. Two
   camera positions alone do not certify correct stereo output.
5. Compare GPU/CPU time at identical eye resolution and graphics settings with
   frame generation off. Only measured sharing of culling, shadows or draw
   preparation would support a performance claim. Two full rendering passes
   could still cost approximately as much as today's two-view route.

If the native renderer supplies coherent per-eye resources and histories, it
may replace some MAIN/VRCAM duplication and repair hooks. That is an integration
hypothesis, not a reason to remove the existing path before the replacement is
verified.

## Effect on the current compatibility changes

The asymmetric-projection migration uses the new descriptor size and fields.
Its temporary cascade-union copy is allowed only for ordinary descriptors with
no mode flags and no child pointers. Native stereo/unknown modes retain the
engine's own cascade fit; a shallow copy must not recurse into live child
cameras and alter their projections.

The initial investigation left the startup guard closed. Subsequent compatibility
work enabled the verified2.50 profile, while retaining MAIN/VRCAM rendering.
Native stereo has not been enabled in the installed game.

## Reproducible local evidence

Parent repository research root: `build/diagnostics/game250/reference-231/`.

* `abi-audit/REPORT.md`, `stereo-create-children-250.asm`, the child-mode and
  pose-setter dumps: native camera field/access and pose-generation evidence.
* `implementation-evidence/stereo-string-references.json` and associated
  `stereo250-*.asm`: native configuration, SDK discovery and render-graph references.
* `inspect_250_migration.py`: reproduces bounded string references and selected
  native instruction windows. Entry unwind ranges are not assumed to cover
  complete semantic functions.
* `verification.json` and `mapping-summary.json`: exact old/new artifact hashes.

2.50 EXE SHA-256:
`d36e74c47592e9c656ae250078cb40b2be63c782af5bfbeff2280d43e653fab4`.
All addresses above are module-relative RVAs for that artifact.

## October26 follow-up: fog and native shader camera contract

Bounded direct-call and instruction analysis is retained under
`build/diagnostics/game250/native-stereo-fog/`. The PE hash matches the artifact
above. REA/Ghidra full-program startup timed out at330 seconds; the new native
findings below come from PE/unwind-bounded Capstone disassembly, not decompilation.

- Camera constructors at RVAs2135EC and42579C copy input+4C bit2 to
  descriptor+399 bit0, then call206EC30 to create the children when set.
- ViewPrepare4240DC calls the display-side getter2284AA0 with eye IDs1/2
  and updates existing children through206F6D0. This is a candidate adaptation
  boundary, not a verified headset-independent renderer.
- Camera shader uploader228C9A0 reads descriptor+398 at228D549 and stores
  its eye tag at offset350 of the360-byte upload (stackbase50, write3A0).
  At228D031 descriptor+399 bit1 also changes the jitter calculation. Preserving
  only old camera matrices is insufficient for a native stereo integration.
- New fog shaders have an ordinary-camera ray path and a native-stereo path
  selected by b0row32.x, with the stereo branch using b6 matrices. The ordinary
  path still multiplies rawNDC by prepare-scene ray scales without the off-centre
  camera shift. It therefore still needs the port's asymmetric-ray correction.

The current shader loader had only four old fog hashes, all absent from the
extracted2.50 compute cache. A targeted migration recipe now covers seven exact
2.50 hashes and26 ray components, leaving the native-stereo matrix branch alone.
Shader assembly/validation passes; runtime binding and visual acceptance remain
required. This repairs the existing path; it does not activate native stereo.

Next native-stereo milestones: trace b6 production and eye-resource allocation;
verify children consume headset poses/asymmetric projections without Samsung
display discovery; capture both completed resources before MapStereo3D with their
matching fences; then compare fog/depth/history/motion at identical resolution.
No claim of single-pass rendering or performance gain is established.
