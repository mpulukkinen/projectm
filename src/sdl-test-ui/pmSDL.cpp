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
*
*
* experimental Stereoscopic SBS driver functionality by
*	RobertPancoast77@gmail.com
*/

#include "pmSDL.hpp"

#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstring>
#include <string>
#include "stb_image_write.h"
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

projectMSDL::projectMSDL(SDL_GLContext glCtx, const std::string& presetPath)
    : _openGlContext(glCtx)
    , _projectM(projectm_create_with_opengl_load_proc(&dispatchLoadProc, nullptr))
    , _playlist(projectm_playlist_create(_projectM))
{
    projectm_get_window_size(_projectM, &_width, &_height);
    projectm_playlist_set_preset_switched_event_callback(_playlist, &projectMSDL::presetSwitchedEvent, static_cast<void*>(this));
    projectm_playlist_add_path(_playlist, presetPath.c_str(), true, false);
    projectm_playlist_set_shuffle(_playlist, _shuffle);
    dumpOpenGLInfo();
    enableGLDebugOutput();
}

projectMSDL::~projectMSDL()
{
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
    SDL_Keymod sdl_mod = (SDL_Keymod) sdl_evt->key.keysym.mod;
    SDL_Keycode sdl_keycode = sdl_evt->key.keysym.sym;

    // Left or Right Gui or Left Ctrl
    if (sdl_mod & KMOD_LGUI || sdl_mod & KMOD_RGUI || sdl_mod & KMOD_LCTRL)
    {
        keymod = true;
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
                done = 1;
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

        case SDLK_SPACE:
            projectm_set_preset_locked(_projectM, !projectm_get_preset_locked(_projectM));
            UpdateWindowTitle();
            break;
        case SDLK_ESCAPE:
            // Stop any preview playback and allow cancelling a render in progress
            is_previewing = false;
            if (is_rendering) {
                is_rendering = false; // render loop will check this and abort
            }
            break;
        case SDLK_h:
            show_ui = !show_ui;
            break;

        case SDLK_F5:
            // Preview audio
            if(!is_previewing)
            {
                if (this->cli_has_audio && this->cli_audio_buf && this->cli_audio_len > 0) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Previewing audio (F5)\n");
                    is_previewing = true;
                    previewAudioAndFeed(this->cli_audio_spec, this->cli_audio_buf, this->cli_audio_len);
                } else {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "No audio provided for preview.\n");
                }
            }
            else
            {
                is_previewing = false;
            }
            break;

        case SDLK_F6:
            // Render sequence
            if(!is_rendering)
            {

                if (this->cli_has_audio && this->cli_audio_buf && this->cli_audio_len > 0 && !this->cli_out_dir.empty() && !this->cli_resolutions.empty()) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Starting render (F6) to %s\n", this->cli_out_dir.c_str());
                    is_rendering = true;
                    renderSequenceFromAudio(this->cli_audio_spec, this->cli_audio_buf, this->cli_audio_len, this->cli_out_dir, this->cli_render_fps ? this->cli_render_fps : this->fps(), this->cli_resolutions);
                } else {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Render parameters missing (audio/out-dir/resolutions).\n");
                }
            }
            break;
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
                }
                break;
            case SDL_KEYDOWN:
                keyHandler(&evt);
                break;

            case SDL_MOUSEBUTTONUP:
                mouseDown = false;
                break;

            case SDL_QUIT:
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

