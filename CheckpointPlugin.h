/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginsettingswindow.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "utils/parser.h"
#include "state.h"

#include "version.h"

constexpr auto PLUGIN_VERSION = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);
constexpr float MAX_DODGE_TIME = 1.2f; // TODO: LOOK AT THIS BEFORE POSSIBLY SUBMITTING A PR FOR THIS BRANCH

template<typename T>
void writePOD(std::ostream& out, const T& t) {
    out.write(reinterpret_cast<const char*>(&t), sizeof(T));
}

template<typename T>
void readPOD(std::istream& in, T& t) {
    T temp;
    in.read(reinterpret_cast<char*>(&temp), sizeof(T));
    if (in.eof()) {
        return;
    }
    t = temp;
}

// Rotator uses ints instead of floats.  Floats are better.
struct Rot {
    float Pitch, Yaw, Roll;
};

// TODO: make this a full-on "RewindMode" class with functions for operations
struct RewindState {
    bool atCheckpoint = false;
    float virtualTimeOffset = 0; // Delta from end of buffer to "now"
    bool justDeletedCheckpoint = false;
    bool justLoadedQuickCheckpoint = false;
    float holdingFor = 0;
    bool deleting = false;
    int buttonsDown = 0x7f;
    float lastRewindVal = 0.f;
    float lastFFVal = 0.f;
};

class CheckpointPlugin : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow, public BakkesMod::Plugin::PluginWindow {
    //Boilerplate
    virtual void onLoad();
    void copyShot(std::vector<std::string> command);
    void mirrorState(std::vector<std::string> command);
    void deleteAllCheckpoints(std::vector<std::string> command);
    void randCheckpoint(std::vector<std::string> command);
    void pasteShot(std::vector<std::string> command);
    void freezeBallUnfreezeCar(std::vector<std::string> command);
    virtual void onUnload();
    void doCheckpoint(std::vector<std::string> command);
    void lockCheckpoint(std::vector<std::string> command);
    void prevCheckpoint(std::vector<std::string> command);
    void nextCheckpoint(std::vector<std::string> command);

    // plugin settings drawing functions
    void RenderSettings();
    std::string GetPluginName();
    void SetImGuiContext(uintptr_t ctx);

    // plugin "window" used for easily throwing up a menu to bind keys
    void Render();
    std::string GetMenuName();
    std::string GetMenuTitle();
    bool ShouldBlockInput();
    bool IsActiveOverlay();
    void OnOpen();
    void OnClose();

private:
    RewindState rewindState;
    std::vector<GameState> history;
    GameState latest;
    std::vector<GameState> checkpoints;
    std::vector<bool> locks;
    size_t curCheckpoint = 0;
    bool rewindMode = false;
    bool freezeBall = false;
    float dodgeExpiration = 0;
    bool hasQuickCheckpoint = false;
    GameState quickCheckpoint;
    float lastRecordTime = 0;
    float lastRewindTime = 0;
    std::vector<GameState> gameHistory;
    int carNum = 0;
    bool playingFromCheckpoint = false;

    // Settings:
    bool deleteFutureHistory = false;
    bool ignorePNNotFrozen = false;
    bool ignorePrev = false;
    bool ignoreNext = false;
    bool ignoreFreezeBall = false;
    bool disableTraining = false;
    bool disableWorkshop = false;
    bool debug = false;
    bool resetOnGoal = false;
    bool resetOnBallGround = false;
    bool nextInsteadOfReset = false;
    bool mirrorLoads = false;
    bool randomizeLoads = false;
    bool showBoost = false;

