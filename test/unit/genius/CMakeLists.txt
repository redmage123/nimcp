# =============================================================================
# Unit Tests for Genius Profiles Module
# =============================================================================
# WHAT: Unit tests for genius profiles and eidetic memory integration
# WHY:  Verify correctness of genius profile APIs and eidetic enhancements
# HOW:  GTest-based unit tests
# =============================================================================

# Genius Profiles Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_genius_profiles.cpp")
    add_executable(unit_genius_profiles
        ${CMAKE_CURRENT_SOURCE_DIR}/test_genius_profiles.cpp
    )

    target_link_libraries(unit_genius_profiles
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(unit_genius_profiles
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_features(unit_genius_profiles PRIVATE cxx_std_17)
    target_compile_definitions(unit_genius_profiles PRIVATE NIMCP_TESTING NIMCP_UNIT_TESTING)

    gtest_discover_tests(unit_genius_profiles
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;genius;profiles"
            TIMEOUT 120
    )

    message(STATUS "  Added Unit test: unit_genius_profiles")
endif()

# Eidetic Memory Unit Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_eidetic_memory.cpp")
    add_executable(unit_eidetic_memory
        ${CMAKE_CURRENT_SOURCE_DIR}/test_eidetic_memory.cpp
    )

    target_link_libraries(unit_eidetic_memory
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(unit_eidetic_memory
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_features(unit_eidetic_memory PRIVATE cxx_std_17)
    target_compile_definitions(unit_eidetic_memory PRIVATE NIMCP_TESTING NIMCP_UNIT_TESTING)

    gtest_discover_tests(unit_eidetic_memory
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;genius;eidetic"
            TIMEOUT 120
    )

    message(STATUS "  Added Unit test: unit_eidetic_memory")
endif()

# =============================================================================
# Custom Target: Run All Genius Unit Tests
# =============================================================================
add_custom_target(test_genius_unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L "unit" -L "genius" --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all genius profiles unit tests"
)
