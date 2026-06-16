#pragma once

#include <TrussC.h>
#include <tcxPhysics.h>
#include "Levels.h"
#include "ChipTunes.h"

using namespace std;
using namespace tc;
using namespace tcx;

// A fired cannonball: heavy dynamic sphere, plays impact sounds.
//
// Mine mechanic (the "stuck" escape hatch): a ball that comes to rest ON the
// platform is NOT removed — it stays as an armed mine. When the NEXT fired ball
// touches it (even a gentle graze), the mine detonates: it fires `exploded`
// (the scene scatters nearby blocks) and disappears. A ball that never settles
// (flew off, or is still in flight) expires on the usual age timer.
class Cannonball : public Node {
public:
    Cannonball(const Vec3& pos, const Vec3& velocity)
        : startPos_(pos), velocity_(velocity) {}

    Event<Vec3> exploded;   // detonation position (scene handles the blast)

    bool isMine() const { return mine_; }
    void detonateNow() { detonate(); }   // proximity trigger (scene-driven)

    void setup() override {
        setName("cannonball");
        setPos(startPos_);
        auto* rb = addMod<RigidBody>(ColliderShape::sphere(0.22f),
                                     BodyType::Dynamic, 9000.0f);
        // low restitution + grippy: a ball that reaches the deck should shed
        // its energy and come to rest (so it can arm as a mine) instead of
        // bouncing straight off the round edge
        rb->setFriction(0.9f).setRestitution(0.06f);
        rb->body().setLinearVelocity(velocity_);
        renderer_ = addMod<ColliderRenderer>();
        renderer_->setColor(toLinearColor(Color(0.22f, 0.23f, 0.28f)));
        hitL_ = rb->onCollisionBegan.listen(this, &Cannonball::onHit);
    }

    void update() override {
        if (stageEditMode()) return;
        float dt = (float)getDeltaTime();
        age_ += dt;

        auto* rb = getMod<RigidBody>();
        Vec3 gp = getGlobalPos();

        if (mine_) {
            // a pinned mine rides the deck: orbit it about the platform axis at
            // the deck's spin rate so it stays exactly where it settled (a
            // dynamic sphere would just roll off the round, spinning edge)
            float w = PLATFORM_ANG_VEL * dt;
            float c = cosf(w), s = sinf(w);
            float dx = gp.x - PLATFORM_CX, dz = gp.z - PLATFORM_CZ;
            setPos(PLATFORM_CX + dx * c - dz * s, gp.y,
                   PLATFORM_CZ + dx * s + dz * c);
            if (gp.y < -10.0f) destroy();
            return;
        }

        if (!mine_) {
            // stay comfortably inside the rim so a mine never pins half-off
            bool onPlatform = platformDist(gp) < PLATFORM_RADIUS - 0.15f &&
                              gp.y > PLATFORM_TOP - 0.1f && gp.y < PLATFORM_TOP + 1.2f;
            float speed = (rb && rb->body().isValid())
                              ? rb->body().getLinearVelocity().length() : 99.0f;
            // A sphere would otherwise roll off the round, spinning deck before
            // it can settle. Once it's down on the deck and already slow, bleed
            // its horizontal velocity so it digs in where it landed (keep the
            // vertical component so gravity still seats it). This makes the
            // mine — the jam-escape — actually reachable.
            if (onPlatform && speed < 2.0f && rb && rb->body().isValid()) {
                Vec3 lv = rb->body().getLinearVelocity();
                rb->body().setLinearVelocity(Vec3(lv.x * 0.82f, lv.y, lv.z * 0.82f));
                rb->body().setAngularVelocity(rb->body().getAngularVelocity() * 0.82f);
            }
            // speed threshold sits above the deck's carry speed (omega*r)
            if (onPlatform && speed < 0.9f) {
                restTime_ += dt;
                if (restTime_ > 0.4f) arm();
            } else {
                restTime_ = 0.0f;
            }
            // a ball that wandered off / never settled expires
            if (age_ > 7.0f) destroy();
        }
        // dropped out of the world (also catches mines knocked off the edge)
        if (gp.y < -10.0f) destroy();
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
        mine_ = true;
        // pin it: switch to kinematic so it no longer rolls — update() then
        // orbits it with the deck. tint amber so the player reads "armed".
        if (auto* rb = getMod<RigidBody>())
            if (rb->body().isValid()) {
                rb->body().setLinearVelocity(Vec3(0, 0, 0));
                rb->body().setAngularVelocity(Vec3(0, 0, 0));
                rb->setBodyType(BodyType::Kinematic);
            }
        if (renderer_) renderer_->setColor(toLinearColor(Color(0.9f, 0.42f, 0.12f)));
    }

    void detonate() {
        if (detonated_) return;
        detonated_ = true;
        Vec3 at = getGlobalPos();
        exploded.notify(at);
        destroy();
    }

    Vec3              startPos_, velocity_;
    float             age_ = 0.0f;
    float             restTime_ = 0.0f;
    bool              mine_ = false;
    bool              detonated_ = false;
    ColliderRenderer* renderer_ = nullptr;
    EventListener     hitL_;
};
