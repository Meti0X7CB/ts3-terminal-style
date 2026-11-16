cmake_minimum_required(VERSION 3.16)
project(terminalstyle VERSION 1.0.0 LANGUAGES CXX)

# Static C++ runtime: the plugin works without the Visual C++ redistributable.
# Safe here because nothing allocated by our CRT is freed by Qt's (and vice versa).
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)

# --- the style plugin: build/styles/terminalstyle.dll ---
add_library(terminalstyle MODULE
    src/terminalstyle.h
    src/terminalstyle.cpp
    src/plugin.cpp
    src/terminalstyle.json)
target_link_libraries(terminalstyle PRIVATE Qt5::Widgets dwmapi)
target_compile_definitions(terminalstyle PRIVATE QT_PLUGIN QT_NO_DEBUG)
set_target_properties(terminalstyle PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/styles
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/styles)

# --- gallery: renders sample widgets with the plugin to a PNG ---
add_executable(gallery test/gallery.cpp)
target_link_libraries(gallery PRIVATE Qt5::Widgets dwmapi)
