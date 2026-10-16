# ============================================================================
# SNN Bridge Exception Regression Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for SNN bridge exception handling regression tests
# WHY:  Ensure API contracts remain stable across versions
# HOW:  Build GoogleTest executable to verify error codes, return values, messages
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping SNN bridge exception regression tests")
    return()
endif()

# Common link libraries
set(SNN_EXCEPTION_REGRESSION_LIBS
    nimcp_snn
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
    z
)

# Common include directories
set(SNN_EXCEPTION_REGRESSION_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/snn
    ${CMAKE_SOURCE_DIR}/test
)

# ============================================================================
# SNN Bridge Exception Regression Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_bridge_exception_regression.cpp)
    add_executable(regression_snn_bridge_exception
        test_snn_bridge_exception_regression.cpp
    )

    target_link_libraries(regression_snn_bridge_exception
        PRIVATE ${SNN_EXCEPTION_REGRESSION_LIBS}
    )

    target_include_directories(regression_snn_bridge_exception
        PRIVATE ${SNN_EXCEPTION_REGRESSION_INCLUDES}
    )

    target_compile_features(regression_snn_bridge_exception PRIVATE cxx_std_17)

    gtest_discover_tests(regression_snn_bridge_exception
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;snn;bridges;exception"
            TIMEOUT 120
    )

    message(STATUS "  Added SNN bridge exception regression test: test_snn_bridge_exception_regression")
endif()
