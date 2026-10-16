# ============================================================================
# NIMCP Fuzzy Logic End-to-End Tests
# ============================================================================
# WHAT: E2E tests for complete fuzzy logic system pipelines
# WHY:  Validate full stack: classic control problems (tipping, temperature),
#       bridge pipelines, multi-variable systems, stress tests, memory
#       lifecycle, concurrent configs, and performance baselines
# HOW:  GTest-based tests building complete FIS configurations end-to-end
# ============================================================================

message(STATUS "  Adding Fuzzy Logic E2E tests...")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_e2e.cpp)
    add_executable(e2e_fuzzy_logic
        ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzzy_e2e.cpp
    )
    target_include_directories(e2e_fuzzy_logic PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )
    target_link_libraries(e2e_fuzzy_logic PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_fuzzy_logic PRIVATE cxx_std_17)
    target_compile_definitions(e2e_fuzzy_logic PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )
    add_test(NAME e2e_fuzzy_logic COMMAND e2e_fuzzy_logic)
    set_tests_properties(e2e_fuzzy_logic PROPERTIES
        LABELS "e2e;utils;fuzzy"
        TIMEOUT 300
    )
    message(STATUS "    Added: e2e_fuzzy_logic")
endif()
