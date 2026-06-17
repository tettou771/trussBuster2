#pragma once

#include <TrussC.h>
#include <tcxPhysics.h>
#include "Levels.h"
#include "ChipTunes.h"
#include "Dodeca.h"

using namespace std;
using namespace tc;
using namespace tcx;

// Shared geometry. The COLLIDER is a 12-sided convex hull (constant), so a
// cannonball is physically a dodecahedron the whole time — it just renders as a
// smooth ball in flight and as the faceted die once it arms (no mid-flight body
// swap, which would crash on a stale Jolt contact).
inline const Mesh& ballSphereMesh() { static Mesh m = createSphere(0.22f, 18); return m; }
inline const Mesh& mineDodecaMesh() { static Mesh m = createDodecahedron(0.26f); return m; }

// A fired cannonball. Comes to rest on the deck -> arms as a mine (the jam
// escape): rotation is frozen so it never rolls, and it pulses (size + glow)
// like it's about to blow. The next ball to graze it detonates it, scattering
// nearby blocks. Anything that drops off the deck is removed promptly.
class Cannonball : public Node {
public:
    Cannonball(const Vec3& pos, const Vec3& velocity)
        : startPos_(pos), velocity_(velocity) {}

    Event<Vec3> exploded;   // detonation position (scene handles the blast)

    bool isMine() const { return mine_; }
    void detonateNow() { detonate(); }   // proximity trigger (scene-driven)
    void forceArm() { arm(); }           // debug: spawn straight as a mine

    void setup() override {
        setName("cannonball");
        setPos(startPos_);
        // CUBE collider (constant). A round (sphere/12-gon) collider keeps
        // rolling on the spinning deck and spirals off the edge before it can
        // settle — like a marble on a turntable. A cube is carried and orbits
        // stably like the blocks, so the ball actually comes to rest and arms.
        // (The render is still a smooth ball in flight / a pulsing 12-sided die
        // once armed — only the collision shape is a cube.)
        auto* rb = addMod<RigidBody>(ColliderShape::box(Vec3(0.4f, 0.4f, 0.4f)),
                                     BodyType::Dynamic, 7000.0f);
        rb->setFriction(0.9f).setRestitution(0.06f);
        rb->body().setLinearVelocity(velocity_);
        hitL_ = rb->onCollisionBegan.listen(this, &Cannonball::onHit);
    }

    void update() override {
        if (stageEditMode()) return;
        float dt = (float)getDeltaTime();
        age_ += dt;

        auto* rb = getMod<RigidBody>();
        Vec3 gp = getGlobalPos();

        // anything that has dropped off the deck disappears promptly
        if (gp.y < 0.5f) { destroy(); return; }

        if (mine_) return;   // pinned in arm() via DOF locks; nothing to do

        // detect coming to rest on the platform (incl. ON TOP of a tall junk
        // block / a stack — generous height cap; the pitch is capped at ~65° so
        // a slow mid-air apex can't false-arm)
        bool onPlatform = platformDist(gp) < PLATFORM_RADIUS - 0.15f &&
                          gp.y > PLATFORM_TOP - 0.1f && gp.y < PLATFORM_TOP + 5.0f;
        float speed = (rb && rb->body().isValid())
                          ? rb->body().getLinearVelocity().length() : 99.0f;
        // light rolling resistance once it's slow and on-deck, so it eventually
        // settles instead of rolling off the round, spinning rim — but gentle
        // (≈half the old damping) so it keeps rolling smoothly for a while
        if (onPlatform && speed < 2.0f && rb && rb->body().isValid()) {
            Vec3 lv = rb->body().getLinearVelocity();
            rb->body().setLinearVelocity(Vec3(lv.x * 0.91f, lv.y, lv.z * 0.91f));
            rb->body().setAngularVelocity(rb->body().getAngularVelocity() * 0.91f);
        }
        if (onPlatform && speed < 0.9f) {
            restTime_ += dt;
            if (restTime_ > 0.4f) arm();
        } else {
            restTime_ = 0.0f;
        }
        if (age_ > 6.0f) destroy();          // never settled -> expire
    }

    void draw() override {
        static Material flyMat;
        static Material mineMat;
        static bool init = false;
        if (!init) {
            flyMat.setBaseColor(toLinearColor(Color(0.22f, 0.23f, 0.28f)));
            mineMat.setBaseColor(toLinearColor(Color(0.95f, 0.40f, 0.10f)));
            init = true;
        }
        if (!mine_) {
            setMaterial(flyMat);
            ballSphereMesh().draw();
            clearMaterial();
            return;
        }
        // armed: pulse the LOOK only (collider stays constant). ~1 Hz, grows
        // ~5% and glows brighter — reads as "about to blow".
        float t = getElapsedTimef();
        float pulse = 0.5f * (1.0f + sinf(TAU * t));     // 0..1, 1 Hz
        float s = 1.0f + 0.15f * pulse;                  // +15% at the peak
        mineMat.setEmissive(toLinearColor(Color(1.0f, 0.45f, 0.12f)));
        mineMat.setEmissiveStrength(0.12f + 0.6f * pulse);
        setMaterial(mineMat);
        pushMatrix();
        scale(s, s, s);
        mineDodecaMesh().draw();
        popMatrix();
        clearMaterial();
    }

private:
    void onHit(Collision& c) {
        jukebox().playImpact(c.speed);
        // armed mine grazed by a freshly fired (non-mine) ball -> detonate, and
        // the triggering ball is consumed by the blast
        if (mine_ && c.otherNode && c.otherNode->getName() == "cannonball") {
            auto* ob = dynamic_cast<Cannonball*>(c.otherNode);
            if (ob && !ob->isMine()) { ob->destroy(); detonate(); }
        }
    }

    void arm() {
        mine_ = true;
        // No DOF locking: the cube collider is already stable on the deck (it
        // doesn't roll, just like the blocks) and is carried/spun by the deck
        // naturally. Locking rotation actually made it worse — a lock-frozen
        // body can't rotate to relieve the spinning deck's contact forces, so
        // they built up and popped it off the deck.
    }

    void detonate() {
        if (detonated_) return;
        detonated_ = true;
        Vec3 at = getGlobalPos();
        exploded.notify(at);
        destroy();
    }

    Vec3          startPos_, velocity_;
    float         age_ = 0.0f;
    float         restTime_ = 0.0f;
    bool          mine_ = false;
    bool          detonated_ = false;
    EventListener hitL_;
};
