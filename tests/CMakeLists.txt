find_package(GTest REQUIRED)

set(TEST_SOURCES
    test_kalman.cpp
    test_gesture_detector.cpp
    test_mapper.cpp
)

add_executable(aura_tests ${TEST_SOURCES})

target_link_libraries(aura_tests
    PRIVATE
    aura_lib
    GTest::gtest_main
)

target_include_directories(aura_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(aura_tests)
