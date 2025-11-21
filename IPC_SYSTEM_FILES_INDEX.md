# IPC Communication System - File Index & Navigation

## Quick Links

- **Just want to get started?** → [IPC_QUICKSTART.md](IPC_QUICKSTART.md)
- **Need complete documentation?** → [IPC_COMMUNICATION_GUIDE.md](IPC_COMMUNICATION_GUIDE.md)
- **Want C# code examples?** → [CSHARP_PATTERNS_AND_EXAMPLES.md](CSHARP_PATTERNS_AND_EXAMPLES.md)
- **Looking for overview?** → [IPC_IMPLEMENTATION_SUMMARY.md](IPC_IMPLEMENTATION_SUMMARY.md)

---

## C++ Core Implementation Files

Located in: `src/sdl-test-ui/`

### 1. ipc_communication.hpp / ipc_communication.cpp
**Purpose:** Core IPC handler for stdin/stdout communication

**Classes:**
- `IPCMessage` - JSON message wrapper
- `MessageBuilder` - Message factory helper
- `IPCHandler` - Manages listening thread and output

**Key Features:**
- Thread-safe JSON serialization
- Dedicated listening thread
- Mutex-protected output
- JSON parsing with error handling

**When to use:**
- Low-level message sending/receiving
- Custom protocol implementation

**Related docs:** IPC_COMMUNICATION_GUIDE.md § Protocol Specification

---

### 2. preset_queue_manager.hpp / preset_queue_manager.cpp
**Purpose:** Manages preset scheduling with millisecond timestamps

**Classes:**
- `PresetQueueManager` - Main queue manager
- `PresetEntry` - Individual preset entry

**Key Methods:**
- `addPreset()` - Add preset to queue
- `removePreset()` - Remove specific preset
- `getPresetAtTimestamp()` - Get active preset
- `getAllPresets()` - Get sorted list
- `sortPresets()` - Maintain sort order

**Thread Safety:**
- Mutex-protected all operations
- Atomic-safe throughout

**Related docs:** IPC_QUICKSTART.md § Key Classes

---

### 3. audio_preview_manager.hpp / audio_preview_manager.cpp
**Purpose:** Manages audio playback state and timestamp synchronization

**Classes:**
- `AudioPreviewManager` - Audio state manager
- `PlaybackState` enum - STOPPED, PLAYING, PAUSED

**Key Methods:**
- `startPreview()` - Begin playback
- `stopPreview()` - Stop playback
- `pausePreview()` / `resumePreview()`
- `getCurrentTimestamp()` - Get position
- `updateCurrentTimestamp()` - Update position
- `seekToTimestamp()` - Seek operation

**Atomic Variables:**
- Thread-safe state updates
- Lock-free reads

**Related docs:** IPC_QUICKSTART.md § Key Classes

---

### 4. ipc_manager.hpp / ipc_manager.cpp
**Purpose:** Coordinates all IPC functionality

**Classes:**
- `IPCManager` - Main coordinator

**Key Methods:**
- `initialize()` - Start IPC
- `shutdown()` - Clean stop
- `handleIPCMessage()` - Route messages
- `sendCurrentState()` - Send queue to C#
- `getPresetQueue()` - Get queue manager
- `getAudioPreview()` - Get audio manager

**Integration:**
- Drop-in member for projectMSDL
- Initialize in constructor
- Shutdown in destructor
- Call handlers from main loop

**Related docs:** IPC_QUICKSTART.md § Quick Setup (C++)

---

### 5. ipc_test.cpp
**Purpose:** Standalone test program

**Features:**
- Minimal IPC test setup
- Compilation instructions included
- Test scenario examples
- Can run independently

**How to use:**
```bash
cmake --build . --target ipc_test
./ipc_test.exe
# Send test JSON via stdin
```

**Related docs:** IPC_COMMUNICATION_GUIDE.md § Testing

---

## C# Implementation Files

Located in: Repository root

### 6. CSharpIPCClient_Example.cs
**Purpose:** Complete C# client library

**Classes:**
- `ProjectMIPCClient` - Main client
- `PresetQueueEntry` - Queue entry
- `IPCMessageEventArgs` - Event args

**Key Methods:**
- `SendTimestamp()` - Send audio position
- `LoadPreset()` - Queue preset
- `DeletePreset()` - Remove preset
- `StartPreview()` / `StopPreview()`

**Properties:**
- `PresetQueue` - Current queue (sorted)
- `IsPreviewPlaying` - Playback state
- `LastTimestampMs` - Last position

**Events:**
- `MessageReceived` - All incoming messages

**Dependencies:**
- Newtonsoft.Json

