#pragma once

#include <TrussC.h>
#include "ChipTunes.h"
#include "Levels.h"

using namespace std;
using namespace tc;

// The player cannon. Aiming state (yaw/pitch) + charge-to-fire power.
// yaw = 0 aims straight down -z; positive yaw aims left (-x).
// fire() emits `fired` with the muzzle position and launch velocity — the
// scene listens and spawns the actual Cannonball.
class Cannon : public Node {
public:
    struct FireArgs {
        Vec3 pos;
        Vec3 velocity;
    };
    Event<FireArgs> fired;

    static constexpr float MIN_SPEED = 9.0f;
    static constexpr float MAX_SPEED = 17.0f;
    static constexpr float MAX_ZONE  = 0.93f;   // release at or above = MAX shot

    // --- aiming -------------------------------------------------------------
    float getYaw() const   { return yaw_; }
    float getPitch() const { return pitch_; }
    void  setYaw(float v)   { yaw_ = clamp(v, -TAU * 0.17f, TAU * 0.17f); }
    void  setPitch(float v) { pitch_ = clamp(v, 0.0f, TAU * 0.18f); }

    float getYawDeg() const    { return rad2deg(yaw_); }
    float getPitchDeg() const  { return rad2deg(pitch_); }
    void  setYawDeg(float v)   { setYaw(deg2rad(v)); }
    void  setPitchDeg(float v) { setPitch(deg2rad(v)); }

    using Super = Node;
    TC_REFLECT(Cannon, Node) {
        TC_VALUE(yawDeg, getYawDeg, setYawDeg)
        TC_VALUE(pitchDeg, getPitchDeg, setPitchDeg)
    }

    // unit aim direction in world space
    Vec3 aimDir() const {
        return Vec3(-sinf(yaw_) * cosf(pitch_), sinf(pitch_), -cosf(yaw_) * cosf(pitch_));
    }

    Vec3 muzzlePos() const {
        return getGlobalPos() + Vec3(0, PIVOT_H, 0) + aimDir() * BARREL_LEN;
    }

    // --- charging (oscillating gauge: release at the top for a MAX shot) -----
    void beginCharge() {
        charging_ = true;
        power_ = 0.15f;
        chargeDir_ = 1.0f;
    }

    // releases at the current gauge position and fires
    void releaseCharge() {
        if (!charging_) return;
        charging_ = false;
        fire(power_);
    }

    void cancelCharge() { charging_ = false; power_ = 0.0f; }
    bool  isCharging() const { return charging_; }
    float getPower() const { return power_; }

    // --- firing -------------------------------------------------------------
    void fire(float power) {
        power = clamp(power, 0.0f, 1.0f);
        power_ = power;
        bool maxShot = power >= MAX_ZONE;
        float speed = MIN_SPEED + power * (MAX_SPEED - MIN_SPEED);
        FireArgs args{muzzlePos(), aimDir() * speed};
        recoil_ = 1.0f;
        flash_ = maxShot ? 1.6f : 1.0f;
        if (maxShot) jukebox().maxFire.play();   // sound-only celebration
        else         jukebox().fire.play();
        fired.notify(args);
    }

    void setup() override {
        setName("cannon");
        barrel_ = createCylinder(0.13f, BARREL_LEN, 18);
        body_   = createBox(0.95f, 0.42f, 0.95f);
        pivot_  = createSphere(0.27f, 16);
        baseMat_.setBaseColor(toLinearColor(Color(0.16f, 0.17f, 0.22f)));
        steelMat_.setBaseColor(toLinearColor(Color(0.35f, 0.38f, 0.45f)));
    }

    void update() override {
        float dt = (float)getDeltaTime();
        if (charging_) {
            // ping-pong between 0.15 and 1.0; tick at the top for ear-timing
            power_ += chargeDir_ * dt * 1.3f;
            if (power_ >= 1.0f) {
                power_ = 1.0f;
                chargeDir_ = -1.0f;
                jukebox().peakTick.play();
            } else if (power_ <= 0.15f) {
                power_ = 0.15f;
                chargeDir_ = 1.0f;
            }
        }
        recoil_ = std::max(0.0f, recoil_ - dt * 5.0f);
        flash_  = std::max(0.0f, flash_ - dt * 7.0f);
    }

    void draw() override {
        // base
        setMaterial(baseMat_);
        pushMatrix();
        translate(0, 0.21f, 0);
        body_.draw();
        popMatrix();

        // pivot + barrel (cylinder axis is +Y; rotate it onto the aim direction)
        setMaterial(steelMat_);
        pushMatrix();
        translate(0, PIVOT_H, 0);
        pivot_.draw();
        rotateY(yaw_);
        rotateX(pitch_ - TAU * 0.25f);
        translate(0, BARREL_LEN * 0.5f - recoil_ * 0.16f, 0);
        barrel_.draw();
        popMatrix();
        clearMaterial();

        // muzzle flash
        if (flash_ > 0.01f) {
            Vec3 m = aimDir() * (BARREL_LEN + 0.15f) + Vec3(0, PIVOT_H, 0);
            setColor(1.0f, 0.85f, 0.3f, flash_);
            drawCircle(m, 0.22f * flash_ + 0.08f);
            setColor(1.0f);
        }
    }

public:
    static constexpr float PIVOT_H    = 0.52f;
    static constexpr float BARREL_LEN = 1.15f;

private:

    float yaw_ = 0.0f;
    float pitch_ = deg2rad(20.0f);
    float power_ = 0.0f;
    bool  charging_ = false;
    float chargeDir_ = 1.0f;
    float recoil_ = 0.0f;
    float flash_ = 0.0f;

    Mesh barrel_, body_, pivot_;
    Material baseMat_, steelMat_;
};
