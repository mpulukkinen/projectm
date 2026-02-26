#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include <algorithm>
#include <chrono>

class IPCManager;

/**
 * Manages preset scheduling with timestamps
 * Presets are ordered by their start timestamps
 */

class PresetQueueManager {
public:
    struct PresetEntry {
        std::string presetName;
        uint64_t startTimestampMs;  // When this preset should start

        PresetEntry(const std::string& name = "", uint64_t ts = 0)
            : presetName(name), startTimestampMs(ts) {}

        // Comparison for sorting
        bool operator<(const PresetEntry& other) const {
            return startTimestampMs < other.startTimestampMs;
        }
    };

    PresetQueueManager(IPCManager& ipcMgr);

    // Add a preset to the queue at specified timestamp
    void addPreset(const std::string& presetName, uint64_t startTimestampMs);

    // Remove a preset from the queue
    bool removePreset(const std::string& presetName, uint64_t timestampMs);

    // Get all presets sorted by timestamp
    std::vector<PresetEntry> getAllPresets() const;

    // Get preset that should be playing at given timestamp
    std::string getPresetAtTimestamp(uint64_t timestampMs) const;

    // Get the full entry that should be playing at given timestamp
    // Returns empty entry (name="", ts=0) if no presets or before first preset
    PresetEntry getActivePresetEntry(uint64_t timestampMs) const;

    // Get next preset after given timestamp
    std::string getNextPreset(uint64_t currentTimestampMs) const;

    // Get previous preset before given timestamp
    std::string getPreviousPreset(uint64_t currentTimestampMs) const;

    // Clear all presets
    void clearAll();

    // Get count of presets
    size_t getPresetCount() const;

    // Check if preset exists in queue at specific timestamp
    bool presetExists(const std::string& presetName, uint64_t timestampMs) const;

    // Get earliest preset start time
    uint64_t getEarliestTimestamp() const;

    // Get latest preset start time
    uint64_t getLatestTimestamp() const;

    // Render the Preset Queue UI
    void renderUI();

private:
    mutable std::mutex mutex;
    std::vector<PresetEntry> presets;  // Always kept sorted by timestamp
    IPCManager& ipcManager;

    // Helper to maintain sorted order
    void sortPresets();
};
