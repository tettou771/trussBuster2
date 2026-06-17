#include "tcApp.h"

void tcApp::setup() {
    // TB_FPS: debug — run the whole app at a low frame rate (slow-machine test)
    if (const char* e = std::getenv("TB_FPS")) setFps((float)atof(e));

    setWindowTitle("TRUSS BUSTER");
    imguiSetup();

#ifndef __EMSCRIPTEN__
    // allow MCP input injection (inert unless TRUSSC_MCP=1); no MCP on web
    mcp::enableDebugger();
    mcp::registerDebuggerTools();
#endif

    jukebox().setup();

    worldScore().fetch();   // load the global high score (HI WORLD)

    scene_ = make_shared<GameScene>();
    addChild(scene_);

    mobile_ = isMobileDevice();
    scene_->setMobile(mobile_);
    hud_ = make_shared<Hud>(scene_.get());
    hud_->setMobile(mobile_);
    addChild(hud_);

    if (mobile_) {
        touch_ = make_shared<TouchControls>(scene_.get());
        addChild(touch_);   // after hud: drawn on top, hit-tested first
    }

    // starfield
    for (int i = 0; i < 90; i++) {
        stars_.push_back({random(0.0f, 1.0f), random(0.0f, 0.62f),
                          random(1.0f, 2.6f), random(0.0f, TAU)});
    }

    // NodeInspector disabled.
    // NodeInspector::instance().setEnabled(false);   // dev tool, opt-in via F1

    registerMcpTools();
}

void tcApp::update() {}

void tcApp::draw() {
    clear(0.045f, 0.04f, 0.09f);

    // night sky backdrop (2D before 3D: needs alpha blend so it doesn't
    // write depth and clip the scene)
    setBlendMode(BlendMode::Alpha);
    float t = getElapsedTimef();
    float W = getWindowWidth(), H = getWindowHeight();
    for (auto& s : stars_) {
        float tw = 0.55f + 0.45f * sinf(t * 1.3f + s.phase);
        setColor(0.85f, 0.88f, 1.0f, 0.65f * tw);
        drawCircle(s.x * W, s.y * H, s.size * 0.9f);
    }
    // moon
    setColor(0.93f, 0.92f, 0.82f, 0.9f);
    drawCircle(W * 0.82f, H * 0.15f, 38);
    setColor(0.045f, 0.04f, 0.09f, 1.0f);
    drawCircle(W * 0.82f - 16, H * 0.15f - 8, 32);

    imguiBegin();
    // NodeInspector::instance().draw(*this);   // node inspector disabled
    if (debugPanel_) drawDebugPanel();
    imguiEnd();
}

void tcApp::keyPressed(int key) {
    // if (key == KEY_F1) { NodeInspector::instance().toggle(); return; }  // disabled
    if (key == KEY_F2) { debugPanel_ = !debugPanel_; return; }
    scene_->handleKey(key, true);
}

void tcApp::keyReleased(int key) {
    scene_->handleKey(key, false);
}

void tcApp::mousePressed(const MouseEventArgs& e) {
    // Click / tap anywhere advances the title / game-over / all-clear screens
    // (and unlocks web audio on the first gesture). No-op during play.
    scene_->confirm();
}

void tcApp::cleanup() {
    imguiShutdown();
}

// ---------------------------------------------------------------------------
// ImGui debug panel — every widget here is also an automation handle (MCP
// imgui_* tools drive them by label).
// ---------------------------------------------------------------------------
void tcApp::drawDebugPanel() {
    ImGui::SetNextWindowPos(ImVec2(20, 90), ImGuiCond_FirstUseEver);
    ImGui::Begin("Debug");

    ImGui::Text("phase: %s  wave: %d  score: %d",
                phaseName(scene_->getPhase()), scene_->getWave(), scene_->getScore());
    ImGui::Text("shots: %d  left: %d  balls: %d  mines: %d",
                scene_->getShots(), scene_->getScoringLeft(),
                scene_->aliveFlyingBalls(), scene_->mineCount());

    Cannon* c = scene_->cannon();
    float yaw = c->getYawDeg();
    if (ImGui::SliderFloat("Yaw", &yaw, -61.0f, 61.0f)) c->setYawDeg(yaw);
    float pitch = c->getPitchDeg();
    if (ImGui::SliderFloat("Pitch", &pitch, 0.0f, 45.0f)) c->setPitchDeg(pitch);
    ImGui::SliderFloat("Power", &dbgPower_, 0.0f, 1.0f);
    if (ImGui::Button("Fire")) scene_->requestFire(dbgPower_);

    bool ap = scene_->isAutopilot();
    if (ImGui::Checkbox("Autopilot", &ap)) scene_->setAutopilot(ap);

    if (ImGui::Button("Start Game")) scene_->startGame();
    ImGui::SameLine();
    if (ImGui::Button("Title")) scene_->toTitle();

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Custom MCP tools — the agent-facing API of the game.
// ---------------------------------------------------------------------------
void tcApp::registerMcpTools() {
#ifdef __EMSCRIPTEN__
    return;   // no MCP server in wasm builds
#else
    mcp::tool("get_state", "Full game state (phase, level, score, shots, blocks, cannon)")
        .bind([this]() { return scene_->stateJson(); });

    mcp::tool("set_aim", "Aim the cannon (yaw: -61..61 deg, + = left; pitch: 0..45 deg)")
        .arg<float>("yaw", "yaw in degrees")
        .arg<float>("pitch", "pitch in degrees")
        .bind([this](const json& a) {
            scene_->setAim(a.value("yaw", 0.0f), a.value("pitch", 20.0f));
            return scene_->stateJson();
        });

    mcp::tool("fire", "Fire the cannon with the given power (0..1)")
        .arg<float>("power", "shot power 0..1")
        .bind([this](const json& a) {
            scene_->requestFire(a.value("power", 0.7f));
            return scene_->stateJson();
        });

    mcp::tool("press_start", "Start the game from the title (or return to title after game over)")
        .bind([this]() {
            if (scene_->getPhase() == Phase::Title) scene_->startGame();
            else                                    scene_->toTitle();
            return scene_->stateJson();
        });

    mcp::tool("set_autopilot", "Enable/disable the CPU player during gameplay")
        .arg<bool>("on", "true = CPU plays")
        .bind([this](const json& a) {
            scene_->setAutopilot(a.value("on", false));
            return scene_->stateJson();
        });

    mcp::tool("spawn_mine", "Debug: drop an already-armed mine on the deck center")
        .bind([this]() {
            scene_->debugSpawnMine();
            return scene_->stateJson();
        });
#endif
}