    void addBind(std::string key, std::string cmd);
    void removeBind(std::string key, std::string cmd);
    void OnPreAsync(std::string funcName);
    void registerVarianceCVars();
    void registerBindingCVars();
    void captureBindKey(std::vector<std::string> params);
    void removeBindKeys(std::vector<std::string> params);
    void applyBindKeys(std::vector<std::string> params);
    void resetDefaultBindKeys(std::vector<std::string> params);
    GameState applyVariance(GameState& s);
    bool rewind(ServerWrapper sw);
    void loadCheckpointFile();
    void saveCheckpointFile();
    void Render(CanvasWrapper canvas);
    void record(ServerWrapper sw);
    void loadLatestCheckpoint();
    void loadCurCheckpoint();
    void loadRandomCheckpoint();
    void loadGameState(const GameState&);
    void log(std::string s);
    void boolvar(std::string name, std::string desc, bool* var);
    std::unique_ptr<GameState> getReplayGameState();
    void setFrozen(bool car, bool ball);
    bool enabled();
    bool enabledLoads();

    // grab RL's window handle to use during keybinding (to clear stuck mouse inputs)
    static inline const HWND rl_hwnd = []() {
        DWORD pid = GetCurrentProcessId();
        static HWND hWnd = NULL;
        WNDENUMPROC EnumWindowsFunc = [](HWND hwnd, LPARAM lParam) -> BOOL {
            DWORD lpdwProcessId;
            GetWindowThreadProcessId(hwnd, &lpdwProcessId);

            if (lpdwProcessId == lParam) {
#ifdef _MBCS
                char str[128] = { 0 };
                GetWindowText(hwnd, str, 128);
                if (strstr(str, "Rocket") != NULL) {
                    hWnd = hwnd;
                    return FALSE;
                }
#else
#ifdef _UNICODE
                wchar_t str[128] = { 0 };
                GetWindowText(hwnd, str, 128);
                if (wcsstr(str, L"Rocket") != NULL) {
                    hWnd = hwnd;
                    return FALSE;
                }
#endif
#endif
            }

            return TRUE;
            };

        EnumWindows(EnumWindowsFunc, pid);
        return hWnd;
        }();

    enum class KEYBIND_ASSIGNWHICH {
        NONE = 0,
        CPT_FREEZE_KEY,
        CPT_DO_CHECKPOINT_KEY,
        CPT_PREV_CHECKPOINT_KEY,
        CPT_NEXT_CHECKPOINT_KEY,
        CPT_FREEZE_BALL_KEY,
        CPT_MIRROR_STATE_KEY,
        CPT_REWIND_KEY,
        CPT_FASTFORWARD_KEY
    } which_is_being_bound = KEYBIND_ASSIGNWHICH::NONE;

    const static inline std::map<KEYBIND_ASSIGNWHICH, std::string> keys_to_cvars = {
        {KEYBIND_ASSIGNWHICH::CPT_FREEZE_KEY, "cpt_freeze_key"},
        {KEYBIND_ASSIGNWHICH::CPT_DO_CHECKPOINT_KEY, "cpt_do_checkpoint_key"},
        {KEYBIND_ASSIGNWHICH::CPT_PREV_CHECKPOINT_KEY,"cpt_prev_checkpoint_key"},
        {KEYBIND_ASSIGNWHICH::CPT_NEXT_CHECKPOINT_KEY, "cpt_next_checkpoint_key"},
        {KEYBIND_ASSIGNWHICH::CPT_FREEZE_BALL_KEY, "cpt_freeze_ball_key"},
        {KEYBIND_ASSIGNWHICH::CPT_MIRROR_STATE_KEY, "cpt_mirror_state_key"},
        {KEYBIND_ASSIGNWHICH::CPT_REWIND_KEY, "cpt_rewind_key"},
        {KEYBIND_ASSIGNWHICH::CPT_FASTFORWARD_KEY, "cpt_fastforward_key"},
    };

    std::vector<std::string> menu_names;

    void OnKeyAxisInput(ActorWrapper aw, void* params, std::string eventName);
    void OnKeyPressed(ActorWrapper aw, void* params, std::string eventName);
    void OpenMenuForKeybinding();
    void close_opened_menus();
    void reopen_closed_menus();
};
