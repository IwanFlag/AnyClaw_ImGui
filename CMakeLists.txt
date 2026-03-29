cmake_minimum_required(VERSION 3.16)
project(AnyClaw_ImGui VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies (ImGui + GLFW + GLAD) ──────────────────────────────────
include(cmake/deps.cmake)

# ── Core sources ─────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/tray.cpp
    src/gui.cpp
    src/openclaw_mgr.cpp
    src/settings.cpp
    src/health.cpp
    src/http_client.cpp
    src/lang.cpp
    src/selfcheck.cpp
    src/resources.rc
)

add_executable(AnyClaw_ImGui WIN32 ${SOURCES})

target_include_directories(AnyClaw_ImGui PRIVATE src)
target_link_libraries(AnyClaw_ImGui PRIVATE imgui)

target_link_libraries(AnyClaw_ImGui PRIVATE
    winhttp
    shell32
    user32
    gdi32
    comctl32
    ole32
    advapi32
    dwmapi
)

target_compile_definitions(AnyClaw_ImGui PRIVATE
    GLFW_EXPOSE_NATIVE_WIN32
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

set_target_properties(AnyClaw_ImGui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

