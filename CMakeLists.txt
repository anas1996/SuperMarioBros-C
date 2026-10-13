cmake_minimum_required(VERSION 3.5)

project(smbc)

add_subdirectory(codegen)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    source/Configuration.cpp
    source/Main.cpp
    source/Emulation/APU.cpp
    source/Emulation/Controller.cpp
    source/Emulation/Palette.cpp
    source/Emulation/PPU.cpp
    source/SMB/SMB.cpp
    source/SMB/SMBData.cpp
    source/SMB/SMBEngine.cpp
    source/Util/Video.cpp)

find_package(SDL2 REQUIRED)

add_executable(smbc ${SOURCE_FILES})
target_link_libraries(smbc ${SDL2_LIBRARY} ${SDL2_LIBRARIES})

add_dependencies(smbc codegen)
