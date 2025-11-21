# IPC System - Quick Start Guide

## What You Have

The C++ IPC system consists of 4 main header/source file pairs:

```
ipc_communication.hpp/cpp       - Core stdin/stdout communication
preset_queue_manager.hpp/cpp    - Manage presets with timestamps
audio_preview_manager.hpp/cpp   - Track audio playback state
ipc_manager.hpp/cpp             - Coordinator that ties everything together
```

Plus a complete C# client example: `CSharpIPCClient_Example.cs`

## Quick Setup (C++)

### 1. Add to CMakeLists.txt

```cmake
# Find or add jsoncpp library
find_package(jsoncpp REQUIRED)

# Add IPC files to your target
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

target_link_libraries(LvsAudioReactiveVisualizer PRIVATE jsoncpp)
```

### 2. Add to pmSDL.hpp

```cpp
#include "ipc_manager.hpp"

class projectMSDL {
private:
    std::unique_ptr<IPCManager> ipcManager;
    // ... existing members ...
};
```

### 3. Initialize in pmSDL.cpp

**Constructor:**
```cpp
projectMSDL::projectMSDL(...) : ... {
    ipcManager = std::make_unique<IPCManager>();
    ipcManager->initialize();
}
```

**Destructor:**
```cpp
projectMSDL::~projectMSDL() {
    if (ipcManager) ipcManager->shutdown();
    // ... rest of cleanup ...
}
```

### 4. Update Main Loop

Add to your render loop (every frame or every N frames):

```cpp
// In mainLoop() or renderFrame()

// Update audio playback position
if (ipcManager && ipcManager->getAudioPreview().isPlaying()) {
    uint64_t currentTimestamp = getCurrentAudioTimestampMs();
    ipcManager->getAudioPreview().updateCurrentTimestamp(currentTimestamp);

    // Load the preset that should be playing now
    std::string presetToPlay = ipcManager->getPresetQueue()
        .getPresetAtTimestamp(currentTimestamp);
    if (!presetToPlay.empty()) {
        loadPreset(presetToPlay);  // Your existing function
    }
}

// Send state updates periodically
static int counter = 0;
if (counter++ % 30 == 0) {  // Every ~500ms at 60fps
    if (ipcManager && ipcManager->hasPendingStateUpdate()) {
        ipcManager->sendCurrentState();
    }
}
```

## Quick Start (C#)

### 1. Add Newtonsoft.Json

```bash
Install-Package Newtonsoft.Json
```

### 2. Create Client

```csharp
using ProjectMIPC;

// Start the C++ app
var client = new ProjectMIPCClient(
    "path/to/LvsAudioReactiveVisualizer.exe",
    "--preset-dir C:\\presets"
);

// Handle messages
client.MessageReceived += (sender, args) =>
{
    Console.WriteLine($"Message: {args.MessageType}");
};
```

### 3. Load Preset at Timestamp

```csharp
// Load "mypreset.milk" at 5000ms
client.LoadPreset("mypreset.milk", 5000);

// Start preview from 0ms
client.StartPreview(0);

// Send current audio timestamp (do this frequently)
client.SendTimestamp(currentAudioPositionMs);
```

### 4. Monitor Queue

```csharp
// View current preset queue (sorted by timestamp)
foreach (var preset in client.PresetQueue)
{
    Console.WriteLine($"{preset.PresetName} at {preset.TimestampMs}ms");
}

// Delete a preset
client.DeletePreset("mypreset.milk", 5000);
```

## Message Flow

### Loading a Preset

```
C#: LOAD_PRESET "mypreset.milk" at 5000ms
        ↓
C++: Add to queue, send PRESET_LOADED confirmation
        ↓
C#: Receive confirmation, update UI
```

### Playing Audio with Presets

```
C#: START_PREVIEW from 0ms
        ↓
C++: Start audio playback
        ↓
C#: Send TIMESTAMP every frame (e.g., 5000ms)
        ↓
C++: Check if preset should change at 5000ms
        ↓
C++: Load "mypreset.milk" if it starts at 5000ms
```

### Queue Display (C#)

Presets are automatically sorted by timestamp in `client.PresetQueue`:

```
Preset 1: "intro.milk"      at 0ms
Preset 2: "verse.milk"      at 10000ms
Preset 3: "chorus.milk"     at 20000ms
Preset 4: "outro.milk"      at 45000ms
```

