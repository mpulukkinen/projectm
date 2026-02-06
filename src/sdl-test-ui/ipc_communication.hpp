#pragma once

#include <string>
#include <vector>
#include <json/json.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <cstdint>

/**
 * IPC Communication Protocol for C# <-> C++ Communication
 * Uses JSON over stdin/stdout for easy parsing and extensibility
 *
 * Message Format: Single-line JSON followed by newline
 */

namespace IPC {

// ============================================================================
// Message Types
// ============================================================================

enum class MessageType {
    // C# -> C++
    TIMESTAMP,                  // C# sends current timestamp
    LOAD_PRESET,               // C# sends preset name and timestamp to start playing
    DELETE_PRESET,             // C# deletes a preset from the queue

    // C++ -> C#
    PRESET_LOADED,             // C++ confirms preset loaded
    CURRENT_STATE,             // C++ sends current state (playing presets, timestamps)
    PREVIEW_STATUS,            // C++ sends preview status update

    // Errors
    ERROR_RESPONSE,             // Error in processing
    START_OFFSET,              // C# sends start offset (ms) for session
    LENGTH                    // C# sends length (ms) for session
};

// ============================================================================
// Preset Queue Entry
// ============================================================================

struct PresetQueueEntry {
    std::string presetName;
    uint64_t timestampMs;      // When this preset should start (milliseconds)

    // Constructor
    PresetQueueEntry(const std::string& name = "", uint64_t ts = 0)
        : presetName(name), timestampMs(ts) {}

    // Serialize to JSON
    Json::Value toJson() const {
        Json::Value obj;
        obj["presetName"] = presetName;
        obj["timestampMs"] = static_cast<Json::Value::Int64>(timestampMs);
        return obj;
    }

    // Deserialize from JSON
    static PresetQueueEntry fromJson(const Json::Value& obj) {
        PresetQueueEntry entry;
        if (obj.isMember("presetName")) entry.presetName = obj["presetName"].asString();
        if (obj.isMember("timestampMs")) entry.timestampMs = obj["timestampMs"].asUInt64();
        return entry;
    }
};

// ============================================================================
// IPC Message Base
// ============================================================================

struct IPCMessage {
    MessageType type;
    Json::Value data;

    // Serialize message to JSON string (single line)
    std::string serialize() const {
        Json::Value msg;
        msg["type"] = static_cast<int>(type);
        msg["data"] = data;

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";  // No indentation for single line
        return Json::writeString(writer, msg);
    }

    // Deserialize message from JSON string
    static IPCMessage deserialize(const std::string& jsonStr) {
        Json::Value root;
        Json::CharReaderBuilder reader;
        std::string errs;

        std::istringstream stream(jsonStr);
        if (!Json::parseFromStream(reader, stream, &root, &errs)) {
            // Return error message
            IPCMessage msg;
            msg.type = MessageType::ERROR_RESPONSE;
            msg.data["error"] = "Failed to parse JSON: " + errs;
            return msg;
        }

        IPCMessage msg;
        msg.type = static_cast<MessageType>(root.get("type", 0).asInt());
        msg.data = root.get("data", Json::Value());
        return msg;
    }
};

// ============================================================================
// Message Builders (C++ -> C#)
// ============================================================================

class MessageBuilder {
public:
    // C# sends timestamp - C++ should acknowledge
    static IPCMessage buildTimestamp(uint64_t timestampMs) {
        IPCMessage msg;
        msg.type = MessageType::TIMESTAMP;
        msg.data["timestampMs"] = static_cast<Json::Value::Int64>(timestampMs);
        return msg;
    }

    // C# sends preset to load at specific timestamp
    static IPCMessage buildLoadPreset(const std::string& presetName, uint64_t startTimestampMs) {
        IPCMessage msg;
        msg.type = MessageType::LOAD_PRESET;
        msg.data["presetName"] = presetName;
        msg.data["startTimestampMs"] = static_cast<Json::Value::Int64>(startTimestampMs);
        return msg;
    }

    // C# requests to delete a preset
    static IPCMessage buildDeletePreset(const std::string& presetName, uint64_t timestampMs) {
        IPCMessage msg;
        msg.type = MessageType::DELETE_PRESET;
        msg.data["presetName"] = presetName;
        msg.data["timestampMs"] = static_cast<Json::Value::Int64>(timestampMs);
        return msg;
    }

    // C++ sends preset loaded confirmation back to C#
    static IPCMessage buildPresetLoaded(const std::string& presetName,
                                       uint64_t startTimestampMs,
                                       uint64_t lastReceivedTimestampMs) {
        IPCMessage msg;
        msg.type = MessageType::PRESET_LOADED;
        msg.data["presetName"] = presetName;
        msg.data["startTimestampMs"] = static_cast<Json::Value::Int64>(startTimestampMs);
        msg.data["lastReceivedTimestampMs"] = static_cast<Json::Value::Int64>(lastReceivedTimestampMs);
        return msg;
    }

    // C++ sends current state of all queued presets
    static IPCMessage buildCurrentState(const std::vector<PresetQueueEntry>& presets,
                                       uint64_t lastReceivedTimestampMs) {
        IPCMessage msg;
        msg.type = MessageType::CURRENT_STATE;
        msg.data["lastReceivedTimestampMs"] = static_cast<Json::Value::Int64>(lastReceivedTimestampMs);

        Json::Value presetsArray(Json::arrayValue);
        for (const auto& preset : presets) {
            presetsArray.append(preset.toJson());
        }
        msg.data["presets"] = presetsArray;
        return msg;
    }

    // C++ sends preview status
    static IPCMessage buildPreviewStatus(bool isPlaying, uint64_t currentTimestampMs) {
        IPCMessage msg;
        msg.type = MessageType::PREVIEW_STATUS;
        msg.data["isPlaying"] = isPlaying;
        msg.data["currentTimestampMs"] = static_cast<Json::Value::Int64>(currentTimestampMs);
        return msg;
    }

    // Error response
    static IPCMessage buildError(const std::string& errorMsg) {
        IPCMessage msg;
        msg.type = MessageType::ERROR_RESPONSE;
        msg.data["msg"] = errorMsg;
        return msg;
    }

    // C# sends start offset (ms) for session
    static IPCMessage buildStartOffset(uint64_t startOffsetMs) {
        IPCMessage msg;
        msg.type = MessageType::START_OFFSET;
        msg.data["startOffsetMs"] = static_cast<Json::Value::Int64>(startOffsetMs);
        return msg;
    }

    // C# sends length (ms) for session
    static IPCMessage buildLength(uint64_t lengthMs) {
        IPCMessage msg;
        msg.type = MessageType::LENGTH;
        msg.data["lengthMs"] = static_cast<Json::Value::Int64>(lengthMs);
        return msg;
    }
};

// ============================================================================
// IPC Handler - Thread-safe communication
// ============================================================================

class IPCHandler {
public:
    using MessageCallback = std::function<void(const IPCMessage&)>;

    IPCHandler();
    ~IPCHandler();

    // Start listening for messages from stdin (runs in separate thread)
    void startListening(MessageCallback callback);

    // Stop listening
    void stopListening();

    // Send message to C# (write to stdout)
    void sendMessage(const IPCMessage& msg);

    // Send message with data to C#
    void sendMessage(const IPCMessage& msg, const std::string& additionalData);

private:
    std::thread listenThread;
    bool isListening;
    std::mutex mutex;

    // Thread function for listening
    void listenThreadFunc(MessageCallback callback);
};

} // namespace IPC
