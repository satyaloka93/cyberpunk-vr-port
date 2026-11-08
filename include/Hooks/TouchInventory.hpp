#pragma once
#include <cstdint>
namespace cvr::input {
// RC5: left thumbrest + R3 is Back/Select, not crouch. Raw clicks still reach
// the overlay first; L3+R3 remains its own chord even while resting the thumb.
inline uint16_t TouchRightStickButton(bool leftThumbrest, bool rightClick, bool leftClick) {
    if (!rightClick) return 0;
    return leftThumbrest && !leftClick ? 0x0020 : 0x0080;
}
}
