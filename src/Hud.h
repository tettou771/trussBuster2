#pragma once

#include <TrussC.h>
#include "GameScene.h"

using namespace std;
using namespace tc;

// 2D overlay: title screen, in-game HUD, game over. Reads game state from the
// scene; never writes it.
class Hud : public RectNode {
public:
    explicit Hud(GameScene* scene) : scene_(scene) {}

    void setMobile(bool m) { mobile_ = m; }

    void setup() override {
        setName("hud");
        disableEvents();   // never block clicks to the 3D scene / inspector
    }

    void update() override {
        setSize((float)getWindowWidth(), (float)getWindowHeight());
    }

    void draw() override {
        setBlendMode(BlendMode::Alpha);
        switch (scene_->getPhase()) {
            case Phase::Title:    drawTitle(); break;
            case Phase::Playing:  drawPlaying(); break;
            case Phase::GameOver: drawPlaying(); drawGameOver(); break;
        }
    }

private:
    GameScene* scene_;
    bool       mobile_ = false;

    static float textW(const string& s, float scale) { return s.size() * 8.0f * scale; }
    float uiScale() const { return clamp(getWidth() / 1100.0f, 0.6f, 1.0f); }

    void center(const string& s, float y, float scale, const Color& c) {
        setColor(c);
        drawBitmapString(s, (getWidth() - textW(s, scale)) * 0.5f, y, scale);
    }

    void centerShadow(const string& s, float y, float scale, const Color& c) {
        float x = (getWidth() - textW(s, scale)) * 0.5f;
        setColor(0.0f, 0.0f, 0.0f, 0.55f);
        drawBitmapString(s, x + scale, y + scale, scale);
        setColor(c);
        drawBitmapString(s, x, y, scale);
    }

    static string pad(int v, int digits = 6) {
        string s = to_string(std::max(0, v));
        while ((int)s.size() < digits) s = "0" + s;
        return s;
    }

    void dim(float a) {
        setColor(0.0f, 0.0f, 0.0f, a);
        drawRect(0, 0, getWidth(), getHeight());
    }

    // --- screens -------------------------------------------------------------
    void drawTitle() {
        float t = getElapsedTimef();
        float W = getWidth(), H = getHeight();

        Color logoCol = Color::fromOKLCH(0.85f, 0.13f, fmodf(t * 0.07f, 1.0f));
        float logoScale = std::min(8.0f, (W - 40) / (12 * 8.0f));
        centerShadow("TRUSS BUSTER", H * 0.18f, logoScale, logoCol);
        center("ENDLESS", H * 0.18f + logoScale * 9.0f + 6, 2.5f * uiScale(),
               Color(0.7f, 0.9f, 0.75f));

        if (fmodf(t, 1.2f) < 0.75f)
            centerShadow(mobile_ ? "TAP TO START" : "CLICK TO START",
                         H * 0.45f, 3.0f * uiScale(), Color(1.0f, 0.95f, 0.6f));

        center("HI         " + pad(scene_->getHiScore()), H * 0.54f, 2.0f * uiScale(),
               Color(0.95f, 0.6f, 0.5f));
        if (worldScore().loaded)
            center("HI WORLD  " + pad(worldScore().score) + " " + worldScore().initials,
                   H * 0.54f + 26 * uiScale(), 2.0f * uiScale(), Color(1.0f, 0.82f, 0.3f));

        setColor(0.4f, 0.42f, 0.5f);
        drawBitmapString("build " __DATE__ " " __TIME__, 24, getHeight() - 14, 1.0f);

        if (fmodf(t, 0.8f) < 0.5f) {
            setColor(0.5f, 0.85f, 0.6f);
            drawBitmapString("CPU DEMO", W - 110, 20, 1.5f);
        }

        if (!mobile_) {
            float y = getHeight() - 92;
            setColor(0.6f, 0.62f, 0.7f);
            drawBitmapString("ARROWS : AIM\nHOLD SPACE : CHARGE & FIRE\nGOLD RECHARGES - SURVIVE THE ENDLESS DECK!", 24, y, 1.5f);
            setColor(0.45f, 0.47f, 0.55f);
            drawBitmapString("F1 : INSPECTOR\nF2 : DEBUG PANEL", getWidth() - 150, y, 1.5f);
        }
    }

