/**
 * C++ IPC Test Program
 *
 * Minimal test program to verify IPC communication is working
 * Run this to test the IPC system independently
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "ipc_manager.hpp"

int main() {
    std::cout << "=== ProjectM IPC Test Program ===" << std::endl;
    std::cout << "This test program verifies the IPC communication system." << std::endl;
    std::cout << std::endl;

    // Create IPC manager
    IPCManager ipcManager;

    std::cout << "[*] Initializing IPC..." << std::endl;
    ipcManager.initialize();

    std::cout << "[+] IPC initialized successfully!" << std::endl;
    std::cout << "[*] Waiting for C# messages (send via stdin)..." << std::endl;
    std::cout << std::endl;

    std::cout << "Test scenarios:" << std::endl;
    std::cout << "1. Send timestamp: {\"type\":0,\"data\":{\"timestampMs\":5000}}" << std::endl;
    std::cout << "2. Load preset:   {\"type\":1,\"data\":{\"presetName\":\"test.milk\",\"startTimestampMs\":5000}}" << std::endl;
    std::cout << "3. Start preview: {\"type\":3,\"data\":{\"fromTimestampMs\":0}}" << std::endl;
    std::cout << "4. Delete preset: {\"type\":2,\"data\":{\"presetName\":\"test.milk\",\"timestampMs\":5000}}" << std::endl;
    std::cout << "5. Stop preview:  {\"type\":4,\"data\":{}}" << std::endl;
    std::cout << std::endl;

    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << std::endl;

    // Simulate some activity for testing
    for (int i = 0; i < 1000; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Periodically send state updates
        if (i % 10 == 0) {
            ipcManager.sendCurrentState();
            ipcManager.sendPreviewStatusUpdate();
        }

        // Check for pending updates
        if (ipcManager.hasPendingStateUpdate()) {
            ipcManager.sendCurrentState();
            ipcManager.clearPendingStateUpdate();
        }
    }

    std::cout << "[*] Shutting down IPC..." << std::endl;
    ipcManager.shutdown();

    std::cout << "[+] Test completed successfully!" << std::endl;

    return 0;
}

/*
 * Compilation:
 *
 * Windows (with vcpkg jsoncpp):
 * g++ -std=c++17 -o ipc_test ipc_test.cpp ipc_communication.cpp \
 *     preset_queue_manager.cpp audio_preview_manager.cpp ipc_manager.cpp \
 *     -I"C:\vcpkg\installed\x64-windows\include" \
 *     -L"C:\vcpkg\installed\x64-windows\lib" -ljsoncpp
 *
 * Or with CMake (add to CMakeLists.txt):
 * add_executable(ipc_test ipc_test.cpp)
 * target_link_libraries(ipc_test ipc_communication preset_queue_manager
 *                                 audio_preview_manager ipc_manager jsoncpp)
 *
 * Usage:
 * 1. Run: .\ipc_test.exe
 * 2. In another terminal or via stdin, send JSON messages:
 *    echo {"type":0,"data":{"timestampMs":5000}} | ipc_test.exe
 *
 * Or interactively:
 *    ipc_test.exe
 *    (paste messages and press Enter)
 */
