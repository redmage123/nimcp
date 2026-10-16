# =============================================================================
# Unit Tests for Plasticity Exception Handling
# =============================================================================
# WHAT: CMakeLists.txt for plasticity exception handling unit tests
# WHY:  Test exception creation, dispatch, and handler chain for plasticity modules
# HOW:  Build test executable and register with CTest
#
# TESTS INCLUDED:
# - Exception creation for all plasticity module types
# - Error code to exception mapping
# - Handler chain dispatch
# - Recovery strategy determination
# - Thread safety
#
# @author NIMCP Development Team
# @date 2026-01-16
# =============================================================================

# Plasticity Exception Handling Unit Tests
add_executable(unit_plasticity_exception_handling
    test_plasticity_exception_handling.cpp
)

target_include_directories(unit_plasticity_exception_handling
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_plasticity_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_plasticity_exception_handling PRIVATE cxx_std_17)

# Register with CTest using gtest_discover_tests for automatic test discovery
gtest_discover_tests(unit_plasticity_exception_handling
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;plasticity;exception"
)

# Alternative manual test registration (if gtest_discover_tests has issues)
# add_test(
#     NAME unit_plasticity_exception_handling
#     COMMAND unit_plasticity_exception_handling --gtest_brief=1
# )
#
# set_tests_properties(unit_plasticity_exception_handling PROPERTIES
#     TIMEOUT 120
#     LABELS "unit;plasticity;exception"
# )

# =============================================================================
# Plasticity Module Exception Unit Tests
# =============================================================================
# WHAT: Unit tests for NIMCP_THROW_TO_IMMUNE exception handling
# WHY:  Test NULL pointer and allocation failure handling in plasticity modules
# HOW:  Test neuromodulators, attention, STDP-PR bridge, STDP utils bridge
#
# @author NIMCP Development Team
# @date 2026-01-25
# =============================================================================

add_executable(unit_plasticity_module_exception
    test_plasticity_module_exception.cpp
)

target_include_directories(unit_plasticity_module_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_plasticity_module_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_plasticity_module_exception PRIVATE cxx_std_17)

gtest_discover_tests(unit_plasticity_module_exception
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;plasticity;exception;neuromodulators;attention;stdp"
)
