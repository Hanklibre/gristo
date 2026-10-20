add_executable(gristo_tests
    TestMain.cpp
    PatternTests.cpp
    GrooveTests.cpp
    HumanizerTests.cpp
    EngineTests.cpp
    ArpTests.cpp
    SoundTests.cpp
)
target_link_libraries(gristo_tests PRIVATE gristo_core gristo_sound)
add_test(NAME gristo_tests COMMAND gristo_tests)
