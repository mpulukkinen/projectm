#include "ipc_communication.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <SDL_log.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

namespace IPC {

IPCHandler::IPCHandler() : isListening(false) {}

IPCHandler::~IPCHandler() {
    stopListening();
}

void IPCHandler::startListening(MessageCallback callback) {
    if (isListening) return;

    #ifdef _WIN32
    // On Windows, unbuffer stdin/stdout for IPC communication
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    #else
    // On Unix, set stdin/stdout to unbuffered
    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);
    #endif

    SDL_Log("IPC: Starting listener thread");
    isListening = true;
    listenThread = std::thread(&IPCHandler::listenThreadFunc, this, callback);
}

void IPCHandler::stopListening() {
    isListening = false;
    if (listenThread.joinable()) {
        // Wait for thread with timeout (max 1 second)
        // Use a loop to check periodically
        auto start = std::chrono::steady_clock::now();
        while (listenThread.joinable()) {
            listenThread.join();
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::milliseconds(1000)) {
                // Timeout - thread didn't exit cleanly, but we must continue
                // This is a defensive measure to prevent hangs
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void IPCHandler::listenThreadFunc(MessageCallback callback) {
    SDL_Log("IPC: Listening thread started");

    std::string line;
    int consecutiveEmptyReads = 0;

    while (isListening) {
        try {
            // Use blocking getline
            if (std::getline(std::cin, line)) {
                consecutiveEmptyReads = 0;  // Reset counter

                if (!line.empty()) {
                    SDL_Log("IPC: Received message: %s", line.c_str());

                    try {
                        IPCMessage msg = IPCMessage::deserialize(line);
                        if (callback) {
                            callback(msg);
                        }
                    } catch (const std::exception& e) {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IPC: Parse error: %s", e.what());
                        IPCMessage errorMsg = MessageBuilder::buildError(
                            std::string("Exception during message processing: ") + e.what()
                        );
                        if (callback) {
                            callback(errorMsg);
                        }
                    }
                }
            } else {
                // getline failed - check if it's EOF or error
                if (std::cin.eof()) {
                    SDL_Log("IPC: stdin reached EOF");
                    isListening = false;
                    break;
                }

                // Transient error - clear and retry
                std::cin.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                consecutiveEmptyReads++;
                if (consecutiveEmptyReads > 100) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "IPC: Too many consecutive empty reads, stopping");
                    isListening = false;
                    break;
                }
            }
        } catch (const std::exception& e) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IPC: Thread exception: %s", e.what());
            isListening = false;
            break;
        }
    }

    SDL_Log("IPC: Listening thread ended");
}

void IPCHandler::sendMessage(const IPCMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex);
    std::string serialized = msg.serialize();
    SDL_Log("IPC: Sending message: %s", serialized.c_str());
    std::cout << serialized << std::endl;
    std::cout.flush();
}

void IPCHandler::sendMessage(const IPCMessage& msg, const std::string& additionalData) {
    std::lock_guard<std::mutex> lock(mutex);
    std::string serialized = msg.serialize();
    SDL_Log("IPC: Sending message with data: %s", serialized.c_str());
    std::cout << serialized;
    if (!additionalData.empty()) {
        std::cout << "\n" << additionalData;
    }
    std::cout << std::endl;
    std::cout.flush();
}

} // namespace IPC
