cmake_minimum_required(VERSION 4.2)

project(ReplayInterop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(NetcodeCore SHARED
    ../ggpo/src/lib/CReplayFile.cpp
    ../ggpo/src/lib/CReplayFile.h
    ../ggpo/src/lib/game_input.h
    NetcodeInterop.cpp
    NetcodeInterop.h
)

target_include_directories(NetcodeCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../ggpo/src/lib
)