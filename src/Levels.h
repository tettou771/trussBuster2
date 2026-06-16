#pragma once

#include <TrussC.h>
#include <vector>

using namespace std;
using namespace tc;

// One block in the playfield. Positions are world space; the rotating platform's
// top surface is at y = PLATFORM_TOP.
struct BlockDef {
    Vec3  pos;
    Vec3  size;
    Color color;
    int   points = 100;
    float friction = 0.65f;   // surface grip
    bool  gold = false;       // gold: glints, busting it CHARGES the cannon
    bool  junk = false;       // junk: oversized gray nuisance, scores nothing
};

// standard look for static scenery / the platform (dark gray, reads as fixed)
inline Color wallColor() { return Color(0.565f, 0.585f, 0.645f); }

// meshPbr treats baseColor as LINEAR; our palette is authored in sRGB.
// Convert at the material boundary or every color washes out desaturated.
inline Color toLinearColor(const Color& c) {
    return Color(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b), c.a);
}

// global stage-edit flag: while on, physics is paused and blocks must not
// bust themselves (they're being dragged around with the gizmo)
inline bool& stageEditMode() { static bool b = false; return b; }

constexpr float PLATFORM_TOP = 1.0f;
constexpr float BLOCK_UNIT   = 0.55f;

// Circular rotating platform footprint (must match RotatingPlatform in GameScene).
// Diameter = 1.1 x the old 6.4-wide box; centered behind the cannon.
constexpr float PLATFORM_CX     = 0.0f;
constexpr float PLATFORM_CZ     = -6.0f;
constexpr float PLATFORM_RADIUS = 3.52f;   // 0.5 * 6.4 * 1.1
constexpr float PLATFORM_ANG_VEL = 0.14f;  // rad/s — slow spin (shared)

// horizontal distance from a world point to the platform's spin axis
inline float platformDist(const Vec3& p) {
    float dx = p.x - PLATFORM_CX, dz = p.z - PLATFORM_CZ;
    return sqrtf(dx * dx + dz * dz);
}

// y center of a block in stacking row `row` (0 = on the platform)
inline float rowY(int row, float blockH = BLOCK_UNIT) {
    return PLATFORM_TOP + blockH * 0.5f + row * (blockH + 0.006f);
}
