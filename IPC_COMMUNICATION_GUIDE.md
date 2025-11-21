# IPC Communication System - Complete Guide

## Overview

This document describes the complete IPC (Inter-Process Communication) system for communicating between a C# application and the C++ projectM visualizer. The system uses JSON messages over stdin/stdout for easy parsing and extensibility.

## Architecture

### Components

#### 1. **ipc_communication.hpp/cpp**
- Core IPC handler that manages stdin/stdout communication
- Runs message listening in a separate thread
- Provides thread-safe message sending

#### 2. **preset_queue_manager.hpp/cpp**
- Manages preset scheduling with timestamps
- Keeps presets sorted by start time
- Provides queries for preset at specific timestamp

#### 3. **audio_preview_manager.hpp/cpp**
- Manages audio playback state and timing
- Tracks current playback position
- Handles seek operations

#### 4. **ipc_manager.hpp/cpp**
- Coordinates all IPC functionality
- Routes incoming messages to appropriate handlers
- Sends state updates back to C#

#### 5. **CSharpIPCClient_Example.cs**
- Complete C# client for communicating with C++
- Handles message serialization/deserialization
- Provides event-based message handling

---

## Communication Protocol

### Message Format

All messages are single-line JSON followed by newline:

```json
{"type": 0, "data": {"field": "value"}}
```

### Message Types

#### C# → C++ Messages

**1. TIMESTAMP (type: 0)**
```json
{
  "type": 0,
  "data": {
    "timestampMs": 5000
  }
}
```
C# sends the current audio timestamp in milliseconds.

**2. LOAD_PRESET (type: 1)**
```json
{
  "type": 1,
  "data": {
    "presetName": "mypreset.milk",
    "startTimestampMs": 10000
  }
}
```
C# requests to load a preset at a specific timestamp.

**3. DELETE_PRESET (type: 2)**
```json
{
  "type": 2,
  "data": {
    "presetName": "mypreset.milk",
    "timestampMs": 10000
  }
}
```
C# requests to delete a preset from the queue.

**4. START_PREVIEW (type: 3)**
```json
{
  "type": 3,
  "data": {
    "fromTimestampMs": 0
  }
}
```
C# requests to start audio preview from a specific timestamp.

**5. STOP_PREVIEW (type: 4)**
```json
{
  "type": 4,
  "data": {}
}
```
C# requests to stop audio preview.

---

#### C++ → C# Messages

**1. PRESET_LOADED (type: 5)**
```json
{
  "type": 5,
  "data": {
    "presetName": "mypreset.milk",
    "startTimestampMs": 10000,
    "lastReceivedTimestampMs": 5000
  }
}
```
C++ confirms that a preset was successfully loaded.

**2. CURRENT_STATE (type: 6)**
```json
{
  "type": 6,
  "data": {
    "lastReceivedTimestampMs": 5000,
    "presets": [
      {
        "presetName": "preset1.milk",
        "timestampMs": 0
      },
      {
        "presetName": "preset2.milk",
        "timestampMs": 10000
      }
    ]
  }
}
```
C++ sends the current state of all queued presets (sorted by timestamp).

**3. PREVIEW_STATUS (type: 7)**
```json
{
  "type": 7,
  "data": {
    "isPlaying": true,
    "currentTimestampMs": 5234
  }
}
```
C++ sends the current preview playback status and position.

**4. ERROR_RESPONSE (type: 8)**
```json
{
  "type": 8,
  "data": {
    "error": "Preset not found"
  }
}
```
C++ sends error information about failed operations.

---

## Implementation Guide

### C++ Side Setup

#### Step 1: Update CMakeLists.txt

Add the new source files to your CMakeLists.txt:

```cmake
# In src/sdl-test-ui/CMakeLists.txt
target_sources(LvsAudioReactiveVisualizer PRIVATE
    ipc_communication.hpp
    ipc_communication.cpp
    preset_queue_manager.hpp
    preset_queue_manager.cpp
    audio_preview_manager.hpp
    audio_preview_manager.cpp
    ipc_manager.hpp
    ipc_manager.cpp
)

# Add jsoncpp dependency
target_link_libraries(LvsAudioReactiveVisualizer PRIVATE jsoncpp)
```

