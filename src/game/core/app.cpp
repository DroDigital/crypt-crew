#include "game/core/app.h"

#include <algorithm>
#include <filesystem>
#include <vector>
#include <raylib.h>
#include <rlgl.h>

#include "engine/core/log.h"
#include "engine/core/paths.h"
#include "game/scenes/gallery_scene.h"
#include "game/scenes/pieces_scene.h"
#include "game/scenes/lobby_scene.h"
#include "game/scenes/run_scene.h"

namespace game {

App::App(const Options& opt, eng::InputRouter& input, GameState& state, eng::AudioManager& audio)
    : opt_(opt), input_(input), state_(state), audio_(audio) {}

bool App::loadAudio() {
    audio_.init();
    bool ok = true;
    try {
        const eng::json a = eng::loadJsonFile("data/audio.json");
        for (auto it = a.at("sfx").begin(); it != a.at("sfx").end(); ++it) {
            const auto& v = *it;
            ok &= audio_.loadSfx(it.key(), v.at("files").get<std::vector<std::string>>(), v.value("voices", 4),
                                 v.value("volume", 1.f), v.value("pitch_var", 0.08f));
        }
        for (auto it = a.at("music").begin(); it != a.at("music").end(); ++it) ok &= audio_.loadMusic(it.key(), it->get<std::string>());
        audio_.setMusicBase(a.value("music_volume", 0.6f));
    } catch (const std::exception& e) {
        eng::log::error("data/audio.json: %s", e.what());
        return false;
    }
    return ok;
}

void App::shutdownAll() {
    scene_.reset();
    pending_.reset();
    physics_.shutdown();
    particles_.shutdown();
    ui_.shutdown();
    chars_.clear();
    assets_.unloadAll();
    renderer_.shutdown();
    audio_.shutdown();
    CloseWindow();
}

bool App::initWindow() {
    const bool offline = opt_.smokeTest || !opt_.captureScene.empty();
    unsigned flags = FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE;
    if (!offline) flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);
    int w = 1600, h = 900;
    try {
        const eng::json cfg = eng::loadJsonFile("data/config.json");
        w = cfg.at("window").at("width").get<int>();
        h = cfg.at("window").at("height").get<int>();
    } catch (const std::exception&) {
    }
    if (opt_.width > 0 && opt_.height > 0) {
        w = opt_.width;
        h = opt_.height;
    }
    const std::string title = std::string("Crypt Crew ") + CC_VERSION;
    InitWindow(w, h, title.c_str());
    if (!IsWindowReady()) {
        // Some drivers/VMs have no multisampled pixel format: retry without the MSAA hint
        // (the 3D scene still gets MSAA through its own offscreen target when supported).
        eng::log::warn("window creation failed with MSAA hint; retrying without it");
        CloseWindow();
        SetConfigFlags(flags & ~static_cast<unsigned>(FLAG_MSAA_4X_HINT));
        InitWindow(w, h, title.c_str());
        if (IsWindowReady()) eng::log::resetCounts();
    }
    if (!IsWindowReady()) return false;
    SetExitKey(KEY_NULL);
    SetWindowMinSize(960, 540);
    return true;
}

bool App::loadAll() {
    try {
        data_.loadAll();
    } catch (const std::exception& e) {
        eng::log::error("data: %s", e.what());
        return false;
    }
    clock_ = eng::FixedClock(data_.config.value("sim_hz", 60.0));
    if (!renderer_.init()) return false;
    try {
        fx_.load();
        const eng::json ui = eng::loadJsonFile("data/ui.json");
        ui_.init(ui.at("font").get<std::string>());
    } catch (const std::exception& e) {
        eng::log::error("%s", e.what());
        return false;
    }
    physics_.init();
    particles_.init(static_cast<int>(data_.cfg("particles", "max")));
    loadAudio();
    return true;
}

