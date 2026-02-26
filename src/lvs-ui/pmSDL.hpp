/**
* projectM -- Milkdrop-esque visualisation SDK
* Copyright (C)2003-2019 projectM Team
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
* See 'LICENSE.txt' included within this release
*
* projectM-sdl
* This is an implementation of projectM using libSDL2
*
* pmSDL.hpp
* Authors: Created by Mischa Spiegelmock on 2017-09-18.
*
*/

#pragma once

#include "opengl.h"
#include <SDL2/SDL.h>

// ----------------------------
#define TEST_ALL_PRESETS 0
#define STEREOSCOPIC_SBS 0

// projectM
#include <projectM-4/playlist.h>
#include <projectM-4/projectM.h>

// projectM SDL
#include "audioCapture.hpp"
#include "loopback.hpp"
#include "setup.hpp"

// IPC System for preset queue management
#include "ipc_manager.hpp"
#include "debug_ipc_ui.hpp"

#if defined _MSC_VER
#include <direct.h>
#endif

#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <sys/stat.h>

#ifdef WASAPI_LOOPBACK
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>

#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>

#include <mmsystem.h>
#include <stdio.h>


#define LOG(format, ...) wprintf(format L"\n", ##__VA_ARGS__)
#define ERR(format, ...) LOG(L"Error: " format, ##__VA_ARGS__)

#endif /** WASAPI_LOOPBACK */

#ifdef _WIN32
#define SDL_MAIN_HANDLED
#include "SDL.h"
#else
#include <SDL2/SDL.h>
#endif /** _WIN32 */
#include <vector>
#include <deque>
#include <atomic>
#include <thread>
#include <map>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>

#if defined(NDEBUG)
#define PM_ENABLE_PRESET_DIAGNOSTICS 0
#else
#define PM_ENABLE_PRESET_DIAGNOSTICS 1
#endif

// Tree node for hierarchical preset organization
struct PresetTreeNode {
    std::map<std::string, PresetTreeNode> folders; // nested folders
    std::vector<std::string> presets;              // preset filenames in this folder (no path)
    std::vector<size_t> preset_indices;            // playlist indices matching presets vector
};

// DATADIR_PATH should be set by the root Makefile if this is being
// built with autotools.
#ifndef DATADIR_PATH
#ifdef DEBUG
#define DATADIR_PATH "."
#ifndef _WIN32
#warning "DATADIR_PATH is not defined - falling back to ./"
#else
#pragma warning "DATADIR_PATH is not defined - falling back to ./"
#endif /** _WIN32 */
#else
#define DATADIR_PATH "/usr/local/share/projectM"
#ifndef _WIN32
#warning "DATADIR_PATH is not defined - falling back to /usr/local/share/projectM"
#endif /** _WIN32 */
#endif
#endif

class projectMSDL
{
public:
    projectMSDL(SDL_GLContext glCtx, const std::string& presetPath);

    ~projectMSDL();

    void init(SDL_Window* window);
    void stretchMonitors();
    void nextMonitor();
    void toggleFullScreen();
    void resize(unsigned int width, unsigned int height);
    void touch(float x, float y, int pressure, int touchtype = 0);
    void touchDrag(float x, float y, int pressure);
    void touchDestroy(float x, float y);
    void touchDestroyAll();
    void renderFrame();
    void presetClicked(size_t i);
    void addPresetToPlaylist(size_t presetIndex);
    void pollEvent();
    bool keymod = false;
    std::string getActivePresetName();
    projectm_handle projectM();
    void setFps(size_t fps);
    size_t fps() const;

    // configure CLI-supplied audio/render parameters
    void configureCli(const SDL_AudioSpec& audioSpec, Uint8* audioBuf, Uint32 audioLen,
                      const std::string& outDir, size_t renderFps, const std::vector<std::pair<int, int>>& resolutions,
                      const std::string& audioFile = "");

    // return a list of preset filenames discovered in the playlist
    std::vector<std::string> listPresets();

    // Build hierarchical tree from flat preset list (called in constructor)
    void buildPresetTree(const std::string& presetPath);

    // Preview audio (play) and feed projectM with PCM data for visualization
    // audioSpec/data: SDL spec and raw audio data (WAV loaded via SDL_LoadWAV)
    // startTimestampMs: start playback at this position in the audio (in milliseconds)
    void previewAudioAndFeed(const SDL_AudioSpec& audioSpec, const Uint8* audioBuf, Uint32 audioLen, uint32_t startTimestampMs = 0);

    // Render frames driven by provided audio buffer and write BMP sequence to outDir
    // resolutions: vector of pair(width,height)
    // fps: target frames per second
    void renderSequenceFromAudio(const SDL_AudioSpec& audioSpec, const Uint8* audioBuf, Uint32 audioLen,
                                 const std::string& outDir, size_t fps, const std::vector<std::pair<int, int>>& resolutions);

    bool done{false};
    bool mouseDown{false};
    bool stretch{false};   // used for toggling stretch mode

    SDL_GLContext _openGlContext{nullptr};

    void togglePreview(bool restart = false);

