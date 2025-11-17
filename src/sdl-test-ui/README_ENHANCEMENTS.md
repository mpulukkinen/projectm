# projectM SDL Test UI - Enhanced with CLI, Rendering, and Preview

This is an enhanced version of the projectM SDL2 test UI with the following new features:

## Features

### Command-Line Arguments
- `--preset-dir DIR` — Load presets from a custom directory (default: system preset path)
- `--audio FILE` — Load and playback audio (WAV, or OGG/MP3 if SDL_mixer available)
- `--out-dir DIR` — Output directory for rendered frame sequence
- `--fps N` — Target FPS for rendering (default: 60)
- `--res WxH,...` — Render resolutions (comma-separated, e.g., `1280x720,1920x1080`)
- `--list-presets` — List all discovered presets and exit
- `--help` — Show usage help

### Preview Mode
- **F5 key** or click the **Preview** button to play audio and feed it to projectM for visualization
- Audio plays while the visualization responds in real-time

### Render Mode
- **F6 key** or click the **Render** button to render the visualization to disk
- Renders all specified resolutions for each frame
- Output format: **JPEG** files named `frame_<width>x<height>_<framenum>.jpg`
- All frames respect the target FPS and audio duration

### Preset Discovery
- Shows all discovered presets when `--list-presets` is used
- Presets are scanned from the specified `--preset-dir` or system default

### On-Screen UI
- **Preview / Render buttons** rendered with labels using SDL_ttf (if available)
- Buttons are clickable with mouse or keyboard shortcuts (F5/F6)
- Clean semi-transparent overlay on the visualization

## Supported Formats

### Audio Input
- **WAV** (always supported via SDL_LoadWAV)
- **OGG, MP3** (supported if SDL_mixer is available in the build)

### Output
- **JPEG** (using stb_image_write)

## Building

### Prerequisites
Ensure your vcpkg manifest includes (updated automatically):
- `sdl2`
- `sdl2-ttf` (for on-screen text labels)
- `sdl2-mixer` (for OGG/MP3 support; optional)
- `glew` (Windows)
- `glm`

### Build Steps
cmake -G "Visual Studio 17 2022" -A "X64" -DCMAKE_TOOLCHAIN_FILE="T:\CodeProjects\vcpkg\scripts\buildsystems\vcpkg.cmake" -DENABLE_SDL_UI=ON -DVCPKG_FEATURE=gui -S . -B ./output
```bash
cd projectm
mkdir build && cd build
cmake -DENABLE_SDL_UI=ON -DVCPKG_FEATURE=gui ..
cmake --build . --config Release
```

The executable will be `LvsAudioReactiveVisualizer`.

## Usage Examples

### List presets in a custom directory
```bash
./LvsAudioReactiveVisualizer --preset-dir /path/to/presets --list-presets
```

### Preview audio with visualization
```bash
./LvsAudioReactiveVisualizer --preset-dir /path/to/presets --audio song.wav
# Then press F5 or click Preview button in the window
```

### Render visualization to JPEG sequence
```bash
./LvsAudioReactiveVisualizer \
  --preset-dir /path/to/presets \
  --audio song.wav \
  --out-dir /path/to/output \
  --fps 30 \
  --res 1280x720,1920x1080
# Then press F6 or click Render button
```

Output files:
- `/path/to/output/frame_1280x720_000000.jpg`
- `/path/to/output/frame_1280x720_000001.jpg`
- `/path/to/output/frame_1920x1080_000000.jpg`
- `/path/to/output/frame_1920x1080_000001.jpg`
- ... (one file per frame per resolution)

### Show help
```bash
./LvsAudioReactiveVisualizer --help
```

## Architecture

### New Files
- `audio_loader.cpp` — Loads WAV (always) and OGG/MP3 (if SDL_mixer available)
- `text_render.cpp` / `text_render.h` — SDL_ttf-based text rendering for UI labels
- `stb_image_write_impl.cpp` — Wrapper to include stb_image_write from SOIL2 vendor tree

### Modified Files
- `projectM_SDL_main.cpp` — Added CLI parsing, audio loading, preset listing
- `setup.cpp` / `setup.hpp` — Accept custom preset directory
- `pmSDL.cpp` / `pmSDL.hpp` — Added preview, render, and CLI configuration methods
- `CMakeLists.txt` — Added new source files and SDL2_ttf/SDL2_mixer dependencies
- `vcpkg.json` — Added SDL2_ttf and SDL2_mixer to gui feature

## Keyboard Controls

### Original Controls
- **R** — Random preset
- **Y** — Toggle shuffle
- **Left/Right arrows** — Previous/next preset
- **Up/Down arrows** — Adjust beat sensitivity
- **Space** — Lock/unlock preset
- **Scroll wheel** — Previous/next preset
- **Ctrl/Cmd+F** — Toggle fullscreen
- **Ctrl/Cmd+Q** — Quit

### New Controls
- **F5** — Preview audio
- **F6** — Render sequence
- **Mouse click** on on-screen buttons — Trigger preview/render

## Limitations & Notes

1. **Audio Format Support**: WAV is always supported. OGG/MP3 require SDL_mixer to be available. The code gracefully handles missing dependencies.
2. **Text Rendering**: If SDL_ttf is not available, buttons will not show labels but remain clickable.
3. **Single-Resolution Preview**: Preview mode plays audio for visualization (not confined to one resolution). Render mode supports multiple resolutions.
4. **Frame Output**: JPEG files are written one per frame per resolution to disk. Large audio files + high FPS can produce many files.
5. **Threading**: Preview mode runs in a background thread to avoid blocking the UI.

## Future Enhancements

- MP3 support without external dependencies (using libmp3lame or similar)
- PNG output format
- Real-time progress indicator during render
- Batch preset rendering
- Custom audio playback device selection
