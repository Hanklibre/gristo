# Mutable Instruments Rings, the module's own code (MIT), unchanged but for portability.
set(MI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/eurorack)
add_library(mi_rings STATIC
    ${MI_DIR}/rings/dsp/part.cc
    ${MI_DIR}/rings/dsp/resonator.cc
    ${MI_DIR}/rings/dsp/string.cc
    ${MI_DIR}/rings/dsp/fm_voice.cc
    ${MI_DIR}/rings/resources.cc
    ${MI_DIR}/stmlib/dsp/units.cc
    ${MI_DIR}/stmlib/utils/random.cc
)
target_include_directories(mi_rings SYSTEM PUBLIC ${MI_DIR})
set_target_properties(mi_rings PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(mi_rings PRIVATE /W0)
else()
    target_compile_options(mi_rings PRIVATE -w)
endif()

# Built-in sounds: drum kits, arp synths, reverb and delay. No JUCE.
add_library(gristo_sound STATIC
    src/Drums.cpp
    src/Fx.cpp
    src/SoundEngine.cpp
    src/Synth.cpp
    src/Beads.cpp
    src/BeadsReverb.cpp
    src/Rings.cpp
)
target_link_libraries(gristo_sound PRIVATE mi_rings)

target_include_directories(gristo_sound PUBLIC include PRIVATE src)

if(MSVC)
    target_compile_options(gristo_sound PRIVATE /W4)
else()
    target_compile_options(gristo_sound PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()
