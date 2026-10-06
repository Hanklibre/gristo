add_executable(gristo_tests
    TestMain.cpp
    PatternTests.cpp
    GrooveTests.cpp
    HumanizerTests.cpp
    EngineTests.cpp
)
target_link_libraries(gristo_tests PRIVATE gristo_core)
add_test(NAME gristo_tests COMMAND gristo_tests)
