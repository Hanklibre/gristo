# Built-in sounds: drum kits, arp synths, reverb and delay. No JUCE.
add_library(gristo_sound STATIC
    src/Drums.cpp
    src/Fx.cpp
    src/SoundEngine.cpp
    src/Synth.cpp
)

target_include_directories(gristo_sound PUBLIC include PRIVATE src)

if(MSVC)
    target_compile_options(gristo_sound PRIVATE /W4)
else()
    target_compile_options(gristo_sound PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()