void App::preloadAssets() {
    // Everything is loaded and cached up front; gameplay never loads.
    const double t0 = GetTime();
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    for (const auto& e : fs::recursive_directory_iterator(eng::resourceDir() / "assets")) {
        const auto ext = e.path().extension().string();
        if (ext == ".glb" || ext == ".gltf")
            files.push_back(fs::relative(e.path(), eng::resourceDir()).generic_string());
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        if (f.rfind("assets/kaykit_dungeon/", 0) == 0) assets_.model(f, "assets/kaykit_dungeon/textures/dungeon_texture.png");
        else assets_.model(f);
    }
    for (const auto& [path, def] : data_.animMaps) chars_.animMap(path, nullptr);
    eng::log::info("preloaded %zu models in %.2fs", files.size(), GetTime() - t0);
}

std::unique_ptr<Scene> App::makeScene(const std::string& name) {
    if (name == "gallery") return std::make_unique<GalleryScene>(*this);
    const uint64_t seed = opt_.seed ? opt_.seed : (offline() ? 1234ULL : static_cast<uint64_t>(GetTime() * 1e6) ^ 0x9E3779B97F4A7C15ULL);
    if (name == "smoke" || name == "hud") {
        // 4-player run on a fixed seed, every player an AI bot.
        RunConfig rc;
        rc.seed = seed;
        rc.allBots = true;
        for (int i = 0; i < 4; ++i) rc.players.push_back(PlayerSetup{i, data_.classOrder[static_cast<size_t>(i)], true});
        if (name == "hud") rc.singleRoom = "combat_pillars";
        return std::make_unique<RunScene>(*this, rc);
    }
    if (name == "revive") {
        RunConfig rc;
        rc.seed = seed;
        rc.allBots = true;
        rc.reviveDemo = true;
        rc.singleRoom = "combat_pillars";
        rc.players.push_back(PlayerSetup{0, "knight", true});
        rc.players.push_back(PlayerSetup{1, "mage", true});
        return std::make_unique<RunScene>(*this, rc);
    }
    if (name.rfind("room:", 0) == 0) {
        RunConfig rc;
        rc.seed = seed;
        rc.allBots = true;
        rc.singleRoom = name.substr(5);
        if (!data_.rooms.count(rc.singleRoom)) {
            eng::log::error("unknown room '%s'", rc.singleRoom.c_str());
            return nullptr;
        }
        rc.players.push_back(PlayerSetup{0, "knight", true});
        return std::make_unique<RunScene>(*this, rc);
    }
    if (name == "lobby") return std::make_unique<LobbyScene>(*this, offline() ? 3 : std::max(opt_.dummies, 0));
    if (name == "run") {
        // M1: player 1 on keyboard+mouse (or the first gamepad), plus optional AI teammates.
        RunConfig rc;
        rc.seed = seed;
        rc.singleRoom = "combat_pillars";
        input_.clearSlots();
        const int dev = IsGamepadAvailable(0) ? 1 : eng::kKeyboardDevice;
        input_.join(dev);
        if (dev != eng::kKeyboardDevice) input_.join(eng::kKeyboardDevice);
        rc.players.push_back(PlayerSetup{0, "knight", false});
        int dummies = opt_.dummies >= 0 ? opt_.dummies : static_cast<int>(data_.cfg("debug", "dummy_teammates"));
        for (int i = 0; i < std::min(dummies, 3); ++i)
            rc.players.push_back(PlayerSetup{i + 1, data_.classOrder[static_cast<size_t>(i + 1)], true});
        return std::make_unique<RunScene>(*this, rc);
    }
    if (name.rfind("pieces:", 0) == 0) {
        std::vector<std::string> names;
        std::string rest = name.substr(7);
        size_t pos = 0;
        while ((pos = rest.find(',')) != std::string::npos) {
            names.push_back(rest.substr(0, pos));
            rest.erase(0, pos + 1);
        }
        if (!rest.empty()) names.push_back(rest);
        return std::make_unique<PiecesScene>(*this, names);
    }
    if (name == "icon") return std::make_unique<GalleryScene>(*this, true);
    eng::log::error("unknown scene '%s'", name.c_str());
    return nullptr;
}

