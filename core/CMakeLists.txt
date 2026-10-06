# Host-independent engine: no JUCE, no allocations on the audio path beyond the
# pending-event vector reserved up front.
add_library(gristo_core STATIC
    src/DrumMap.cpp
    src/Engine.cpp
    src/Groove.cpp
    src/Humanizer.cpp
    src/MapData.cpp
    src/PatternGenerator.cpp
)

target_include_directories(gristo_core PUBLIC include)

if(MSVC)
    target_compile_options(gristo_core PRIVATE /W4)
else()
    target_compile_options(gristo_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()

# Regenerate src/MapData.cpp from data/corpus (needs python3 + numpy).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(gristo_map
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/mapbuilder/build_map.py
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Rebuilding the drum map from data/corpus")
endif()
