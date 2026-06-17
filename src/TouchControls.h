#pragma once

#include <TrussC.h>
#include "GameScene.h"

using namespace std;
using namespace tc;

// One semi-transparent on-screen button. Press/release is forwarded to the
// scene as the equivalent key, so touch and keyboard share one code path.
// Works through touch-as-mouse (first finger = mouse) — no touch API needed.
// Drawn as a squircle (superellipse, n=4): one convex polygon, so the
// translucent fill has no overlap seams.
class TouchButton : public RectNode {
public:
    TouchButton(int key, const string& glyph, float glyphScale, GameScene* scene)
        : key_(key), glyph_(glyph), glyphScale_(glyphScale), scene_(scene) {}

    void setHint(bool h) { hint_ = h; }

    void setup() override {
        setName("btn_" + glyph_);
        enableEvents();
        // a finger sliding off the button still releases it
        releaseL_ = events().mouseReleased.listen([this](MouseEventArgs&) {
            if (pressed_) { pressed_ = false; scene_->handleKey(key_, false); }
        });
    }

    void draw() override {
        if (shapeW_ != getWidth() || shapeH_ != getHeight()) buildShape();
        setBlendMode(BlendMode::Alpha);

        // attention: a fading squircle halo radiates out + a "HOLD" caption,
        // hinting that fire is a press-and-hold (driven externally, e.g. wave 1)
        bool hinting = hint_ && !pressed_;
        if (hinting) {
            float ph = fmodf(getElapsedTimef() * 1.0f, 1.0f);   // 0..1 loop
            float sc = 1.0f + ph * 0.7f;
            pushMatrix();
            translate(getWidth() * 0.5f, getHeight() * 0.5f);
            scale(sc, sc, 1.0f);
            translate(-getWidth() * 0.5f, -getHeight() * 0.5f);
            setColor(1.0f, 0.9f, 0.45f, (1.0f - ph) * 0.45f);
            shape_.drawFill();
            popMatrix();
        }

        fill();
        float pulse = hinting ? 0.12f * (0.5f + 0.5f * sinf(getElapsedTimef() * 5.0f)) : 0.0f;
        setColor(1.0f, 1.0f, 1.0f, pressed_ ? 0.34f : 0.13f + pulse);
        shape_.drawFill();
        setColor(1.0f, 1.0f, 1.0f, pressed_ ? 0.95f : 0.55f);
        setTextAlign(Direction::Center, Direction::Center);
        drawBitmapString(glyph_, getWidth() * 0.5f, getHeight() * 0.5f, glyphScale_);
        if (hinting) {
            setColor(1.0f, 0.95f, 0.6f, 0.55f + 0.45f * sinf(getElapsedTimef() * 4.0f));
            drawBitmapString("HOLD!", getWidth() * 0.5f, -14.0f, 1.6f);
        }
        setTextAlign(Direction::Left, Direction::Top);
    }

protected:
    bool onMousePress(const MouseEventArgs&) override {
        pressed_ = true;
        scene_->handleKey(key_, true);
        return true;   // consume
    }

private:
    void buildShape() {
        shapeW_ = getWidth();
        shapeH_ = getHeight();
        shape_.clear();
        float a = shapeW_ * 0.5f, b = shapeH_ * 0.5f;
        const int   SEG = 48;
        const float n   = 4.0f;   // superellipse exponent (squircle)
        for (int i = 0; i < SEG; i++) {
            float t = (float)i / SEG * TAU;
            float c = cosf(t), s = sinf(t);
            float x = a + a * copysignf(powf(fabsf(c), 2.0f / n), c);
            float y = b + b * copysignf(powf(fabsf(s), 2.0f / n), s);
            if (i == 0) shape_.moveTo(x, y);
            else        shape_.lineTo(x, y);
        }
        shape_.close();
    }

    int           key_;
    string        glyph_;
    float         glyphScale_;
    GameScene*    scene_;
    bool          pressed_ = false;
    bool          hint_ = false;
    EventListener releaseL_;
    Path          shape_;
    float         shapeW_ = -1, shapeH_ = -1;
};

// Mobile control overlay: arrow pad bottom-left, fire button bottom-right.
class TouchControls : public Node {
public:
    explicit TouchControls(GameScene* scene) : scene_(scene) {}

    void setup() override {
        setName("touchControls");
        auto mk = [&](int key, const string& g, float w, float h, float gs) {
            auto b = make_shared<TouchButton>(key, g, gs, scene_);
            b->setSize(w, h);
            addChild(b);
            return b;
        };
        const float s = BTN;
        up_    = mk(KEY_UP,    "^",  s, s, 2.0f);
        down_  = mk(KEY_DOWN,  "v",  s, s, 2.0f);
        left_  = mk(KEY_LEFT,  "<",  s, s, 2.0f);
        right_ = mk(KEY_RIGHT, ">",  s, s, 2.0f);
        fire_  = mk(KEY_SPACE, "FIRE", 84, 60, 1.5f);
    }

    void update() override {
        // re-layout every frame: window size changes on rotation / resize
        float H = (float)getWindowHeight();
        float W = (float)getWindowWidth();
        const float s = BTN, gap = 5;
        float cx = 24 + s * 1.5f + gap;          // dpad center
        float cy = H - 24 - s * 1.5f - gap;
        up_->setPos(cx - s * 0.5f, cy - s * 1.5f - gap);
        down_->setPos(cx - s * 0.5f, cy + s * 0.5f + gap);
        left_->setPos(cx - s * 1.5f - gap, cy - s * 0.5f);
        right_->setPos(cx + s * 0.5f + gap, cy - s * 0.5f);
        fire_->setPos(W - 84 - 24, H - 60 - 28);

        // first wave only: nudge the player that FIRE is a press-and-hold
        fire_->setHint(scene_->getPhase() == Phase::Playing &&
                       scene_->getWave() == 1 &&
                       !scene_->cannon()->isCharging());
    }

private:
    static constexpr float BTN = 40;
    GameScene* scene_;
    shared_ptr<TouchButton> up_, down_, left_, right_, fire_;
};
