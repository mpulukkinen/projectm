#include "preset_queue_manager.hpp"
#include "imgui.h"
#include "ipc_manager.hpp"

PresetQueueManager::PresetQueueManager(IPCManager& ipcMgr): ipcManager(ipcMgr)
{
}

void PresetQueueManager::addPreset(const std::string& presetName, uint64_t startTimestampMs) {
    std::lock_guard<std::mutex> lock(mutex);

    // Check if ANY preset exists at this timestamp
    auto it = std::find_if(presets.begin(), presets.end(),
        [&](const PresetEntry& e) {
            return e.startTimestampMs == startTimestampMs;
        });

    if (it != presets.end()) {
        // Replace existing preset at this timestamp
        it->presetName = presetName;
        // No need to sort as timestamp didn't change
    } else {
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

PresetQueueManager::PresetEntry PresetQueueManager::getActivePresetEntry(uint64_t timestampMs) const {
    std::lock_guard<std::mutex> lock(mutex);

    PresetEntry currentEntry; // Default empty

    for (const auto& preset : presets) {
        if (preset.startTimestampMs <= timestampMs) {
            currentEntry = preset;
        } else {
            break;  // Since sorted, no need to continue
        }
    }

    return currentEntry;
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

        // Get session start offset for displaying relative timestamps
        uint64_t sessionOffset = ipcManager.getSessionStartOffsetMs();

        for (size_t i = 0; i < presets.size(); ++i) {
            auto& preset = presets[i];
            ImGui::PushID(static_cast<int>(i));

            size_t last_sep = preset.presetName.find_last_of("/\\");
            std::string fname = (last_sep != std::string::npos) ? preset.presetName.substr(last_sep + 1) : preset.presetName;

            ImGui::Text("%s", fname.c_str());

            // Timestamp editing
            // Timestamps are stored as absolute (sessionOffset + relative)
            // Display as relative to session start (presetTimestamp - sessionOffset)
            // If timestamp equals sessionOffset, it displays as 0 (start of session)
            uint64_t relativeTimestamp = (preset.startTimestampMs > sessionOffset) ?
                (preset.startTimestampMs - sessionOffset) : 0;

            // First preset is always locked to relative 0 (session start)
            if (i == 0) {
                ImGui::Text("Start: %llu ms (session start)", relativeTimestamp);
            } else {
                int ts = static_cast<int>(relativeTimestamp);
                if (ImGui::InputInt("Start (ms)", &ts)) {
                    if (ts < 0) ts = 0;
                    // Convert back to absolute: absolute = sessionOffset + relative
                    preset.startTimestampMs = sessionOffset + static_cast<uint64_t>(ts);
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

            ImGui::SameLine();

            if (ImGui::Button("Jump to time")) {
                // Jump to the preset's actual timestamp (absolute)
                ipcManager.setLastReceivedTimestamp(preset.startTimestampMs);
            }

            ImGui::Separator();
            ImGui::PopID();
        }
    }
    ImGui::End();
}
