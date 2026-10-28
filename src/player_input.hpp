#pragma once

#include "vec2.hpp"

namespace tennis {

// Platform-independent per-frame input. SDL, touch controls, gamepads and
// remote players should all produce this same command type.
struct PlayerInputCommand {
    Vec2 movement;
    bool sprintHeld = false;
};

// A completed swing can be recorded or sent over the network independently of
// raw SDL events. Most values are normalized after the selected mapping
// profile. lateralAimPixels deliberately stays in logical court pixels so the
// mouse's horizontal travel maps one-to-one to the requested landing offset.
struct StrokeInputCommand {
    double power = 0.0;
    double verticalSpin = 0.0;
    double sideSpin = 0.0;
    double flatDrive = 1.0;
    double direction = 0.0;
    double lateralAimPixels = 0.0;
    double height = 0.14;
    double smoothness = 1.0;
};

} // namespace tennis
