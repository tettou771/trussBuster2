#pragma once

#include <TrussC.h>
#include <tcxPhysics.h>
#include "ChipTunes.h"
#include "GoldGlint.h"
#include "Levels.h"
#include "Block.h"
#include "Cannonball.h"
#include "Cannon.h"
#include "ExplosionFx.h"

using namespace std;
using namespace tc;
using namespace tcx;

// Endless play: no stages. Knock blocks off the slowly-spinning platform; clear
// every scoring block and a fresh wave drops in. Gold blocks recharge the
// cannon; oversized gray "junk" blocks just clog the deck and grow more common
// the longer you survive. Out of shots with nothing in flight = game over.
enum class Phase { Title, Playing, GameOver };

inline const char* phaseName(Phase p) {
    switch (p) {
        case Phase::Title:    return "Title";
        case Phase::Playing:  return "Playing";
        case Phase::GameOver: return "GameOver";
    }
    return "?";
}

// A static physics box with a renderer (the cannon pedestal).
class StaticProp : public Node {
public:
    StaticProp(const Vec3& pos, const Vec3& size, const Color& color)
        : pos_(pos), size_(size), color_(color) {}

    void setup() override {
        setPos(pos_);
        addMod<RigidBody>(ColliderShape::box(size_), BodyType::Kinematic);
        addMod<ColliderRenderer>()->setColor(toLinearColor(color_));
    }

private:
    Vec3 pos_, size_;
    Color color_;
};

// The playfield: a kinematic cylinder, same height as the old box platform but
// 10% wider in diameter, spinning slowly about its vertical axis. Riders are
// carried by friction (MoveKinematic derives angular velocity from the per-frame
// node rotation), so the whole tower of blocks orbits the cannon.
class RotatingPlatform : public Node {
public:
    void setup() override {
        setName("platform");
        setPos(PLATFORM_CX, 0.5f, PLATFORM_CZ);
        addMod<RigidBody>(ColliderShape::cylinder(PLATFORM_RADIUS, 1.0f),
                          BodyType::Kinematic)->setFriction(1.6f);
        addMod<ColliderRenderer>()->setColor(toLinearColor(wallColor()));
    }

    void update() override {
        if (stageEditMode()) return;
        angle_ += PLATFORM_ANG_VEL * (float)getDeltaTime();
        setEuler(Vec3(0, angle_, 0));   // kinematic body follows -> carries riders
    }

private:
    float angle_ = 0.0f;
};

// The 3D world: camera scope, physics, cannon, blocks, endless game loop.
class GameScene : public Node {
public:
    // --- state accessors (HUD / MCP) ----------------------------------------
    Phase  getPhase() const       { return phase_; }
    int    getScore() const       { return score_; }
    int    getHiScore() const     { return hiScore_; }
    int    getShots() const       { return shots_; }
    int    getWave() const        { return waveCount_; }
    int    getScoringLeft() const { return scoringLeft_; }
    bool   isAutopilot() const    { return autopilot_; }
    Cannon* cannon()              { return cannon_.get(); }

    // --- stage-editing debug API (inspector / MCP) ---------------------------
    bool isEditMode() const { return editMode_; }
    void setEditMode(bool on) {
        editMode_ = on;
        stageEditMode() = on;
        if (!on) {
            for (auto& c : towerRoot_->getChildren()) {
                if (c->isDead()) continue;
                if (auto* rb = c->getMod<RigidBody>()) {
                    rb->body().setLinearVelocity(Vec3(0, 0, 0));
                    rb->body().setAngularVelocity(Vec3(0, 0, 0));
                }
            }
        }
    }

    bool getFalse() const { return false; }
    void doRestart(bool v) { if (v) startGame(); }

    using Super = Node;
    TC_REFLECT(GameScene, Node) {
        TC_VALUE(score, getScore)
        TC_VALUE(shots, getShots)
        TC_VALUE(wave, getWave)
        TC_VALUE(editMode, isEditMode, setEditMode)
        TC_VALUE(restart, getFalse, doRestart)
    }

    int aliveBalls() const {
        int n = 0;
        for (auto& c : ballsRoot_->getChildren())
            if (!c->isDead()) n++;
        return n;
    }

