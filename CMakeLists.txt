cmake_minimum_required(VERSION 3.16)
project(KidDraw VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- SDL2 discovery (supports: vcpkg / apt / homebrew) ----
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(sdl2 CONFIG QUIET)
endif()
if(NOT SDL2_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
    endif()
endif()

add_executable(kid_draw src/main.cpp)

target_compile_options(kid_draw PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

target_link_libraries(kid_draw PRIVATE SDL2::SDL2)
