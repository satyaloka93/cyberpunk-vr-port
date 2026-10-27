#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cvr::stereo::camera_layout {

// Cyberpunk 2.50 / file 3.0.88.46235 only. Paired native bodies and exact
// linker identities are recorded in the game250 camera ABI audit. This is
// independent of the RED4ext SDK layouts and does not enable the runtime gate.
inline constexpr uintptr_t ViewPrepareRva = 0x4240DC;
inline constexpr uintptr_t PlaneOverrideRva = 0x422D2C;
inline constexpr uintptr_t CascadeFitRva = 0x2A23AC;

// Graph-context prepare 0x423180 uses the same view base for key (+28),
// width/height (+34/+38) and its four-descriptor copy (+E0, stride3E0).
inline constexpr size_t ViewKey = 0x28;
inline constexpr size_t RenderSize = 0x34;
inline constexpr size_t InputFov = 0x2C70;
inline constexpr size_t FovScale = 0x25D4;
inline constexpr size_t FovAdd = 0x2B70;
inline constexpr size_t PlaneOverrides = 0x2470;
inline constexpr size_t PlaneOverrideStride = 0x10;
inline constexpr int PlaneCount = 4;
inline constexpr size_t DescriptorBase = 0xE0;
inline constexpr size_t DescriptorStride = 0x3E0;

// Descriptor internals retain these offsets in 2.50; the new tail does not.
inline constexpr size_t Fov = 0x20, Aspect = 0x28, Zoom = 0x2C;
inline constexpr size_t ShiftX = 0x30, ShiftY = 0x34;
inline constexpr size_t Projection00 = 0x190, Projection11 = 0x1A4;
inline constexpr size_t Projection20 = 0x1B0, Projection21 = 0x1B4;
inline constexpr size_t JitterX = 0x370, JitterY = 0x374;
// Native descriptor copy213403..415 and projection2A347D/2A3490.
inline constexpr size_t Width = 0x378, Height = 0x37C;
// +399 bit0 owns two child cameras; bit1 selects the per-eye projection.
// Child tag +398 is 1/2. Native stereo-create 0x206EC30 offsets their poses.
inline constexpr size_t DescriptorMode = 0x399;
inline constexpr size_t ChildCamera0 = 0x3D0, ChildCamera1 = 0x3D8;
using DescriptorBytes = std::array<uint8_t, DescriptorStride>;

struct NativeRenderDimensions {
    int32_t renderWidth{}, renderHeight{}, outputWidth{}, outputHeight{};
};

// RenderRes 0x425054 still writes the first 0x28 bytes with these dimensions,
// but now owns a larger rolling resize state and clamps each render dimension
// to at least 16. Consumers must use its result, not reconstruct it from scale.
inline bool ReadNativeRenderDimensions(NativeRenderDimensions& out, const void* state) {
    if (!state) return false;
    std::array<int32_t, 10> fields{};
    std::memcpy(fields.data(), state, sizeof(fields));
    if (fields[0] <= 0 || fields[1] <= 0 || fields[8] <= 0 || fields[9] <= 0 ||
        fields[0] > 65536 || fields[1] > 65536 || fields[8] > 65536 || fields[9] > 65536)
        return false;
    out = {fields[0], fields[1], fields[8], fields[9]};
    return true;
}

// New native modes can traverse nested descriptors at +3D0/+3D8, and native
// projection also branches on +399. Never pass a shallow copy containing
// those references to the cascade fit. Unsupported modes keep their original
// native fit; their flags and children are never edited or force-disabled.
// The caller owns fault handling for reading the game's descriptor memory.
inline bool CopyIndependentDescriptor(DescriptorBytes& out, const uint8_t* camera) {
    if (!camera) return false;
    std::memcpy(out.data(), camera, out.size());
    uint64_t child0{}, child1{};
    std::memcpy(&child0, out.data() + ChildCamera0, sizeof(child0));
    std::memcpy(&child1, out.data() + ChildCamera1, sizeof(child1));
    return out[DescriptorMode] == 0 && child0 == 0 && child1 == 0;
}

static_assert(ChildCamera1 + sizeof(uint64_t) == DescriptorStride);
static_assert(DescriptorBase + PlaneCount * DescriptorStride < PlaneOverrides);

} // namespace cvr::stereo::camera_layout
