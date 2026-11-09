# Rebuild / source for 0.2.3-psvr2.1-rc1

The runtime is the tested Release build at `631a180b65da8c026ce3cf7afe03dfbd48adf484`; later release-tag commits change packaging/documentation only. DLL SHA-256: `bfc0340e5bed7130adf7be15f5023b435e6d00697b16edf09e49849046f4ad4b`.

The accompanying Source ZIP contains this fork's source, including the modified LGPL V557 reconstruction, build scripts and licenses. Git submodules are pinned below; initialize them from the tagged git checkout. The CMake scripts fetch the remaining pinned dependencies. You may modify/rebuild the component and relink/replace the complete port DLL; no signing or installation lock prevents this. Original port files retain their existing license. See `licenses/OFXR-LGPL-3.0.txt`, `licenses/OFXR-GPL-3.0.txt`, and `externals/v557-reconstruction/provenance.json`.

Use Windows, Visual Studio 2022 C++ x64 tools, Windows SDK 10.0.26100.0 (fxc/dxc), CMake, Git, and Rust/cargo if building the redscript compiler. In a VS developer shell:

```powershell
git clone --branch 0.2.3-psvr2.1-rc1 https://github.com/satyaloka93/cyberpunk-vr-port.git
cd cyberpunk-vr-port
git submodule update --init --recursive
cmake -S . -B build-release -G "Visual Studio 17 2022" -A x64
cmake --build build-release --config Release --target cyberpunkvrport_stereo --parallel 4
powershell -ExecutionPolicy Bypass -File scripts/build_dist.ps1 -BuildDir build-release -Version 0.2.3-psvr2.1-rc1 -Zip
```

Pins used by the tested runtime:

- RED4ext.SDK: `5246e659d783e5834cc08a90e56c8932c9ed3449` (marklove5102/RED4ext.SDK). Do not substitute an older SDK with different 2.5 object layouts.
- MinHook: `d94c64d32ea37bc4f5ee47d580709f70c6fb6080`.
- FBT IK: `39f65bef1f46b0ea945dbd46aafbe4c7aaf89b8d`.
- FidelityFX SDK: `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`; backend_dx12, frameinterpolation, opticalflow static Release libraries. `CVR_FRAMEGEN_PREBUILT=ON` was used locally to reuse those pinned libraries; a fresh build defaults to building them.
- NVIDIA DLSS SDK: `374959484e79a640feaba44c93ac8cfb0a03f5b5`, pinned in cmake/NativeFramegen.cmake. The game's own runtime DLSS DLLs are not distributed.
- redscript: `v1.0.0-preview.22`; the local tested package reuses the official 0.2.2 scc_lib.dll through `CVR_SCC_LIB_PREBUILT`. A fresh build defaults to cargo.
- Runtime shader compiler: the official 0.2.3 archive supplies dxcompiler.dll SHA-256 `97abd3b3083607dc29cc3f8eae56c39c349b76ee3c20deeb2f66258708e55bd8` and dxil.dll `fda4dde92222ddb8564c3173ce48faa56f7e1c7b46b0b67dada1182ee07a4123`, identical to the tested installation. Pass `-DxcDirectory <extracted dxc directory>` when packaging to preserve this exact pair; otherwise the packaging script uses the installed Windows SDK.
- Other CMake pins include OpenXR release-1.0.34, ImGui v1.91.1-docking, pugixml v1.14, yaml-cpp 0.8.0.

The release was packaged from the already-tested build, not rebuilt during publication. The fresh-checkout commands document the normal build route; byte-for-byte reproducibility is not claimed. To verify or replace only the LGPL component, its CMake target and reconstruction tests are documented in `docs/fixes/v557-reconstruction.md`; rebuilding the main target relinks its static library into the port. Preserve all third-party attribution when distributing a modified build.
