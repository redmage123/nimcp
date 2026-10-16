# ============================================================================
# NIMCP Fuzzy Logic Integration Tests
# ============================================================================
# WHAT: Integration tests for fuzzy types, operators, inference, and bridge
# WHY:  Verify cross-module composition (types+operators, inference+bridge,
#       SNN conversion, training scheduling, ANFIS, bio-async modulation)
# HOW:  GTest-based tests linking against nimcp library
# ============================================================================

message(STATUS "  Adding Fuzzy Logic integration tests...")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_integration.cpp)
    add_executable(integration_fuzzy_logic
        ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_integration.cpp
    )
    target_include_directories(integration_fuzzy_logic PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )
    target_link_libraries(integration_fuzzy_logic PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(integration_fuzzy_logic PRIVATE cxx_std_17)
    target_compile_definitions(integration_fuzzy_logic PRIVATE
        NIMCP_TESTING
    )
    add_test(NAME integration_fuzzy_logic COMMAND integration_fuzzy_logic)
    set_tests_properties(integration_fuzzy_logic PROPERTIES
        LABELS "integration;utils;fuzzy"
        TIMEOUT 180
    )
    message(STATUS "    Added: integration_fuzzy_logic")
endif()