    // balls still in flight (mines on the deck don't count — they only matter
    // once another ball touches them)
    int aliveFlyingBalls() const {
        int n = 0;
        for (auto& c : ballsRoot_->getChildren()) {
            if (c->isDead()) continue;
            auto* b = dynamic_cast<Cannonball*>(c.get());
            if (b && !b->isMine()) n++;
        }
        return n;
    }

    int mineCount() const {
        int n = 0;
        for (auto& c : ballsRoot_->getChildren()) {
            if (c->isDead()) continue;
            auto* b = dynamic_cast<Cannonball*>(c.get());
            if (b && b->isMine()) n++;
        }
        return n;
    }

    json stateJson() {
        return json{
            {"phase", phaseName(phase_)},
            {"score", score_},
            {"hiScore", hiScore_},
            {"shots", shots_},
            {"wave", waveCount_},
            {"scoringLeft", scoringLeft_},
            {"ballsInFlight", aliveFlyingBalls()},
            {"mines", mineCount()},
            {"autopilot", autopilot_},
            {"cannon", {
                {"yawDeg", cannon_->getYawDeg()},
                {"pitchDeg", cannon_->getPitchDeg()},
                {"power", cannon_->getPower()},
                {"charging", cannon_->isCharging()},
            }},
        };
    }

    // --- commands (keys / ImGui / MCP) ---------------------------------------
    void startGame() {
        score_ = 0;
        shots_ = START_SHOTS;
        waveCount_ = 0;
        clearField();
        spawnWave(true);
        phase_ = Phase::Playing;
        settle_ = 0;
        setAutopilot(false);    // human takes over from the attract demo
        jukebox().startJingle.play();
        if (!jukebox().bgm.isPlaying()) jukebox().bgm.play();
    }

    void toTitle() {
        phase_ = Phase::Title;
        score_ = 0;
        shots_ = START_SHOTS;
        waveCount_ = 0;
        clearField();
        spawnWave(true);
        setAutopilot(true);     // attract demo plays itself
        aiTimer_ = 1.0f;
        if (!jukebox().bgm.isPlaying()) jukebox().bgm.play();
    }

    void requestFire(float power) {
        if (phase_ == Phase::Playing) {
            if (shots_ <= 0) { jukebox().dryFire.play(); return; }
            cannon_->fire(power);
        } else if (phase_ == Phase::Title) {
            cannon_->fire(power);
        }
    }

    void setAim(float yawDeg, float pitchDeg) {
        cannon_->setYawDeg(yawDeg);
        cannon_->setPitchDeg(pitchDeg);
    }

    void setAutopilot(bool on) { autopilot_ = on; aiTimer_ = 0; aiAiming_ = false; }

    // debug: drop an already-armed mine on the deck (verification handle)
    void debugSpawnMine() {
        auto b = make_shared<Cannonball>(Vec3(0, 1.3f, PLATFORM_CZ), Vec3(0, 0, 0));
        ballExplodeL_.push_back(b->exploded.listen(this, &GameScene::onBallExploded));
        ballsRoot_->addChild(b);
        b->forceArm();
    }

    // Click / tap advances the non-gameplay screens (and satisfies the browser
    // autoplay gate so audio starts here on web). No-op during play.
    void confirm() {
        if (phase_ == Phase::Title)         startGame();
        else if (phase_ == Phase::GameOver) toTitle();
    }

