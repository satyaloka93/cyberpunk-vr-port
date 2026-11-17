#pragma once
#include <cstdint>
#include "Anim/BikeWheelieAssist.hpp"
namespace cvr::bikebank {
bool Ready();
bool MotionReady();
cvr::anim::BikeAttitude LastMotion();
struct Sample {
    uint64_t stampUs{};
    float nativeDegrees{},requestedDegrees{},targetDegrees{},actualDegrees{},maxDegrees{},steering{};
    // 0 applied, 1 invalid layout, 2 scripted tilt, 3 airborne, 4 input unavailable.
    unsigned reason=4;
};
Sample LastSample();
}
