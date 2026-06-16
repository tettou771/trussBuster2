#pragma once

#include <TrussC.h>
#include "Levels.h"

using namespace std;
using namespace tc;

// Shared animated "glint" texture for gold blocks. Every gold block samples the
// SAME texture (a synchronized shimmer reads as a coordinated sparkle), so the
// per-frame pixel work is paid once regardless of how many gold blocks exist.
// Authored in LINEAR space (baseColor stays white) to match the brightness of a
// flat toLinearColor(gold) material. Call update() ONCE per frame (a Texture
// can only be uploaded once per frame) from the main thread.
struct GoldGlint {
    static constexpr int SIZE = 64;

    void update(float t) {
        if (!inited_) {
            px_.allocate(SIZE, SIZE, 4);
            paint(t);
            tex_.allocate(px_, TextureUsage::Dynamic);
            inited_ = true;
            return;
        }
        paint(t);
        tex_.loadData(px_);
    }

    const Texture& texture() const { return tex_; }

private:
    // A diagonal highlight sweeps across once every PERIOD seconds (the "tsuya"
    // glint); outside the brief sweep window the face is plain brushed gold.
    void paint(float t) {
        const float PERIOD = 3.0f, SWEEP = 0.55f;
        float ph = fmodf(t, PERIOD);
        // highlight center travels -0.3 .. 1.3 along the diagonal during SWEEP
        float hc = (ph < SWEEP) ? (-0.3f + (ph / SWEEP) * 1.6f) : 99.0f;
        const Color goldLin = toLinearColor(Color(1.0f, 0.78f, 0.12f));
        for (int y = 0; y < SIZE; y++) {
            for (int x = 0; x < SIZE; x++) {
                float u = (x + 0.5f) / SIZE, v = (y + 0.5f) / SIZE;
                float d = (u + v) * 0.5f;                       // diagonal 0..1
                float band = expf(-powf((d - hc) / 0.09f, 2.0f));
                float shade = 0.82f + 0.18f * v;                // brushed gradient
                Color c(goldLin.r * shade, goldLin.g * shade, goldLin.b * shade, 1.0f);
                c.r = min(1.0f, c.r + band);
                c.g = min(1.0f, c.g + band * 0.95f);
                c.b = min(1.0f, c.b + band * 0.75f);
                px_.setColor(x, y, c);
            }
        }
    }

    Texture tex_;
    Pixels  px_;
    bool    inited_ = false;
};

inline GoldGlint& goldGlint() { static GoldGlint g; return g; }
