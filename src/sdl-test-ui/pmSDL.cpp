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
* pmSDL.cpp
* Authors: Created by Mischa Spiegelmock on 2017-09-18.
* Modified by Matti Pulkkinen, 2025-2026
*
*
* experimental Stereoscopic SBS driver functionality by
*	RobertPancoast77@gmail.com
*/

#include "pmSDL.hpp"

#include <vector>
#include <thread>

#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>
#include <limits>
#include "../../../vendor/SOIL2/src/SOIL2/stb_image_write.h"
#include "text_render.h"
// Dear ImGui
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl2.h"

namespace {
auto dispatchLoadProc(const char* name, void* userData) -> void*
{
    // Dispatch load proc to SDL
    return SDL_GL_GetProcAddress(name);
}
} // namespace

// Helper to reset the preview clock when jumping to a time
static void resetPreviewClock() {
    preview_clock_initialized = false;
}

#if defined(_WIN32)
#define PMGL_APIENTRY APIENTRY
#else
#define PMGL_APIENTRY
#endif

#if PM_ENABLE_PRESET_DIAGNOSTICS
static void PMGL_APIENTRY projectMGLDebugCallback(GLenum source,
                                                  GLenum type,
                                                  GLuint id,
                                                  GLenum severity,
                                                  GLsizei length,
                                                  const GLchar* message,
                                                  const void* userParam)
{
    (void)source;
    (void)type;
    (void)id;
    (void)severity;
    (void)length;
    (void)userParam;
    SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "[GL Debug] %s", message ? message : "<null>");
}
#endif

projectMSDL::projectMSDL(SDL_GLContext glCtx, const std::string& presetPath)
    : _openGlContext(glCtx)
    , _projectM(projectm_create_with_opengl_load_proc(&dispatchLoadProc, nullptr))
    , _playlist(projectm_playlist_create(_projectM))
    , preset_base_path(presetPath)
{
    projectm_get_window_size(_projectM, &_width, &_height);
    projectm_playlist_set_preset_switched_event_callback(_playlist, &projectMSDL::presetSwitchedEvent, static_cast<void*>(this));
    projectm_playlist_set_preset_switch_failed_event_callback(_playlist, &projectMSDL::presetSwitchFailedEvent, static_cast<void*>(this));
    projectm_playlist_add_path(_playlist, presetPath.c_str(), true, false);
    projectm_playlist_set_shuffle(_playlist, _shuffle);
    dumpOpenGLInfo();
    enableGLDebugOutput();
}

projectMSDL::~projectMSDL()
{
    // Shutdown IPC manager
    if (ipcManager) {
        try {
            ipcManager->shutdown();
        } catch (const std::exception& e) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error shutting down IPC manager: %s\n", e.what());
        }
        ipcManager.reset();
    }

    // Shutdown ImGui if initialized
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    projectm_playlist_destroy(_playlist);
    _playlist = nullptr;
    projectm_destroy(_projectM);
    _projectM = nullptr;
}

/* Stretch projectM across multiple monitors */
void projectMSDL::stretchMonitors()
{
    int displayCount = SDL_GetNumVideoDisplays();
    if (displayCount >= 2)
    {
        std::vector<SDL_Rect> displayBounds;
        for (int i = 0; i < displayCount; i++)
        {
            displayBounds.push_back(SDL_Rect());
            SDL_GetDisplayBounds(i, &displayBounds.back());
        }

        int mostXLeft = 0;
        int mostXRight = 0;
        int mostYUp = 0;
        int mostYDown = 0;

        for (int i = 0; i < displayCount; i++)
        {
            if (displayBounds[i].x < mostXLeft)
            {
                mostXLeft = displayBounds[i].x;
            }
            if ((displayBounds[i].x + displayBounds[i].w) > mostXRight)
            {
                mostXRight = displayBounds[i].x + displayBounds[i].w;
            }
        }
        for (int i = 0; i < displayCount; i++)
        {
            if (displayBounds[i].y < mostYUp)
            {
                mostYUp = displayBounds[i].y;
            }
            if ((displayBounds[i].y + displayBounds[i].h) > mostYDown)
            {
                mostYDown = displayBounds[i].y + displayBounds[i].h;
            }
        }

        int mostWide = abs(mostXLeft) + abs(mostXRight);
        int mostHigh = abs(mostYUp) + abs(mostYDown);

        SDL_SetWindowPosition(_sdlWindow, mostXLeft, mostYUp);
        SDL_SetWindowSize(_sdlWindow, mostWide, mostHigh);
    }
}

/* Moves projectM to the next monitor */
void projectMSDL::nextMonitor()
{
    int displayCount = SDL_GetNumVideoDisplays();
    int currentWindowIndex = SDL_GetWindowDisplayIndex(_sdlWindow);
    if (displayCount >= 2)
    {
        std::vector<SDL_Rect> displayBounds;
        int nextWindow = currentWindowIndex + 1;
        if (nextWindow >= displayCount)
        {
            nextWindow = 0;
        }

        for (int i = 0; i < displayCount; i++)
        {
            displayBounds.push_back(SDL_Rect());
            SDL_GetDisplayBounds(i, &displayBounds.back());
        }
        SDL_SetWindowPosition(_sdlWindow, displayBounds[nextWindow].x, displayBounds[nextWindow].y);
        SDL_SetWindowSize(_sdlWindow, displayBounds[nextWindow].w, displayBounds[nextWindow].h);
    }
}

