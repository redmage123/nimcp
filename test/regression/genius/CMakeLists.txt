# =============================================================================
# Genius Profiles Regression Tests
# =============================================================================
# WHAT: Regression tests for genius profile system stability and correctness
# WHY:  Ensure genius profiles maintain consistent behavior across versions
# HOW:  Test parameter consistency, eidetic presets, blending, error codes
# =============================================================================

# =============================================================================
# Genius Profiles Regression Test
# Tests:
# - Profile parameter consistency (all 9 types)
# - Eidetic preset reproducibility (Tesla, Mozart, von Neumann, Kim Peek, Wiltshire)
# - Blending determinism and transitivity
# - State machine transitions (UNINITIALIZED -> READY -> ACTIVE -> SUSPENDED)
# - Error code consistency
# - Bridge lifecycle (create/destroy with bridge base pattern)
# =============================================================================

add_executable(regression_genius_profiles
    test_genius_regression.cpp
)

target_link_libraries(regression_genius_profiles
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        pthread
        m
)

target_compile_features(regression_genius_profiles PRIVATE cxx_std_17)

add_test(
    NAME regression_genius_profiles
    COMMAND regression_genius_profiles
)

set_tests_properties(regression_genius_profiles
    PROPERTIES
    LABELS "regression;genius;profiles;brain"
    TIMEOUT 120
)

# =============================================================================
# Eidetic Memory Regression Tests
# =============================================================================
# Tests:
# - Preset consistency (Tesla, Mozart, von Neumann, Kim Peek, Wiltshire)
# - Validation rule consistency
# - Utility function determinism (scale, decay)
# - Error code stability
# =============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_eidetic_regression.cpp")
    add_executable(regression_eidetic_memory
        test_eidetic_regression.cpp
    )

    target_link_libraries(regression_eidetic_memory
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            pthread
            m
    )

    target_compile_features(regression_eidetic_memory PRIVATE cxx_std_17)

    add_test(
        NAME regression_eidetic_memory
        COMMAND regression_eidetic_memory
    )

    set_tests_properties(regression_eidetic_memory
        PROPERTIES
        LABELS "regression;genius;eidetic"
        TIMEOUT 120
    )

    message(STATUS "  Added Regression test: regression_eidetic_memory")
endif()

# =============================================================================
# Brain Factory Regression Tests
# =============================================================================
# Tests:
# - Brain creation regression (boundary cases, memory leaks)
# - State consistency regression
# - Concurrent access regression
# - Type validation regression
# =============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_brain_factory_regression.cpp")
    add_executable(regression_brain_factory
        test_brain_factory_regression.cpp
    )

    target_link_libraries(regression_brain_factory
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            pthread
            m
    )

    target_compile_features(regression_brain_factory PRIVATE cxx_std_17)

    add_test(
        NAME regression_brain_factory
        COMMAND regression_brain_factory
    )

    set_tests_properties(regression_brain_factory
        PROPERTIES
        LABELS "regression;genius;brain_factory"
        TIMEOUT 900
    )

    message(STATUS "  Added Regression test: regression_brain_factory")
endif()

# =============================================================================
# Custom Target: Run All Genius Regression Tests
# =============================================================================
add_custom_target(test_genius_regression
    COMMAND ${CMAKE_CTEST_COMMAND} -L "regression" -L "genius" --output-on-failure
    DEPENDS
        regression_genius_profiles
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all genius profiles regression tests"
)
