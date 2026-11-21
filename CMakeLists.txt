cmake_minimum_required(VERSION 3.16)
project(ArcadeZombieFPS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "macOS deployment target")
endif()

# Raylib via FetchContent (no external package manager required).
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
)
FetchContent_MakeAvailable(raylib)

add_executable(ArcadeZombieFPS
    src/main.cpp
    src/game.cpp
    src/player.cpp
    src/weapon.cpp
    src/zombie.cpp
    src/world.cpp
)

target_include_directories(ArcadeZombieFPS PRIVATE include)

target_link_libraries(ArcadeZombieFPS PRIVATE raylib)

if(APPLE)
    target_link_libraries(ArcadeZombieFPS PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

set_target_properties(ArcadeZombieFPS PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.arcadezombiefps"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
    XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++17"
)
