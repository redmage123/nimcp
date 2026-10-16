# ============================================================================
# Brain Factory Integration Tests
# ============================================================================

# Helper macro for brain factory integration tests
macro(add_brain_factory_integration_test TEST_NAME)
    add_executable(integration_core_brain_factory_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(integration_core_brain_factory_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    target_include_directories(integration_core_brain_factory_${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(integration_core_brain_factory_${TEST_NAME}
        PRIVATE cxx_std_17
    )
    target_compile_definitions(integration_core_brain_factory_${TEST_NAME}
        PRIVATE NIMCP_TESTING
    )
    add_test(NAME integration_core_brain_factory_${TEST_NAME}
             COMMAND integration_core_brain_factory_${TEST_NAME})
    set_tests_properties(integration_core_brain_factory_${TEST_NAME}
        PROPERTIES
            LABELS "integration;core;brain;factory"
            TIMEOUT 180
    )
endmacro()

# Test 1: Factory Integration - Complete workflows
add_executable(integration_core_brain_factory_test_factory_integration
    test_factory_integration.cpp
)
target_link_libraries(integration_core_brain_factory_test_factory_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(integration_core_brain_factory_test_factory_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME integration_core_brain_factory_test_factory_integration
         COMMAND integration_core_brain_factory_test_factory_integration)
set_tests_properties(integration_core_brain_factory_test_factory_integration PROPERTIES LABELS "integration")

# Test 2: Hypothalamus initialization integration
add_brain_factory_integration_test(test_brain_init_hypothalamus_integration)

# Test 3: Memory Optimization Integration (Opt 2+3+4+6)
add_brain_factory_integration_test(test_brain_memory_opt_integration)
