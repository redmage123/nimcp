# ============================================================================
# Omnidirectional Inference Integration Tests
# ============================================================================
#
# WHAT: Integration tests for Phase 2 omnidirectional inference bridges
# WHY:  Verify all bridges (Sensory, CC, Occipital, Broca, KG) work together
# HOW:  Multi-bridge scenarios with JEPA, Hopfield, Hierarchy, and Replay

message(STATUS "Adding omnidirectional inference integration tests...")

# Integration Test: Multi-bridge omnidirectional inference
add_test_binary(
    test_omni_bridges_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_omni_bridges_integration.cpp
    integration
)

# ============================================================================
# World Model Bridge Integration Tests
# ============================================================================
#
# WHAT: Integration tests for all 11 World Model bridges
# WHY:  Verify WM bridges work together with shared world model
# HOW:  Multi-bridge update cycles, cross-bridge data flow, resource sharing

message(STATUS "Adding World Model bridges integration tests...")

# DISABLED: Test needs API updates to match current WM bridge config API
# TODO: Update omni_wm_security_immune_bridge_config_t and omni_wm_logging_bridge_config_t usage
#
# # Integration Test: All 11 WM bridges working together
# add_test_binary(
#     test_omni_wm_bridges_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_omni_wm_bridges_integration.cpp
#     integration
# )
#
# # Integration Test: Memory + Cognitive bridge integration
# # Tests: Replay -> WM training -> Prediction -> Goal conditioning
# add_test_binary(
#     test_omni_wm_memory_cognitive_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_omni_wm_memory_cognitive_integration.cpp
#     integration
# )

# DISABLED: Test needs API updates to match current WM plasticity bridge API
# TODO: Update to use current omni_wm_plasticity_bridge_* and omni_wm_tom_bridge_* functions
#
# # Integration Test: Theory of Mind + Plasticity bridge integration
# # Tests: Belief prediction -> STDP modulation -> Social learning
# add_test_binary(
#     test_omni_wm_tom_plasticity_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_omni_wm_tom_plasticity_integration.cpp
#     integration
# )

message(STATUS "  Configured World Model bridges integration tests")
message(STATUS "  Configured omnidirectional inference integration tests")
