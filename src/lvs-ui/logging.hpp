#pragma once
#include <string>
#include <fstream>
#include <mutex>

class FileLogger {
public:
    FileLogger(const std::string& filename);
    void log(const std::string& message);
private:
    std::string logFilePath;
    std::mutex logMutex;
};
