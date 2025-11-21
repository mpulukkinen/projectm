# CMakeLists.txt Integration for IPC System
#
# This is a snippet to add to your existing CMakeLists.txt
# Located at: src/sdl-test-ui/CMakeLists.txt

# ============================================================================
# IPC COMMUNICATION SYSTEM - Add this section
# ============================================================================

# Find or add jsoncpp dependency
find_package(jsoncpp REQUIRED)

# Add IPC source files to your executable/library target
target_sources(LvsAudioReactiveVisualizer PRIVATE
    # Existing sources...
    # ... your existing .cpp files ...

    # IPC Communication System (NEW)
    ipc_communication.hpp
    ipc_communication.cpp
    preset_queue_manager.hpp
    preset_queue_manager.cpp
    audio_preview_manager.hpp
    audio_preview_manager.cpp
    ipc_manager.hpp
    ipc_manager.cpp
)

# Link jsoncpp library
target_link_libraries(LvsAudioReactiveVisualizer PRIVATE jsoncpp)

# Optional: Build standalone test program
if(BUILD_TESTS OR BUILD_IPC_TEST)
    add_executable(ipc_test
        ipc_test.cpp
        ipc_communication.cpp
        preset_queue_manager.cpp
        audio_preview_manager.cpp
        ipc_manager.cpp
    )

    target_link_libraries(ipc_test PRIVATE jsoncpp)

    # Include directories
    target_include_directories(ipc_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# ============================================================================
# END IPC COMMUNICATION SYSTEM
# ============================================================================


# ============================================================================
# INSTALLATION (if building vcpkg with jsoncpp)
# ============================================================================
#
# Before building, ensure jsoncpp is installed:
#
# Using vcpkg:
#   vcpkg install jsoncpp
#
# Or using system package manager:
#   Ubuntu/Debian: sudo apt-get install libjsoncpp-dev
#   macOS: brew install jsoncpp
#   Windows (vcpkg): vcpkg install jsoncpp
#
# ============================================================================


# ============================================================================
# CMAKE CONFIGURATION OPTIONS
# ============================================================================
#
# Add these options to your top-level CMakeLists.txt if you want:
#
# option(BUILD_IPC_TEST "Build IPC test program" OFF)
# option(ENABLE_IPC_LOGGING "Enable IPC debug logging" OFF)
#
# Then use in your code:
# if(ENABLE_IPC_LOGGING)
#     target_compile_definitions(LvsAudioReactiveVisualizer PRIVATE IPC_DEBUG_LOGGING=1)
# endif()
#
# ============================================================================


# ============================================================================
# COMPILER FLAGS (if needed)
# ============================================================================
#
# If you need C++17 or later (recommended):
#
# set_target_properties(LvsAudioReactiveVisualizer PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED ON
# )
#
# ============================================================================


# ============================================================================
# COMPLETE EXAMPLE (entire CMakeLists.txt section)
# ============================================================================
#
# Here's a complete working example you can use as reference:
#
# cmake_minimum_required(VERSION 3.10)
# project(LvsAudioReactiveVisualizer)
#
# # C++ Standard
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
#
# # Find dependencies
# find_package(SDL2 REQUIRED)
# find_package(OpenGL REQUIRED)
# find_package(jsoncpp REQUIRED)
# find_package(projectm CONFIG REQUIRED)
#
# # Create executable
# add_executable(LvsAudioReactiveVisualizer
#     # Existing sources
#     projectM_SDL_main.cpp
#     pmSDL.cpp
#     setup.cpp
#     audioCapture.cpp
#     loopback.cpp
#     audio_loader.cpp
#     ConfigFile.cpp
#     stb_image_write.cpp
#     stb_image_write_impl.cpp
#
#     # IPC System (NEW)
#     ipc_communication.cpp
#     preset_queue_manager.cpp
#     audio_preview_manager.cpp
#     ipc_manager.cpp
# )
#
# # Link libraries
# target_link_libraries(LvsAudioReactiveVisualizer PRIVATE
#     SDL2::SDL2
#     OpenGL::OpenGL
#     jsoncpp
#     projectm::projectm
# )
#
# # Include directories
# target_include_directories(LvsAudioReactiveVisualizer PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${SDL2_INCLUDE_DIRS}
# )
#
# # IPC Test executable (optional)
# add_executable(ipc_test
#     ipc_test.cpp
#     ipc_communication.cpp
#     preset_queue_manager.cpp
#     audio_preview_manager.cpp
#     ipc_manager.cpp
# )
#
# target_link_libraries(ipc_test PRIVATE jsoncpp)
#
# ============================================================================


# ============================================================================
# PLATFORM-SPECIFIC NOTES
# ============================================================================
#
# Windows (MSVC):
# - jsoncpp available through vcpkg
# - Command: vcpkg install jsoncpp:x64-windows
# - CMake automatically finds it if VCPKG_ROOT is set
#
# Linux (GCC/Clang):
# - Install: sudo apt-get install libjsoncpp-dev
# - Usually auto-detected by find_package()
#
# macOS (Clang):
# - Install: brew install jsoncpp
# - Or use MacPorts: sudo port install jsoncpp
#
# ============================================================================


# ============================================================================
# TROUBLESHOOTING
# ============================================================================
#
# Error: "jsoncpp not found"
# Solution: Install jsoncpp development files, then clear CMake cache
#
# Error: "ipc_communication.hpp: No such file or directory"
# Solution: Verify files are in src/sdl-test-ui/ directory
#
# Error: "Linking failed"
# Solution: Ensure target_link_libraries includes jsoncpp
#
# ============================================================================
