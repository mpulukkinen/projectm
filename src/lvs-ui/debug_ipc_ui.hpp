#pragma once

#include "ipc_manager.hpp"

class projectMSDL;

namespace DebugIPCUI {

    /**
     * Renders the Debug IPC UI overlay using ImGui.
     * @param ipcManager Pointer to the IPCManager instance.
     */
    void render(IPCManager* ipcManager, projectMSDL* mainController);

} // namespace DebugIPCUI
