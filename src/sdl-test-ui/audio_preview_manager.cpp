#include "audio_preview_manager.hpp"
#include <chrono>

AudioPreviewManager::AudioPreviewManager()
    : state(PlaybackState::STOPPED)
    , currentTimestampMs(0)
    , startTimestampMs(0)
    , playbackStartTimeMs(0)
    , totalDurationMs(0)
{
}

AudioPreviewManager::~AudioPreviewManager() {
    stopPreview();
}

void AudioPreviewManager::startPreview(uint64_t fromTimestampMs) {
    startTimestampMs = fromTimestampMs;
    currentTimestampMs = fromTimestampMs;
    playbackStartTime = std::chrono::system_clock::now();
    state = PlaybackState::PLAYING;
}

void AudioPreviewManager::stopPreview() {
    state = PlaybackState::STOPPED;
    currentTimestampMs = 0;
    startTimestampMs = 0;
}

void AudioPreviewManager::pausePreview() {
    if (state == PlaybackState::PLAYING) {
        state = PlaybackState::PAUSED;
    }
}

void AudioPreviewManager::resumePreview() {
    if (state == PlaybackState::PAUSED) {
        playbackStartTime = std::chrono::system_clock::now();
        state = PlaybackState::PLAYING;
    }
}

uint64_t AudioPreviewManager::getCurrentTimestamp() const {
    return currentTimestampMs.load();
}

AudioPreviewManager::PlaybackState AudioPreviewManager::getState() const {
    return state.load();
}

void AudioPreviewManager::setAudioFilePath(const std::string& filePath) {
    audioFilePath = filePath;
}

bool AudioPreviewManager::isPlaying() const {
    return state.load() == PlaybackState::PLAYING;
}

void AudioPreviewManager::seekToTimestamp(uint64_t timestampMs) {
    if (timestampMs <= totalDurationMs) {
        currentTimestampMs = timestampMs;
        startTimestampMs = timestampMs;
        playbackStartTime = std::chrono::system_clock::now();
    }
}

uint64_t AudioPreviewManager::getTotalDurationMs() const {
    return totalDurationMs;
}

void AudioPreviewManager::updateCurrentTimestamp(uint64_t timestampMs) {
    if (state == PlaybackState::PLAYING) {
        currentTimestampMs = timestampMs;

        // Safety check: stop if exceeded duration
        if (timestampMs >= totalDurationMs) {
            state = PlaybackState::STOPPED;
        }
    }
}
