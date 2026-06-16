#pragma once

#include <TrussC.h>
#include <tcxImGui.h>
#include <tcxNodeInspector.h>
#include "GameScene.h"
#include "Hud.h"
#include "Mobile.h"
#include "TouchControls.h"

using namespace std;
using namespace tc;
using namespace tcx;

// TRUSS BUSTER — knock truss towers off the platform with a cannon.
//
//   CLICK / TAP    start (and continue after game over / all clear)
//   ARROWS         aim
//   SPACE (hold)   charge & fire
//   F1             node inspector
//   F2             debug panel (aim sliders, autopilot, level jump)
//
// MCP: get_state / set_aim / fire / press_start / goto_level / set_autopilot
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void keyReleased(int key) override;
    void mousePressed(const MouseEventArgs& e) override;
    void cleanup() override;

private:
    void registerMcpTools();
    void drawDebugPanel();

    shared_ptr<GameScene>     scene_;
    shared_ptr<Hud>           hud_;
    shared_ptr<TouchControls> touch_;
    bool                      mobile_ = false;
    bool                      debugPanel_ = false;
    float                     dbgPower_ = 0.7f;

    // starfield background (normalized coords)
    struct Star { float x, y, size, phase; };
    vector<Star> stars_;
};
