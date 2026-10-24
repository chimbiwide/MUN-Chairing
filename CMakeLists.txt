cmake_minimum_required(VERSION 3.22)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(CMAKE_HOST_WIN32 AND NOT CMAKE_GENERATOR MATCHES "MinGW Makefiles|Ninja|Visual Studio")
    message(FATAL_ERROR
        "On Windows, configure with the MinGW preset (gcc and mingw32-make on PATH):\n"
        "  cmake --preset windows\n"
        "  cmake --build --preset windows -j\n"
        "The generator '${CMAKE_GENERATOR}' needs the Visual Studio C compiler.")
endif()

if(UNIX AND NOT APPLE AND NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
    find_program(MUN_GCC16 gcc-16)
    if(MUN_GCC16)
        set(CMAKE_C_COMPILER ${MUN_GCC16} CACHE FILEPATH "C compiler")
    endif()
endif()

project(MUNTimer LANGUAGES C)

if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_C_COMPILER_VERSION VERSION_LESS 16)
    message(FATAL_ERROR
        "#embed needs GCC 16, but ${CMAKE_C_COMPILER} is GCC ${CMAKE_C_COMPILER_VERSION}.\n"
        "Install gcc-16 or configure with -DCMAKE_C_COMPILER=<path to GCC 16>.")
endif()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
set(PLATFORM Desktop CACHE STRING "raylib platform" FORCE)
set(OPENGL_VERSION OFF CACHE STRING "raylib OpenGL version")
set(USE_EXTERNAL_GLFW OFF CACHE STRING "Use system GLFW")
set(USE_AUDIO ON CACHE BOOL "raylib audio")
# X11/Wayland exist only on Linux. On Windows, GLFW's own options select Win32.
if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/third_party/raylib/cmake")
include(CompilerFlags)
add_subdirectory(third_party/raylib/src raylib)
# F12 would otherwise drop screenshot000.png files into the folder the chair runs from
target_compile_definitions(raylib PRIVATE SUPPORT_SCREEN_CAPTURE=0)

# chair logic without raylib, shared by the app and the tests
add_library(mun_core STATIC
    src/text.c
    src/duration.c
    src/clock.c
    src/speakers.c
    src/roster.c
    src/countries.c
    src/motion.c
    src/session.c
    src/chair.c
    src/store.c
)
target_include_directories(mun_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_options(mun_core PRIVATE -Wall -Wextra)
# CompilerFlags above resets CMAKE_C_STANDARD to 99, so each target asks for C23 itself
set_target_properties(mun_core PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)
if(UNIX)
    target_link_libraries(mun_core PUBLIC m)
endif()

add_executable(MUNTimer
    src/main.c
    src/gui.c
    src/assets.c
    src/ui.c
    src/demo.c
    src/view_floor.c
    src/view_motions.c
    src/view_setup.c
    src/view_rollcall.c
    src/view_present.c
    src/view_settings.c
)
# GUI subsystem: double-clicking the exe opens the app window only.
# The .rc is the Explorer icon. windres finds the .ico beside the script.
if(WIN32)
    set_target_properties(MUNTimer PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(MUNTimer PRIVATE data/app.rc)
endif()
target_include_directories(MUNTimer PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(MUNTimer PRIVATE mun_core raylib)
target_compile_options(MUNTimer PRIVATE -Wall -Wextra)
set_target_properties(MUNTimer PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)
# raygui is third party code, so its warnings are not ours to fix
set_source_files_properties(src/gui.c PROPERTIES COMPILE_OPTIONS "-w")