void App::saveCapture(int frame) {
    std::error_code ec;
    std::filesystem::create_directories(eng::u8path(opt_.outDir), ec);
    std::string sceneName = opt_.captureScene;
    if (sceneName.rfind("pieces:", 0) == 0) sceneName = "pieces";
    for (char& c : sceneName)
        if (c == ':' || c == '/') c = '_';
    const std::string file = opt_.outDir + "/" + sceneName + "_f" + std::to_string(frame) + ".png";
    rlDrawRenderBatchActive();  // flush pending 2D batch before reading back
    Image img = LoadImageFromScreen();
    if (!ExportImage(img, file.c_str())) eng::log::error("capture: failed to write %s", file.c_str());
    else eng::log::info("capture: wrote %s (%dx%d)", file.c_str(), img.width, img.height);
    UnloadImage(img);
}

int App::runSmokeTest() {
    // Simulate 3,600 fixed steps as fast as possible, rendering every 10th step.
    const int kSteps = 3600;
    for (int step = 0; step < kSteps; ++step) {
        if (pending_) scene_ = std::move(pending_);
        scene_->fixedUpdate(clock_.stepF());
        simTime_ += clock_.step();
        std::string why;
        if (!scene_->healthy(why)) {
            eng::log::error("smoke: step %d: %s", step, why.c_str());
            return 1;
        }
        if (step % 10 == 0) {
            ui_.beginFrame();
            scene_->frameUpdate(clock_.stepF());
            BeginDrawing();
            ClearBackground(BLACK);
            scene_->render(1.f);
            EndDrawing();
        }
    }
    const int errors = eng::log::errorCount();
    eng::log::info("smoke: %d steps, scene '%s', %d error(s), %d asset failure(s)", kSteps, scene_->name(), errors,
                   assets_.failures());
    return (errors == 0 && assets_.failures() == 0) ? 0 : 1;
}

int App::run() {
    eng::log::installRaylibHook(opt_.verbose);
    eng::log::info("Crypt Crew %s, resources at %s", CC_VERSION, eng::resourceDir().string().c_str());
    if (!initWindow()) {
        eng::log::error("could not create a window / OpenGL 3.3 context");
        return 77;  // distinct code: CI falls back to --validate when no GL is available
    }
    if (!loadAll()) {
        CloseWindow();
        return 1;
    }
    const bool capture = !opt_.captureScene.empty();
    if (opt_.captureScene.rfind("pieces:", 0) != 0) preloadAssets();
    scene_ = makeScene(capture ? opt_.captureScene : (opt_.smokeTest ? "smoke" : "lobby"));
    if (!scene_) {
        CloseWindow();
        return 1;
    }
    if (opt_.smokeTest) {
        const int code = runSmokeTest();
        shutdownAll();
        return code;
    }

    int frame = 0;
    while (!quit_) {
        if (WindowShouldClose()) break;
        if (pending_) scene_ = std::move(pending_);
#if defined(CC_DEBUG)
        if (IsKeyPressed(KEY_F5)) {
            std::string err;
            if (data_.reload(err)) eng::log::info("F5: data reloaded");
            else eng::log::warn("F5: reload failed: %s", err.c_str());
        }
#endif
        const double realDt = capture ? clock_.step() : static_cast<double>(GetFrameTime());
        eng::RawDeviceState devices[eng::kMaxDevices];
        eng::DeadZones dz;
        dz.move = data_.cfg("aim", "move_deadzone");
        dz.aim = data_.cfg("aim", "stick_deadzone");
        eng::pollDevices(devices, dz);
        input_.update(devices);
        audio_.update(static_cast<float>(realDt));
        ui_.beginFrame();
        const int steps = clock_.advance(realDt);
        for (int i = 0; i < steps; ++i) {
            scene_->fixedUpdate(clock_.stepF());
            simTime_ += clock_.step();
        }
        scene_->frameUpdate(static_cast<float>(realDt));

        BeginDrawing();
        ClearBackground(BLACK);
        scene_->render(clock_.alpha());
        ++frame;
        if (capture && (frame == opt_.captureFrames || frame == opt_.captureFrames / 2)) saveCapture(frame);
        EndDrawing();
        if (capture && frame >= opt_.captureFrames) break;
    }
    shutdownAll();
    const int errors = eng::log::errorCount();
    if (errors > 0) eng::log::warn("finished with %d error(s)", errors);
    return (capture && errors > 0) ? 1 : 0;
}

}  // namespace game