void projectMSDL::toggleFullScreen()
{
    if (_isFullScreen)
    {
        SDL_SetWindowFullscreen(_sdlWindow, 0);
        _isFullScreen = false;
        SDL_ShowCursor(true);
    }
    else
    {
        SDL_ShowCursor(false);
        SDL_SetWindowFullscreen(_sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
        _isFullScreen = true;
    }
}

void projectMSDL::scrollHandler(SDL_Event* sdl_evt)
{
    // handle mouse scroll wheel - up++
    if (sdl_evt->wheel.y > 0)
    {
        projectm_playlist_play_previous(_playlist, true);
    }
    // handle mouse scroll wheel - down--
    if (sdl_evt->wheel.y < 0)
    {
        projectm_playlist_play_next(_playlist, true);
    }
}

void projectMSDL::keyHandler(SDL_Event* sdl_evt)
{
    // Block hotkeys if ImGui wants keyboard input (e.g., search box focused)
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }

    SDL_Keymod sdl_mod = (SDL_Keymod) sdl_evt->key.keysym.mod;
    SDL_Keycode sdl_keycode = sdl_evt->key.keysym.sym;

    // Left or Right Gui or Left Ctrl
    if (sdl_mod & KMOD_LGUI || sdl_mod & KMOD_RGUI || sdl_mod & KMOD_LCTRL)
    {
        keymod = true;
    }

    if(is_rendering)
    {
        if(SDLK_ESCAPE == sdl_keycode)
        {
            // Allow cancelling a render in progress
            is_rendering = false;
            is_previewing = false;
            return;
        }
        // Only allow cancel / quit
        if(sdl_keycode == SDLK_q)
        {
            if (sdl_mod & KMOD_LGUI || sdl_mod & KMOD_RGUI || sdl_mod & KMOD_LCTRL)
            {
                // cmd/ctrl-q = quit
                is_rendering = false;
                is_previewing = false;
                done = true;
                return;
            }
        }
        return;//
    }

    // handle keyboard input (for our app first, then projectM)
    switch (sdl_keycode)
    {
        case SDLK_a:
            projectm_set_aspect_correction(_projectM, !projectm_get_aspect_correction(_projectM));
            break;

        case SDLK_q:
            if (sdl_mod & KMOD_LGUI || sdl_mod & KMOD_RGUI || sdl_mod & KMOD_LCTRL)
            {
                // cmd/ctrl-q = quit
                is_rendering = false;
                is_previewing = false;
                done = true;
                return;
            }
            break;

        case SDLK_s:
            if (sdl_mod & KMOD_LGUI || sdl_mod & KMOD_RGUI || sdl_mod & KMOD_LCTRL)
            {
                // command-s: [s]tretch monitors
                // Stereo requires fullscreen
#if !STEREOSCOPIC_SBS
                if (!this->stretch)
                { // if stretching is not already enabled, enable it.
                    stretchMonitors();
                    this->stretch = true;
                }
                else
                {
                    toggleFullScreen(); // else, just toggle full screen so we leave stretch mode.
                    this->stretch = false;
                }
#endif
                return; // handled
            }

        case SDLK_m:
            if (sdl_mod & KMOD_LGUI || sdl_mod & KMOD_RGUI || sdl_mod & KMOD_LCTRL)
            {
                // command-m: change [m]onitor
                // Stereo requires fullscreen
#if !STEREOSCOPIC_SBS
                nextMonitor();
#endif
                this->stretch = false; // if we are switching monitors, ensure we disable monitor stretching.
                return;                // handled
            }

        case SDLK_f:
            if (sdl_mod & KMOD_LGUI || sdl_mod & KMOD_RGUI || sdl_mod & KMOD_LCTRL)
            {
                // command-f: fullscreen
                // Stereo requires fullscreen
#if !STEREOSCOPIC_SBS
                toggleFullScreen();
#endif
                this->stretch = false; // if we are toggling fullscreen, ensure we disable monitor stretching.
                return;                // handled
            }
            break;

        case SDLK_r:
            // Use playlist shuffle to randomize.
            projectm_playlist_set_shuffle(_playlist, true);
            projectm_playlist_play_next(_playlist, true);
            projectm_playlist_set_shuffle(_playlist, _shuffle);
            break;

        case SDLK_y:
            _shuffle = !_shuffle;
            projectm_playlist_set_shuffle(_playlist, _shuffle);
            break;

        case SDLK_LEFT:
            projectm_playlist_play_previous(_playlist, true);
            break;

        case SDLK_RIGHT:
            projectm_playlist_play_next(_playlist, true);
            break;

        case SDLK_UP:
            projectm_set_beat_sensitivity(_projectM, projectm_get_beat_sensitivity(_projectM) + 0.01f);
            break;

        case SDLK_DOWN:
            projectm_set_beat_sensitivity(_projectM, projectm_get_beat_sensitivity(_projectM) - 0.01f);
            break;

        case SDLK_KP_PLUS:
        case SDLK_PLUS:
            preset_duration_sec++;
            projectm_set_preset_duration(_projectM, preset_duration_sec);
            break;
        case SDLK_KP_MINUS:
        case SDLK_MINUS:
            if(preset_duration_sec > 1)
            {
                preset_duration_sec--;
                projectm_set_preset_duration(_projectM, preset_duration_sec);
            }
            break;

        case SDLK_SPACE:
            preset_lock = !preset_lock;
            projectm_set_preset_locked(_projectM, preset_lock);
            UpdateWindowTitle();
            break;
        case SDLK_ESCAPE:
            // Stop any preview playback and allow cancelling a render in progress
            is_previewing = false;
            is_rendering = false; // render loop will check this and abort
            break;
        case SDLK_h:
            show_ui = !show_ui;
            break;
        case SDLK_t:
            // Preview audio
            render_as_transparency = !render_as_transparency;
            break;
        case SDLK_F5:
            // Preview audio
            togglePreview();
            break;

        case SDLK_F6:
            // Render sequence
            startRendering();
            break;

#if PM_ENABLE_PRESET_DIAGNOSTICS
        case SDLK_F9:
            debug_preset_diagnostics = !debug_preset_diagnostics;
            if (debug_preset_diagnostics) {
                setupGLDebugOutput();
            }
            black_frame_streak = 0;
            diagnostic_frame_counter = 0;
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Preset diagnostics: %s", debug_preset_diagnostics ? "ON" : "OFF");
            break;
#endif
    }
}

void projectMSDL::startRendering()
{
    if (!is_rendering)
    {
        is_previewing = false; // stop preview if running
        if (ipcManager) {
            // Keep "Render" behavior consistent with pressing "Save playlist".
            ipcManager->sendCurrentState();
            ipcManager->pendingStateUpdate = true;
        }
        projectm_set_preset_locked(_projectM, preset_lock);
        if (this->cli_has_audio && this->cli_audio_buf && this->cli_audio_len > 0 && !this->cli_out_dir.empty() && !this->cli_resolutions.empty())
        {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Starting render (F6) to %s\n", this->cli_out_dir.c_str());
            is_rendering = true;
            renderSequenceFromAudio(this->cli_audio_spec, this->cli_audio_buf, this->cli_audio_len, this->cli_out_dir,
                this->cli_render_fps ? this->cli_render_fps : this->fps(), this->cli_resolutions);
        }
        else
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Render parameters missing (audio/out-dir/resolutions).\n");
        }
    }
}

void projectMSDL::togglePreview(bool restart)
{
    if (restart)
    {
        is_previewing = false;
    }
    if (!is_previewing)
    {
        doPreviewTransition = false;
        preview_clock_initialized = false; // reset clock on new preview start
        if (this->cli_has_audio && this->cli_audio_buf && this->cli_audio_len > 0)
        {
            // Use last received timestamp from IPC for preview start
            // getLastReceivedTimestamp() returns relative position within session
            // Add sessionStartOffsetMs to get absolute position in audio
            uint32_t startTimestampMs = ipcManager ? static_cast<uint32_t>(ipcManager->getLastReceivedTimestamp()) : 0;
            if (ipcManager) {
                startTimestampMs += static_cast<uint32_t>(ipcManager->getSessionStartOffsetMs());
            }
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Previewing audio (F5) from absolute offset %u ms (session offset %llu ms + relative %llu ms)\n",
                startTimestampMs,
                ipcManager ? ipcManager->getSessionStartOffsetMs() : 0,
                ipcManager ? ipcManager->getLastReceivedTimestamp() : 0);
            is_previewing = true;
            previewAudioAndFeed(this->cli_audio_spec, this->cli_audio_buf, this->cli_audio_len, startTimestampMs);
        }
        else
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "No audio provided for preview.\n");
        }
    }
    else
    {
        is_previewing = false;
    }
}


void projectMSDL::resize(unsigned int width_, unsigned int height_)
{
    _width = width_;
    _height = height_;

    // Hide cursor if window size equals desktop size
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0)
    {
        SDL_ShowCursor(_isFullScreen ? SDL_DISABLE : SDL_ENABLE);
    }

    projectm_set_window_size(_projectM, _width, _height);
}

void projectMSDL::resetPreviewClock()
{
    ::resetPreviewClock();
}

void projectMSDL::pollEvent()
{
    SDL_Event evt;

    int mousex = 0;
    float mousexscale = 0;
    int mousey = 0;
    float mouseyscale = 0;
    int mousepressure = 0;
    while (SDL_PollEvent(&evt))
    {
        // Forward events to ImGui
        ImGui_ImplSDL2_ProcessEvent(&evt);

        switch (evt.type)
        {
            case SDL_WINDOWEVENT:
                int h, w;
                SDL_GL_GetDrawableSize(_sdlWindow, &w, &h);
                switch (evt.window.event)
                {
                    case SDL_WINDOWEVENT_RESIZED:
                        resize(w, h);
                        break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        resize(w, h);
                        break;
                    case SDL_WINDOWEVENT_CLOSE:
                        done = true;
                        is_rendering = false;
                        is_previewing = false;
                        break;
                }
                break;
            case SDL_KEYDOWN:
                keyHandler(&evt);
                break;

            case SDL_MOUSEBUTTONUP:
                mouseDown = false;
                break;

            case SDL_QUIT:
                is_previewing = false;
                is_rendering = false;
                done = true;
                break;
        }
    }

    // Handle dragging your waveform when mouse is down.
    if (mouseDown)
    {
        // Get mouse coordinates when you click.
        SDL_GetMouseState(&mousex, &mousey);
        // Scale those coordinates. libProjectM supports a scale of 0.1 instead of absolute pixel coordinates.
        mousexscale = (mousex / (float) _width);
        mouseyscale = ((_height - mousey) / (float) _height);
        // Drag Touch.
        touchDrag(mousexscale, mouseyscale, mousepressure);
    }
}

