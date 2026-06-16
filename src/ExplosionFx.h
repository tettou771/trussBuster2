#pragma once

#include <TrussC.h>
#include "Levels.h"

using namespace std;
using namespace tc;

// A short-lived blast flash: an expanding additive sphere that fades out and
// self-destructs. Lives as a child of the scene, drawn inside the camera scope.
class ExplosionFx : public Node {
public:
    ExplosionFx(const Vec3& pos, float radius) : pos_(pos), radius_(radius) {}

    void setup() override { setName("explosionFx"); }

    void update() override {
        life_ += (float)getDeltaTime() / DURATION;
        if (life_ >= 1.0f) destroy();
    }

    void draw() override {
        if (sphere_.getNumVertices() == 0) sphere_ = createSphere(1.0f, 16);
        float e = 0.25f + life_ * 0.95f;          // expand
        float a = (1.0f - life_) * 0.8f;          // fade

        // additive flash — restore the depth-writing 3D pipeline afterwards or
        // subsequent geometry renders depth-less (back faces over front faces)
        setBlendMode(BlendMode::Add);
        setColor(1.0f, 0.75f, 0.3f, a);
        pushMatrix();
        translate(pos_);
        scale(radius_ * e, radius_ * e, radius_ * e);
        sphere_.draw();
        popMatrix();
        // bright core
        setColor(1.0f, 0.95f, 0.7f, a * 0.9f);
        pushMatrix();
        translate(pos_);
        scale(radius_ * e * 0.5f, radius_ * e * 0.5f, radius_ * e * 0.5f);
        sphere_.draw();
        popMatrix();

        setColor(1.0f);
        setBlendMode(BlendMode::Alpha);
        if (internal::pipeline3dInitialized) sgl_load_pipeline(internal::pipeline3d);
    }

private:
    static constexpr float DURATION = 0.45f;
    Vec3  pos_;
    float radius_;
    float life_ = 0.0f;
    Mesh  sphere_;
};
