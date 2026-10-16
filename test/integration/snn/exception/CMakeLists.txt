# ============================================================================
# SNN Bridge Exception Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for SNN bridge exception handling integration tests
# WHY:  Test cross-bridge exception propagation and recovery scenarios
# HOW:  Build GoogleTest executable linking all required modules
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping SNN bridge exception integration tests")
    return()
endif()

# Common link libraries
set(SNN_EXCEPTION_INTEGRATION_LIBS
    nimcp_snn
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
    z
)

# Common include directories
set(SNN_EXCEPTION_INTEGRATION_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/snn
    ${CMAKE_SOURCE_DIR}/test
)

# ============================================================================
# SNN Bridge Exception Integration Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_bridge_exception_integration.cpp)
    add_executable(integration_snn_bridge_exception
        test_snn_bridge_exception_integration.cpp
    )

    target_link_libraries(integration_snn_bridge_exception
        PRIVATE ${SNN_EXCEPTION_INTEGRATION_LIBS}
    )

    target_include_directories(integration_snn_bridge_exception
        PRIVATE ${SNN_EXCEPTION_INTEGRATION_INCLUDES}
    )

    target_compile_features(integration_snn_bridge_exception PRIVATE cxx_std_17)

    gtest_discover_tests(integration_snn_bridge_exception
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;snn;bridges;exception"
            TIMEOUT 180
    )

    message(STATUS "  Added SNN bridge exception integration test: test_snn_bridge_exception_integration")
endif()
