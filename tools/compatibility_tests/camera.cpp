#include "Stereo/EngineCameraLayout.hpp"
#include <cassert>
#include <cstdio>

namespace layout = cvr::stereo::camera_layout;

int main() {
    layout::DescriptorBytes source{}, copy{};
    for (size_t i = 0; i < source.size(); ++i) source[i] = uint8_t(i * 31 + 7);
    source[layout::DescriptorMode] = 0;
    const uint64_t zero = 0;
    std::memcpy(source.data() + layout::ChildCamera0, &zero, sizeof(zero));
    std::memcpy(source.data() + layout::ChildCamera1, &zero, sizeof(zero));
    const auto original = source;
    assert(layout::CopyIndependentDescriptor(copy, source.data()));
    assert(copy == source); // Full new tail is copied, not the former 0x3A0 bytes.
    copy[layout::Fov] ^= 0xff;
    assert(source == original); // Union projection edits cannot change the source.

    for (unsigned bit = 0; bit != 8; ++bit) {
        source = original;
        source[layout::DescriptorMode] = uint8_t(1u << bit);
        assert(!layout::CopyIndependentDescriptor(copy, source.data()));
        assert(copy == source); // Reject every unknown mode conservatively.
    }
    for (const auto offset : {layout::ChildCamera0, layout::ChildCamera1}) {
        source = original;
        const uint64_t child = 0x100000000ULL; // Check all eight bytes, not just low DWORD.
        std::memcpy(source.data() + offset, &child, sizeof(child));
        assert(!layout::CopyIndependentDescriptor(copy, source.data()));
    }
    const auto beforeNull = copy;
    assert(!layout::CopyIndependentDescriptor(copy, nullptr));
    assert(copy == beforeNull);
    // Exercise the publication contract with a native resize result different
    // from a naive output*scale calculation, plus DLAA and minimum-size cases.
    const auto checkDimensions = [](int32_t rw, int32_t rh, int32_t ow, int32_t oh) {
        const std::array<int32_t, 10> state{rw, rh, 21, 22, 23, 24, 25, 26, ow, oh};
        const auto before = state;
        layout::NativeRenderDimensions dims{};
        assert(layout::ReadNativeRenderDimensions(dims, state.data()));
        assert(dims.renderWidth == rw && dims.renderHeight == rh);
        assert(dims.outputWidth == ow && dims.outputHeight == oh);
        assert(state == before);
    };
    checkDimensions(1008, 1011, 3000, 3000);
    checkDimensions(3000, 3000, 3000, 3000);
    checkDimensions(16, 16, 24, 24);
    layout::NativeRenderDimensions dims{};
    const std::array<int32_t, 10> invalid{};
    assert(!layout::ReadNativeRenderDimensions(dims, invalid.data()));
    assert(!layout::ReadNativeRenderDimensions(dims, nullptr));
    std::puts("camera snapshot: plain full copy, isolation, 8 modes, 2 child slots, null passed");
    std::puts("native render sizes: exact engine result, DLAA, minimum size, invalid/null passed");
}
