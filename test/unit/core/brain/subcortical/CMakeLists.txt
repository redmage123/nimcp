# ============================================================================
# Subcortical Brain Structures Unit Tests
# ============================================================================
# Tests for basal ganglia action selection system
#
# Test Coverage:
# - Striatum (D1/D2 pathways, dopamine modulation)
# - Globus Pallidus (GPe/GPi, tonic inhibition)
# - Substantia Nigra (SNc dopamine, SNr output)
# - Subthalamic Nucleus (hyperdirect pathway, emergency stop)
# - Integrated Basal Ganglia (action selection, habits)

# Test: Basal Ganglia Comprehensive Tests
add_test_binary(
    unit_core_brain_subcortical_basal_ganglia_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_basal_ganglia.cpp
    unit
)

# Test: Thalamus Comprehensive Tests
add_test_binary(
    unit_core_brain_subcortical_thalamus_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_thalamus.cpp
    unit
)

# Test: Amygdala Comprehensive Tests
add_test_binary(
    unit_core_brain_subcortical_amygdala_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_amygdala.cpp
    unit
)

# Test: Amygdala-Training Bridge Tests
add_test_binary(
    unit_core_brain_subcortical_amygdala_training_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_amygdala_training_bridge.cpp
    unit
)

# Test: Amygdala-Attention Bridge Tests
add_test_binary(
    unit_core_brain_subcortical_amygdala_attention_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_amygdala_attention_bridge.cpp
    unit
)

# Test: Amygdala-Autobiographical Memory Bridge Tests
add_test_binary(
    unit_core_brain_subcortical_amygdala_autobio_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_amygdala_autobio_bridge.cpp
    unit
)

# Test: Amygdala-Stress/Wellbeing Bridge Tests
add_test_binary(
    unit_core_brain_subcortical_amygdala_stress_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_amygdala_stress_bridge.cpp
    unit
)

# Test: Amygdala-FEP Bridge Tests (threat as prediction error)
add_test_binary(
    unit_core_brain_subcortical_amygdala_fep_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_amygdala_fep_bridge.cpp
    unit
)

# Test: Basal Ganglia-FEP Bridge Tests (action selection as free energy minimization)
add_test_binary(
    unit_core_brain_subcortical_basal_ganglia_fep_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_basal_ganglia_fep_bridge.cpp
    unit
)

# Test: Basal Ganglia-Thalamus Bridge Tests (motor relay)
add_test_binary(
    unit_core_brain_subcortical_basal_ganglia_thalamus_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_basal_ganglia_thalamus_bridge.cpp
    unit
)

# Test: Basal Ganglia-Amygdala Bridge Tests (emotional modulation)
add_test_binary(
    unit_core_brain_subcortical_basal_ganglia_amygdala_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_basal_ganglia_amygdala_bridge.cpp
    unit
)

# Test: Basal Ganglia-Training Bridge Tests (plasticity)
add_test_binary(
    unit_core_brain_subcortical_basal_ganglia_training_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_basal_ganglia_training_bridge.cpp
    unit
)

# Test: Basal Ganglia-Executive Bridge Tests (goal-directed control)
add_test_binary(
    unit_core_brain_subcortical_basal_ganglia_executive_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_basal_ganglia_executive_bridge.cpp
    unit
)

# Test: Basal Ganglia Striosome-Matrix Tests (value/motivation + motor)
add_test_binary(
    unit_core_brain_subcortical_bg_striosome_matrix_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bg_striosome_matrix.cpp
    unit
)

# Test: Basal Ganglia Sequence Chunking Tests (skill learning)
add_test_binary(
    unit_core_brain_subcortical_bg_sequence_chunking_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bg_sequence_chunking.cpp
    unit
)

# Test: Basal Ganglia Vigor/Effort Tests (action intensity)
add_test_binary(
    unit_core_brain_subcortical_bg_vigor_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bg_vigor.cpp
    unit
)

# Test: Subcortical KG Wiring Tests (hippocampus, amygdala node registration)
add_test_binary(
    unit_core_brain_subcortical_kg_wiring_test
    ${CMAKE_CURRENT_SOURCE_DIR}/test_subcortical_kg_wiring.cpp
    unit
)
