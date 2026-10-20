# LVS visualizer: Linux AddressSanitizer diagnostics

This is a **diagnostic-only** build. It does not replace or modify the normal release binaries.
The asynchronous preset discovery and UI batching remain enabled, including for 9,000+ presets.

## Configure and build

Install the Linux development dependencies normally required by this repository (SDL2,
OpenGL, JsonCpp and any dependencies described in the repository build instructions).
From the repository root, using GCC or Clang:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_SDL_UI=ON -DLVS_ENABLE_ASAN=ON
cmake --build build-asan --target LvsAudioReactiveVisualizer -j2
```

The option instruments the projectM targets built in this CMake tree as well as the
visualizer; it is opt-in and **off by default**. For reliable traces use a fresh build
directory and do not substitute non-instrumented copies of projectM shared libraries.
For a fully instrumented dependency graph, external/prebuilt dependencies may need
separate sanitizer builds.

## Reproduce the crash

Launch the built `LvsAudioReactiveVisualizer` with the same arguments used by LVS,
including `--preset-dir` and `--audio`, using real paths on your machine.
Capture stderr; the visualizer uses stdout for IPC.

```bash
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:print_stacktrace=1 \
  ./build-asan/src/lvs-ui/LvsAudioReactiveVisualizer \
  --preset-dir "/path/to/milkPresets" --audio "/path/to/audio.wav" \
  2> visualizer-asan.log
```

Adjust the executable path for your generator's build layout. For Snap Steam
testing, ensure the instrumented executable and its ASan runtime are visible
inside the Steam process runtime; if not, run the binary directly outside
Steam first. Avoid shipping ASan binaries in the production Steam depot.

Look for `ERROR: AddressSanitizer`, its stack trace, and the surrounding
`[startup +...ms]` entries. Record which environment (native Steam vs.
Snap Steam), preset directory and GPU/driver was used.

## Added diagnostic boundaries

- Entry to the render loop and the first frame.
- Entry to the preset-discovery worker, including thread ID.
- The first directory entry, first matching preset, and progress every 2,000 entries.
- Playlist insertion batches, with counts, timing, and main-thread ID.
- Worker completion or caught filesystem errors.

The output intentionally avoids a log entry for every one of the 9,000+
presets. The scan and queue algorithm are unchanged.
