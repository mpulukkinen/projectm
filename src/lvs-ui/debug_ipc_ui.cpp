#include "debug_ipc_ui.hpp"
#include "pmSDL.hpp"
#include "imgui.h"
#include <string>

namespace DebugIPCUI {
    uint64_t timeStamp = 0;
    uint64_t sessionStartOffset = 0;
    uint64_t sessionLength = 0;

    void render(IPCManager* ipcManager, projectMSDL* mainController) {
        if (!ipcManager) return;

        // Position window at top-right corner
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = work_pos.x + work_size.x - PAD;
        window_pos.y = work_pos.y + PAD;
        window_pos_pivot.x = 1.0f;
        window_pos_pivot.y = 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.75f); // Transparent background

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("Debug IPC UI", nullptr, window_flags)) {
            ImGui::Text("Debug IPC Simulation");
            ImGui::Separator();

            // Show current session info
            ImGui::Text("Session Start Offset: %llu ms", sessionStartOffset);
            ImGui::Text("Session Length: %llu ms", sessionLength);
            ImGui::Text("Current Timestamp: %llu ms", timeStamp);
            ImGui::Separator();

            // Horizontal layout for buttons
            // Button 1: Load Preset A
            if (ImGui::Button("Load Preset A")) {
                // Simulate receiving a LOAD_PRESET message
                IPC::IPCMessage msg = IPC::MessageBuilder::buildLoadPreset("presets/milkdrop/Simple/Painterly.milk", 0);
                ipcManager->handleIPCMessage(msg);
            }
            ImGui::SameLine();

            // Button 2: Load Preset B
            if (ImGui::Button("Load Preset B")) {
                // Simulate receiving a LOAD_PRESET message with a future timestamp
                IPC::IPCMessage msg = IPC::MessageBuilder::buildLoadPreset("presets/milkdrop/Simple/MegaSwirl.milk", 5000);
                ipcManager->handleIPCMessage(msg);
            }
            ImGui::SameLine();

            // Button 4: Send Timestamp backward
            if (ImGui::Button("Rewind 10sec")) {
                // Simulate receiving a TIMESTAMP message
                if(timeStamp < 10000)
                {
                    timeStamp = 0;
                }
                else
                {
                    timeStamp -= 10000;
                }
                IPC::IPCMessage msg = IPC::MessageBuilder::buildTimestamp(timeStamp);
                ipcManager->handleIPCMessage(msg);
            }

            ImGui::SameLine();

            // Button 3: Send Timestamp forward
            if (ImGui::Button("Forward 10sec")) {
                // Simulate receiving a TIMESTAMP message
                timeStamp += 10000;
                IPC::IPCMessage msg = IPC::MessageBuilder::buildTimestamp(timeStamp);
                ipcManager->handleIPCMessage(msg);
            }

            ImGui::Separator();
            ImGui::Text("Session Offset Simulation (+/- 1000ms)");

            // Session offset buttons
            if (ImGui::Button("-1000ms")) {
                if (sessionStartOffset >= 1000) {
                    sessionStartOffset -= 1000;
                } else {
                    sessionStartOffset = 0;
                }
                IPC::IPCMessage msg = IPC::MessageBuilder::buildStartOffset(sessionStartOffset);
                ipcManager->handleIPCMessage(msg);
            }
            ImGui::SameLine();
            if (ImGui::Button("+1000ms")) {
                sessionStartOffset += 1000;
                IPC::IPCMessage msg = IPC::MessageBuilder::buildStartOffset(sessionStartOffset);
                ipcManager->handleIPCMessage(msg);
            }

            ImGui::SameLine();
            if (ImGui::Button("Reset Offset")) {
                sessionStartOffset = 0;
                IPC::IPCMessage msg = IPC::MessageBuilder::buildStartOffset(sessionStartOffset);
                ipcManager->handleIPCMessage(msg);
            }

            ImGui::Separator();
            ImGui::Text("Session Length Simulation");

            // Session length buttons
            if (ImGui::Button("Length: 20000ms")) {
                sessionLength = 20000;
                IPC::IPCMessage msg = IPC::MessageBuilder::buildLength(sessionLength);
                ipcManager->handleIPCMessage(msg);
            }
            ImGui::SameLine();

            if (ImGui::Button("Length: 60000ms")) {
                sessionLength = 60000;
                IPC::IPCMessage msg = IPC::MessageBuilder::buildLength(sessionLength);
                ipcManager->handleIPCMessage(msg);
            }
            ImGui::SameLine();

            if (ImGui::Button("Clear Length")) {
                sessionLength = 0;
                IPC::IPCMessage msg = IPC::MessageBuilder::buildLength(sessionLength);
                ipcManager->handleIPCMessage(msg);
            }
        }
        ImGui::End();
    }

} // namespace DebugIPCUI