// This touches the screen to generate a waveform at X / Y.
void projectMSDL::touch(float x, float y, int pressure, int touchtype)
{
#ifdef PROJECTM_TOUCH_ENABLED
    projectm_touch(_projectM, x, y, pressure, static_cast<projectm_touch_type>(touchtype));
#endif
}

// This moves the X Y of your existing waveform that was generated by a touch (only if you held down your click and dragged your mouse around).
void projectMSDL::touchDrag(float x, float y, int pressure)
{
    projectm_touch_drag(_projectM, x, y, pressure);
}

// Remove waveform at X Y
void projectMSDL::touchDestroy(float x, float y)
{
    projectm_touch_destroy(_projectM, x, y);
}

// Remove all waveforms
void projectMSDL::touchDestroyAll()
{
    projectm_touch_destroy_all(_projectM);
}

void projectMSDL::updatePresetFromQueue(uint64_t timestampMs, bool doTransition) {
    if (!ipcManager) return;

    auto& presetQueue = ipcManager->getPresetQueue();
    // Shift timestamp by sessionStartOffsetMs so preset schedule aligns with session offset
    auto activeEntry = presetQueue.getActivePresetEntry(timestampMs);

    // If no active preset (e.g. before first timestamp), do nothing
    if (activeEntry.presetName.empty()) return;

    // Only switch if this specific scheduled item hasn't been applied yet
    // This prevents constant re-triggering of the same preset
    if (activeEntry.startTimestampMs != this->lastAppliedPresetTimestamp || timestampMs == 0) {
        std::string activePreset = activeEntry.presetName;

        // Find preset index in playlist with robust path matching
        // We do this search every time we need to switch, which is infrequent
        for (size_t i = 0; i < preset_list.size(); ++i) {
            std::string path = preset_list[i];

            // Extract filename from both paths for comparison
            size_t sep = path.find_last_of("/\\");
            std::string pathFilename = (sep != std::string::npos) ? path.substr(sep + 1) : path;

            size_t activeSep = activePreset.find_last_of("/\\");
            std::string activeFilename = (activeSep != std::string::npos) ? activePreset.substr(activeSep + 1) : activePreset;

            // First try exact filename match
            bool isMatch = (pathFilename == activeFilename);

            // If not matched by filename, try path suffix matching
            // This handles cases where activePreset is "folder/preset.milk" and path is "/full/path/to/folder/preset.milk"
            if (!isMatch && activePreset.find_last_of("/\\") != std::string::npos) {
                // Normalize paths: convert backslashes to forward slashes for consistent comparison
                std::string normalizedPath = path;
                std::string normalizedActive = activePreset;
                std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
                std::replace(normalizedActive.begin(), normalizedActive.end(), '\\', '/');

                // Check if the path ends with the active preset (relative path match)
                if (normalizedPath.length() >= normalizedActive.length()) {
                    size_t pos = normalizedPath.length() - normalizedActive.length();
                    isMatch = (normalizedPath.substr(pos) == normalizedActive);
                }
            }

            // Also try reverse: check if active preset is a suffix that matches full path
            if (!isMatch && path.find_last_of("/\\") != std::string::npos) {
                std::string normalizedPath = path;
                std::string normalizedActive = activePreset;
                std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
                std::replace(normalizedActive.begin(), normalizedActive.end(), '\\', '/');

                if (normalizedActive.length() >= normalizedPath.length()) {
                    size_t pos = normalizedActive.length() - normalizedPath.length();
                    isMatch = (normalizedActive.substr(pos) == normalizedPath);
                }
            }

            if (isMatch) {
                // Found it! Switch.
                projectm_playlist_set_position(_playlist, static_cast<uint32_t>(i), !doTransition);

                // Lock it so projectM doesn't auto-switch away immediately
                projectm_set_preset_locked(_projectM, true);

                // Update our state
                this->lastAppliedPresetTimestamp = activeEntry.startTimestampMs;
                UpdateWindowTitle();
                break;
            }
        }
    }
}

void projectMSDL::renderFrame()
{
    if (ipcManager && ipcManager->pendingStateUpdate) {
        ipcManager->sendCurrentState();
        ipcManager->pendingStateUpdate = false;
    }

    if (ipcManager && ipcManager->needsFirstPresetAutoLoad) {
        auto queuedPresets = ipcManager->getPresetQueue().getAllPresets();
        if (!queuedPresets.empty()) {
            // Force a one-time switch to the first queued preset.
            this->lastAppliedPresetTimestamp = std::numeric_limits<uint64_t>::max();
            updatePresetFromQueue(queuedPresets.front().startTimestampMs, false);
            isInitialPresetLoaded = true;
        }
        ipcManager->needsFirstPresetAutoLoad = false;
    }

    if (!is_rendering && show_ui)
    {
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if(is_previewing)
        {
            if (!preview_clock_initialized) {
                this->preview_start_time = std::chrono::steady_clock::now();
                preview_clock_initialized = true;
            }
        }

        // Update IPC manager with current playback timestamp and check for preset changes
        if (ipcManager) {
            // Check if we need to reset the preview clock (e.g., when jumping to a time)
            if (ipcManager->needsPreviewClockReset) {
                resetPreviewClock();
                ipcManager->needsPreviewClockReset = false;
            }

            // Check if timestamp was changed externally (e.g., by "Jump to time" button)
            bool timestampChanged = (lastPreviewedPresetTimestamp != ipcManager->getLastReceivedTimestamp());

            if (is_previewing) {
                try {
                    // Get current playback timestamp (in milliseconds from start of audio)
                    // Using a monotonic clock to track preview time


                    auto current_time = std::chrono::steady_clock::now();
                    auto elapsed = current_time - this->preview_start_time;
                    uint64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

                    // getLastReceivedTimestamp() returns position within session
                    // Add session offset to get absolute position in audio
                    uint64_t initialTimestamp = this->ipcManager->getLastReceivedTimestamp();
                    if (this->ipcManager->getSessionStartOffsetMs() > 0) {
                        initialTimestamp += this->ipcManager->getSessionStartOffsetMs();
                    }
                    elapsed_ms += initialTimestamp;

                    // Update audio preview timestamp

                    // Check if there's an active preset at the current timestamp
                    updatePresetFromQueue(static_cast<uint64_t>(elapsed_ms), doPreviewTransition);
                    doPreviewTransition = true;

                    // Update lastPreviewedPresetTimestamp when timestamp changed externally
                    if (timestampChanged) {
                        lastPreviewedPresetTimestamp = ipcManager->getLastReceivedTimestamp();
                    }
                } catch (const std::exception& e) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "IPC preview update error: %s\n", e.what());
                }
            }
            else if (timestampChanged || !isInitialPresetLoaded)
            {
                // Update preset if IPC timestamp changed
                updatePresetFromQueue(ipcManager->getLastReceivedTimestamp(), true);
                lastPreviewedPresetTimestamp = ipcManager->getLastReceivedTimestamp();
                isInitialPresetLoaded = true;
            }
        }

        renderProjectMFrameWithDiagnostics();
        // Dear ImGui overlay
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame(_sdlWindow);
        ImGui::NewFrame();

#ifdef DEBUG
        DebugIPCUI::render(ipcManager.get(), this);
