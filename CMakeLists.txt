cmake_minimum_required(VERSION 3.16)
project(AutoClicker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- Fetch Dear ImGui ---
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking # Or use a specific commit/tag like v1.89.9
)

FetchContent_MakeAvailable(imgui)
# ------------------------

# Add executable
add_executable(AutoClicker WIN32 
    src/main.cpp 
    src/AutoClicker.rc
)

# Copy icon file to build directory
add_custom_command(TARGET AutoClicker PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/src/cursor_208x256_1jZ_icon.ico
    ${CMAKE_CURRENT_BINARY_DIR}/cursor_208x256_1jZ_icon.ico
    COMMENT "Copying icon file to build directory"
)

# Include ImGui source files directly
target_sources(AutoClicker PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

# Include directories
target_include_directories(AutoClicker PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link necessary libraries
target_link_libraries(AutoClicker PRIVATE
    user32
    gdi32
    d3d11
    d3dcompiler # Required by imgui_impl_dx11
    dwmapi      # Often needed for modern Win32 features
)

# Add definitions
target_compile_definitions(AutoClicker PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

# --- Optional: Improve Debugging Experience ---
if(MSVC)
    # Enable helpful warnings
    target_compile_options(AutoClicker PRIVATE /W4 /WX-) # Warning level 4, don't treat warnings as errors

    # Set subsystem to Windows explicitly (usually handled by WIN32 flag)
    # target_link_options(AutoClicker PRIVATE /SUBSYSTEM:WINDOWS)

    # Add PDB information for debugging
    target_compile_options(AutoClicker PRIVATE /Zi)
    target_link_options(AutoClicker PRIVATE /DEBUG)
endif()

# --- Optional: Installation ---
# install(TARGETS AutoClicker DESTINATION bin)

# --- VSCode Specific Settings (CMake Tools extension) ---
# These settings help CMake Tools configure IntelliSense correctly
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Suggest using the Visual Studio generator if available
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    message(STATUS "Using Visual Studio generator.")
elseif(CMAKE_GENERATOR MATCHES "Ninja" AND CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
     message(STATUS "Using Ninja generator with MSVC.")
# Add other generator checks if needed (e.g., MinGW)
else()
    message(WARNING "Consider using a Visual Studio or Ninja (with MSVC) generator for best results on Windows.")
endif()