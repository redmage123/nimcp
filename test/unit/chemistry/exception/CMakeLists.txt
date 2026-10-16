# =============================================================================
# Unit Tests for Chemistry Exception Handling
# =============================================================================
# WHAT: CMakeLists.txt for chemistry exception handling unit tests
# WHY:  Test exception creation, dispatch, and handler chain for chemistry module
# HOW:  Build test executable and register with CTest
#
# TESTS INCLUDED:
# - Exception creation for all chemistry operation types
# - Error code to exception mapping
# - Handler chain dispatch
# - Recovery strategy determination
# - Thread safety
#
# @author NIMCP Development Team
# @date 2026-01-20
# =============================================================================

# Chemistry Exception Handling Unit Tests
add_executable(unit_chemistry_exception_handling
    test_chemistry_exception_handling.cpp
)

target_include_directories(unit_chemistry_exception_handling
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_chemistry_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_chemistry_exception_handling PRIVATE cxx_std_17)

# Register with CTest using gtest_discover_tests for automatic test discovery
gtest_discover_tests(unit_chemistry_exception_handling
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;chemistry;exception"
)

# Alternative manual test registration (if gtest_discover_tests has issues)
# add_test(
#     NAME unit_chemistry_exception_handling
#     COMMAND unit_chemistry_exception_handling --gtest_brief=1
# )
#
# set_tests_properties(unit_chemistry_exception_handling PROPERTIES
#     TIMEOUT 120
#     LABELS "unit;chemistry;exception"
# )
