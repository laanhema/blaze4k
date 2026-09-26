cmake_minimum_required(VERSION 3.24)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
project(tundra-dance VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

# 1. SDL3
set(SDL_SHARED OFF CACHE BOOL "Build SDL static library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.8
    GIT_SHALLOW TRUE
)

# 2. GLAD (OpenGL 3.3 Core)
set(GLAD_PROFILE "core" CACHE STRING "GLAD OpenGL profile" FORCE)
set(GLAD_API "gl=3.3" CACHE STRING "GLAD API" FORCE)
set(GLAD_GENERATOR "c" CACHE STRING "GLAD Language" FORCE)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
    GIT_SHALLOW TRUE
)

# 3. nlohmann/json
set(JSON_BuildTests OFF CACHE BOOL "Build JSON tests" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

# 4. miniaudio
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
    GIT_SHALLOW TRUE
)

# 5. stb (stb_truetype, etc.)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3 glad nlohmann_json miniaudio stb)

# Create interface target for miniaudio
if(NOT TARGET miniaudio)
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})
endif()

# Create interface target for stb
if(NOT TARGET stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

# Core library target
add_library(tundra_core STATIC
    src/app/window.cpp
    src/app/app.cpp
)

target_include_directories(tundra_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tundra_core PUBLIC
    SDL3::SDL3-static
    glad
    nlohmann_json::nlohmann_json
    miniaudio
    stb
    ${CMAKE_DL_LIBS}
)

# Main executable
add_executable(tundra-dance
    src/main.cpp
)

target_link_libraries(tundra-dance PRIVATE
    tundra_core
)

# Testing
enable_testing()
add_subdirectory(tests)
