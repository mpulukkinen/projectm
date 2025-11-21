#include "ipc_manager.hpp"
#include <iostream>
#include <SDL_log.h>

IPCManager::IPCManager()
    : lastReceivedTimestampMs(0)
    , pendingStateUpdate(false)
{
}

IPCManager::~IPCManager() {
    shutdown();
}

void IPCManager::initialize() {
    if (!ipcHandler) {
        SDL_Log("IPC Manager: Initializing...");
        ipcHandler = std::make_unique<IPC::IPCHandler>();

        // Start listening for messages from C#
        ipcHandler->startListening([this](const IPC::IPCMessage& msg) {
            SDL_Log("IPC Manager: Handling message type %d", (int)msg.type);
            handleIPCMessage(msg);
        });

        SDL_Log("IPC Manager: Initialization complete");
    }
}

void IPCManager::shutdown() {
    if (ipcHandler) {
        ipcHandler->stopListening();
        ipcHandler.reset();
    }
}

void IPCManager::handleIPCMessage(const IPC::IPCMessage& msg) {
    switch (msg.type) {
        case IPC::MessageType::TIMESTAMP:
            handleTimestampMessage(msg);
            break;
        case IPC::MessageType::LOAD_PRESET:
            handleLoadPresetMessage(msg);
            break;
        case IPC::MessageType::DELETE_PRESET:
            handleDeletePresetMessage(msg);
            break;
        case IPC::MessageType::START_PREVIEW:
            handleStartPreviewMessage(msg);
            break;
        case IPC::MessageType::STOP_PREVIEW:
            handleStopPreviewMessage(msg);
            break;
        default:
            // Unknown message type - send error
            if (ipcHandler) {
                ipcHandler->sendMessage(
                    IPC::MessageBuilder::buildError("Unknown message type")
                );
            }
            break;
    }

}

void IPCManager::handleTimestampMessage(const IPC::IPCMessage& msg) {
    // C# sends current timestamp - store it
    if (msg.data.isMember("timestampMs")) {
        lastReceivedTimestampMs = msg.data["timestampMs"].asUInt64();
        pendingStateUpdate = true;
    }
}

void IPCManager::handleLoadPresetMessage(const IPC::IPCMessage& msg) {
    // C# requests to load a preset at specific timestamp
    if (msg.data.isMember("presetName") && msg.data.isMember("startTimestampMs")) {
        std::string presetName = msg.data["presetName"].asString();
        uint64_t startTimestamp = msg.data["startTimestampMs"].asUInt64();

        SDL_Log("IPC: Loading preset '%s' at %llu ms", presetName.c_str(), startTimestamp);

        // Add preset to queue
        presetQueue.addPreset(presetName, startTimestamp);

        // Send confirmation back
        if (ipcHandler) {
            ipcHandler->sendMessage(
                IPC::MessageBuilder::buildPresetLoaded(
                    presetName,
                    startTimestamp,
                    lastReceivedTimestampMs
                )
            );
        }

        pendingStateUpdate = true;
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "IPC: Missing presetName or startTimestampMs");
        if (ipcHandler) {
            ipcHandler->sendMessage(
                IPC::MessageBuilder::buildError("Missing presetName or startTimestampMs")
            );
        }
    }
}

void IPCManager::handleDeletePresetMessage(const IPC::IPCMessage& msg) {
    // C# requests to delete a preset
    if (msg.data.isMember("presetName") && msg.data.isMember("timestampMs")) {
        std::string presetName = msg.data["presetName"].asString();
        uint64_t timestamp = msg.data["timestampMs"].asUInt64();

        bool removed = presetQueue.removePreset(presetName, timestamp);

        if (removed) {
            pendingStateUpdate = true;
        }
    }
}

void IPCManager::handleStartPreviewMessage(const IPC::IPCMessage& msg) {
    // C# requests to start audio preview
    if (msg.data.isMember("fromTimestampMs")) {
        uint64_t fromTimestamp = msg.data["fromTimestampMs"].asUInt64();

        // TODO: Implement actual audio playback from timestamp
        // This would involve seeking the audio file/loopback to the given timestamp
    }
}

void IPCManager::handleStopPreviewMessage(const IPC::IPCMessage& msg) {
    // C# requests to stop audio preview

    // TODO: Implement actual audio stop
}

void IPCManager::sendCurrentState() {
    if (!ipcHandler) return;

    auto presets = presetQueue.getAllPresets();
    std::vector<IPC::PresetQueueEntry> ipcPresets;

    for (const auto& preset : presets) {
        ipcPresets.emplace_back(preset.presetName, preset.startTimestampMs);
    }

    ipcHandler->sendMessage(
        IPC::MessageBuilder::buildCurrentState(ipcPresets, lastReceivedTimestampMs)
    );

    pendingStateUpdate = false;
}

void IPCManager::sendPreviewStatusUpdate() {
    if (!ipcHandler) return;
}
