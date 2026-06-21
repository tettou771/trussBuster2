#pragma once

#include <TrussC.h>
#include <tcxPhysics.h>
#include "Levels.h"

using namespace std;
using namespace tc;
using namespace tcx;

// Cancels the turntable's numerical outward drift, exactly.
//
// A dynamic body riding a kinematic platform that rotates by Δθ = ω·dt each step
// follows the tangent and overshoots the arc by the sagitta ε = ½·r·(ω·dt)² — a
// pure integration artifact that, left alone, walks every rider outward as
// r(t) = r₀·e^(k t), k = ω²/(2·hz). Since we know ω and the step dt exactly (we
// run on the world's fixed step), we can subtract that overshoot every step
// instead of tuning a gain: scale the rider's radius toward the spin axis by the
// same (ω·dt)²/2 it just gained. Parameter-free; the correction is ~µm per step.
//
// Driven by physicsUpdate so it runs in lock-step with the sim, with the same dt
// the platform turned by — that's what makes the cancellation exact.
class TurntableRiderMod : public Mod {
protected:
    void setup() override {
        physL_ = defaultWorld().physicsUpdate.listen([this](float dt) { correct(dt); });
    }

    void correct(float dt) {
        auto* rb = getOwner()->getMod<RigidBody>();
        if (!rb || !rb->body().isValid()) return;

        Vec3 p = rb->body().getPosition();              // world space
        if (p.y < PLATFORM_TOP - 0.1f) return;          // fallen below the deck — leave it
        float dx = p.x - PLATFORM_CX, dz = p.z - PLATFORM_CZ;
        float r2 = dx * dx + dz * dz;
        if (r2 < 1e-4f) return;                          // at the axis — nothing to pull
        if (r2 > PLATFORM_RADIUS * PLATFORM_RADIUS) return;  // past the rim — let it fly off

        // Exact per-step overshoot fraction: ε/r = ½·(ω·dt)². Independent of r, so
        // this is a uniform radial shrink toward the axis (keeps y / spin intact).
        float wdt = PLATFORM_ANG_VEL * dt;
        float shrink = 0.5f * wdt * wdt;
        p.x = PLATFORM_CX + dx * (1.0f - shrink);
        p.z = PLATFORM_CZ + dz * (1.0f - shrink);
        rb->body().setPosition(p);                      // velocity kept; sub-mm nudge
    }

    void onDestroy() override { physL_ = EventListener(); }

private:
    EventListener physL_;
};