#### Step 2: Integrate with projectMSDL

In `pmSDL.hpp`, add member variable:
```cpp
private:
    std::unique_ptr<IPCManager> ipcManager;
```

In `pmSDL.cpp` constructor:
```cpp
projectMSDL::projectMSDL(...)
    : ...
{
    // ... existing code ...

    ipcManager = std::make_unique<IPCManager>();
    ipcManager->initialize();
}
```

In `pmSDL.cpp` destructor:
```cpp
projectMSDL::~projectMSDL()
{
    if (ipcManager) {
        ipcManager->shutdown();
    }
    // ... existing cleanup code ...
}
```

#### Step 3: Update Main Loop

In the main render loop (in `mainLoop` or `renderFrame`):

```cpp
// Update preview based on current audio position
if (ipcManager && ipcManager->getAudioPreview().isPlaying()) {
    uint64_t currentAudioTimestampMs = app->getCurrentAudioTimestamp();
    app->ipcManager->getAudioPreview().updateCurrentTimestamp(currentAudioTimestampMs);

    // Get preset that should be playing now
    std::string currentPreset = app->ipcManager->getPresetQueue()
        .getPresetAtTimestamp(currentAudioTimestampMs);

    if (!currentPreset.empty()) {
        // Load this preset (implement this in your existing code)
        app->loadPreset(currentPreset);
    }
}

// Periodically send state updates to C#
static uint32_t updateCounter = 0;
if (updateCounter++ % 30 == 0) {  // Every ~500ms at 60fps
    if (app->ipcManager && app->ipcManager->hasPendingStateUpdate()) {
        app->ipcManager->sendCurrentState();
    }
}

// Send preview status updates
static uint32_t statusCounter = 0;
if (statusCounter++ % 60 == 0) {  // Every ~1000ms at 60fps
    if (app->ipcManager) {
        app->ipcManager->sendPreviewStatusUpdate();
    }
}
```

---

### C# Side Usage

#### Step 1: Add Dependencies

```bash
Install-Package Newtonsoft.Json
```

#### Step 2: Create Client Instance

```csharp
using ProjectMIPC;

// Start the C++ process with IPC
var ipcClient = new ProjectMIPCClient(
    "path/to/LvsAudioReactiveVisualizer.exe",
    "--preset-dir C:\\path\\to\\presets"
);

// Subscribe to messages
ipcClient.MessageReceived += (sender, args) =>
{
    Console.WriteLine($"Received message: {args.MessageType}");
};
```

#### Step 3: Send Commands

```csharp
// Update current timestamp (typically in audio position callback)
ipcClient.SendTimestamp(currentAudioPositionMs);

// Load a preset at specific timestamp
ipcClient.LoadPreset("mypreset.milk", 5000);

// Delete a preset
ipcClient.DeletePreset("mypreset.milk", 5000);

// Start audio preview
ipcClient.StartPreview(0);

// Stop audio preview
ipcClient.StopPreview();
```

#### Step 4: Monitor State

```csharp
// Access current preset queue
foreach (var preset in ipcClient.PresetQueue)
{
    Console.WriteLine($"{preset.PresetName} at {preset.TimestampMs}ms");
}

// Check if preview is playing
if (ipcClient.IsPreviewPlaying)
{
    Console.WriteLine("Preview is playing");
}
```

---

## Workflow Example

### User Flow

1. **C# UI**: User selects preset "cool.milk" and sets it to start at 5000ms
2. **C# → C++**: Sends `LOAD_PRESET` message
3. **C++**: Adds preset to queue, sends `PRESET_LOADED` confirmation
4. **C# UI**: Updates UI to show queued preset
5. **C# → C++**: Sends `START_PREVIEW` from 0ms
6. **C++**: Starts audio playback
7. **C++ → C#**: Periodically sends `PREVIEW_STATUS` with current timestamp
8. **C++**: At 5000ms, loads "cool.milk" preset (via `getPresetAtTimestamp()`)
9. **C++ → C#**: Sends `CURRENT_STATE` with updated queue

