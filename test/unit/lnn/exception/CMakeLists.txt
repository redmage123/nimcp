# =============================================================================
# Unit Tests for LNN Exception Handling
# =============================================================================
# WHAT: CMakeLists.txt for LNN exception handling unit tests
# WHY:  Test exception creation, dispatch, and handler chain for LNN modules
# HOW:  Build test executable and register with CTest
#
# TESTS INCLUDED:
# - Exception creation for all LNN module types
# - Error code to exception mapping
# - Handler chain dispatch
# - Recovery strategy determination
# - Thread safety
#
# @author NIMCP Development Team
# @date 2026-01-20
# =============================================================================

# LNN Exception Handling Unit Tests
add_executable(unit_lnn_exception_handling
    test_lnn_exception_handling.cpp
)

target_include_directories(unit_lnn_exception_handling
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_lnn_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_lnn_exception_handling PRIVATE cxx_std_17)

# Register with CTest using gtest_discover_tests for automatic test discovery
gtest_discover_tests(unit_lnn_exception_handling
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;lnn;exception"
)

# Alternative manual test registration (if gtest_discover_tests has issues)
# add_test(
#     NAME unit_lnn_exception_handling
#     COMMAND unit_lnn_exception_handling --gtest_brief=1
# )
#
# set_tests_properties(unit_lnn_exception_handling PROPERTIES
#     TIMEOUT 120
#     LABELS "unit;lnn;exception"
# )
