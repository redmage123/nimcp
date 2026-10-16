# ============================================================================
# Hemispheric Brain Unit Tests
# ============================================================================
# Tests for bilateral brain architecture with two hemispheres and corpus callosum
#
# Test Coverage:
# - Hemisphere lifecycle (create, destroy)
# - Hemisphere processing (update, infer, train)
# - Corpus callosum (communication, bandwidth)
# - Hemispheric brain (coordination, modes)
# - Lateralization (dominance, plasticity)
# - Split-brain mode

# Test: Hemispheric Brain Comprehensive Tests
add_test_binary(
    unit_core_brain_hemispheric_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hemispheric_brain.cpp
    unit
)
set_tests_properties(unit_core_brain_hemispheric_test PROPERTIES
    TIMEOUT 300
    RESOURCE_LOCK "brain_heavy"
)

# Test: Hemispheric Immune Bridge Tests
add_test_binary(
    unit_core_brain_hemispheric_immune_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hemispheric_immune_bridge.cpp
    unit
)
set_tests_properties(unit_core_brain_hemispheric_immune_test PROPERTIES
    TIMEOUT 300
    RESOURCE_LOCK "brain_heavy"
)

# Test: Hemispheric Sleep Bridge Tests
add_test_binary(
    unit_core_brain_hemispheric_sleep_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hemispheric_sleep_bridge.cpp
    unit
)
set_tests_properties(unit_core_brain_hemispheric_sleep_test PROPERTIES
    TIMEOUT 600
    RESOURCE_LOCK "brain_heavy"
)

# Test: Hemispheric FEP Bridge Tests
add_test_binary(
    unit_core_brain_hemispheric_fep_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hemispheric_fep_bridge.cpp
    unit
)
set_tests_properties(unit_core_brain_hemispheric_fep_test PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")

# Test: Hemispheric Glial Bridge Tests
add_test_binary(
    unit_core_brain_hemispheric_glial_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hemispheric_glial_bridge.cpp
    unit
)
set_tests_properties(unit_core_brain_hemispheric_glial_test PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")

# Test: Hemispheric Injury and Recovery Tests
add_test_binary(
    unit_core_brain_hemispheric_injury_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hemispheric_injury.cpp
    unit
)
set_tests_properties(unit_core_brain_hemispheric_injury_test PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")

# Test: Split-Brain Experiments Tests
add_test_binary(
    unit_core_brain_split_brain_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_split_brain_experiments.cpp
    unit
)
set_tests_properties(unit_core_brain_split_brain_test PROPERTIES TIMEOUT 300 RESOURCE_LOCK "brain_heavy")

# Test: Hemispheric Memory Optimization Tests (Opt 5, 7)
add_test_binary(
    unit_core_brain_hemispheric_memory_opt_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hemispheric_memory_opt.cpp
    unit
)
set_tests_properties(unit_core_brain_hemispheric_memory_opt_test PROPERTIES
    TIMEOUT 300
    RESOURCE_LOCK "brain_heavy"
)

# Test: Hemispheric Language Bridge Tests (lightweight — no full brain_t)
add_test_binary(
    unit_core_brain_hemispheric_language_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hemispheric_language_bridge.cpp
    unit
)
set_tests_properties(unit_core_brain_hemispheric_language_test PROPERTIES TIMEOUT 60)
