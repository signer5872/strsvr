cmake_minimum_required(VERSION 3.5)
project(strsvr VERSION 1.0.0 LANGUAGES CXX)

# 代码只用到 C++14 特性，这样 CMake 3.5 也能识别（17 需要 3.8+）
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt5 components (no hard version pin: works with Qt 5.8+)
find_package(Qt5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    SerialPort
)
message(STATUS "Qt5 version: ${Qt5Core_VERSION}")

# Optional: Qt5 PrintSupport for reports
find_package(Qt5 OPTIONAL_COMPONENTS PrintSupport)

# Source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

file(GLOB_RECURSE UI_FILES
    "src/*.ui"
)

# Create executable
add_executable(strsvr
    ${SOURCES}
    ${UI_FILES}
)

target_include_directories(strsvr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(strsvr PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::SerialPort
)

if(Qt5PrintSupport_FOUND)
    target_link_libraries(strsvr PRIVATE Qt5::PrintSupport)
endif()

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_compile_definitions(strsvr PRIVATE
        LINUX
        _GNU_SOURCE
    )
    # Link pthread
    target_link_libraries(strsvr PRIVATE pthread)
endif()

if(WIN32)
    target_compile_definitions(strsvr PRIVATE
        WIN32
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Installation
install(TARGETS strsvr DESTINATION bin)

message(STATUS "========================================")
message(STATUS "STRSVR Linux Port v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")
