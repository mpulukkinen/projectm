#include "debug_ipc_ui.hpp"
#include "imgui.h"
#include <string>

namespace DebugIPCUI {

    void render(IPCManager* ipcManager) {
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

            // Button 3: Send Timestamp
            if (ImGui::Button("Send Timestamp")) {
                // Simulate receiving a TIMESTAMP message
                static uint64_t ts = 1000;
                IPC::IPCMessage msg = IPC::MessageBuilder::buildTimestamp(ts);
                ipcManager->handleIPCMessage(msg);
                ts += 1000;
            }

            // New row
            // Button 4: Start Preview
            if (ImGui::Button("Start Preview")) {
                IPC::IPCMessage msg = IPC::MessageBuilder::buildStartPreview(0);
                ipcManager->handleIPCMessage(msg);
            }
            ImGui::SameLine();

            // Button 5: Stop Preview
            if (ImGui::Button("Stop Preview")) {
                IPC::IPCMessage msg = IPC::MessageBuilder::buildStopPreview();
                ipcManager->handleIPCMessage(msg);
            }
        }
        ImGui::End();
    }

} // namespace DebugIPCUI
