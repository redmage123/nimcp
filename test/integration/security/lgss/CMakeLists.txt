# LGSS Integration Tests

# Test files
set(LGSS_INTEGRATION_TESTS
    test_lgss_integration.cpp
)

# Create test executable
add_executable(test_lgss_integration ${LGSS_INTEGRATION_TESTS})

target_link_libraries(test_lgss_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        Threads::Threads
)

target_include_directories(test_lgss_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Add test
add_test(NAME lgss_integration_tests COMMAND test_lgss_integration)

# Set test properties
set_tests_properties(lgss_integration_tests PROPERTIES
    TIMEOUT 120
    LABELS "integration;security;lgss"
)
