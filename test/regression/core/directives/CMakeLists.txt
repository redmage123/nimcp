# ============================================================================
# NIMCP Core Directives Regression Tests
# ============================================================================
# WHAT: Regression tests for action history and reciprocity evaluation
# WHY:  Ensure fixes remain fixed, thread safety, memory safety
# HOW:  Stress testing, concurrent access, edge cases

add_executable(regression_core_directives
    regression_core_directives.cpp
)

target_link_libraries(regression_core_directives
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(regression_core_directives PRIVATE cxx_std_17)

target_include_directories(regression_core_directives
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME regression_core_directives
         COMMAND regression_core_directives)

set_tests_properties(regression_core_directives
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;core;directives;ethics"
)
