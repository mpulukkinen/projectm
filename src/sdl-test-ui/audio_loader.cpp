#include <SDL.h>
#include <string>
#include <cstring>

// Simple audio loader using SDL_LoadWAV only (no SDL_mixer dependency).
// This supports WAV format only. For OGG/MP3 support, SDL_mixer would be needed.

bool loadAudioFile(const std::string& path, SDL_AudioSpec& outSpec, Uint8** outBuf, Uint32* outLen) {
    if (path.empty() || !outBuf || !outLen) return false;

    // Ensure outSpec is zero-initialized so fields like callback/userdata
    // are not left as uninitialized garbage. If callback is non-null
    // SDL_OpenAudioDevice may attempt to call it from the audio thread.
    outSpec = SDL_AudioSpec();
    outSpec.callback = nullptr;
    outSpec.userdata = nullptr;

    // Load WAV using SDL_LoadWAV
    SDL_AudioSpec wavSpec;
    Uint8* wavBuf = nullptr;
    Uint32 wavLen = 0;
    if (SDL_LoadWAV(path.c_str(), &wavSpec, &wavBuf, &wavLen)) {
        outSpec.freq = wavSpec.freq;
        outSpec.format = wavSpec.format;
        outSpec.channels = wavSpec.channels;
        outSpec.samples = wavSpec.samples;

        *outLen = wavLen;
        *outBuf = (Uint8*)malloc(wavLen);
        memcpy(*outBuf, wavBuf, wavLen);
        SDL_FreeWAV(wavBuf);
        return true;
    }

    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load audio file (WAV format supported only): %s", path.c_str());
    return false;
}
