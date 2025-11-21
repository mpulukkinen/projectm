/**
 * projectM -- Milkdrop-esque visualisation SDK
 * Copyright (C)2003-2021 projectM Team
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
 * main.cpp
 * Authors: Created by Mischa Spiegelmock on 6/3/15.
 *
 *
 *	RobertPancoast77@gmail.com :
 * experimental Stereoscopic SBS driver functionality
 * WASAPI loopback implementation
 *
 *
 */

#include "pmSDL.hpp"
#include <cstdlib>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// audio loader helper (implemented in audio_loader.cpp)
bool loadAudioFile(const std::string& path, SDL_AudioSpec& outSpec, Uint8** outBuf, Uint32* outLen);

static int mainLoop(void *userData) {
    projectMSDL **appRef = (projectMSDL **)userData;
    auto app = *appRef;

#if UNLOCK_FPS
    auto start = startUnlockedFPSCounter();
#endif

    // frame rate limiter
    int fps = app->fps();
    if (fps <= 0)
        fps = 60;
    const Uint32 frame_delay = 1000/fps;
    Uint32 last_time = SDL_GetTicks();

    // loop
    while (!app->done) {
        // render
        app->renderFrame();
        processLoopbackFrame(app);

#if UNLOCK_FPS
        advanceUnlockedFPSCounterFrame(start);
#else
        app->pollEvent();
        Uint32 elapsed = SDL_GetTicks() - last_time;
        if (elapsed < frame_delay)
            SDL_Delay(frame_delay - elapsed);
        last_time = SDL_GetTicks();
#endif
    }

    return 0;
}

int main(int argc, char *argv[]) {
    // Configure stdio for IPC before anything else
    #ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    #else
    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);
    #endif

    // Parse command-line arguments
    std::string presetDir;
    std::string audioFile;
    std::string outDir;
    size_t targetFps = 0;
    std::vector<std::pair<int,int>> resolutions;
    bool listPresets = false;

    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a == "--preset-dir" && i + 1 < argc) {
            presetDir = argv[++i];
        } else if (a == "--audio" && i + 1 < argc) {
            audioFile = argv[++i];
        } else if (a == "--out-dir" && i + 1 < argc) {
            outDir = argv[++i];
        } else if (a == "--fps" && i + 1 < argc) {
            targetFps = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (a == "--res" && i + 1 < argc) {
            // one or multiple resolutions separated by commas, e.g. 1280x720,1920x1080
            std::string r = argv[++i];
            size_t pos = 0;
            while (pos < r.size()) {
                size_t comma = r.find(',', pos);
                std::string token = (comma == std::string::npos) ? r.substr(pos) : r.substr(pos, comma - pos);
                size_t x = token.find('x');
                if (x != std::string::npos) {
                    int w = std::stoi(token.substr(0, x));
                    int h = std::stoi(token.substr(x + 1));
                    resolutions.emplace_back(w, h);
                }
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        } else if (a == "--list-presets") {
            listPresets = true;
        } else if (a == "--help" || a == "-h") {
            printf("Usage: %s [--preset-dir DIR] [--audio FILE] [--out-dir DIR] [--fps N] [--res WxH,...] [--list-presets]\n", argv[0]);
            printf("Supported audio formats: WAV, OGG (with SDL_mixer)\n");
            printf("Output format: JPEG\n");
            return 0;
        }
    }

    projectMSDL *app = setupSDLApp(presetDir);

    // List presets if requested (use project's playlist API)
    if (listPresets) {
        auto presets = app->listPresets();
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Found %zu presets:\n", presets.size());
        for (size_t i = 0; i < presets.size(); ++i) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "  %zu: %s", i, presets[i].c_str());
        }
    }

    // Load provided audio file (WAV/OGG/MP3 via SDL_mixer fallback)
    SDL_AudioSpec wavSpec;
    Uint8* wavBuf = nullptr;
    Uint32 wavLen = 0;
    if (!audioFile.empty()) {
        if (!loadAudioFile(audioFile, wavSpec, &wavBuf, &wavLen)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load audio file %s", audioFile.c_str());
        } else {
            app->configureCli(wavSpec, wavBuf, wavLen, outDir, targetFps, resolutions, audioFile);
            double seconds = 0.0;
            if (wavSpec.freq > 0 && wavSpec.channels > 0)
                seconds = (double)wavLen / (wavSpec.freq * wavSpec.channels * (SDL_AUDIO_BITSIZE(wavSpec.format)/8));
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loaded audio: %s (%.2f sec)", audioFile.c_str(), seconds);
        }
    }

    int status = mainLoop(&app);


    // cleanup
    SDL_GL_DeleteContext(app->_openGlContext);
#if !FAKE_AUDIO
    if (!app->wasapi) // not currently using WASAPI, so we need to endAudioCapture.
        app->endAudioCapture();
#endif
    // free any loaded audio buffer (was allocated by loadAudioFile)
    if (wavBuf) free(wavBuf);
    delete app;

    return status;
}