### User Deletes Preset

1. **C# UI**: User clicks delete on "cool.milk" (5000ms)
2. **C# → C++**: Sends `DELETE_PRESET` message
3. **C++**: Removes preset from queue
4. **C++ → C#**: Sends `CURRENT_STATE` confirmation
5. **C# UI**: Updates UI, preset no longer shown

---

## Threading Model

### Thread Safety

- **IPC Handler**: Listens on separate thread, thread-safe message sending via mutex
- **Preset Queue Manager**: All operations are thread-safe with mutex
- **Audio Preview Manager**: Uses atomic variables for state management

### Main Thread Integration

```cpp
// In render thread (safe):
ipcManager->getPresetQueue().getPresetAtTimestamp(timestamp);
ipcManager->getAudioPreview().updateCurrentTimestamp(timestamp);

// From IPC thread (safe):
ipcManager->getPresetQueue().addPreset(name, timestamp);
ipcManager->getAudioPreview().startPreview(timestamp);
```

---

## Error Handling

### C++ Errors

If C++ encounters an error, it sends an `ERROR_RESPONSE`:

```json
{
  "type": 8,
  "data": {
    "error": "Preset file not found"
  }
}
```

### C# Error Handling

```csharp
ipcClient.MessageReceived += (sender, args) =>
{
    if (args.MessageType == MessageType.ERROR_RESPONSE)
    {
        string error = args.Data["error"]?.Value<string>();
        Console.WriteLine($"Error from C++: {error}");

        // Handle error in UI
        ShowErrorDialog(error);
    }
};
```

---

## Performance Considerations

1. **Update Frequency**: Send timestamps at audio update rate (44.1kHz / 512 samples ≈ 86 times/sec)
2. **State Updates**: Send full state every 500ms (not on every timestamp)
3. **Message Queue**: Preset queue is sorted, lookup is O(n) but typically small (<100 presets)
4. **Memory**: Each message is ~100-500 bytes, negligible impact

---

## Future Enhancements

1. **Binary Protocol**: For higher performance, implement binary format instead of JSON
2. **Compression**: Compress state updates if queue becomes very large
3. **Network IPC**: Switch from stdin/stdout to network sockets for remote control
4. **State Persistence**: Save/load preset queues to disk
5. **Undo/Redo**: Track command history for UI undo functionality

---

## Testing

### Unit Tests (C++)

```cpp
// Test preset queue ordering
PresetQueueManager queue;
queue.addPreset("a.milk", 1000);
queue.addPreset("b.milk", 500);
queue.addPreset("c.milk", 2000);

auto all = queue.getAllPresets();
assert(all[0].startTimestampMs == 500);
assert(all[1].startTimestampMs == 1000);
assert(all[2].startTimestampMs == 2000);
```

### Integration Tests (C#/C++)

```csharp
var client = new ProjectMIPCClient(exePath);

// Load preset
client.LoadPreset("test.milk", 5000);

// Wait for confirmation
Thread.Sleep(100);

// Check state
assert(client.PresetQueue.Count == 1);
assert(client.PresetQueue[0].TimestampMs == 5000);
```

---

## Troubleshooting

### No Messages Received

- Check that C++ process is still running: `if (cppProcess.HasExited)`
- Check stdout is redirected: `RedirectStandardOutput = true`
- Check for exceptions in IPC handler

### Messages Not Processed

- Verify JSON format: Use online JSON validator
- Check message type enum values match
- Enable debug logging in both C# and C++

### Audio Out of Sync

- Ensure timestamp updates are frequent enough
- Use audio driver timestamp, not application clock
- Account for audio latency in calculations

---

## Complete Example

See `CSharpIPCClient_Example.cs` for full working example including:
- Process startup
- Message sending/receiving
- State management
- Error handling
- Resource cleanup
