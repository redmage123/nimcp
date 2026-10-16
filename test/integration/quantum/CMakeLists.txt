# ============================================================================
# Quantum Bridge Integration Tests
# ============================================================================
# WHAT: Integration tests for quantum bridges across cognitive modules
# WHY:  Verify quantum acceleration works in cross-module scenarios
# HOW:  Test bridge connections, data flow, and unified statistics

add_executable(integration_quantum_bridges
    test_quantum_bridges_integration.cpp
)

target_include_directories(integration_quantum_bridges PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(integration_quantum_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)

target_compile_features(integration_quantum_bridges PRIVATE cxx_std_17)

add_test(NAME integration_quantum_bridges COMMAND integration_quantum_bridges)
set_tests_properties(integration_quantum_bridges PROPERTIES LABELS "integration;quantum")

# ============================================================================
# Quantum-Brain Integration Tests
# ============================================================================
# WHAT: Integration tests for quantum RNG and hybrid optimization with brain
# WHY:  Verify quantum bridges work with full brain initialization
# HOW:  Test quantum RNG in neural computations and deadlock prevention
#
# NOTE: Disabled until quantum_bio_async_bridge implementation is complete
# TODO: Re-enable when src/quantum/integration/nimcp_quantum_bio_async_bridge.c is implemented
#
# add_executable(integration_quantum_brain
#     test_quantum_brain_integration.cpp
# )
#
# target_include_directories(integration_quantum_brain PRIVATE
#     ${CMAKE_SOURCE_DIR}/include
# )
#
# target_link_libraries(integration_quantum_brain
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#         m
#         pthread
# )
#
# target_compile_features(integration_quantum_brain PRIVATE cxx_std_17)
#
# add_test(NAME integration_quantum_brain COMMAND integration_quantum_brain)
# set_tests_properties(integration_quantum_brain PROPERTIES LABELS "integration;quantum;brain")
