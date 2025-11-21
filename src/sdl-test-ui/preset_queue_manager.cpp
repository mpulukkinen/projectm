#include "preset_queue_manager.hpp"

PresetQueueManager::PresetQueueManager() {}

void PresetQueueManager::addPreset(const std::string& presetName, uint64_t startTimestampMs) {
    std::lock_guard<std::mutex> lock(mutex);

    // Check if preset already exists at this timestamp
    auto it = std::find_if(presets.begin(), presets.end(),
        [&](const PresetEntry& e) {
            return e.presetName == presetName && e.startTimestampMs == startTimestampMs;
        });

    if (it == presets.end()) {
        presets.emplace_back(presetName, startTimestampMs);
        sortPresets();
    }
}

bool PresetQueueManager::removePreset(const std::string& presetName, uint64_t timestampMs) {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = std::find_if(presets.begin(), presets.end(),
        [&](const PresetEntry& e) {
            return e.presetName == presetName && e.startTimestampMs == timestampMs;
        });

    if (it != presets.end()) {
        presets.erase(it);
        return true;
    }
    return false;
}

std::vector<PresetQueueManager::PresetEntry> PresetQueueManager::getAllPresets() const {
    std::lock_guard<std::mutex> lock(mutex);
    return presets;
}

std::string PresetQueueManager::getPresetAtTimestamp(uint64_t timestampMs) const {
    std::lock_guard<std::mutex> lock(mutex);

    // Find preset that should be playing at this timestamp
    // A preset plays until the next preset starts
    std::string currentPreset;

    for (const auto& preset : presets) {
        if (preset.startTimestampMs <= timestampMs) {
            currentPreset = preset.presetName;
        } else {
            break;  // Since sorted, no need to continue
        }
    }

    return currentPreset;
}

std::string PresetQueueManager::getNextPreset(uint64_t currentTimestampMs) const {
    std::lock_guard<std::mutex> lock(mutex);

    for (const auto& preset : presets) {
        if (preset.startTimestampMs > currentTimestampMs) {
            return preset.presetName;
        }
    }

    return "";  // No next preset
}

std::string PresetQueueManager::getPreviousPreset(uint64_t currentTimestampMs) const {
    std::lock_guard<std::mutex> lock(mutex);

    std::string previousPreset;

    for (const auto& preset : presets) {
        if (preset.startTimestampMs < currentTimestampMs) {
            previousPreset = preset.presetName;
        } else {
            break;
        }
    }

    return previousPreset;
}

void PresetQueueManager::clearAll() {
    std::lock_guard<std::mutex> lock(mutex);
    presets.clear();
}

size_t PresetQueueManager::getPresetCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return presets.size();
}

bool PresetQueueManager::presetExists(const std::string& presetName, uint64_t timestampMs) const {
    std::lock_guard<std::mutex> lock(mutex);

    return std::any_of(presets.begin(), presets.end(),
        [&](const PresetEntry& e) {
            return e.presetName == presetName && e.startTimestampMs == timestampMs;
        });
}

uint64_t PresetQueueManager::getEarliestTimestamp() const {
    std::lock_guard<std::mutex> lock(mutex);

    if (presets.empty()) return 0;
    return presets.front().startTimestampMs;
}

uint64_t PresetQueueManager::getLatestTimestamp() const {
    std::lock_guard<std::mutex> lock(mutex);

    if (presets.empty()) return 0;
    return presets.back().startTimestampMs;
}

void PresetQueueManager::sortPresets() {
    std::sort(presets.begin(), presets.end());
}
