/**
 * Example: IPC Integration with projectM
 *
 * This file demonstrates how to integrate the IPC communication system
 * into the existing projectM application.
 */

#pragma once

#include "ipc_communication.hpp"
#include "preset_queue_manager.hpp"
#include "audio_preview_manager.hpp"
#include <memory>
#include <cstdint>

/**
 * IPCManager - Coordinates all IPC-related functionality
 * This would be instantiated in projectMSDL class
 */
class IPCManager {
public:
    IPCManager();
    ~IPCManager();

    // Initialize IPC handler (call this once at startup)
    void initialize();

    // Shutdown IPC handler (call before exit)
    void shutdown();

    // Process messages from C# application
    void handleIPCMessage(const IPC::IPCMessage& msg);

    // Send current state to C# (call periodically or when state changes)
    void sendCurrentState();

    // Send preview status update to C#
    void sendPreviewStatusUpdate();

    // Get the preset queue manager
    PresetQueueManager& getPresetQueue() { return presetQueue; }

    // Get the audio preview manager
    AudioPreviewManager& getAudioPreview() { return audioPreview; }

    // Get last received timestamp from C#
    uint64_t getLastReceivedTimestamp() const { return lastReceivedTimestampMs; }

    // Check if there's a pending state update to send
    bool hasPendingStateUpdate() const { return pendingStateUpdate; }

    // Mark state update as sent
    void clearPendingStateUpdate() { pendingStateUpdate = false; }

private:
    std::unique_ptr<IPC::IPCHandler> ipcHandler;
    PresetQueueManager presetQueue;
    AudioPreviewManager audioPreview;
    uint64_t lastReceivedTimestampMs;
    bool pendingStateUpdate;

    // Handle specific message types
    void handleTimestampMessage(const IPC::IPCMessage& msg);
    void handleLoadPresetMessage(const IPC::IPCMessage& msg);
    void handleDeletePresetMessage(const IPC::IPCMessage& msg);
    void handleStartPreviewMessage(const IPC::IPCMessage& msg);
    void handleStopPreviewMessage(const IPC::IPCMessage& msg);
};

/**
 * INTEGRATION POINTS IN projectMSDL CLASS:
 *
 * 1. In pmSDL.hpp header, add member:
 *    std::unique_ptr<IPCManager> ipcManager;
 *
 * 2. In pmSDL.cpp constructor:
 *    ipcManager = std::make_unique<IPCManager>();
 *    ipcManager->initialize();
 *
 * 3. In pmSDL.cpp destructor:
 *    if (ipcManager) ipcManager->shutdown();
 *
 * 4. In main render loop (mainLoop function or renderFrame method):
 *    // Update preview timestamp based on current audio position
 *    if (ipcManager->getAudioPreview().isPlaying()) {
 *        uint64_t currentTimestamp = getCurrentAudioTimestamp();
 *        ipcManager->getAudioPreview().updateCurrentTimestamp(currentTimestamp);
 *
 *        // Get the preset that should be playing now
 *        std::string currentPreset = ipcManager->getPresetQueue()
 *            .getPresetAtTimestamp(currentTimestamp);
 *        if (!currentPreset.empty()) {
 *            loadPreset(currentPreset);  // Load the appropriate preset
 *        }
 *    }
 *
 *    // Periodically send state updates
 *    static uint32_t updateCounter = 0;
 *    if (updateCounter++ % 30 == 0) {  // Every ~500ms at 60fps
 *        ipcManager->sendCurrentState();
 *    }
 */
