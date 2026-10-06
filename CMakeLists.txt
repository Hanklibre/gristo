cmake_minimum_required(VERSION 3.22)

project(Gristo VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(GRISTO_BUILD_TESTS "Build the core unit tests" ON)
option(GRISTO_BUILD_PLUGIN "Build the JUCE plugin (VST3 / AU / Standalone)" ON)
set(GRISTO_JUCE_DIR "" CACHE PATH "Local JUCE checkout; fetched from GitHub when empty")

add_subdirectory(core)

if(GRISTO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GRISTO_BUILD_PLUGIN)
    if(GRISTO_JUCE_DIR)
        add_subdirectory(${GRISTO_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.4
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()
    add_subdirectory(plugin)
endif()

option(GRISTO_BUILD_TOOLS "Build the command-line render tool" ON)
if(GRISTO_BUILD_TOOLS)
    add_executable(gristo_render tools/render/main.cpp)
    target_link_libraries(gristo_render PRIVATE gristo_core)
endif()
