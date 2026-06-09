cmake_minimum_required(VERSION 4.2)

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_GENERATOR_TOOLSET "v143" CACHE STRING "Platform Toolset" FORCE)
endif()

project(NetcodeInterop LANGUAGES CXX)
project(NetcodeInterop VERSION 0.5.1)

if(MSVC AND NOT CMAKE_VS_PLATFORM_TOOLSET STREQUAL "v143")
    message(FATAL_ERROR "This project requires MSVC platform toolset v143.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(NetcodeInterop SHARED
    ../ggpo/src/lib/CReplayFile.cpp
    ../ggpo/src/lib/CReplayFile.h
    ../ggpo/src/lib/game_input.h
    NetcodeInterop.cpp
    NetcodeInterop.h
)

target_include_directories(NetcodeInterop PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../ggpo/src/lib
)