#include "ipc_manager.hpp"
#include "logging.hpp"
#include <iostream>
#include <SDL_log.h>
#include <direct.h>

IPCManager::IPCManager()
    : lastReceivedTimestampMs(0)
    , pendingStateUpdate(false),
      presetQueue(*this)
{
    // Log file in working directory
    logger = std::make_shared<FileLogger>("ipc_messages.log");
    // Immediately log presence
    if (logger) logger->log("I AM HERE");
    // Print working directory for diagnostics
    char cwd[512];
    if (_getcwd(cwd, sizeof(cwd))) {
        SDL_Log("IPCManager: Current working directory: %s", cwd);
    } else {
        SDL_Log("IPCManager: Could not get current working directory");
    }
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
        // Log received message
        if (logger) {
            SDL_Log("IPCManager: Logging received message: %s", msg.serialize().c_str());
            logger->log(std::string("RECV: ") + msg.serialize());
        }
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
        case IPC::MessageType::START_OFFSET:
            handleStartOffsetMessage(msg);
            break;
        case IPC::MessageType::LENGTH:
            handleLengthMessage(msg);
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
        setLastReceivedTimestamp(msg.data["timestampMs"].asUInt64());
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

        // Signal UI/render loop to auto-load the first queued preset.
        if (presetQueue.getPresetCount() == 1) {
            needsFirstPresetAutoLoad = true;
        }

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

void IPCManager::handleStartOffsetMessage(const IPC::IPCMessage& msg) {
    if (msg.data.isMember("timestampMs")) {
        sessionStartOffsetMs = msg.data["timestampMs"].asUInt64();
        SDL_Log("IPC: Set session start offset to %llu ms", sessionStartOffsetMs);
    }
}

void IPCManager::handleLengthMessage(const IPC::IPCMessage& msg) {
    if (msg.data.isMember("timestampMs")) {
        sessionLengthMs = msg.data["timestampMs"].asUInt64();
        SDL_Log("IPC: Set session length to %llu ms", sessionLengthMs);
    }
}

void IPCManager::sendCurrentState() {
    if (!ipcHandler) return;

    auto presets = presetQueue.getAllPresets();
    std::vector<IPC::PresetQueueEntry> ipcPresets;

    for (const auto& preset : presets) {
        ipcPresets.emplace_back(preset.presetName, preset.startTimestampMs);
    }

        if (logger) {
            SDL_Log("IPCManager: Logging SEND current state");
            logger->log(std::string("SEND: ") + IPC::MessageBuilder::buildCurrentState(ipcPresets, lastReceivedTimestampMs).serialize());
        }
    ipcHandler->sendMessage(
        IPC::MessageBuilder::buildCurrentState(ipcPresets, lastReceivedTimestampMs)
    );

    pendingStateUpdate = false;
}

void IPCManager::sendPreviewStatusUpdate(const IPC::IPCMessage& msg) {
    if (!this->ipcHandler) return;
    if (this->logger) {
        SDL_Log("IPCManager: Logging SEND preview status update: %s", msg.serialize().c_str());
        this->logger->log(std::string("SEND: ") + msg.serialize());
    }
    this->ipcHandler->sendMessage(msg);
}
