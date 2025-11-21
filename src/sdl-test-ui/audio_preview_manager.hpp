#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <atomic>

/**
 * Manages audio preview playback with timestamp synchronization
 * Handles seeking to specific timestamps and syncing with preset queue
 */

class AudioPreviewManager {
public:
    enum class PlaybackState {
        STOPPED,
        PLAYING,
        PAUSED
    };

    AudioPreviewManager();
    ~AudioPreviewManager();

    // Start audio preview from specified timestamp
    void startPreview(uint64_t fromTimestampMs);

    // Stop audio preview
    void stopPreview();

    // Pause audio preview
    void pausePreview();

    // Resume audio preview
    void resumePreview();

    // Get current playback timestamp
    uint64_t getCurrentTimestamp() const;

    // Get playback state
    PlaybackState getState() const;

    // Set audio file path for preview
    void setAudioFilePath(const std::string& filePath);

    // Check if audio is currently playing
    bool isPlaying() const;

    // Seek to specific timestamp (milliseconds)
    void seekToTimestamp(uint64_t timestampMs);

    // Get total audio duration (milliseconds)
    uint64_t getTotalDurationMs() const;

    // Update current timestamp (called from main audio loop)
    void updateCurrentTimestamp(uint64_t timestampMs);

private:
    std::atomic<PlaybackState> state;
    std::atomic<uint64_t> currentTimestampMs;
    std::atomic<uint64_t> startTimestampMs;  // When preview was started
    std::atomic<uint64_t> playbackStartTimeMs;  // System time when playback started
    std::string audioFilePath;
    uint64_t totalDurationMs;

    // Helper to calculate elapsed time
    std::chrono::system_clock::time_point playbackStartTime;
};
