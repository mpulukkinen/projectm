#include "preset_queue_manager.hpp"
#include "imgui.h"

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

void PresetQueueManager::renderUI() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 350, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Preset Queue", nullptr)) {
        std::lock_guard<std::mutex> lock(mutex);

        if (ImGui::Button("Clear All")) {
            presets.clear();
        }

        ImGui::Separator();

        for (size_t i = 0; i < presets.size(); ++i) {
            auto& preset = presets[i];
            ImGui::PushID(static_cast<int>(i));

            ImGui::Text("%s", preset.presetName.c_str());

            // Timestamp editing
            // First preset is always locked to 0
            if (i == 0) {
                ImGui::Text("Start: 0 ms (Locked)");
            } else {
                int ts = static_cast<int>(preset.startTimestampMs);
                if (ImGui::InputInt("Start (ms)", &ts)) {
                    if (ts < 0) ts = 0;
                    // Update timestamp and re-sort
                    // Since we are iterating, modifying the vector directly is risky if we sort immediately
                    // But here we are just changing a value. However, sorting might invalidate iterators/indices if we were using them differently.
                    // Since we are iterating by index, we can modify. But we need to re-sort after the loop or handle it carefully.
                    // Simplest approach: modify, then break and re-sort (UI will update next frame)
                    preset.startTimestampMs = static_cast<uint64_t>(ts);
                    sortPresets();
                    ImGui::PopID();
                    break;
                }
            }

            if (ImGui::Button("Remove")) {
                presets.erase(presets.begin() + i);
                ImGui::PopID();
                break;
            }

            ImGui::Separator();
            ImGui::PopID();
        }
    }
    ImGui::End();
}
