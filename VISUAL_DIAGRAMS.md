# IPC System - Visual Reference & Flowcharts

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        SYSTEM ARCHITECTURE                      │
└─────────────────────────────────────────────────────────────────┘

    C# Application                    C++ projectM Visualizer
    ┌──────────────────┐              ┌──────────────────────────┐
    │                  │              │                          │
    │  WinForms UI     │              │   pmSDL (Main App)       │
    │  or Console      │              │                          │
    │                  │              │  ┌────────────────────┐  │
    │  ┌────────────┐  │              │  │   IPCManager       │  │
    │  │ProjectMIPC │  │─ stdin/out ──┼──│                    │  │
    │  │Client      │  │   (JSON)     │  │ ┌──────────────┐  │  │
    │  └─────┬──────┘  │              │  │ │ IPCHandler   │  │  │
    │        │         │              │  │ │              │  │  │
    │  Presets Queue   │              │  │ │ Listening    │  │  │
    │  (ObsCollection)│              │  │ │ Thread       │  │  │
    │        │         │              │  │ └──────────────┘  │  │
    │        │         │              │  │ ┌──────────────┐  │  │
    │        └─────────┼──────────────┼──┼─│PresetQueue   │  │  │
    │                  │              │  │ │Manager       │  │  │
    │  Audio Player    │              │  │ │              │  │  │
    │  (timestamp)     │              │  │ │ - Add preset │  │  │
    │        │         │              │  │ │ - Get active │  │  │
    │        └─────────┼──────────────┼──┼─│ - Remove     │  │  │
    │                  │              │  │ └──────────────┘  │  │
    │                  │              │  │ ┌──────────────┐  │  │
    │                  │              │  │ │AudioPreview  │  │  │
    │                  │              │  │ │Manager       │  │  │
    │                  │              │  │ │              │  │  │
    │                  │              │  │ │ - Play/Stop  │  │  │
    │                  │              │  │ │ - Timestamp  │  │  │
    │                  │              │  │ └──────────────┘  │  │
    │                  │              │  └────────────────────┘  │
    │                  │              │                          │
    │                  │              │  Render Loop             │
    │                  │              │  - Get preset at time    │
    │                  │              │  - Load preset           │
    │                  │              │  - Send state updates    │
    │                  │              │                          │
    └──────────────────┘              └──────────────────────────┘
```

## Message Flow Diagrams

### Scenario 1: Load Preset at Timestamp

```
Timeline (milliseconds):

C# Application                         C++ projectM
─────────────────────────────────────────────────────

User selects "cool.milk" for 5000ms
       │
       │ LoadPreset("cool.milk", 5000)
       ├─────────────────────────────────────────────→
       │        {"type":1,"data":{"presetName":"cool.milk"...}}
       │
       │                                  Add to queue
       │                                  (sorted by timestamp)
       │
       │ ←─────────────────────────────────────────────
       │ PRESET_LOADED confirmation
       │
UI updated with queue
[0ms: intro.milk]
[5000ms: cool.milk]
[10000ms: outro.milk]
```

### Scenario 2: Audio Playback with Preset Switching

```
Timeline:

C# Audio Player               C++ projectM              Visual Output
───────────────────────────────────────────────────────────────────

Start audio at 0ms
│
├─ Send timestamp: 0ms ───────────→ Load "intro.milk"  ──→ Show intro
│
├─ Send timestamp: 2500ms ────────→ Still "intro.milk"
│
├─ Send timestamp: 5000ms ────────→ Load "cool.milk"   ──→ Transition
│
├─ Send timestamp: 7500ms ────────→ Still "cool.milk"
│
├─ Send timestamp: 10000ms ──────→ Load "outro.milk"   ──→ Outro
│
└─ Send timestamp: 15000ms ──────→ Still "outro.milk"
```

### Scenario 3: Delete Preset from Queue

```
Before:
┌─────────────────────────┐
│ Preset Queue            │
├─────────────────────────┤
│ intro.milk      at 0ms  │
│ cool.milk       at 5ms  │ ← User selects and deletes
│ outro.milk     at 10ms  │
└─────────────────────────┘

Delete Request:
C#: DeletePreset("cool.milk", 5000)
    ↓
    {"type":2,"data":{"presetName":"cool.milk","timestampMs":5000}}
    ↓
C++: Remove from queue

After:
┌─────────────────────────┐
│ Preset Queue            │
├─────────────────────────┤
│ intro.milk      at 0ms  │
│ outro.milk     at 10ms  │
└─────────────────────────┘
```

## Message Sequence Diagram

```
C# Client                          C++ Server
    │                                  │
    │ 1. TIMESTAMP(0)                  │
    ├─────────────────────────────────→│
    │                                  │ Check queue
    │                                  │
    │ 2. LOAD_PRESET("cool", 5000)    │
    ├─────────────────────────────────→│
    │                                  │ Add to queue
    │ 3. PRESET_LOADED confirm        │
    │←─────────────────────────────────┤
    │                                  │
    │ 4. START_PREVIEW                │
    ├─────────────────────────────────→│
    │                                  │ Begin audio
    │                                  │
    │ 5. TIMESTAMP(1000)               │
    ├─────────────────────────────────→│
    │                                  │
    │ 6. PREVIEW_STATUS               │
    │←─────────────────────────────────┤
    │                                  │
    │ 7. TIMESTAMP(5000)               │
    ├─────────────────────────────────→│
    │                                  │ Preset switch!
    │ 8. CURRENT_STATE                │
    │←─────────────────────────────────┤
    │                                  │
    │ 9. DELETE_PRESET("cool", 5000)  │
    ├─────────────────────────────────→│
    │                                  │ Remove from queue
    │                                  │
    │ 10. CURRENT_STATE (updated)     │
    │←─────────────────────────────────┤
    │                                  │
    │ 11. STOP_PREVIEW                │
    ├─────────────────────────────────→│
    │                                  │ Stop audio
    │                                  │
