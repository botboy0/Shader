cmake_minimum_required(VERSION 3.20)
project(magnetar LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- glad (OpenGL 4.5 core loader) ---
add_library(glad STATIC src/glad/glad.c)
target_include_directories(glad PUBLIC src/glad)

# --- SDL3 ---
find_package(SDL3 REQUIRED)

# --- OpenGL (portable: resolves to opengl32 on Windows, -lGL on Linux) ---
find_package(OpenGL REQUIRED)

# --- ImGui (docking branch, SDL3 + OpenGL3 backends) ---
add_library(imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/backends/imgui_impl_sdl3.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    external/imgui
    external/imgui/backends
)
target_include_directories(imgui PRIVATE
    src/glad   # glad.h for custom GL loader
    src/ui     # magnetar_imconfig.h
)
target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM  # Use glad instead of ImGui's built-in GL loader
)
target_compile_definitions(imgui PRIVATE
    IMGUI_USER_CONFIG="magnetar_imconfig.h"  # Include glad.h via ImGui's config mechanism
)
target_link_libraries(imgui PUBLIC SDL3::SDL3 glad)

# --- ImGuiColorTextEdit (vendored, header + impl) ---
add_library(imgui_color_text_edit STATIC
    external/ImGuiColorTextEdit/TextEditor.cpp
)
target_include_directories(imgui_color_text_edit PUBLIC
    external/ImGuiColorTextEdit
)
target_link_libraries(imgui_color_text_edit PUBLIC imgui)

# --- Main executable ---
add_executable(magnetar
    src/main.cpp
    src/core/app.cpp
    src/core/renderer.cpp
    src/core/frame_control.cpp
    src/ui/imgui_manager.cpp
    src/ui/layout.cpp
    src/ui/theme.cpp
    src/ui/settings.cpp
    src/ui/tab_manager.cpp
    src/ui/file_dialog.cpp
    src/ui/template_picker.cpp
    src/ui/shortcuts.cpp
    src/ui/session.cpp
    src/editor/code_editor.cpp
    src/editor/compiler.cpp
    src/editor/autocomplete.cpp
    src/shader/shadertoy_format.cpp
    src/shader/uniform_bridge.cpp
    src/shader/multipass_pipeline.cpp
    src/shader/multipass_parser.cpp
)

# --- Embed application icon on Windows ---
if(WIN32)
    target_sources(magnetar PRIVATE src/magnetar.rc)
endif()

target_include_directories(magnetar PRIVATE src external)
target_link_libraries(magnetar PRIVATE SDL3::SDL3 glad imgui imgui_color_text_edit OpenGL::GL)

# --- GUI subsystem on Windows (no console window) ---
if(WIN32)
    target_link_options(magnetar PRIVATE -mwindows)
endif()

# --- Copy runtime DLLs next to executable (MinGW builds) ---
if(WIN32 AND MINGW)
    set(_MINGW_BIN_DIR "${CMAKE_FIND_ROOT_PATH}/bin")
    if(NOT EXISTS "${_MINGW_BIN_DIR}")
        # Fallback: derive from compiler path
        get_filename_component(_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    endif()
    set(_RUNTIME_DLLS
        libgcc_s_seh-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
    )
    foreach(_dll ${_RUNTIME_DLLS})
        if(EXISTS "${_MINGW_BIN_DIR}/${_dll}")
            add_custom_command(TARGET magnetar POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_MINGW_BIN_DIR}/${_dll}"
                    "$<TARGET_FILE_DIR:magnetar>"
                COMMENT "Copying ${_dll}"
            )
        endif()
    endforeach()

    # Copy SDL3.dll from its imported location
    add_custom_command(TARGET magnetar POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:magnetar>"
        COMMENT "Copying SDL3.dll"
    )
endif()
