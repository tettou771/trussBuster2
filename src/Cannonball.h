#pragma once

#include <TrussC.h>
#include <tcxPhysics.h>
#include "Levels.h"
#include "ChipTunes.h"
#include "Dodeca.h"

using namespace std;
using namespace tc;
using namespace tcx;

inline const Mesh& ballSphereMesh() { static Mesh m = createSphere(0.22f, 18); return m; }
inline const Mesh& mineDodecaMesh() { static Mesh m = createDodecahedron(0.26f); return m; }

// A cannonball has two lives, as two SEPARATE bodies (we never swap a collider
// on a live body — that crashes on a stale Jolt contact):
//
//   flying (armed=false): a SPHERE collider that matches the rendered ball.
//     When it comes to rest on the deck it fires `settled` and removes itself;
//     the scene then spawns a bomb in its place.
//   bomb  (armed=true): a CUBE collider (stable — sits and is carried by the
//     deck like the blocks, instead of rolling off as a round shape does),
//     rendered as a pulsing 12-sided die. The next ball to graze it detonates
//     it (scene handles the blast + consuming the trigger).
class Cannonball : public Node {
public:
    Cannonball(const Vec3& pos, const Vec3& velocity, bool armed = false)
        : startPos_(pos), velocity_(velocity), mine_(armed) {}

    Event<Vec3> exploded;   // bomb detonated here (scene scatters nearby blocks)
    Event<Vec3> settled;    // flying ball came to rest here (scene spawns a bomb)

    bool isMine() const { return mine_; }
    void detonateNow() { detonate(); }   // proximity trigger (scene-driven)

    void setup() override {
        setName("cannonball");
        setPos(startPos_);
        ColliderShape shape = mine_ ? ColliderShape::box(Vec3(0.4f, 0.4f, 0.4f))
                                    : ColliderShape::sphere(0.22f);
        auto* rb = addMod<RigidBody>(shape, BodyType::Dynamic, mine_ ? 7000.0f : 9000.0f);
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

        if (mine_) return;   // a bomb just sits there (cube, carried by the deck)

        // flying ball: detect coming to rest on the deck (incl. on top of a tall
        // junk block / stack — generous height cap; the pitch is capped at 45°,
        // so a slow mid-air apex can't false-arm)
        bool onPlatform = platformDist(gp) < PLATFORM_RADIUS - 0.15f &&
                          gp.y > PLATFORM_TOP - 0.1f && gp.y < PLATFORM_TOP + 5.0f;
        float speed = (rb && rb->body().isValid())
                          ? rb->body().getLinearVelocity().length() : 99.0f;
        // light rolling resistance once it's slow and on-deck, so the rolling
        // sphere settles instead of rolling off the round, spinning rim
        if (onPlatform && speed < 2.0f && rb && rb->body().isValid()) {
            Vec3 lv = rb->body().getLinearVelocity();
            rb->body().setLinearVelocity(Vec3(lv.x * 0.91f, lv.y, lv.z * 0.91f));
            rb->body().setAngularVelocity(rb->body().getAngularVelocity() * 0.91f);
        }
        if (onPlatform && speed < 0.9f) {
            restTime_ += dt;
            if (restTime_ > 0.4f) {
                // hand off to a bomb at this spot, then remove the flying ball
                Vec3 at = gp;
                settled.notify(at);
                destroy();
                return;
            }
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
        // ~15% and glows brighter — reads as "about to blow".
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
