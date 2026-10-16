#=============================================================================
# CMakeLists.txt - Integration tests for Config Module
#=============================================================================
# WHAT: Build configuration for config integration tests
# WHY:  Enable end-to-end testing of config system
# HOW:  GoogleTest integration with all config modules

cmake_minimum_required(VERSION 3.14)

# Find GoogleTest
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/utils/config
    ${GTEST_INCLUDE_DIRS}
)

#=============================================================================
# Test: Config Integration
#=============================================================================
add_executable(test_config_integration
    test_config_integration.cpp
)

target_link_libraries(test_config_integration
    nimcp_config
    nimcp_dynamic_config
    nimcp_security
    nimcp_unified_memory
    GTest::gtest
    GTest::gtest_main
    pthread
    stdc++fs  # For std::filesystem
)

add_test(NAME test_config_integration COMMAND test_config_integration)

#=============================================================================
# Test Properties
#=============================================================================

# Set test labels and timeout
set_tests_properties(test_config_integration
    PROPERTIES
        LABELS "integration;config;end-to-end"
        TIMEOUT 120  # Longer timeout for integration tests
)

# Set working directory for temp file access
set_tests_properties(test_config_integration
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Enable code coverage if requested
if(ENABLE_COVERAGE)
    target_compile_options(test_config_integration PRIVATE --coverage)
    target_link_options(test_config_integration PRIVATE --coverage)
endif()

# Enable sanitizers if requested
if(ENABLE_SANITIZERS)
    set(SANITIZER_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
    target_compile_options(test_config_integration PRIVATE ${SANITIZER_FLAGS})
    target_link_options(test_config_integration PRIVATE ${SANITIZER_FLAGS})
endif()

# Verbose test output for integration tests
if(VERBOSE_TESTS)
    target_compile_definitions(test_config_integration PRIVATE VERBOSE_TESTS)
endif()