    void handleKey(int key, bool down) {
        if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) {
            held_[key] = down;
            return;
        }
        if (key == KEY_SPACE && phase_ == Phase::Playing && !autopilot_) {
            if (down && !cannon_->isCharging()) {
                if (shots_ > 0) cannon_->beginCharge();
                else            jukebox().dryFire.play();
            } else if (!down && cannon_->isCharging()) {
                cannon_->releaseCharge();
            }
        }
    }

    // --- lifecycle -----------------------------------------------------------
    void setup() override {
        setName("scene");

        defaultWorld().setup();
        defaultWorld().setGravity(Vec3(0, -12.0f, 0));
        defaultWorld().addGroundPlane(0.0f);

        // fixed play camera; SHIFT+drag orbits (debug). Target sits between the
        // cannon (z=6.5) and the deck (z=-6) and the camera is pulled back so the
        // cannon stays on screen in the foreground.
        cam_.setTarget(0.0f, 1.3f, -1.4f);
        cam_.setDistance(16.5f);
        cam_.setAzimuth(0.22f);
        cam_.setElevation(0.40f);
        cam_.enableMouseInput();
        cam_.setDragModifier(EasyCam::Modifier::Shift);

        keyLight_.setDirectional(Vec3(-0.4f, -1.0f, -0.55f));
        keyLight_.setDiffuse(1.0f, 0.96f, 0.88f);
        keyLight_.setIntensity(2.4f);
        addLight(keyLight_);

        fillLight_.setDirectional(Vec3(0.6f, -0.25f, 0.5f));
        fillLight_.setDiffuse(0.55f, 0.58f, 0.70f);
        fillLight_.setIntensity(1.1f);
        addLight(fillLight_);

        platform_ = make_shared<RotatingPlatform>();
        addChild(platform_);

        auto pedestal = make_shared<StaticProp>(Vec3(0, 0.25f, 6.5f),
                                                Vec3(1.5f, 0.5f, 1.5f),
                                                Color(0.42f, 0.44f, 0.50f));
        pedestal->setName("pedestal");
        addChild(pedestal);

        cannon_ = make_shared<Cannon>();
        cannon_->setPos(0, 0.5f, 6.5f);
        addChild(cannon_);

        towerRoot_ = make_shared<Node>();
        towerRoot_->setName("tower");
        addChild(towerRoot_);

        ballsRoot_ = make_shared<Node>();
        ballsRoot_->setName("balls");
        addChild(ballsRoot_);

        firedL_ = cannon_->fired.listen(this, &GameScene::onFired);

        toTitle();
    }

    void update() override {
        float dt = std::min(0.05f, std::max(0.0f, (float)getDeltaTime()));

        // animate the shared gold-glint texture once per frame (a Texture can
        // only be uploaded once per frame; never let a Block do this)
        goldGlint().update(getElapsedTimef());

        if (editMode_) return;   // stage editing: physics paused

        // substep so fast balls don't tunnel thin blocks at low fps
        int substeps = std::min(4, std::max(1, (int)ceilf(dt / (1.0f / 60.0f))));
        defaultWorld().update(dt, substeps);

        // manual aiming with held arrow keys
        if (phase_ == Phase::Playing && !autopilot_) {
            float aimRate = 0.2f;
            if (held_[KEY_LEFT])  cannon_->setYaw(cannon_->getYaw() + aimRate * dt);
            if (held_[KEY_RIGHT]) cannon_->setYaw(cannon_->getYaw() - aimRate * dt);
            if (held_[KEY_UP])    cannon_->setPitch(cannon_->getPitch() + aimRate * dt);
            if (held_[KEY_DOWN])  cannon_->setPitch(cannon_->getPitch() - aimRate * dt);
        }

        // autopilot: attract demo on the title, optional CPU player in game
        if (phase_ == Phase::Title || (phase_ == Phase::Playing && autopilot_))
            updateAutopilot(dt);

        // proximity detonation: a still-flying ball that grazes an armed mine
        // sets it off (a robust backstop to physical contact — "even a weak
        // touch explodes"). Collect mines and live balls, then test pairs.
        {
            vector<Cannonball*> mines, flying;
            for (auto& c : ballsRoot_->getChildren()) {
                if (c->isDead()) continue;
                auto* b = dynamic_cast<Cannonball*>(c.get());
                if (!b) continue;
                if (b->isMine()) mines.push_back(b);
                else             flying.push_back(b);
            }
            for (auto* m : mines) {
                if (m->isDead()) continue;
                for (auto* f : flying) {
                    if (f->isDead()) continue;
                    if ((m->getGlobalPos() - f->getGlobalPos()).length() < 0.6f) {
                        m->detonateNow();
                        f->destroy();      // the triggering shot is consumed
                        break;
                    }
                }
            }
        }

        // prune ball-explosion listeners once the field is clear of balls
        if (!ballsRoot_->getChildren().empty() && aliveBalls() == 0)
            ballExplodeL_.clear();

        // game over: out of shots, nothing in flight, dust settled
        if (phase_ == Phase::Playing && shots_ <= 0 && aliveFlyingBalls() == 0 &&
            !cannon_->isCharging()) {
            settle_ += dt;
            if (settle_ > 2.8f) gameOver();
        } else {
            settle_ = 0.0f;
        }
    }

    void draw() override {
        // ground grid
        setColor(0.22f, 0.23f, 0.30f);
        const float ext = 14.0f, stp = 1.0f;
        for (float a = -ext; a <= ext + 0.001f; a += stp) {
            drawLine(Vec3(a, 0.005f, -ext), Vec3(a, 0.005f, ext));
            drawLine(Vec3(-ext, 0.005f, a), Vec3(ext, 0.005f, a));
        }
    }