#endif


        ImGui::SetNextWindowBgAlpha(0.65f);
        // position overlay at top-left corner
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::Begin("projectM Overlay", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::TextWrapped("Hotkeys:");
        ImGui::BulletText("Ctrl + Q: Quit");
        ImGui::BulletText("Ctrl/Cmd+S: Stretch monitors");
        ImGui::BulletText("Ctrl/Cmd+M: Change monitor");
        ImGui::BulletText("Ctrl/Cmd+F: Fullscreen");
        ImGui::BulletText("A: Toggle aspect correction");
        ImGui::BulletText("R: Random preset (next)");

        // ShuffleText
        std::string shuffleText = _shuffle ? "On" : "Off";
        std::string lockText = preset_lock ? "Locked" : "Unlocked";

        ImGui::BulletText("Y: Toggle shuffle (%s)", shuffleText.c_str());
        ImGui::BulletText("Left/Right: Prev/Next preset");
        ImGui::BulletText("Up/Down: Beat sensitivity +/- (%f)", projectm_get_beat_sensitivity(_projectM));

        if (!preset_lock)
        {
            ImGui::BulletText("+/-: Preset duration before transition (s) (%d)", preset_duration_sec);
        }

        ImGui::BulletText("Space: Lock/Unlock preset (%s)", lockText.c_str());

        std::string transparency = render_as_transparency ? "transparent" : "black";
        ImGui::BulletText("T: Rendering background: %s", transparency.c_str());
        ImGui::BulletText("F5: Preview audio");
        ImGui::BulletText("F6: Render sequence");
 #if PM_ENABLE_PRESET_DIAGNOSTICS
        ImGui::BulletText("F9: Preset diagnostics (%s)", debug_preset_diagnostics ? "On" : "Off");
        ImGui::BulletText("H: Hide this menu");
        ImGui::Checkbox("Debug Preset Diagnostics", &debug_preset_diagnostics);
#else
        ImGui::BulletText("H: Hide this menu");
#endif

        ImGui::Separator();

        // Show current playback time (timeMs) and session offset
        uint64_t currentTimeMs = 0;
        uint64_t sessionLength = ipcManager ? ipcManager->getSessionLengthMs() : 0;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - this->preview_start_time;
        uint64_t elapsedSincePreviewStart = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        currentTimeMs = ipcManager->getLastReceivedTimestamp() + (is_previewing ? elapsedSincePreviewStart : 0);
        ImGui::Text("Playback position: %llu ms", currentTimeMs);

        uint64_t sessionOffset = ipcManager ? ipcManager->getSessionStartOffsetMs() : 0;
        ImGui::Text("Session offset: %llu ms", sessionOffset);
        if (sessionLength > 0) {
            ImGui::Text("Session length: %llu ms", sessionLength);
        }

        if(ImGui::Button("Preview audio"))
        {
            togglePreview();
        }

        ImGui::SameLine();

        if (ImGui::Button("Save playlist"))
        {
            ipcManager->sendCurrentState();
        }

        ImGui::SameLine();

        if(ImGui::Button("Render sequence"))
        {
            // defer start until after ImGui frame ends to avoid nested NewFrame() calls
            this->pending_render_request = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Exit"))
        {
            done = true;
        }

        ImGui::Separator();
        ImGui::Text("Audio: %s", this->cli_audio_file.empty() ? "(none)" : this->cli_audio_file.c_str());

        ImGui::SameLine();

        ImGui::Text("Time: %dms", this->ipcManager->getLastReceivedTimestamp());

        ImGui::Separator();
        ImGui::Text("Presets:");

        // Search box for presets. When non-empty, show filtered flat list instead of the tree.
        float colWidth = 400;
        ImGui::PushItemWidth(colWidth);
        if (ImGui::InputText("Search Presets", this->preset_search, IM_ARRAYSIZE(this->preset_search))) {
            // user typed - nothing else required here; filtering occurs below
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Search")) {
            this->preset_search[0] = '\0';
            auto_focus_tree_on_preset_change = true;
            focusTreeOnCurrentPreset();
        }
        ImGui::PopItemWidth();

        // Trim search string whitespace for checking
        std::string search_term(this->preset_search);
        auto ltrim = [](std::string &s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        };
        auto rtrim = [](std::string &s) {
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
        };
        ltrim(search_term);
        rtrim(search_term);

        // If there's a search term, render a filtered flat list of presets
        if (!search_term.empty()) {
            // case-insensitive search
            std::string query = search_term;
            std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c){ return std::tolower(c); });

            // collect matching indices and display names
            std::vector<size_t> matches;
            std::vector<std::string> match_names;
            for (size_t i = 0; i < preset_list.size(); ++i) {
                std::string full = preset_list[i];
                size_t last_sep = full.find_last_of("/\\");
                std::string fname = (last_sep != std::string::npos) ? full.substr(last_sep + 1) : full;
                std::string fname_l = fname;
                std::transform(fname_l.begin(), fname_l.end(), fname_l.begin(), [](unsigned char c){ return std::tolower(c); });
                if (fname_l.find(query) != std::string::npos) {
                    matches.push_back(i);
                    match_names.push_back(fname);
                }
            }

            // Display results in the same multi-column layout
            if (!matches.empty()) {
                int columns = std::min(4, (static_cast<int>(matches.size()) + 19) / 20);
                if (ImGui::BeginTable("preset_search_table", columns, ImGuiTableFlags_SizingStretchSame)) {
                    int rows = (static_cast<int>(matches.size()) + columns - 1) / columns;
                    for (int r = 0; r < rows; ++r) {
                        ImGui::TableNextRow();
                        for (int c = 0; c < columns; ++c) {
                            ImGui::TableSetColumnIndex(c);
                            int idx = r + c * rows;
                            if (idx < static_cast<int>(matches.size())) {
                                size_t presetIndex = matches[idx];
                                const std::string& name = match_names[idx];
                                ImGui::PushID(static_cast<int>(presetIndex));

                                // Determine selection state
                                uint32_t current_pos = projectm_playlist_get_position(_playlist);
                                bool is_selected = (current_pos == presetIndex);

                                if (ImGui::Selectable(name.c_str(), is_selected, 0, ImVec2(colWidth, 0))) {
                                    presetClicked(presetIndex);
                                }
                                ImGui::PopID();
                            }
                        }
                    }
                    ImGui::EndTable();
                }
            } else {
                ImGui::TextDisabled("No presets match '%s'", search_term.c_str());
            }
        } else if (!tree_path.empty()) {
            PresetTreeNode* current_node = tree_path.back();

            // Show breadcrumb navigation
            if (tree_path.size() > 1) {
                if (ImGui::Button("< Back")) {
                    tree_path.pop_back();
                }
            }

            // Display current folder's subfolders and presets in multi-column format
            // Calculate items per column (max 20 items)
            int max_items_per_col = 20;
            int folder_count = static_cast<int>(current_node->folders.size());
            int preset_count = static_cast<int>(current_node->presets.size());
            int total_items = folder_count + preset_count;

            if (total_items > 0) {
                int columns = (total_items + max_items_per_col - 1) / max_items_per_col;
                columns = std::min(columns, 4); // Cap at 4 columns

                if (ImGui::BeginTable("preset_tree_table", columns, ImGuiTableFlags_SizingStretchSame)) {

                    // Create sorted list of items: folders first, then presets
                    std::vector<std::pair<std::string, bool>> items; // (name, is_folder)
                    for (const auto& folder_pair : current_node->folders) {
                        items.push_back({folder_pair.first, true});
                    }
                    for (const auto& preset : current_node->presets) {
                        items.push_back({preset, false});
                    }

                    // Calculate rows and render in column-major order
                    int rows = (total_items + columns - 1) / columns;
                    for (int r = 0; r < rows; ++r) {
                        ImGui::TableNextRow();
                        for (int c = 0; c < columns; ++c) {
                            ImGui::TableSetColumnIndex(c);
                            int idx = r + c * rows;

                            if (idx < static_cast<int>(items.size())) {
                                const auto& item = items[idx];
                                bool is_folder = item.second;

                                if (is_folder) {
                                    // Render folder as clickable button
                                    if (ImGui::Button(("->" + item.first).c_str(), ImVec2(colWidth, 0))) {
                                        // Navigate into this folder
                                        if (current_node->folders.find(item.first) != current_node->folders.end()) {
                                            tree_path.push_back(&current_node->folders[item.first]);
                                        }
                                    }
                                } else {
                                    // Render preset as selectable
                                    std::string display = " - " + item.first;
                                    ImGui::PushID(idx);

                                    // Find this preset in the flat list to check if it's selected
                                    // (This is a simplified check - ideally we'd track tree-aware selection)
                                    uint32_t current_pos = projectm_playlist_get_position(_playlist);
                                    bool is_selected = false;
                                    if (current_pos < preset_list.size()) {
                                        // Compare with current preset path
                                        auto current_preset = projectm_playlist_item(_playlist, current_pos);
                                        if (current_preset) {
                                            std::string current_name = current_preset;
                                            // Extract filename from full path
                                            size_t last_sep = current_name.find_last_of("/\\");
                                            if (last_sep != std::string::npos) {
                                                current_name = current_name.substr(last_sep + 1);
                                            }
                                            is_selected = (current_name == item.first);
                                            projectm_playlist_free_string(current_preset);
                                        }
                                    }

                                    if (ImGui::Selectable(display.c_str(), is_selected, 0, ImVec2(colWidth, 0))) {
                                        // Find and select this preset in the playlist
                                        // Search through preset_list for matching filename
                                        for (size_t i = 0; i < preset_list.size(); ++i) {
                                            std::string path = preset_list[i];
                                            size_t last_sep = path.find_last_of("/\\");
                                            std::string filename = (last_sep != std::string::npos) ? path.substr(last_sep + 1) : path;
                                            if (filename == item.first) {
                                                presetClicked(i);
                                                break;
                                            }
                                        }
                                    }
                                    ImGui::PopID();
                                }
                            }
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();

        // Preset Queue Window
        if (ipcManager) {
            ipcManager->getPresetQueue().renderUI();
        }

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(_sdlWindow);

        // If a render was requested via the UI, start it now that ImGui frame has finished
        if (this->pending_render_request) {
            this->pending_render_request = false;
            startRendering();
        }
    }
    else if(!is_rendering)
    {
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderProjectMFrameWithDiagnostics();
         // Dear ImGui overlay
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame(_sdlWindow);
        ImGui::NewFrame();

        ImGui::SetNextWindowBgAlpha(0.65f);
        // position overlay at top-left corner
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::Begin("projectM Overlay", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if(ImGui::Button("Show Menu (H)"))
        {
            show_ui = true;
        }

        if(ImGui::Button("F5: Preview audio"))
        {
            togglePreview();
        }

        if(ImGui::Button("F6: Render sequence"))
        {
            this->pending_render_request = true;
        }

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(_sdlWindow);

        // If a render was requested via the UI in this simpler overlay, start it now
        if (this->pending_render_request) {
            this->pending_render_request = false;
            startRendering();
        }
    }
}

void projectMSDL::presetClicked(size_t i)
{
    if (i >= preset_list.size()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Ignored preset click: index %zu out of range (%zu)\n", i, preset_list.size());
        return;
    }

    auto_focus_tree_on_preset_change = true;
    projectm_playlist_set_position(_playlist, static_cast<uint32_t>(i), true);
    projectm_set_preset_locked(_projectM, preset_lock);
    UpdateWindowTitle();

    // Add to preset queue
    if (ipcManager)
    {
        uint64_t timestamp = ipcManager->getLastReceivedTimestamp();
        ipcManager->getPresetQueue().addPreset(preset_list[i], timestamp);
    }
}

void projectMSDL::init(SDL_Window* window)
{
    _sdlWindow = window;
    projectm_set_window_size(_projectM, _width, _height);

    // Initialize Dear ImGui for SDL + OpenGL2
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(_sdlWindow, _openGlContext);
    ImGui_ImplOpenGL2_Init();
}

std::string projectMSDL::getActivePresetName()
{
    unsigned int index = projectm_playlist_get_position(_playlist);
    if (index)
    {
        auto presetName = projectm_playlist_item(_playlist, index);
        std::string presetNameString(presetName);
        projectm_playlist_free_string(presetName);
        return presetNameString;
    }
    return {};
}

void projectMSDL::presetSwitchedEvent(bool isHardCut, unsigned int index, void* context)
{
    auto app = reinterpret_cast<projectMSDL*>(context);
    auto presetName = projectm_playlist_item(app->_playlist, index);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Displaying preset: %s\n", presetName);

    app->_presetName = presetName;
    projectm_playlist_free_string(presetName);

    // Full cache rebuild is expensive for large libraries, so only do it if playlist size changed.
    // Otherwise, just keep UI folder focus aligned to the active preset.
    if (app->preset_list.size() != static_cast<size_t>(projectm_playlist_size(app->_playlist))) {
        app->refreshPresetCache(app->auto_focus_tree_on_preset_change);
    } else if (app->auto_focus_tree_on_preset_change) {
        app->focusTreeOnCurrentPreset();
    }
    app->UpdateWindowTitle();
}

void projectMSDL::renderProjectMFrameWithDiagnostics()
{
#if PM_ENABLE_PRESET_DIAGNOSTICS
    if (debug_preset_diagnostics) {
        setupGLDebugOutput();
        logGLErrors("before projectm_opengl_render_frame");
    }

    projectm_opengl_render_frame(_projectM);

    if (!debug_preset_diagnostics || render_as_transparency) {
        return;
    }

    logGLErrors("after projectm_opengl_render_frame");

    // Sample every few frames to keep overhead low.
    diagnostic_frame_counter++;
    if ((diagnostic_frame_counter % 5u) != 0u) {
        return;
    }

    const GLint samplePoints[5][2] = {
        {static_cast<GLint>(_width / 2), static_cast<GLint>(_height / 2)},
        {static_cast<GLint>(_width / 4), static_cast<GLint>(_height / 4)},
        {static_cast<GLint>((_width * 3) / 4), static_cast<GLint>(_height / 4)},
        {static_cast<GLint>(_width / 4), static_cast<GLint>((_height * 3) / 4)},
        {static_cast<GLint>((_width * 3) / 4), static_cast<GLint>((_height * 3) / 4)}
    };

    int darkSamples = 0;
    for (const auto& p : samplePoints) {
        GLubyte rgba[4] = {0, 0, 0, 0};
        glReadPixels(p[0], p[1], 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        if (rgba[0] < 8 && rgba[1] < 8 && rgba[2] < 8) {
            darkSamples++;
        }
    }

    if (darkSamples >= 4) {
        black_frame_streak++;
    } else {
        black_frame_streak = 0;
    }

    if (black_frame_streak == 20) {
        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
        uint32_t currentPos = projectm_playlist_get_position(_playlist);
        char* currentPreset = projectm_playlist_item(_playlist, currentPos);

        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                    "Diagnostics: mostly-black output streak detected. preset='%s' path='%s' pos=%u vendor='%s' gl='%s' glsl='%s'",
                    _presetName.c_str(),
                    currentPreset ? currentPreset : "<null>",
                    currentPos,
                    vendor ? vendor : "<null>",
                    version ? version : "<null>",
                    glsl ? glsl : "<null>");

        if (currentPreset) {
            projectm_playlist_free_string(currentPreset);
        }
    }
#else
    projectm_opengl_render_frame(_projectM);
#endif
}

#if PM_ENABLE_PRESET_DIAGNOSTICS
void projectMSDL::setupGLDebugOutput()
{
    if (gl_debug_output_initialized) {
        return;
    }

#if defined(GL_DEBUG_OUTPUT)
    if (glDebugMessageCallback != nullptr) {
        glEnable(GL_DEBUG_OUTPUT);
#if defined(GL_DEBUG_OUTPUT_SYNCHRONOUS)
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
        glDebugMessageCallback(projectMGLDebugCallback, nullptr);
        gl_debug_output_initialized = true;
        SDL_LogInfo(SDL_LOG_CATEGORY_RENDER, "OpenGL debug callback enabled");
    }
#endif
}

void projectMSDL::logGLErrors(const char* stage)
{
    GLenum err = glGetError();
    while (err != GL_NO_ERROR) {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "[GL Error][%s] 0x%x", stage ? stage : "unknown", err);
        err = glGetError();
    }
}
#endif

void projectMSDL::presetSwitchFailedEvent(const char* presetFilename, const char* message, void* context)
{
    auto app = reinterpret_cast<projectMSDL*>(context);
    if (!app) {
        return;
    }

    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Preset load failed and was removed from playlist: %s | reason: %s\n",
                presetFilename ? presetFilename : "<unknown>",
                message ? message : "<no details>");

    app->refreshPresetCache(app->auto_focus_tree_on_preset_change);
}

projectm_handle projectMSDL::projectM()
{
    return _projectM;
}

void projectMSDL::setFps(size_t fps)
{
    _fps = fps;
}

size_t projectMSDL::fps() const
{
    return _fps;
}

void projectMSDL::configureCli(const SDL_AudioSpec& audioSpec, Uint8* audioBuf, Uint32 audioLen,
                               const std::string& outDir, size_t renderFps, const std::vector<std::pair<int,int>>& resolutions,
                               const std::string& audioFile) {
    cli_audio_spec = audioSpec;
    cli_audio_buf = audioBuf;
    cli_audio_len = audioLen;
    cli_out_dir = outDir;
    cli_render_fps = renderFps;
    cli_resolutions = resolutions;
    cli_has_audio = (audioBuf != nullptr && audioLen > 0);
    // store provided audio filename (may be empty)
    // add member cli_audio_file to class if not present (it's safe to add to header)
    this->cli_out_dir = outDir;
    // we will store the audio filename in a new member; if header isn't updated this is no-op
    try {
        // store by looking up the symbol - simple approach: add member in header (done)
    } catch(...) {}
    // set audio file string via new member if available
    // (we rely on hdr change that adds cli_audio_file)
    // attempt to assign
    // NOTE: if header wasn't updated this line will still compile because header changed above
    this->cli_out_dir = outDir; // keep existing behavior
    // store filename in a member created in header; assign via qualified name
    // the actual member is `cli_audio_file`
#ifdef _MSC_VER
    // MSVC: assign directly
    this->cli_audio_file = audioFile;
#else
    this->cli_audio_file = audioFile;
#endif
}

void projectMSDL::buildPresetTree(const std::string& presetPath) {
    // Clear previous tree
    preset_tree.folders.clear();
    preset_tree.presets.clear();

    // Parse full paths from preset_list and build hierarchical tree
    for (const auto& fullPath : preset_list) {
        // Compute a path relative to the provided presetPath base.
        // Use std::filesystem::relative when possible, fall back to
        // stripping the prefix if relative() throws or isn't appropriate.
        std::string relPath = fullPath;
        try {
            std::filesystem::path fp(fullPath);
            std::filesystem::path base(presetPath);
            std::filesystem::path rp = std::filesystem::relative(fp, base);
            relPath = rp.generic_string();
        } catch (...) {
            // Fallback: if fullPath starts with presetPath, strip that prefix
            if (!presetPath.empty() && fullPath.size() >= presetPath.size() &&
                fullPath.compare(0, presetPath.size(), presetPath) == 0) {
                relPath = fullPath.substr(presetPath.size());
            }
        }

        // Trim any leading separators left after stripping base
        while (!relPath.empty() && (relPath.front() == '\\' || relPath.front() == '/')) {
            relPath.erase(0, 1);
        }

        // Split the relative path by separators (\ or /)
        std::vector<std::string> parts;
        std::string current;
        for (char c : relPath) {
            if (c == '\\' || c == '/') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) parts.push_back(current);

        // If no parts, skip
        if (parts.empty()) continue;

        // Last part is the filename (preset), rest are folders
        std::string filename = parts.back();
        parts.pop_back();

        // Navigate/create folder hierarchy and add preset to leaf
        PresetTreeNode* current_node = &preset_tree;
        for (const auto& folder : parts) {
            if (current_node->folders.find(folder) == current_node->folders.end()) {
                current_node->folders[folder] = PresetTreeNode();
            }
            current_node = &current_node->folders[folder];
        }

        // Add preset filename to current node
        current_node->presets.push_back(filename);
    }
}

void projectMSDL::focusTreeOnPresetPath(const std::string& fullPresetPath)
{
    tree_path.clear();
    tree_path.push_back(&preset_tree);

    if (fullPresetPath.empty()) {
        return;
    }

    std::string relPath = fullPresetPath;
    try {
        std::filesystem::path fp(fullPresetPath);
        std::filesystem::path base(preset_base_path);
        std::filesystem::path rp = std::filesystem::relative(fp, base);
        relPath = rp.generic_string();
    } catch (...) {
        if (!preset_base_path.empty() && fullPresetPath.size() >= preset_base_path.size() &&
            fullPresetPath.compare(0, preset_base_path.size(), preset_base_path) == 0) {
            relPath = fullPresetPath.substr(preset_base_path.size());
        }
    }

    while (!relPath.empty() && (relPath.front() == '\\' || relPath.front() == '/')) {
        relPath.erase(0, 1);
    }

    std::vector<std::string> parts;
    std::string current;
    for (char c : relPath) {
        if (c == '\\' || c == '/') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }

    if (parts.size() <= 1) {
        return;
    }

    parts.pop_back(); // remove filename, keep only folder chain

    PresetTreeNode* node = &preset_tree;
    for (const auto& folder : parts) {
        auto it = node->folders.find(folder);
        if (it == node->folders.end()) {
            break;
        }
        node = &it->second;
        tree_path.push_back(node);
    }
}

void projectMSDL::focusTreeOnCurrentPreset()
{
    if (!_playlist) {
        tree_path.clear();
        tree_path.push_back(&preset_tree);
        return;
    }

    uint32_t currentPos = projectm_playlist_get_position(_playlist);
    char* currentPreset = projectm_playlist_item(_playlist, currentPos);
    if (!currentPreset) {
        tree_path.clear();
        tree_path.push_back(&preset_tree);
        return;
    }

    std::string fullPath(currentPreset);
    projectm_playlist_free_string(currentPreset);
    focusTreeOnPresetPath(fullPath);
}

void projectMSDL::refreshPresetCache(bool focusCurrentPreset)
{
    preset_list = listPresets();
    buildPresetTree(preset_base_path);
    if (focusCurrentPreset) {
        focusTreeOnCurrentPreset();
    } else {
        tree_path.clear();
        tree_path.push_back(&preset_tree);
    }
}

std::vector<std::string> projectMSDL::listPresets() {
    std::vector<std::string> out;
    if (!_playlist) return out;
    uint32_t size = projectm_playlist_size(_playlist);
    for (uint32_t i = 0; i < size; ++i) {
        char* p = projectm_playlist_item(_playlist, i);
        if (!p) {
            continue;
        }

        std::string fullPath(p);
        out.emplace_back(fullPath);
        projectm_playlist_free_string(p);

        // This is no longer needed, but keep this still here
        /*if (p) {
            std::string fullPath(p);
            // Find the last occurrence of a path separator
            size_t last_separator = fullPath.find_last_of("/\\");
            if (last_separator != std::string::npos) {
                // Extract the filename part
                out.emplace_back(fullPath.substr(last_separator + 1));
            } else {
                // No separator found, the whole string is the filename
                out.emplace_back(fullPath);
            }
            projectm_playlist_free_string(p);
        }*/
    }
    return out;
}

void projectMSDL::UpdateWindowTitle()
{
    std::string title = "Lyric Video Studio - Milkdrop Visualizer ➫ " + _presetName;
    if (projectm_get_preset_locked(_projectM))
    {
        title.append(" [locked]");
    }
    SDL_SetWindowTitle(_sdlWindow, title.c_str());
}

// Helper: feed a chunk of PCM (interleaved) to projectM as int16 samples
static void feedPCMToProjectM(projectm_handle projectM, const Uint8* buf, Uint32 len, const SDL_AudioSpec& spec) {
    if (!buf || len == 0) return;

    int channels = spec.channels;
    SDL_AudioFormat fmt = spec.format;

    // Only handle S16 and F32 here. Convert as necessary.
    if (fmt == AUDIO_S16SYS) {
        // len is bytes; samplesPerChannel = len / (2*channels)
        int16_t* samples = (int16_t*)buf;
        size_t samplesPerChannel = len / (2 * channels);
        // projectm expects interleaved int16 with sample count (per channel)
        projectm_pcm_add_int16(projectM, samples, static_cast<int>(samplesPerChannel), channels == 2 ? PROJECTM_STEREO : PROJECTM_MONO);
    } else if (fmt == AUDIO_F32SYS) {
        // convert float to int16
        const float* f = (const float*)buf;
        size_t frames = len / (4 * channels);
        std::vector<int16_t> tmp(frames * channels);
        for (size_t i = 0; i < frames * channels; ++i) {
            float v = f[i];
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            tmp[i] = static_cast<int16_t>(v * 32767.0f);
        }
        projectm_pcm_add_int16(projectM, tmp.data(), static_cast<int>(frames), channels == 2 ? PROJECTM_STEREO : PROJECTM_MONO);
    } else if (fmt == AUDIO_U8) {
        // unsigned 8-bit, convert to signed int16
        size_t frames = len / channels;
        std::vector<int16_t> tmp(frames * channels);
        for (size_t i = 0; i < frames * channels; ++i) {
            tmp[i] = static_cast<int16_t>(((int)buf[i] - 128) << 8);
        }
        projectm_pcm_add_int16(projectM, tmp.data(), static_cast<int>(frames), channels == 2 ? PROJECTM_STEREO : PROJECTM_MONO);
    } else {
        // Unsupported format
    }
}

void projectMSDL::previewAudioAndFeed(const SDL_AudioSpec& audioSpec, const Uint8* audioBuf, Uint32 audioLen, uint32_t startTimestampMs) {
    // Increment generation counter to invalidate any previous threads
    uint32_t current_gen = ++preview_generation;

    // Spawn a thread to progressively feed audio to projectM and optionally play via SDL audio
    std::thread([this, audioSpec, audioBuf, audioLen, startTimestampMs, current_gen]() {
        // Calculate byte position from timestamp
        Uint32 bytesPerSec = audioSpec.freq * audioSpec.channels * (SDL_AUDIO_BITSIZE(audioSpec.format)/8);
        Uint32 startByteOffset = (bytesPerSec * (startTimestampMs / 1000));
        // Clamp to valid range
        if (startByteOffset >= audioLen) {
            startByteOffset = 0;
        }
        // If sessionLengthMs is set, clamp preview to that length
        Uint32 previewLen = audioLen - startByteOffset;
        if (ipcManager && ipcManager->getSessionLengthMs() > 0) {
            Uint32 maxLen = static_cast<Uint32>((bytesPerSec * ipcManager->getSessionLengthMs()) / 1000);
            if (maxLen < previewLen) previewLen = maxLen;
        }
        // Only open audio device if not already playing system audio
        SDL_AudioSpec want = audioSpec;
        SDL_AudioSpec have;
        SDL_AudioDeviceID dev = 0;
        if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
            SDL_Init(SDL_INIT_AUDIO);
        }
        dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (dev != 0) {
            SDL_PauseAudioDevice(dev, 0);
            SDL_QueueAudio(dev, audioBuf + startByteOffset, previewLen);
        }
        // Feed audio progressively per frame starting from the seek position
        double totalSeconds = (double)previewLen / bytesPerSec;
        double startSeconds = (double)startByteOffset / bytesPerSec;
        double remainingSeconds = totalSeconds;
        size_t totalFrames = static_cast<size_t>(remainingSeconds * this->fps());
        if (totalFrames == 0) totalFrames = 1;
        double bytesPerFrame = (double)bytesPerSec / (double)this->fps();
        const Uint8* ptr = audioBuf + startByteOffset;
        Uint32 remaining = previewLen;

        for (size_t f = 0; f < totalFrames && remaining > 0 && is_previewing; ++f) {
            // Check if a new preview request has superseded this one
            if (this->preview_generation.load() != current_gen || !is_previewing || done) {
                break;
            }

            Uint32 take = static_cast<Uint32>(std::min<double>((double)remaining, bytesPerFrame));
            feedPCMToProjectM(this->_projectM, ptr, take, audioSpec);
            ptr += take;
            remaining -= take;
            SDL_Delay(static_cast<Uint32>(1000.0 / this->fps()));
        }

        // Close playback device
        if (dev != 0) {
            SDL_Delay(500);
            SDL_CloseAudioDevice(dev);
        }
    }).detach();
}

// ============================================================================
// 1. HELPER: A "Resolve" Buffer (Put this class somewhere in your header or above the function)
// ============================================================================
struct ResolveBuffer {
    GLuint fbo = 0;
    GLuint color_texture = 0;
    GLuint depth_texture = 0;
    int width = 0;
    int height = 0;

    void resize(int w, int h) {
        if (width == w && height == h) return;
        width = w; height = h;

        if (fbo == 0) glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Color attachment (RGB, not sRGB to avoid format issues)
        if (color_texture == 0) glGenTextures(1, &color_texture);
        glBindTexture(GL_TEXTURE_2D, color_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);

        // Depth attachment (needed for proper projectM rendering)
        if (depth_texture == 0) glGenTextures(1, &depth_texture);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);

        // Verify completeness
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "FBO incomplete: 0x%x", status);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    ~ResolveBuffer() {
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (color_texture) glDeleteTextures(1, &color_texture);
        if (depth_texture) glDeleteTextures(1, &depth_texture);
    }
};

