if(NOT ENABLE_SDL_UI OR ENABLE_EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL Android)
    return()
endif()

add_executable(LvsAudioReactiveVisualizer
        ConfigFile.cpp
        ConfigFile.h
        audio_loader.cpp
        loopback.cpp
        loopback.hpp
        stb_image_write_impl.cpp
        debug_ipc_ui.cpp
        debug_ipc_ui.hpp
        pmSDL.cpp
        pmSDL.hpp
        logging.cpp
        logging.hpp
        projectM_SDL_main.cpp
        setup.cpp
        setup.hpp
        # IPC System for preset queue management
        ipc_communication.cpp
        ipc_communication.hpp
        preset_queue_manager.cpp
        preset_queue_manager.hpp
        ipc_manager.cpp
        ipc_manager.hpp
        )

target_link_libraries(LvsAudioReactiveVisualizer
        PRIVATE
        libprojectM::playlist
        GLM::GLM
        SDL2::SDL2
        SDL2::SDL2main
        JsonCpp::JsonCpp
        ${CMAKE_DL_LIBS}
        glad
        )

# Integrate Dear ImGui via FetchContent and compile its sources into this target
include(FetchContent)
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.89.8
)
FetchContent_MakeAvailable(imgui)

if (imgui_POPULATED)
    message(STATUS "Adding Dear ImGui sources to LvsAudioReactiveVisualizer")
    set(IMGUI_SOURCES
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
    )
    target_sources(LvsAudioReactiveVisualizer PRIVATE ${IMGUI_SOURCES})
    target_include_directories(LvsAudioReactiveVisualizer PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
endif()

if(MSVC)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        set_target_properties(LvsAudioReactiveVisualizer
                PROPERTIES
                VS_DPI_AWARE "PerMonitor"
                )
    else()
        message(AUTHOR_WARNING
                "You're using a CMake version less than 3.16 with Visual Studio.\n"
                "The resulting projectMSDL executable will not be DPI-aware and possibly render at a "
                "lower-than-expected resolution on high-DPI displays.")
    endif()

    add_custom_command(TARGET LvsAudioReactiveVisualizer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:libprojectM::projectM>
            $<TARGET_FILE:libprojectM::playlist>
            $<TARGET_FILE_DIR:LvsAudioReactiveVisualizer>
            )
endif()
