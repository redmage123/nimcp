# LGSS Adversarial Tests

# Test files
set(LGSS_ADVERSARIAL_TESTS
    test_lgss_adversarial.cpp
)

# Create test executable
add_executable(test_lgss_adversarial ${LGSS_ADVERSARIAL_TESTS})

target_link_libraries(test_lgss_adversarial
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        Threads::Threads
)

target_include_directories(test_lgss_adversarial
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Add test
add_test(NAME lgss_adversarial_tests COMMAND test_lgss_adversarial)

# Set test properties
set_tests_properties(lgss_adversarial_tests PROPERTIES
    TIMEOUT 180
    LABELS "adversarial;security;lgss"
)