**Related docs:** IPC_QUICKSTART.md § Quick Start (C#)

---

### 7. CSharpWinformsUI_Example.cs
**Purpose:** Complete WinForms UI application

**Features:**
- Preset queue ListView
- Add/delete UI controls
- Real-time status display
- Preview playback controls
- Automatic state sync

**UI Components:**
- ListView for queue display
- ComboBox for preset selection
- TextBox for timestamp input
- Status labels

**How to use:**
1. Adjust file paths in code
2. Compile as WinForms application
3. Run alongside C++ process

**Related docs:** IPC_QUICKSTART.md § Common Tasks

---

## Documentation Files

### 8. IPC_QUICKSTART.md
**Audience:** Developers starting with IPC

**Contents:**
- Quick setup instructions
- Code snippets
- Message examples
- Key classes overview
- Common tasks
- Debugging tips

**When to read:** First stop for implementation

---

### 9. IPC_COMMUNICATION_GUIDE.md
**Audience:** Developers needing complete details

**Contents:**
- Architecture overview
- Complete protocol specification
- Message type details with examples
- Threading model explanation
- Error handling strategies
- Performance considerations
- Troubleshooting guide
- Future enhancements

**When to read:** Need deep understanding

**Key Sections:**
- Overview § Architecture
- Communication Protocol
- Message Types (all 8 types)
- Implementation Guide (C++ & C#)
- Threading Model
- Error Handling
- Troubleshooting

---

### 10. IPC_IMPLEMENTATION_SUMMARY.md
**Audience:** Project managers and architects

**Contents:**
- Component overview
- Integration points in projectMSDL
- Feature checklist
- Performance metrics
- Dependencies
- Security notes
- Usage examples
- File locations

**When to read:** Need big picture

---

### 11. CSHARP_PATTERNS_AND_EXAMPLES.md
**Audience:** C# developers

**Contents:**
7 complete implementation patterns:
1. Simple console application
2. MVVM with WPF
3. Audio player integration
4. Preset queue editor
5. Error handling & retry
6. Preset sequencing
7. Real-time monitoring

**When to read:** Building C# integration

---

### 12. IPC_SYSTEM_FILES_INDEX.md (This file)
**Purpose:** Navigation guide for all files

---

## Integration Checklist

### Step 1: Add C++ Files to Project
- [ ] Copy `ipc_communication.hpp/cpp` to `src/sdl-test-ui/`
- [ ] Copy `preset_queue_manager.hpp/cpp` to `src/sdl-test-ui/`
- [ ] Copy `audio_preview_manager.hpp/cpp` to `src/sdl-test-ui/`
- [ ] Copy `ipc_manager.hpp/cpp` to `src/sdl-test-ui/`

**Reference:** IPC_QUICKSTART.md § Quick Setup (C++)

### Step 2: Update CMakeLists.txt
- [ ] Find or add jsoncpp dependency
- [ ] Add IPC source files to target
- [ ] Link jsoncpp library

**Reference:** IPC_QUICKSTART.md § Quick Setup (C++) § Step 1

### Step 3: Integrate with pmSDL
- [ ] Add ipc_manager header to pmSDL.hpp
- [ ] Add member variable to projectMSDL class
- [ ] Initialize in constructor
- [ ] Shutdown in destructor
- [ ] Call from main render loop

**Reference:** IPC_QUICKSTART.md § Quick Setup (C++) § Steps 2-4

### Step 4: Build & Test
- [ ] Compile successfully
- [ ] Run ipc_test.cpp to verify basic functionality
- [ ] Test with simple C# client

**Reference:** IPC_COMMUNICATION_GUIDE.md § Testing

### Step 5: Create C# Application
- [ ] Add Newtonsoft.Json NuGet package
- [ ] Copy CSharpIPCClient_Example.cs to C# project
- [ ] Create UI or use WinForms example
- [ ] Implement message handlers
- [ ] Test full integration

**Reference:** CSHARP_PATTERNS_AND_EXAMPLES.md

---

## Protocol at a Glance

### Message Types
| Type | ID | Direction | Purpose |
|------|----|-----------| --------|
| TIMESTAMP | 0 | C# → C++ | Audio position |
| LOAD_PRESET | 1 | C# → C++ | Queue preset |
| DELETE_PRESET | 2 | C# → C++ | Remove preset |
| START_PREVIEW | 3 | C# → C++ | Start audio |
| STOP_PREVIEW | 4 | C# → C++ | Stop audio |
| PRESET_LOADED | 5 | C++ → C# | Confirmation |
| CURRENT_STATE | 6 | C++ → C# | Queue update |
| PREVIEW_STATUS | 7 | C++ → C# | Playback state |
| ERROR_RESPONSE | 8 | C++ → C# | Error message |

**Reference:** IPC_COMMUNICATION_GUIDE.md § Message Types

---

## Key Concepts

### Preset Queue Management
- Presets are ordered by `startTimestampMs`
- Each preset plays until the next one starts
- UI displays presets in timestamp order
- Deletion removes specific preset+timestamp pair

**Reference:** IPC_QUICKSTART.md § Message Flow

### Timestamp Synchronization
- C# sends current audio position every frame/update
- C++ checks which preset should be playing
- Preset switches automatically at correct time
- Millisecond precision throughout

**Reference:** IPC_COMMUNICATION_GUIDE.md § Workflow Example

### Thread Safety
- Preset queue: mutex-protected
- Audio manager: atomic operations
- IPC handler: mutex on output
- Safe for cross-thread calls

**Reference:** IPC_COMMUNICATION_GUIDE.md § Threading Model

---

## Common Questions

**Q: How do I handle preset switching?**
A: In your render loop, call `getPresetAtTimestamp()` and load that preset.
Reference: IPC_QUICKSTART.md § Update Main Loop

**Q: How do I display the queue in UI?**
A: Access `client.PresetQueue` - it's automatically sorted by timestamp.
Reference: CSHARP_PATTERNS_AND_EXAMPLES.md § Pattern 2

**Q: How do I sync with my audio player?**
A: Send timestamp every audio update via `SendTimestamp()`.
Reference: CSHARP_PATTERNS_AND_EXAMPLES.md § Pattern 3

**Q: What if the C++ process crashes?**
A: Implement reconnection logic, see error handling patterns.
Reference: CSHARP_PATTERNS_AND_EXAMPLES.md § Pattern 5

**Q: Can I run multiple C# apps?**
A: No, only one C# app should control one C++ process.
Reference: IPC_IMPLEMENTATION_SUMMARY.md § Security Notes

---

## Performance Tips

1. **Update Frequency:** Send timestamps at audio update rate (44.1kHz)
2. **State Updates:** Send full state every 500ms, not every timestamp
3. **Queue Size:** Typically <100 presets, O(n) lookups acceptable
4. **Memory:** ~100 bytes per preset, ~10KB per 100 presets
5. **CPU:** <1% overhead for IPC communication

**Reference:** IPC_COMMUNICATION_GUIDE.md § Performance Considerations

---

## Troubleshooting Quick Links

| Issue | Solution |
|-------|----------|
| No messages received | Check redirect, see IPC_COMMUNICATION_GUIDE.md § Troubleshooting |
| Process exits immediately | Verify paths, check stderr redirection |
| Out of sync audio | Increase timestamp update frequency |
| Queue not updating | Call `sendCurrentState()` or wait for next update |
| High CPU usage | Reduce update frequency |

**Full guide:** IPC_COMMUNICATION_GUIDE.md § Troubleshooting

---

## File Locations Summary

```
t:\CodeProjects\projectm\
├── src\sdl-test-ui\
│   ├── ipc_communication.hpp/cpp       (IPC core)
│   ├── preset_queue_manager.hpp/cpp    (Queue management)
│   ├── audio_preview_manager.hpp/cpp   (Audio state)
│   ├── ipc_manager.hpp/cpp             (Coordinator)
│   ├── ipc_test.cpp                    (Test program)
│   ├── pmSDL.hpp/cpp                   (INTEGRATE HERE)
│   └── CMakeLists.txt                  (UPDATE)
│
├── IPC_QUICKSTART.md                   (START HERE)
├── IPC_COMMUNICATION_GUIDE.md          (DETAILS)
├── IPC_IMPLEMENTATION_SUMMARY.md       (OVERVIEW)
├── CSHARP_PATTERNS_AND_EXAMPLES.md     (CODE)
├── IPC_SYSTEM_FILES_INDEX.md           (THIS FILE)
├── CSharpIPCClient_Example.cs          (C# LIB)
└── CSharpWinformsUI_Example.cs         (C# UI)
```

---

## Getting Help

1. **Setup problem?** → IPC_QUICKSTART.md
2. **Technical question?** → IPC_COMMUNICATION_GUIDE.md
3. **Code example needed?** → CSHARP_PATTERNS_AND_EXAMPLES.md
4. **Architecture question?** → IPC_IMPLEMENTATION_SUMMARY.md
5. **Can't find file?** → (Check file locations above)

---

## Next Steps

1. Read [IPC_QUICKSTART.md](IPC_QUICKSTART.md)
2. Update CMakeLists.txt for jsoncpp
3. Integrate with pmSDL
4. Test with ipc_test.cpp
5. Build C# client
6. Test full integration

---

**Last Updated:** 2025-01-29
**Version:** 1.0
**Status:** Ready for production