    void drawPlaying() {
        float W = getWidth(), H = getHeight();
        float k = uiScale();
        bool narrow = W < 700;
        float ts = 2.0f * k;
        float lineH = 13.0f * ts;

        // wave (top-left)
        setColor(0.95f, 0.95f, 1.0f);
        drawBitmapString("WAVE " + to_string(scene_->getWave()), 24, 22, ts);
        setColor(0.65f, 0.8f, 0.95f);
        drawBitmapString("LEFT " + to_string(scene_->getScoringLeft()), 24, 22 + lineH, ts);

        // score (top-right)
        string sc = "SCORE " + pad(scene_->getScore());
        string hi = "HI    " + pad(scene_->getHiScore());
        setColor(1.0f, 1.0f, 0.95f);
        drawBitmapString(sc, W - textW(sc, ts) - 24, 22, ts);
        setColor(0.8f, 0.65f, 0.6f);
        drawBitmapString(hi, W - textW(hi, ts) - 24, 22 + lineH, ts);

        // shots: cannonball icons, wrapped every 10
        float sx = 24, sy = mobile_ ? 22 + lineH * 2 : H - 52;
        float dotR = 8.0f * k, dotStep = 26.0f * k;
        setColor(0.9f, 0.9f, 0.95f);
        drawBitmapString("SHOTS", sx, sy, ts);
        int shots = std::min(scene_->getShots(), 40);   // cap the icon row
        for (int i = 0; i < shots; i++) {
            setColor(0.85f, 0.88f, 0.95f);
            drawCircle(sx + textW("SHOTS ", ts) + (i % 10) * dotStep,
                       sy + ts * 4.0f + (i / 10) * (dotR * 2.2f), dotR);
        }
        if (scene_->getShots() > 40) {
            setColor(1.0f, 0.9f, 0.5f);
            drawBitmapString("+" + to_string(scene_->getShots() - 40),
                             sx + textW("SHOTS ", ts) + 10 * dotStep, sy + ts * 2.0f, ts);
        }

        if (scene_->isAutopilot() && fmodf(getElapsedTimef(), 0.8f) < 0.5f)
            center("AUTO", narrow ? 22.0f : 22 + lineH, ts, Color(0.5f, 0.85f, 0.6f));

        // oscillating power gauge while charging
        Cannon* c = scene_->cannon();
        if (c->isCharging()) {
            float gw = std::min(280.0f, W - 48), gh = 18;
            float gx = mobile_ ? (W - gw) * 0.5f : 24;
            float gy = mobile_ ? H - 175 : H - 100;
            float p = c->getPower();
            bool inZone = p >= Cannon::MAX_ZONE;
            setColor(0.85f, 0.85f, 0.9f);
            drawBitmapString("POWER", gx, gy - 22, 1.5f);
            setColor(0.0f, 0.0f, 0.0f, 0.5f);
            drawRect(gx - 4, gy - 4, gw + 8, gh + 8);
            setColor(1.0f, 1.0f, 1.0f, 0.25f);
            drawRect(gx + gw * Cannon::MAX_ZONE, gy, gw * (1.0f - Cannon::MAX_ZONE), gh);
            if (inZone) setColor(1.0f, 1.0f, 1.0f);
            else        setColor(Color::fromHSB(0.33f * (1.0f - p), 0.85f, 0.95f));
            drawRect(gx, gy, gw * p, gh);
        } else if (!mobile_ && !scene_->isAutopilot()) {
            setColor(0.6f, 0.62f, 0.7f);
            drawBitmapString("HOLD SPACE TO FIRE", 24, H - 100, 1.5f);
        }
    }

    void drawGameOver() {
        dim(0.55f);
        float k = uiScale(), H = getHeight(), t = getElapsedTimef(), y = H * 0.26f;
        centerShadow("GAME OVER", y, 6.0f * k, Color(0.95f, 0.35f, 0.3f));
        center("WAVE  " + to_string(scene_->getWave()), y + 78 * k, 2.5f * k, Color(0.7f, 0.85f, 0.95f));
        center("SCORE  " + pad(scene_->getScore()), y + 114 * k, 3.0f * k, Color(0.95f, 0.95f, 1.0f));
        center("HI        " + pad(scene_->getHiScore()), y + 156 * k, 2.0f * k, Color(0.8f, 0.65f, 0.6f));
        if (worldScore().loaded)
            center("HI WORLD  " + pad(worldScore().score) + " " + worldScore().initials,
                   y + 186 * k, 2.0f * k, Color(1.0f, 0.82f, 0.3f));

        if (scene_->isEnteringInitials()) {
            if (fmodf(t, 0.7f) < 0.45f)
                center("NEW WORLD RECORD!", y + 232 * k, 2.5f * k, Color(1.0f, 0.9f, 0.3f));
            string e = scene_->getInitialsEntry();
            string slot;
            for (int i = 0; i < 3; i++)
                slot += (i < (int)e.size()) ? string(1, e[i])
                      : (i == (int)e.size() && fmodf(t, 0.6f) < 0.3f) ? "_" : " ";
            center("INITIALS [" + slot + "]", y + 270 * k, 2.5f * k, Color(1.0f, 1.0f, 1.0f));
            if (!mobile_)
                center("TYPE 3 LETTERS + ENTER", y + 304 * k, 1.5f * k, Color(0.7f, 0.7f, 0.75f));
        } else if (fmodf(t, 1.2f) < 0.75f) {
            center(mobile_ ? "TAP TO CONTINUE" : "CLICK TO CONTINUE",
                   y + 232 * k, 2.0f * k, Color(0.8f, 0.8f, 0.85f));
        }
    }
};