// Make sure this persists between frames (static or member variable)
static ResolveBuffer resolveFBO;

void projectMSDL::renderSequenceFromAudio(const SDL_AudioSpec& audioSpec, const Uint8* audioBuf, Uint32 audioLen,
                                 const std::string& outDir, size_t fps, const std::vector<std::pair<int,int>>& resolutions) {
    if (!std::filesystem::exists(outDir)) {
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
    }

    glClearColor(0.0f, 0.0f, 0.0f, render_as_transparency ? 0.0f : 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Compute total frames from audio length and adjust audio pointer and length to start from sessionStartOffsetMs
    Uint32 bytesPerSec = audioSpec.freq * audioSpec.channels * (SDL_AUDIO_BITSIZE(audioSpec.format)/8);
    Uint32 startByteOffset = 0;
    if (ipcManager && ipcManager->getSessionStartOffsetMs() > 0) {
        startByteOffset = static_cast<Uint32>((bytesPerSec * ipcManager->getSessionStartOffsetMs()) / 1000);
        if (startByteOffset >= audioLen) {
            startByteOffset = 0; // fallback to start if offset is out of range
        }
    }
    Uint32 renderLen = audioLen - startByteOffset;
    if (ipcManager && ipcManager->getSessionLengthMs() > 0) {
        Uint32 maxLen = static_cast<Uint32>((bytesPerSec * ipcManager->getSessionLengthMs()) / 1000);
        if (maxLen < renderLen) renderLen = maxLen;
    }
    double seconds = (double)renderLen / bytesPerSec;
    size_t totalFrames = static_cast<size_t>(seconds * fps);
    if (totalFrames == 0) totalFrames = 1;
    double bytesPerFrame = (double)bytesPerSec / (double)fps;
    const Uint8* ptr = audioBuf + startByteOffset;
    Uint32 remaining = renderLen;

    // Save current window/render state so we can restore it when finished or cancelled
    int saved_w = static_cast<int>(_width);
    int saved_h = static_cast<int>(_height);
    bool saved_fullscreen = _isFullScreen;
    bool saved_stretch = this->stretch;

    static std::vector<unsigned char> gammaLUT;
    if (gammaLUT.empty()) {
        gammaLUT.resize(256);
        for (int i = 0; i < 256; ++i) {
            // Convert Linear to sRGB (Gamma 2.2 approximation)
            float v = std::pow(i / 255.0f, 1.0f / 2.2f) * 255.0f;
            gammaLUT[i] = (unsigned char)std::min(255.0f, std::max(0.0f, v));
        }
    }

    int w = resolutions.at(0).first;
    int h = resolutions.at(0).second;

    resolveFBO.resize(w, h);

    // resize window/render target
    this->resize(w, h);

    glDisable(GL_SCISSOR_TEST);

    // Initialize FBO: clear it to black before first render (frame 0)
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO.fbo);
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Reset last applied preset timestamp for rendering
    this->lastAppliedPresetTimestamp = 0;

    bool doTransition = false;
    //glEnable(GL_FRAMEBUFFER_SRGB);
    // For each frame, feed audio slice and render for each resolution
    for (size_t frameIndex = 0; frameIndex < totalFrames && is_rendering; ++frameIndex) {

        Uint32 take = static_cast<Uint32>(std::min<double>((double)remaining, bytesPerFrame));
        if (take > 0) {
            feedPCMToProjectM(this->_projectM, ptr, take, audioSpec);
            ptr += take;
            remaining -= take;
        }

        // Handle preset switching based on timestamp
        if (ipcManager) {
            // Calculate current timestamp in ms (relative to session start)
            double current_time_sec = (double)frameIndex / (double)fps;
            uint64_t current_time_ms = (uint64_t)(current_time_sec * 1000.0);
            uint64_t session_time_ms = ipcManager->getSessionStartOffsetMs() + current_time_ms;
            // If sessionLength is set and we've reached the end, stop rendering
            if (ipcManager->getSessionLengthMs() > 0 && current_time_ms >= ipcManager->getSessionLengthMs()) {
                SDL_Log("Render: session length reached (%llu ms), stopping.", ipcManager->getSessionLengthMs());
                break;
            }

            updatePresetFromQueue(session_time_ms, doTransition);
            doTransition = true;
        }

        // CRITICAL: Bind the 4K FBO BEFORE rendering so projectM renders at full resolution
        glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO.fbo);
        glViewport(0, 0, w, h);
        // NO CLEAR — preserve afterglow/history from previous frame
        projectm_opengl_render_frame_fbo(_projectM, resolveFBO.fbo);

        // ensure previous GL ops are finished and pack alignment is 1
        glFinish();
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        // Read from the FBO we just rendered into
        glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFBO.fbo);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        if (render_as_transparency)
        {
            // read pixels as RGBA (with alpha channel for transparency)
            std::vector<unsigned char> pixels(w * h * 4);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            // 3. Post-Process: Generate Alpha from Brightness (Luma Keying)
            // Iterate through every pixel
            for (size_t i = 0; i < pixels.size(); i += 4) {
                unsigned char r = pixels[i];
                unsigned char g = pixels[i + 1];
                unsigned char b = pixels[i + 2];

                // Calculate brightness.
                // Simple method: Use the maximum value of R, G, or B.
                // This ensures that if a pixel is pure Red (255,0,0), it is fully opaque.
                // If a pixel is Black (0,0,0), Alpha becomes 0 (Transparent).
                unsigned char brightness = std::max({r, g, b});

                // Overwrite the Alpha channel (pixels[i+3]) with the calculated brightness
                pixels[i + 3] = brightness;
            }

            // flip vertically because GL origin is bottom-left
            std::vector<unsigned char> flipped(w * h * 4);
            for (int y = 0; y < h; ++y) {
                memcpy(&flipped[(h - 1 - y) * w * 4], &pixels[y * w * 4], w * 4);
            }

            // write PNG using stb_image_write (preserve alpha)
            char filename[1024];
            snprintf(filename, sizeof(filename), "%s/%09zu.png", outDir.c_str(), frameIndex);
            int comp = 4;  // RGBA
            if (!stbi_write_png(filename, w, h, comp, flipped.data(), w * comp)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to write PNG: %s", filename);
            }
        }
        else
        {
            // read pixels
            std::vector<unsigned char> pixels(w * h * 3);
            glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            // flip vertically because GL origin is bottom-left
            std::vector<unsigned char> flipped(w * h * 3);
            for (int y = 0; y < h; ++y) {
                memcpy(&flipped[(h - 1 - y) * w * 3], &pixels[y * w * 3], w * 3);
            }

            // write JPEG using stb_image_write
            char filename[1024];
            snprintf(filename, sizeof(filename), "%s/%09zu.jpg", outDir.c_str(), frameIndex);
            int quality = 100;
            int comp = 3;
            if (!stbi_write_jpg(filename, w, h, comp, flipped.data(), quality)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to write JPEG: %s", filename);
            }
        }

        // Preview: Blit 4K FBO to window for live preview during render
        glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFBO.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Window
        glViewport(0, 0, _width, _height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Blit 4K -> window (with scaling if window is smaller)
        glBlitFramebuffer(0, 0, w, h, 0, 0, static_cast<int>(_width), static_cast<int>(_height), GL_COLOR_BUFFER_BIT, GL_LINEAR);

        glBindFramebuffer(GL_FRAMEBUFFER, 0); // Back to Window

        // update progress (fraction of frames completed)
        if (totalFrames > 0) {
            this->render_progress.store(static_cast<float>(frameIndex + 1) / static_cast<float>(totalFrames));
        }

        // Poll events while rendering so user can close / interact
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            ImGui_ImplSDL2_ProcessEvent(&evt);
            switch (evt.type) {
                case SDLK_ESCAPE:
                case SDL_QUIT:
                    is_rendering = false;
                    break;
                default:
                    break;
            }
        }
        // Render a simple centered ImGui progress overlay so user sees progress
        {
            ImGui_ImplOpenGL2_NewFrame();
            ImGui_ImplSDL2_NewFrame(_sdlWindow);
            ImGui::NewFrame();

            ImGuiIO& io = ImGui::GetIO(); (void)io;
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always, ImVec2(0, 0));
            ImGui::Begin("Rendering", nullptr, flags);
            float prog = this->render_progress.load();
            ImGui::Text("Rendering frames: %zu / %zu", frameIndex + 1, totalFrames);
            ImGui::ProgressBar(prog, ImVec2(400.0f, 0.0f));
            if(ImGui::Button("Cancel"))
            {
                is_rendering = false;
            }
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(_sdlWindow);
        }

        if (!is_rendering) break;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // finished (or cancelled) - restore window/render state
    if (_isFullScreen != saved_fullscreen) {
        // toggle back to previous fullscreen state
        toggleFullScreen();
    }
    // restore saved size
    this->resize(static_cast<unsigned int>(saved_w), static_cast<unsigned int>(saved_h));
    this->stretch = saved_stretch;

    // final progress and state
    this->render_progress.store(is_rendering ? 1.0f : 0.0f);
    is_rendering = false;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Render complete. %zu frames written to %s", totalFrames, outDir.c_str());
    done = true;
    exit(0); // exit after rendering in CLI mode
}
