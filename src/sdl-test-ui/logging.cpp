#include "logging.hpp"
#include <SDL_log.h>
#include <fstream>
#include <chrono>
#include <ctime>

FileLogger::FileLogger(const std::string& filename) {
    logFilePath = filename;
}

void FileLogger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::ofstream logFile(logFilePath, std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        char timebuf[32];
        std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));
        logFile << timebuf << " | " << message << std::endl;
        logFile.flush();
    } else {
        // Print error to stderr and SDL log if file cannot be opened
        fprintf(stderr, "FileLogger ERROR: Could not open log file: %s\n", logFilePath.c_str());
        SDL_Log("FileLogger ERROR: Could not open log file: %s", logFilePath.c_str());
    }
}
