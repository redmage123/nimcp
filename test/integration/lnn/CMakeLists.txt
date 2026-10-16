# ============================================================================
# LNN Integration Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for LNN integration tests
# WHY:  Verify LNN composes correctly with cross-module subsystems
#       (thalamic routing, substrate, plasticity, etc.)
# HOW:  Build GoogleTest executables linking nimcp_lnn + nimcp
# ============================================================================

if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping LNN integration tests")
    return()
endif()

set(LNN_INTEGRATION_TEST_LIBS
    nimcp_lnn
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

set(LNN_INTEGRATION_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/lnn
)

# ============================================================================
# LNN Substrate Integration Test (Phase 2 — ATP/temperature modulation)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lnn_substrate_integration.cpp)
    add_executable(integration_lnn_test_lnn_substrate_integration
        test_lnn_substrate_integration.cpp
    )

    target_link_libraries(integration_lnn_test_lnn_substrate_integration
        PRIVATE ${LNN_INTEGRATION_TEST_LIBS}
    )

    target_include_directories(integration_lnn_test_lnn_substrate_integration
        PRIVATE ${LNN_INTEGRATION_TEST_INCLUDES}
    )

    target_compile_features(integration_lnn_test_lnn_substrate_integration
        PRIVATE cxx_std_17)

    gtest_discover_tests(integration_lnn_test_lnn_substrate_integration
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;lnn;substrate"
            TIMEOUT 300
    )

    message(STATUS "  Added LNN substrate integration test: "
                   "test_lnn_substrate_integration")
endif()

# ============================================================================
# LNN Thalamic Compose Test (Phase 2 — attention-gated input modulation)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lnn_thalamic_compose.cpp)
    add_executable(integration_lnn_test_lnn_thalamic_compose
        test_lnn_thalamic_compose.cpp
    )

    target_link_libraries(integration_lnn_test_lnn_thalamic_compose
        PRIVATE ${LNN_INTEGRATION_TEST_LIBS}
    )

    target_include_directories(integration_lnn_test_lnn_thalamic_compose
        PRIVATE ${LNN_INTEGRATION_TEST_INCLUDES}
    )

    target_compile_features(integration_lnn_test_lnn_thalamic_compose
        PRIVATE cxx_std_17)

    gtest_discover_tests(integration_lnn_test_lnn_thalamic_compose
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;lnn;thalamic"
            TIMEOUT 180
    )

    message(STATUS "  Added LNN integration test: test_lnn_thalamic_compose")
endif()
