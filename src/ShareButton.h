#pragma once

#include <TrussC.h>
#include "GameScene.h"
#include "WorldScore.h"

using namespace std;
using namespace tc;

// A clickable SHARE button shown on the game-over screen. Posts the run (score +
// game link) to the native share sheet on mobile / an X (Twitter) intent on
// desktop. It's a RectNode so it owns its own click; tcApp gates the
// screen-advance tap against its bounds so a share-click doesn't also leave.
class ShareButton : public RectNode {
public:
    explicit ShareButton(GameScene* scene) : scene_(scene) {}

    void setup() override {
        setName("shareButton");
        enableEvents();
        releaseL_ = events().mouseReleased.listen([this](MouseEventArgs&) { pressed_ = false; });
    }

    bool screenHit(float x, float y) const {
        return visible_ && x >= getX() && x <= getX() + getWidth()
                        && y >= getY() && y <= getY() + getHeight();
    }

    void update() override {
        // Stay active (so update keeps running); show only on the game-over
        // screen, and not while typing initials. When hidden, park off-screen so
        // it never intercepts a tap.
        visible_ = scene_->getPhase() == Phase::GameOver && !scene_->isEnteringInitials();
        if (!visible_) { setPos(-9999, -9999); return; }
        float W = (float)getWindowWidth(), H = (float)getWindowHeight();
        float k = clamp(W / 1100.0f, 0.6f, 1.0f);
        setSize(190 * k, 52 * k);
        setPos((W - getWidth()) * 0.5f, H * 0.82f);
    }

    void draw() override {
        if (!visible_) return;
        setBlendMode(BlendMode::Alpha);
        setColor(0.11f, 0.63f, 0.95f, pressed_ ? 1.0f : 0.88f);   // X-ish blue
        drawRect(0, 0, getWidth(), getHeight());
        setColor(1.0f, 1.0f, 1.0f, 0.25f);
        drawRect(0, 0, getWidth(), 2);                            // top highlight
        setColor(1.0f, 1.0f, 1.0f, 0.97f);
        setTextAlign(Direction::Center, Direction::Center);
        drawBitmapString("SHARE", getWidth() * 0.5f, getHeight() * 0.5f,
                         2.2f * (getWidth() / 190.0f));
        setTextAlign(Direction::Left, Direction::Top);
    }

protected:
    bool onMousePress(const MouseEventArgs&) override {
        if (!visible_) return false;   // hidden: let the tap pass through
        pressed_ = true;
        scene_->share();
        return true;   // consume
    }

private:
    GameScene*    scene_;
    bool          visible_ = false;
    bool          pressed_ = false;
    EventListener releaseL_;
};