protected:
    void beginDraw() override {
        cam_.begin();
        setCameraPosition(cam_.getPosition());
    }

    void endDraw() override {
        if (phase_ == Phase::Playing && !autopilot_) drawTrajectory();
        cam_.end();
    }

private:
    // --- trajectory guide ----------------------------------------------------
    void drawTrajectory() {
        if (dotMesh_.getNumVertices() == 0) dotMesh_ = createSphere(0.06f, 10);
        // yellow dotted guide: the MAX-power arc (always shown while aiming)
        drawArc(Cannon::MAX_SPEED, Color(1.0f, 0.9f, 0.45f, 0.6f),
                                   Color(1.0f, 0.85f, 0.45f, 0.9f));
        // while charging, overlay THIS instant's power arc in opaque white, so
        // you can read exactly where the shot lands if you release right now
        if (cannon_->isCharging()) {
            float spd = Cannon::MIN_SPEED +
                        cannon_->getPower() * (Cannon::MAX_SPEED - Cannon::MIN_SPEED);
            drawArc(spd, Color(1.0f, 1.0f, 1.0f, 1.0f), Color(1.0f, 1.0f, 1.0f, 1.0f));
        }
        setColor(1.0f);
    }

    // Sample a ballistic arc at `speed` and place dots at equal arc-length
    // intervals, stopping at the first physics hit (bigger marker there).
    void drawArc(float speed, const Color& dotC, const Color& hitC) {
        Vec3  p = cannon_->muzzlePos();
        Vec3  v = cannon_->aimDir() * speed;
        float g = defaultWorld().getGravity().y;
        setColor(dotC);
        const float step = 0.02f, gap = 0.55f;
        float acc = gap;
        Vec3  prev = p;
        int   dots = 0;
        for (float t = step; t < 3.0f && dots < 48; t += step) {
            Vec3 cur = p + v * t + Vec3(0, 0.5f * g * t * t, 0);
            Vec3 seg = cur - prev;
            float len = seg.length();
            if (len > 1e-6f) {
                if (auto h = defaultWorld().raycast(prev, seg / len, len)) {
                    setColor(hitC);
                    pushMatrix();
                    translate(h.point);
                    scale(1.8f, 1.8f, 1.8f);
                    dotMesh_.draw();
                    popMatrix();
                    break;
                }
            }
            acc += len;
            prev = cur;
            if (acc >= gap) {
                acc = 0; dots++;
                pushMatrix();
                translate(cur);
                dotMesh_.draw();
                popMatrix();
            }
            if (cur.y < 0.05f) break;
        }
    }

    // --- field management ----------------------------------------------------
    void clearField() {
        for (auto& c : towerRoot_->getChildren()) c->destroy();
        for (auto& c : ballsRoot_->getChildren()) c->destroy();
        blockL_.clear();
        ballExplodeL_.clear();
        scoringLeft_ = 0;
        cannon_->cancelCharge();
        cannon_->setYawDeg(0);
        cannon_->setPitchDeg(20);
    }

    // highest top surface among the blocks still on the deck (platform top if
    // the deck is empty) — new waves drop in from above this so they never
    // spawn intersecting what's already there
    float highestTopY() const {
        float top = PLATFORM_TOP;
        for (auto& c : towerRoot_->getChildren()) {
            if (c->isDead()) continue;
            auto* b = dynamic_cast<Block*>(c.get());
            if (b && !b->isBusted())
                top = std::max(top, b->getGlobalPos().y + b->getSize().y * 0.5f);
        }
        return top;
    }

    // Pick a horizontal slot inside the deck that doesn't overlap blocks placed
    // so far this wave (rejection sampling on the footprint half-extent).
    Vec3 pickSlot(float halfExtent, vector<Vec3>& taken, vector<float>& takenHE) {
        float usable = PLATFORM_RADIUS - halfExtent - 0.25f;
        if (usable < 0.2f) usable = 0.2f;
        Vec3 best(PLATFORM_CX, 0, PLATFORM_CZ);
        for (int attempt = 0; attempt < 40; attempt++) {
            float r = sqrtf(random(0.0f, 1.0f)) * usable;   // uniform over disc
            float a = random(0.0f, TAU);
            Vec3 cand(PLATFORM_CX + r * cosf(a), 0, PLATFORM_CZ + r * sinf(a));
            bool ok = true;
            for (size_t i = 0; i < taken.size(); i++) {
                float dx = cand.x - taken[i].x, dz = cand.z - taken[i].z;
                float need = halfExtent + takenHE[i] + 0.18f;
                if (dx * dx + dz * dz < need * need) { ok = false; break; }
            }
            if (ok) { best = cand; break; }
            best = cand;   // fall back to the last try if nothing clears
        }
        taken.push_back(best);
        takenHE.push_back(halfExtent);
        return best;
    }

    void addBlock(const BlockDef& d, bool counted) {
        auto b = make_shared<Block>(d);
        Block* bp = b.get();
        blockL_.push_back(b->busted.listen(
            [this, bp](int& pts) { onBlockBusted(bp, pts); }));
        towerRoot_->addChild(b);
        if (counted) scoringLeft_++;
    }

    // Drop a fresh wave from above the current stack. Always at least one gold;
    // junk count (and odds) climb with the wave number.
    void spawnWave(bool initial) {
        waveCount_++;

        // palette for ordinary scoring blocks
        static const Color palette[] = {
            Color(0.45f, 0.85f, 0.70f), Color(0.95f, 0.55f, 0.45f),
            Color(0.55f, 0.78f, 0.95f), Color(0.92f, 0.78f, 0.50f),
            Color(0.55f, 0.35f, 0.75f), Color(0.45f, 0.85f, 0.55f),
        };

        int normals = initial ? 4 : (int)random(3.0f, 5.99f);
        int golds   = 1;

        // difficulty ramp: 0 at wave 1, saturating toward 1
        float ramp = clamp((waveCount_ - 1) * 0.10f, 0.0f, 1.0f);
        int junks = 0;
        if (!initial) {
            // expected junk grows ~0..3 with the ramp, plus a coin-flip bonus
            junks = (int)floorf(ramp * 2.6f);
            if (random(0.0f, 1.0f) < ramp) junks++;
            junks = std::min(junks, 4);
        }

        float baseY = std::min(std::max(highestTopY() + 0.7f, PLATFORM_TOP + 1.4f),
                               PLATFORM_TOP + 6.0f);

        vector<Vec3>  taken;
        vector<float> takenHE;

        // golds first (they get prime, central-ish slots)
        for (int i = 0; i < golds; i++) {
            BlockDef d;
            d.size = Vec3(0.5f, 0.5f, 0.5f);
            d.gold = true; d.points = 500;
            d.color = Color(1.0f, 0.82f, 0.1f);
            float he = std::max(d.size.x, d.size.z) * 0.5f;
            Vec3 s = pickSlot(he, taken, takenHE);
            d.pos = Vec3(s.x, baseY + d.size.y * 0.5f, s.z);
            addBlock(d, true);
        }
        // ordinary targets
        for (int i = 0; i < normals; i++) {
            BlockDef d;
            float sz = random(0.48f, 0.6f);
            d.size = Vec3(sz, sz, sz);
            d.points = 100;
            d.color = palette[(int)random(0.0f, 5.999f)];
            float he = sz * 0.5f;
            Vec3 s = pickSlot(he, taken, takenHE);
            d.pos = Vec3(s.x, baseY + d.size.y * 0.5f, s.z);
            addBlock(d, true);
        }
        // junk: 1.5-3x an ordinary block, gray like the deck, worthless
        for (int i = 0; i < junks; i++) {
            BlockDef d;
            float sz = BLOCK_UNIT * random(1.5f, 3.0f);
            d.size = Vec3(sz, sz, sz);
            d.points = 0; d.junk = true;
            d.color = wallColor();
            d.friction = 0.8f;
            float he = sz * 0.5f;
            Vec3 s = pickSlot(he, taken, takenHE);
            d.pos = Vec3(s.x, baseY + d.size.y * 0.5f + 0.05f, s.z);
            addBlock(d, false);
        }
    }

    // --- events --------------------------------------------------------------
    void onFired(Cannon::FireArgs& args) {
        auto ball = make_shared<Cannonball>(args.pos, args.velocity);
        ballExplodeL_.push_back(ball->exploded.listen(this, &GameScene::onBallExploded));
        ballsRoot_->addChild(ball);
        if (phase_ == Phase::Playing) {
            shots_--;
            settle_ = 0;
        }
    }

    void onBlockBusted(Block* b, int points) {
        if (b->isJunk()) return;   // junk: not a target, no score, not counted
        scoringLeft_--;
        if (phase_ == Phase::Playing) {
            score_ += points;
            hiScore_ = std::max(hiScore_, score_);
            if (b->isGold()) {
                shots_ += GOLD_CHARGE;
                jukebox().charge.play();
            }
        }
        if (scoringLeft_ <= 0) onWaveCleared();
    }

    void onWaveCleared() {
        if (phase_ == Phase::GameOver) return;
        // small breather, then the next wave rains in (works for attract too)
        spawnWave(false);
    }

    // a stuck-ball mine detonated: boom + scatter everything nearby
    void onBallExploded(Vec3& pos) {
        jukebox().explosion.play();
        auto fx = make_shared<ExplosionFx>(pos, 1.8f);
        addChild(fx);

        const float R = BLAST_R;
        for (auto& c : towerRoot_->getChildren()) {
            if (c->isDead()) continue;
            auto* b = dynamic_cast<Block*>(c.get());
            if (!b || b->isBusted()) continue;
            auto* rb = b->getMod<RigidBody>();
            if (!rb || !rb->body().isValid()) continue;
            Vec3 d = b->getGlobalPos() - pos;
            float dist = d.length();
            if (dist > R) continue;
            float falloff = 1.0f - dist / R;   // linear
            Vec3 dir = (dist > 1e-3f) ? d / dist : Vec3(0, 1, 0);
            dir.y += 0.55f;                    // lift so blocks fly off, not slide
            float len = dir.length();
            if (len > 1e-6f) dir = dir / len;
            rb->body().applyImpulse(dir * (BLAST_IMPULSE * falloff));
        }
    }

    void gameOver() {
        phase_ = Phase::GameOver;
        hiScore_ = std::max(hiScore_, score_);
        jukebox().bgm.stop();
        jukebox().overJingle.play();
        callAfter(5.0, [this]() {
            if (phase_ == Phase::GameOver) toTitle();
        });
    }

    // --- autopilot (attract demo + optional CPU player) ----------------------
    void updateAutopilot(float dt) {
        if (phase_ == Phase::Playing && shots_ <= 0) return;

        if (!aiAiming_) {
            aiTimer_ += dt;
            if (aiTimer_ < aiInterval_) return;
            if (pickTargetAndSolve()) aiAiming_ = true;
            else                      aiTimer_ = 0;
            return;
        }
        float rate = 2.2f * dt;
        float y = cannon_->getYaw(), p = cannon_->getPitch();
        y += clamp(aiYaw_ - y, -rate, rate);
        p += clamp(aiPitch_ - p, -rate, rate);
        cannon_->setYaw(y);
        cannon_->setPitch(p);
        if (fabsf(aiYaw_ - y) < 0.008f && fabsf(aiPitch_ - p) < 0.008f) {
            requestFire(aiPower_);
            aiAiming_ = false;
            aiTimer_ = 0;
            aiInterval_ = random(1.4f, 2.2f);
        }
    }

    bool pickTargetAndSolve() {
        // prefer scoring blocks; fall back to junk only if nothing else
        vector<Block*> scoring, any;
        for (auto& c : towerRoot_->getChildren()) {
            if (c->isDead()) continue;
            auto* b = dynamic_cast<Block*>(c.get());
            if (!b || b->isBusted()) continue;
            any.push_back(b);
            if (!b->isJunk()) scoring.push_back(b);
        }
        vector<Block*>& pool = scoring.empty() ? any : scoring;
        if (pool.empty()) return false;
        sort(pool.begin(), pool.end(), [](Block* a, Block* b) {
            return a->getGlobalPos().y < b->getGlobalPos().y;
        });
        int span = std::max(1, (int)pool.size() / 2);
        Block* target = pool[(int)random(0.0f, (float)span - 0.001f)];

        Vec3 base = cannon_->getGlobalPos() + Vec3(0, Cannon::PIVOT_H, 0);
        Vec3 tpos = target->getGlobalPos();
        float g = 12.0f;
        float yawSol = atan2f(-(tpos.x - base.x), -(tpos.z - base.z));

        for (float power : {0.8f, 0.88f, 0.7f, 0.92f, 0.6f}) {
            float v = Cannon::MIN_SPEED + power * (Cannon::MAX_SPEED - Cannon::MIN_SPEED);
            Vec3 from = base;
            float pitch = 0.0f;
            bool ok = false;
            for (int pass = 0; pass < 2; pass++) {
                Vec3 d3 = tpos - from;
                float dist = sqrtf(d3.x * d3.x + d3.z * d3.z);
                float h = d3.y;
                float disc = v * v * v * v - g * (g * dist * dist + 2.0f * h * v * v);
                if (disc < 0) { ok = false; break; }
                pitch = atanf((v * v - sqrtf(disc)) / (g * dist));
                ok = (pitch >= 0.0f && pitch <= deg2rad(45.0f));
                if (!ok) break;
                Vec3 dir(-sinf(yawSol) * cosf(pitch), sinf(pitch), -cosf(yawSol) * cosf(pitch));
                from = base + dir * Cannon::BARREL_LEN;
            }
            if (!ok) continue;
            float n = (scoringLeft_ <= 2) ? 0.0f : 1.0f;
            aiYaw_   = yawSol + n * random(-0.008f, 0.008f);
            aiPitch_ = pitch + n * random(-0.006f, 0.006f);
            aiPower_ = clamp(power + n * random(-0.02f, 0.02f), 0.0f, 0.92f);
            return true;
        }
        aiYaw_ = yawSol;
        aiPitch_ = deg2rad(35.0f);
        aiPower_ = 0.92f;
        return true;
    }

    // --- members -------------------------------------------------------------
    static constexpr int   START_SHOTS   = 10;
    static constexpr int   GOLD_CHARGE   = 5;
    static constexpr float BLAST_R = 2.07f;    // blast reach (m) — 2/3 of the old 3.1
    static constexpr float BLAST_IMPULSE = 7000.0f;

    EasyCam cam_;
    Light   keyLight_, fillLight_;

    shared_ptr<Cannon>           cannon_;
    shared_ptr<RotatingPlatform> platform_;
    shared_ptr<Node>             towerRoot_;
    shared_ptr<Node>             ballsRoot_;
    Mesh                         dotMesh_;
    EventListener                firedL_;
    vector<EventListener>        blockL_;
    vector<EventListener>        ballExplodeL_;
    bool                         editMode_ = false;

    Phase phase_ = Phase::Title;
    int   score_ = 0;
    int   hiScore_ = 0;
    int   shots_ = 0;
    int   waveCount_ = 0;
    int   scoringLeft_ = 0;
    float settle_ = 0;
    bool  autopilot_ = false;

    std::map<int, bool> held_;

    bool  aiAiming_ = false;
    float aiTimer_ = 0, aiInterval_ = 2.0f;
    float aiYaw_ = 0, aiPitch_ = 0, aiPower_ = 0.7f;
};