void projectMSDL::renderFrame()
{
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    projectm_opengl_render_frame(_projectM);

    if(!is_rendering && show_ui)
    {
        // Dear ImGui overlay
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame(_sdlWindow);
        ImGui::NewFrame();

        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGui::Begin("projectM Overlay", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::TextWrapped("Hotkeys:");
        ImGui::BulletText("Ctrl + Q: Quit");
        ImGui::BulletText("Ctrl/Cmd+S: Stretch monitors");
        ImGui::BulletText("Ctrl/Cmd+M: Change monitor");
        ImGui::BulletText("Ctrl/Cmd+F: Fullscreen");
        ImGui::BulletText("A: Toggle aspect correction");
        ImGui::BulletText("R: Random preset (next)");
        ImGui::BulletText("Y: Toggle shuffle");
        ImGui::BulletText("Left/Right: Prev/Next preset");
        ImGui::BulletText("Up/Down: Beat sensitivity +/- (%f)", projectm_get_beat_sensitivity(_projectM));
        ImGui::BulletText("Space: Lock/Unlock preset");
        ImGui::BulletText("F5: Preview audio");
        ImGui::BulletText("F6: Render sequence");
        ImGui::BulletText("H: Hide this menu");

        ImGui::Separator();
        ImGui::Text("Audio: %s", this->cli_audio_file.empty() ? "(none)" : this->cli_audio_file.c_str());
        ImGui::Separator();
        ImGui::Text("Presets:");
        auto presets = listPresets();
        size_t total = presets.size();
        const int columns = 3;
        if (total > 0) {
            // You can set a fixed width for the entire table.
            // For example, if you estimate a character is ~8 pixels wide, 100 chars would be ~800 pixels.
            int width = 600;
            int table_width = columns * width; // A rough estimation for width

            ImGui::PushItemWidth(table_width);

            // Using ImGuiTableFlags_SizingStretchSame ensures all columns are equal.
            if (ImGui::BeginTable("preset_table", columns, ImGuiTableFlags_SizingStretchSame)) {
                int rows = static_cast<int>((total + columns - 1) / columns);

                for (int r = 0; r < rows; ++r) {
                    ImGui::TableNextRow();
                    for (int c = 0; c < columns; ++c) {
                        ImGui::TableSetColumnIndex(c);
                        size_t idx = static_cast<size_t>(r) + static_cast<size_t>(c) * static_cast<size_t>(rows);

                        if (idx < total) {
                            // Clickable/selectable preset entry. Truncate long names for display.
                            std::string display = std::to_string(idx) + ": " + presets[idx];
                            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + width);
                            // Use a unique ID per item to avoid collisions in ImGui
                            ImGui::PushID(static_cast<int>(idx));
                            if (ImGui::Selectable(display.c_str(), false, 0, ImVec2((float)width, 0.0f))) {
                                // User clicked this preset: switch playlist position (hard cut)
                                projectm_playlist_set_position(_playlist, static_cast<uint32_t>(idx), true);
                                // Update window title to reflect newly selected preset
                                UpdateWindowTitle();
                            }
                            ImGui::PopID();
                            ImGui::PopTextWrapPos();
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::PopItemWidth();
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    }
    SDL_GL_SwapWindow(_sdlWindow);
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

#ifdef WASAPI_LOOPBACK
    wasapi = true;
#endif
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

    app->UpdateWindowTitle();
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

std::vector<std::string> projectMSDL::listPresets() {
    std::vector<std::string> out;
    if (!_playlist) return out;
    uint32_t size = projectm_playlist_size(_playlist);
    for (uint32_t i = 0; i < size; ++i) {
        char* p = projectm_playlist_item(_playlist, i);
        if (p) {
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
        }
    }
    return out;
}

void projectMSDL::UpdateWindowTitle()
{
    std::string title = "projectM ➫ " + _presetName;
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

void projectMSDL::previewAudioAndFeed(const SDL_AudioSpec& audioSpec, const Uint8* audioBuf, Uint32 audioLen) {
    // Spawn a thread to progressively feed audio to projectM and optionally play via SDL audio
    std::thread([this, audioSpec, audioBuf, audioLen]() {
        // Try to open an audio device for playback
        SDL_AudioSpec want = audioSpec;
        SDL_AudioSpec have;
        SDL_AudioDeviceID dev = 0;
        if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
            SDL_Init(SDL_INIT_AUDIO);
        }
        dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (dev != 0) {
            SDL_PauseAudioDevice(dev, 0);
            SDL_QueueAudio(dev, audioBuf, audioLen);
        }

        // Feed audio progressively per frame
        double seconds = (double)audioLen / (audioSpec.freq * audioSpec.channels * (SDL_AUDIO_BITSIZE(audioSpec.format)/8));
        size_t totalFrames = static_cast<size_t>(seconds * this->fps());
        if (totalFrames == 0) totalFrames = 1;

        // bytes per second
        Uint32 bytesPerSec = audioSpec.freq * audioSpec.channels * (SDL_AUDIO_BITSIZE(audioSpec.format)/8);
        double bytesPerFrame = (double)bytesPerSec / (double)this->fps();

        const Uint8* ptr = audioBuf;
        Uint32 remaining = audioLen;

        for (size_t f = 0; f < totalFrames && remaining > 0 && !done; ++f) {
            Uint32 take = static_cast<Uint32>(std::min<double>((double)remaining, bytesPerFrame));
            feedPCMToProjectM(this->_projectM, ptr, take, audioSpec);
            ptr += take;
            remaining -= take;
            SDL_Delay(static_cast<Uint32>(1000.0 / this->fps()));

            if(!is_previewing) {
                break; // stop if preview flag cleared
            }
        }

        // let playback finish
        if (dev != 0) {
            SDL_Delay(500);
            SDL_CloseAudioDevice(dev);
        }
    }).detach();
}

void projectMSDL::renderSequenceFromAudio(const SDL_AudioSpec& audioSpec, const Uint8* audioBuf, Uint32 audioLen,
                                 const std::string& outDir, size_t fps, const std::vector<std::pair<int,int>>& resolutions) {
    if (!std::filesystem::exists(outDir)) {
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
    }

    // Compute total frames from audio length
    double seconds = (double)audioLen / (audioSpec.freq * audioSpec.channels * (SDL_AUDIO_BITSIZE(audioSpec.format)/8));
    size_t totalFrames = static_cast<size_t>(seconds * fps);
    if (totalFrames == 0) totalFrames = 1;

    Uint32 bytesPerSec = audioSpec.freq * audioSpec.channels * (SDL_AUDIO_BITSIZE(audioSpec.format)/8);
    double bytesPerFrame = (double)bytesPerSec / (double)fps;

    const Uint8* ptr = audioBuf;
    Uint32 remaining = audioLen;

    // Save current window/render state so we can restore it when finished or cancelled
    int saved_w = static_cast<int>(_width);
    int saved_h = static_cast<int>(_height);
    bool saved_fullscreen = _isFullScreen;
    bool saved_stretch = this->stretch;

    // For each frame, feed audio slice and render for each resolution
    for (size_t frameIndex = 0; frameIndex < totalFrames && !done; ++frameIndex) {
        if(!is_rendering)
        {
            break;
        }
        Uint32 take = static_cast<Uint32>(std::min<double>((double)remaining, bytesPerFrame));
        if (take > 0) {
            feedPCMToProjectM(this->_projectM, ptr, take, audioSpec);
            ptr += take;
            remaining -= take;
        }

        for (const auto &res : resolutions) {
            int w = res.first;
            int h = res.second;
            // resize window/render target
            this->resize(w, h);
            // render
            glViewport(0,0,w,h);
            glClearColor(0,0,0,0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            projectm_opengl_render_frame(_projectM);

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
            snprintf(filename, sizeof(filename), "%s/frame_%dx%d_%06zu.jpg", outDir.c_str(), w, h, frameIndex);
            int quality = 90;
            int comp = 3;
            if (!stbi_write_jpg(filename, w, h, comp, flipped.data(), quality)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to write JPEG: %s", filename);
            }
        }
        // update progress (fraction of frames completed)
        if (totalFrames > 0) {
            this->render_progress.store(static_cast<float>(frameIndex + 1) / static_cast<float>(totalFrames));
        }

        // Poll events while rendering so user can close / interact
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            ImGui_ImplSDL2_ProcessEvent(&evt);
            switch (evt.type) {
                case SDL_QUIT:
                    done = true;
                    break;
                case SDL_KEYDOWN:
                    // allow key handling (e.g., abort with ESC)
                    keyHandler(&evt);
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
            ImGui::SetNextWindowPos(ImVec2(static_cast<float>(_width) * 0.5f, static_cast<float>(_height) * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("Rendering", nullptr, flags);
            float prog = this->render_progress.load();
            ImGui::Text("Rendering frames: %zu / %zu", frameIndex + 1, totalFrames);
            ImGui::ProgressBar(prog, ImVec2(400.0f, 0.0f));
            ImGui::Text("Press ESC to cancel");
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(_sdlWindow);
        }

        if (done) break;
    }

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
}