    // Reset preview clock (used when jumping to a time)
    void resetPreviewClock();

private:
    struct PresetCacheData {
        std::vector<std::string> preset_list;
        std::vector<std::string> preset_filename_display;
        std::vector<std::string> preset_filename_lower;
        std::unordered_map<std::string, size_t> preset_index_by_path_lower;
        std::unordered_map<std::string, size_t> preset_index_by_relpath_lower;
        std::unordered_map<std::string, size_t> preset_index_by_filename_lower;
        PresetTreeNode preset_tree;
    };

    std::chrono::steady_clock::time_point preview_start_time{};
    void focusTreeOnCurrentPreset();
    void focusTreeOnPresetPath(const std::string& fullPresetPath);
    void startPresetDiscovery();
    void applyDiscoveredPresetBatch();
    void applyPendingPresetCache();
    void buildPresetCacheFromListAsync(std::vector<std::string> presetListSnapshot, bool focusCurrentPreset);
    void refreshPresetCache(bool focusCurrentPreset = true);
    bool updatePresetFromQueue(uint64_t timestampMs, bool doTransition);
    static void presetSwitchedEvent(bool isHardCut, uint32_t index, void* context);
    static void presetSwitchFailedEvent(const char* presetFilename, const char* message, void* context);

    void UpdateWindowTitle();

    void scrollHandler(SDL_Event*);
    void keyHandler(SDL_Event*);

    void startRendering();
    bool pending_render_request{false};
#if PM_ENABLE_PRESET_DIAGNOSTICS
    void setupGLDebugOutput();
    void logGLErrors(const char* stage);
    void renderProjectMFrameWithDiagnostics();
#else
    void renderProjectMFrameWithDiagnostics();
#endif

    projectm_handle _projectM{nullptr};
    projectm_playlist_handle _playlist{nullptr};

    SDL_Window* _sdlWindow{nullptr};
    bool _isFullScreen{false};
    size_t _width{0};
    size_t _height{0};
    size_t _fps{60};

    bool _shuffle{false};

    std::string _presetName; //!< Current preset name
    // CLI-provided audio and render options (set by main)
    SDL_AudioSpec cli_audio_spec;
    Uint8* cli_audio_buf{nullptr};
    Uint32 cli_audio_len{0};
    std::string cli_out_dir;
    std::string cli_audio_file; //!< optional audio filename passed from CLI
    size_t cli_render_fps{0};
    std::vector<std::pair<int, int>> cli_resolutions;
    bool cli_has_audio{false};
    bool is_rendering{false};
    std::atomic<float> render_progress{0.0f}; // 0.0 .. 1.0
    std::thread render_thread; // optional thread handle if needed in future
    std::thread preset_cache_thread;
    std::thread preset_discovery_thread;
    std::mutex preset_cache_mutex;
    std::mutex preset_discovery_mutex;
    std::unique_ptr<PresetCacheData> pending_preset_cache{};
    std::deque<std::string> discovered_preset_paths{};
    std::vector<std::string> inserted_preset_paths{};
    bool pending_cache_focus_current_preset{false};
    std::atomic<bool> preset_cache_build_in_progress{false};
    std::atomic<bool> preset_cache_pending_apply{false};
    std::atomic<bool> preset_discovery_in_progress{false};
    std::atomic<bool> preset_discovery_finished{false};
    bool preset_discovery_started{false};
    bool preset_discovery_applied_once{false};
    std::atomic<size_t> discovered_preset_total{0};
    std::atomic<size_t> inserted_preset_total{0};
    bool is_previewing{false};
    bool show_ui{true};
    bool preset_lock{true};
    bool render_as_transparency{false};
    Uint16 preset_duration_sec{20};
    std::string preset_base_path;
    std::vector<std::string> preset_list{};
    std::vector<std::string> preset_filename_display{};
    std::vector<std::string> preset_filename_lower{};
    std::unordered_map<std::string, size_t> preset_index_by_path_lower{};
    std::unordered_map<std::string, size_t> preset_index_by_relpath_lower{};
    std::unordered_map<std::string, size_t> preset_index_by_filename_lower{};
    std::string cached_search_query{};
    std::vector<size_t> cached_search_matches{};
    std::string last_preset_load_error{};
    char preset_search[256] = { 0 };

    // Preset tree structure and navigation
    PresetTreeNode preset_tree;
    std::vector<PresetTreeNode*> tree_path; // breadcrumb navigation path through tree

    // IPC system for preset queue management
    std::unique_ptr<IPCManager> ipcManager{nullptr};

    std::atomic<uint32_t> preview_generation{0};
    uint64_t lastAppliedPresetTimestamp{0};
    uint64_t lastPreviewedPresetTimestamp{0};
    bool doPreviewTransition{false};
    bool isInitialPresetLoaded{false};
    bool auto_focus_tree_on_preset_change{false};
#if PM_ENABLE_PRESET_DIAGNOSTICS
    bool debug_preset_diagnostics{false};
    bool gl_debug_output_initialized{false};
    uint32_t diagnostic_frame_counter{0};
    uint32_t black_frame_streak{0};
#endif

};
