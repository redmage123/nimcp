# LGSS End-to-End Tests

# Test files
set(LGSS_E2E_TESTS
    test_lgss_e2e.cpp
)

# Create test executable
add_executable(test_lgss_e2e ${LGSS_E2E_TESTS})

target_link_libraries(test_lgss_e2e
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        Threads::Threads
)

target_include_directories(test_lgss_e2e
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Add test
add_test(NAME lgss_e2e_tests COMMAND test_lgss_e2e)

# Set test properties
set_tests_properties(lgss_e2e_tests PROPERTIES
    TIMEOUT 300
    LABELS "e2e;security;lgss"
)