## Key Classes

### PresetQueueManager (C++)
```cpp
ipcManager->getPresetQueue().addPreset(name, timestampMs);
ipcManager->getPresetQueue().removePreset(name, timestampMs);
ipcManager->getPresetQueue().getPresetAtTimestamp(currentMs);
ipcManager->getPresetQueue().getAllPresets();
```

### AudioPreviewManager (C++)
```cpp
ipcManager->getAudioPreview().startPreview(fromMs);
ipcManager->getAudioPreview().stopPreview();
ipcManager->getAudioPreview().updateCurrentTimestamp(currentMs);
ipcManager->getAudioPreview().getCurrentTimestamp();
ipcManager->getAudioPreview().isPlaying();
```

### ProjectMIPCClient (C#)
```csharp
client.LoadPreset(name, timestampMs);
client.DeletePreset(name, timestampMs);
client.StartPreview(fromMs);
client.StopPreview();
client.SendTimestamp(currentMs);
```

## JSON Message Examples

### C# Sends: Load Preset
```json
{"type": 1, "data": {"presetName": "cool.milk", "startTimestampMs": 5000}}
```

### C++ Sends: Confirmation
```json
{"type": 5, "data": {"presetName": "cool.milk", "startTimestampMs": 5000, "lastReceivedTimestampMs": 0}}
```

### C++ Sends: State Update
```json
{
  "type": 6,
  "data": {
    "lastReceivedTimestampMs": 0,
    "presets": [
      {"presetName": "cool.milk", "timestampMs": 5000}
    ]
  }
}
```

## Common Tasks

### 1. Update UI Preview List
```csharp
// This happens automatically - presets are sorted by timestamp
presetListView.ItemsSource = client.PresetQueue;
```

### 2. Sync with Audio Player
```csharp
// In your audio position update callback:
client.SendTimestamp((ulong)audioPlayer.CurrentPosition.TotalMilliseconds);
```

### 3. Add Preset from Dropdown
```csharp
void OnPresetSelected(string presetName, int startTimeSeconds)
{
    client.LoadPreset(presetName, (ulong)startTimeSeconds * 1000);
}
```

### 4. Remove Preset from List
```csharp
void OnDeleteClicked(PresetQueueEntry preset)
{
    client.DeletePreset(preset.PresetName, preset.TimestampMs);
}
```

## Debugging

### Enable Logging (C#)
```csharp
client.MessageReceived += (sender, args) =>
{
    Console.WriteLine($"[{DateTime.Now:HH:mm:ss.fff}] Type: {args.MessageType}");
    Console.WriteLine($"Data: {args.Data}");
};
```

### Monitor Process (C#)
```csharp
if (cppProcess.HasExited)
{
    Console.WriteLine($"C++ process exited with code: {cppProcess.ExitCode}");
}
```

### Check Stderr (C#)
```csharp
// Modify ProjectMIPCClient to also capture stderr:
RedirectStandardError = true;
var stderrReader = new StreamReader(cppProcess.StandardError);
// ... read errors as needed
```

## Testing Standalone

### Test C++ IPC Manually

```bash
# Start the process and type JSON messages:
LvsAudioReactiveVisualizer.exe --preset-dir C:\presets

# Type a message (press Enter):
{"type": 0, "data": {"timestampMs": 5000}}

# Press Enter to send, should receive confirmation
```

### Test C# Client

```csharp
static void Main()
{
    var client = new ProjectMIPCClient(
        "LvsAudioReactiveVisualizer.exe",
        "--preset-dir C:\\presets"
    );

    client.MessageReceived += (s, e) =>
        Console.WriteLine($"Received: {e.MessageType}");

    // Test messages
    client.SendTimestamp(0);
    Thread.Sleep(100);

    client.LoadPreset("test.milk", 5000);
    Thread.Sleep(100);

    client.StartPreview(0);
    Thread.Sleep(1000);

    client.Dispose();
}
```

## Next Steps

1. **Add CMake configuration** for jsoncpp dependency
2. **Integrate with pmSDL** - update constructor/destructor/render loop
3. **Test basic flow** - load preset, verify queue
4. **Implement audio playback** - sync timestamps with actual audio
5. **Build C# UI** - list presets, add/delete, play preview

See `IPC_COMMUNICATION_GUIDE.md` for detailed documentation.
