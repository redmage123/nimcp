# Wernicke's Area Regression Tests
# WHAT: Build configuration for Wernicke regression tests
# WHY:  Ensure API stability and signal processing consistency

# Find GTest
if(NOT GTest_FOUND)
    find_package(GTest QUIET)
endif()

if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping Wernicke regression tests")
    return()
endif()

# Common include directories
set(WERNICKE_REGRESSION_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Common link libraries
set(WERNICKE_REGRESSION_LIBS
    GTest::GTest
    GTest::Main
    nimcp
    pthread
    m
)

# Backward Compatibility Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_wernicke_backward_compat.cpp")
    add_executable(regression_wernicke_backward_compat
        test_wernicke_backward_compat.cpp
    )
    target_include_directories(regression_wernicke_backward_compat PRIVATE
        ${WERNICKE_REGRESSION_INCLUDES}
    )
    target_link_libraries(regression_wernicke_backward_compat PRIVATE
        ${WERNICKE_REGRESSION_LIBS}
    )
    target_compile_definitions(regression_wernicke_backward_compat PRIVATE
        NIMCP_TESTING
    )
    set_target_properties(regression_wernicke_backward_compat PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    gtest_discover_tests(regression_wernicke_backward_compat
        TEST_PREFIX "regression/wernicke/backward_compat/"
        PROPERTIES LABELS "regression;wernicke;backward_compat"
    )
    message(STATUS "  + regression_wernicke_backward_compat")
endif()

# Signal Regression Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_wernicke_signal_regression.cpp")
    add_executable(regression_wernicke_signal
        test_wernicke_signal_regression.cpp
    )
    target_include_directories(regression_wernicke_signal PRIVATE
        ${WERNICKE_REGRESSION_INCLUDES}
    )
    target_link_libraries(regression_wernicke_signal PRIVATE
        ${WERNICKE_REGRESSION_LIBS}
    )
    target_compile_definitions(regression_wernicke_signal PRIVATE
        NIMCP_TESTING
    )
    set_target_properties(regression_wernicke_signal PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    gtest_discover_tests(regression_wernicke_signal
        TEST_PREFIX "regression/wernicke/signal/"
        PROPERTIES LABELS "regression;wernicke;signal"
    )
    message(STATUS "  + regression_wernicke_signal")
endif()