```

## Data Structure Diagrams

### Preset Queue (Sorted by Timestamp)

```
┌─────────────────────────────────────────────────────────┐
│  PresetQueueManager                                    │
│                                                         │
│  std::vector<PresetEntry> presets  (sorted)            │
│  std::mutex mutex  (thread safety)                     │
│                                                         │
│  presets:                                               │
│  ┌──────────────────────────────────┐                  │
│  │ PresetEntry[0]                   │                  │
│  │ - presetName: "intro.milk"       │                  │
│  │ - startTimestampMs: 0            │                  │
│  └──────────────────────────────────┘                  │
│  ┌──────────────────────────────────┐                  │
│  │ PresetEntry[1]                   │                  │
│  │ - presetName: "verse.milk"       │                  │
│  │ - startTimestampMs: 5000         │                  │
│  └──────────────────────────────────┘                  │
│  ┌──────────────────────────────────┐                  │
│  │ PresetEntry[2]                   │                  │
│  │ - presetName: "chorus.milk"      │                  │
│  │ - startTimestampMs: 15000        │                  │
│  └──────────────────────────────────┘                  │
│                                                         │
└─────────────────────────────────────────────────────────┘

Timestamp Lookup Example:
currentTime = 7000ms
→ Search: Find largest startTimestampMs ≤ 7000ms
→ Result: "verse.milk" (started at 5000ms)
```

### JSON Message Structure

```
┌─────────────────────────────────────────────────┐
│ IPCMessage                                      │
│                                                 │
│ ┌──────────────────┐                           │
│ │ "type": 0        │ ← Message type enum       │
│ └──────────────────┘                           │
│                                                 │
│ ┌──────────────────────────────────────────┐   │
│ │ "data": {                                 │   │
│ │   "field1": value1,                       │   │
│ │   "field2": value2,                       │   │
│ │   ...                                     │   │
│ │ }                                         │   │
│ └──────────────────────────────────────────┘   │
│                                                 │
│ Serialized: Single line JSON                   │
│ {"type":0,"data":{"field1":value1}}           │
│                                                 │
└─────────────────────────────────────────────────┘
```

## Thread Safety Model

```
Main Render Thread              IPC Thread              Audio Thread
──────────────────────────────────────────────────────────────────

│                               │                       │
├──────────────────────────────┬┼─────────────────────→ Get timestamp
│                              ││
├─ ipcManager->getPresetQueue()││
│  .getPresetAtTimestamp(ms)   ││
│ ↓ (mutex protected)          ││
├─ Check queue                 ││
│ ↓                            ││
├─ Load preset if changed      ││
│ ↓                            ││
├─ Call sendCurrentState()     ││
│ (if pending)                 ││
│ ↓ (mutex on write)           ││
├──────────────────────────────┼┼────→ stdout
│                              ││      (to C#)
                               ││
                               ├─ Listen on stdin
                               │ ↓
                               ├─ Parse JSON
                               │ ↓
                               ├─ Call callback
                               │ (from main thread)
                               │ ↓
                               ├─ Add to queue
                               │ (mutex protected)
                               │ ↓
                               ├─ Send confirm
                               │ (mutex on write)
                               │ ↓
                               └──→ stdout
```

## State Machine: Audio Preview

```
                    ┌──────────────────┐
                    │  STOPPED state   │
                    └──────────────────┘
                            ▲
                            │
                startPreview()
                            │
                            ▼
                    ┌──────────────────┐
                    │  PLAYING state   │
                    └──────────────────┘
                            │
                ┌───────────┼───────────┐
                │           │           │
          pausePreview() PLAYING PLAYING
                │           │           │
                ▼           │           ▼
        ┌──────────────────┐│    stopPreview()
        │  PAUSED state    ││            │
        └──────────────────┘│            │
                ▲          │            │
                │          └────────────┤
         resumePreview()   │            │
                │          │            │
                └──────────┼────────────┘
                           │
                           ▼
                    ┌──────────────────┐
                    │  STOPPED state   │
                    └──────────────────┘
```

## Performance Profile

```
Operation                Time        Memory        Frequency
─────────────────────────────────────────────────────────────
Send timestamp          <1ms        ~50 bytes     Every frame (60+fps)
Load preset            <1ms        ~100 bytes    On demand
Delete preset          <1ms        var           On demand
Get preset at time     <1ms        0 bytes       Every frame
State update (100px)   <2ms        ~2KB          Every 500ms
Total per frame:       ~2ms        ~50KB         Continuous

Overhead:
- CPU: <1% at 60fps
- Memory: ~50KB baseline + 100B per preset
- Threads: +1 (IPC listening)
```

## Integration Points in projectMSDL

```
projectMSDL Class
│
├─ Constructor
│  ├─ Create ipcManager
│  └─ Initialize IPC
│
├─ Destructor
│  ├─ Shutdown IPC
│  └─ Delete ipcManager
│
├─ renderFrame() or mainLoop()
│  ├─ Update audio timestamp
│  ├─ Get preset at timestamp
│  ├─ Load if changed
│  ├─ Send state updates (periodic)
│  └─ Send preview status (periodic)
│
└─ (existing functions remain unchanged)
```

---

These visual diagrams help understand:
- System architecture
- Message flow timing
- Data structure organization
- Threading model
- State transitions
- Integration points
