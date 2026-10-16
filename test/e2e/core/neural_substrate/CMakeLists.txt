# ============================================================================
# NIMCP Neural Substrate End-to-End Tests
# ============================================================================
# WHAT: Build configuration for neural substrate E2E tests
# WHY:  Test complete substrate pipelines including metabolic, health,
#       immune integration, and GPU acceleration
# HOW:  Link against full NIMCP library with GTest for comprehensive testing
#
# E2E TESTS:
# - test_metabolic_processing_e2e: Metabolic state lifecycle and energy dynamics
# - test_substrate_health_e2e: Physical substrate health and maintenance
# - test_immune_substrate_e2e: Immune-substrate bidirectional integration
# - test_gpu_acceleration_e2e: GPU-accelerated substrate processing
# ============================================================================

message(STATUS "")
message(STATUS "-- Configuring Neural Substrate E2E Tests")

# ============================================================================
# Metabolic Processing E2E Tests
# ============================================================================

add_e2e_test(e2e_test_metabolic_processing
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_metabolic_processing_e2e.cpp
)

# Set longer timeout for metabolic tests (extended simulations)
set_tests_properties(e2e_test_metabolic_processing PROPERTIES
    TIMEOUT 300
    RUN_SERIAL TRUE
)

# ============================================================================
# Substrate Health E2E Tests
# ============================================================================

add_e2e_test(e2e_test_substrate_health
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_substrate_health_e2e.cpp
)

# Set longer timeout for health tests (long-term stability simulations)
set_tests_properties(e2e_test_substrate_health PROPERTIES
    TIMEOUT 300
    RUN_SERIAL TRUE
)

# ============================================================================
# Immune-Substrate E2E Tests
# ============================================================================

add_e2e_test(e2e_test_immune_substrate
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_immune_substrate_e2e.cpp
)

# Set longer timeout for immune tests (inflammation cascades, recovery)
set_tests_properties(e2e_test_immune_substrate PROPERTIES
    TIMEOUT 300
    RUN_SERIAL TRUE
)

# ============================================================================
# GPU Acceleration E2E Tests
# ============================================================================

add_e2e_test(e2e_test_gpu_acceleration
    ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_gpu_acceleration_e2e.cpp
)

# Set longer timeout for GPU tests (large-scale simulations, benchmarking)
set_tests_properties(e2e_test_gpu_acceleration PROPERTIES
    TIMEOUT 600
    RUN_SERIAL TRUE
)

message(STATUS "  Added 4 Neural Substrate E2E tests")
