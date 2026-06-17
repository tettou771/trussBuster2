#pragma once

#include <TrussC.h>
#include <tcxPhysics.h>
#include "Levels.h"
#include "ChipTunes.h"
#include "Dodeca.h"

using namespace std;
using namespace tc;
using namespace tcx;

// A fired cannonball: a heavy dynamic sphere that plays impact sounds.
//
// Mine mechanic (the "stuck" escape hatch): a ball that comes to rest ON the
// platform is NOT removed — it arms as a mine. Arming swaps the rolling sphere
// collider for a BOX (so it obeys normal physics but resists rolling) and the
// render switches to a 12-sided die. When the NEXT fired ball touches/grazes
// it, the mine detonates: it fires `exploded` (the scene scatters nearby
// blocks) and disappears. A ball that never settles expires on the age timer.
class Cannonball : public Node {
public:
    Cannonball(const Vec3& pos, const Vec3& velocity)
        : startPos_(pos), velocity_(velocity) {}

    Event<Vec3> exploded;   // detonation position (scene handles the blast)

    bool isMine() const { return mine_; }
    void detonateNow() { detonate(); }   // proximity trigger (scene-driven)
    void forceArm() { mine_ = true; }    // debug: spawn straight as a mine

    void setup() override {
        setName("cannonball");
        setPos(startPos_);
        auto* rb = addMod<RigidBody>(ColliderShape::sphere(0.22f),
                                     BodyType::Dynamic, 9000.0f);
        // low restitution + grippy: a ball that reaches the deck should shed
        // its energy and settle (so it can arm) instead of bouncing away
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

        if (mine_) {
            // a mine obeys normal physics but resists rolling: heavily damp its
            // spin and horizontal drift each frame (keep vertical so gravity
            // still acts). It can still be knocked off the deck and fall — that's
            // fine, no artificial pinning. Render is a 12-sided die.
            if (rb && rb->body().isValid()) {
                Vec3 lv = rb->body().getLinearVelocity();
                rb->body().setLinearVelocity(Vec3(lv.x * 0.6f, lv.y, lv.z * 0.6f));
                rb->body().setAngularVelocity(rb->body().getAngularVelocity() * 0.35f);
            }
            if (gp.y < -10.0f) destroy();
            return;
        }

        // detect coming to rest on the platform -> arm as a mine
        bool onPlatform = platformDist(gp) < PLATFORM_RADIUS - 0.15f &&
                          gp.y > PLATFORM_TOP - 0.1f && gp.y < PLATFORM_TOP + 1.2f;
        float speed = (rb && rb->body().isValid())
                          ? rb->body().getLinearVelocity().length() : 99.0f;
        // A sphere would otherwise roll off the round, spinning deck before it
        // can settle. Once it's down on the deck and already slow, bleed its
        // horizontal velocity so it digs in where it landed (keep the vertical
        // component so gravity still seats it).
        if (onPlatform && speed < 2.0f && rb && rb->body().isValid()) {
            Vec3 lv = rb->body().getLinearVelocity();
            rb->body().setLinearVelocity(Vec3(lv.x * 0.82f, lv.y, lv.z * 0.82f));
            rb->body().setAngularVelocity(rb->body().getAngularVelocity() * 0.82f);
        }
        if (onPlatform && speed < 0.9f) {
            restTime_ += dt;
            if (restTime_ > 0.4f) arm();
        } else {
            restTime_ = 0.0f;
        }
        if (age_ > 7.0f) destroy();          // never settled -> expire
        if (gp.y < -10.0f) destroy();         // dropped out of the world
    }

    void draw() override {
        static Mesh sphereMesh = createSphere(0.22f, 18);
        static Mesh dodecaMesh = createDodecahedron(0.30f);
        static Material flyMat, mineMat;
        static bool matInit = false;
        if (!matInit) {
            flyMat.setBaseColor(toLinearColor(Color(0.22f, 0.23f, 0.28f)));
            mineMat.setBaseColor(toLinearColor(Color(0.95f, 0.42f, 0.12f)));
            matInit = true;
        }
        if (mine_) { setMaterial(mineMat); dodecaMesh.draw(); clearMaterial(); }
        else       { setMaterial(flyMat);  sphereMesh.draw();  clearMaterial(); }
    }

private:
    void onHit(Collision& c) {
        jukebox().playImpact(c.speed);
        // armed mine grazed by a freshly fired (non-mine) ball -> detonate
        if (mine_ && c.otherNode && c.otherNode->getName() == "cannonball") {
            auto* ob = dynamic_cast<Cannonball*>(c.otherNode);
            if (ob && !ob->isMine()) detonate();
        }
    }

    void arm() {
        // Just flag it — the body is NOT recreated (recreating a RigidBody mid
        // physics-step leaves Jolt holding a stale contact, which crashes the
        // next dispatch). update() damps the sphere so it resists rolling, and
        // draw() renders the 12-sided die.
        mine_ = true;
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
