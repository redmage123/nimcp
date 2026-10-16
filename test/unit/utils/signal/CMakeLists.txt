#=============================================================================
# CMakeLists.txt - Signal Processing Unit Tests
#=============================================================================

# Signal Filter Unit Tests
add_executable(unit_utils_signal_test_signal_filter
    test_signal_filter.cpp
)

target_link_libraries(unit_utils_signal_test_signal_filter
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(
    NAME unit_utils_signal_test_signal_filter
    COMMAND unit_utils_signal_test_signal_filter
)

set_tests_properties(unit_utils_signal_test_signal_filter
    PROPERTIES
    LABELS "unit;utils;signal"
    TIMEOUT 60
)

# Hilbert Transform Unit Tests
add_executable(unit_utils_signal_test_hilbert
    test_hilbert.cpp
)

target_link_libraries(unit_utils_signal_test_hilbert
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(
    NAME unit_utils_signal_test_hilbert
    COMMAND unit_utils_signal_test_hilbert
)

set_tests_properties(unit_utils_signal_test_hilbert
    PROPERTIES
    LABELS "unit;utils;signal;hilbert"
    TIMEOUT 60
)

# Signal Filter Bands Test (standalone executable)
add_executable(unit_utils_signal_test_signal_filter_bands
    test_signal_filter_bands.cpp
)

target_link_libraries(unit_utils_signal_test_signal_filter_bands
    nimcp
)

add_test(
    NAME unit_utils_signal_test_signal_filter_bands
    COMMAND unit_utils_signal_test_signal_filter_bands
)

set_tests_properties(unit_utils_signal_test_signal_filter_bands
    PROPERTIES
    LABELS "unit;utils;signal;bands"
    TIMEOUT 60
)

#=============================================================================
# Signal Exception Integration Tests (uses Check framework)
#=============================================================================

# Find Check library using pkg-config
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CHECK check)
endif()

if(CHECK_FOUND)
    add_executable(unit_utils_signal_test_signal_exception
        test_signal_exception.c
    )

    target_include_directories(unit_utils_signal_test_signal_exception PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )

    target_link_libraries(unit_utils_signal_test_signal_exception
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )

    add_test(
        NAME unit_utils_signal_test_signal_exception
        COMMAND unit_utils_signal_test_signal_exception
    )

    set_tests_properties(unit_utils_signal_test_signal_exception
        PROPERTIES
        LABELS "unit;utils;signal;exception"
        TIMEOUT 120
    )
else()
    message(STATUS "Check library not found - skipping signal exception tests")
endif()
