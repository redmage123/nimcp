# Example programs

# ============================================================================
# NIMCP Simple Demo - PRIMARY REFERENCE IMPLEMENTATION (Phase 9)
# ============================================================================
# Clean, simple, easy-to-understand demo showcasing all Phase 9 features:
# - Brain API (high-level)
# - Multi-modal processing
# - Epistemic filtering (bias prevention)
# - Ethical reasoning
# START HERE if you're new to NIMCP!
add_executable(simple_demo simple_demo.c)
target_link_libraries(simple_demo nimcp m)
target_include_directories(simple_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# ============================================================================
# Phase 10.2: Working Memory Integration Demo
# ============================================================================
# Demonstrates:
# - Miller's 7±2 working memory buffer
# - COW snapshot/restore with working memory
# - Temporal decay and attention refresh
# - Zero training impact (inference only)
add_executable(working_memory_integration_demo working_memory_integration_demo.c)
target_link_libraries(working_memory_integration_demo nimcp m)
target_include_directories(working_memory_integration_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# Phase 10.2: Working Memory Public API Demo (End-to-End Test)
add_executable(working_memory_public_api_demo working_memory_public_api_demo.c)
target_link_libraries(working_memory_public_api_demo nimcp m)
target_include_directories(working_memory_public_api_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.0 Example
add_executable(event_demo event_demo.c)
target_link_libraries(event_demo nimcp)
target_include_directories(event_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.5 Brain API Example
add_executable(brain_demo brain_demo.c)
target_link_libraries(brain_demo nimcp)
target_include_directories(brain_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.5 Ethics Engine Example
add_executable(ethics_demo ethics_demo.c)
target_link_libraries(ethics_demo nimcp)
target_include_directories(ethics_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.5 Infant Learning Demo (conceptual)
add_executable(infant_demo infant_learning_demo.c)
target_link_libraries(infant_demo nimcp)
target_include_directories(infant_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.5 Integrated Learning Demo (FULLY FUNCTIONAL)
add_executable(integrated_demo integrated_learning_demo.c)
target_link_libraries(integrated_demo nimcp m)
target_include_directories(integrated_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.6 Brain Probe Demo
add_executable(brain_probe_demo brain_probe_demo.c)
target_link_libraries(brain_probe_demo nimcp)
target_include_directories(brain_probe_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.6 Izhikevich Neuron Model Demo
add_executable(izhikevich_demo izhikevich_demo.c)
target_link_libraries(izhikevich_demo nimcp m)
target_include_directories(izhikevich_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Programmable Synapses Demo (DISABLED - API mismatch)
# add_executable(programmable_synapses_demo programmable_synapses_demo.c)
# target_link_libraries(programmable_synapses_demo nimcp m)
# target_include_directories(programmable_synapses_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 NLP Integration Test (Synapses + Attention + Neuromodulation)
add_executable(nlp_integration_test nlp_integration_test.c)
target_link_libraries(nlp_integration_test nimcp m)
target_include_directories(nlp_integration_test PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Fractal Network Demo (Scale-Free Topology + Pink Noise)
add_executable(fractal_network_demo fractal_network_demo.c)
target_link_libraries(fractal_network_demo nimcp m)
target_include_directories(fractal_network_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Phase 2 - NLP with Fractal Networks
add_executable(nlp_integration_demo nlp_integration_demo.c)
target_link_libraries(nlp_integration_demo nimcp m)
target_include_directories(nlp_integration_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Phase 2 - Pink Noise Weight Initialization Test
add_executable(test_pink_weights test_pink_weights.c)
target_link_libraries(test_pink_weights nimcp m)
target_include_directories(test_pink_weights PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Phase 3 - Integration Test
add_executable(test_phase3 test_phase3.c)
target_link_libraries(test_phase3 nimcp m)
target_include_directories(test_phase3 PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Phase 4 - STDP + Eligibility Traces Test (Simplified)
add_executable(test_phase4 test_phase4_simple.c)
target_link_libraries(test_phase4 nimcp m)
target_include_directories(test_phase4 PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Phase 4 - Full System Integration Test (DISABLED - API mismatch)
# add_executable(full_system_integration_test full_system_integration_test.c)
# target_link_libraries(full_system_integration_test nimcp m)
# target_include_directories(full_system_integration_test PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Phase 8 - Multi-Modal Integration Demo (unified architecture)
add_executable(multimodal_integration_demo multimodal_integration_demo.c)
target_link_libraries(multimodal_integration_demo nimcp m)
target_include_directories(multimodal_integration_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Phase 8.8 - Speech Cortex Demo (phoneme recognition, prosody, lexical access)
add_executable(speech_cortex_demo speech_cortex_demo.c)
target_link_libraries(speech_cortex_demo nimcp m)
target_include_directories(speech_cortex_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# NIMCP 2.7 Phase 8.8 - Comprehensive Demo (ALL features: visual+audio+speech+direct+fractal+pink noise)
add_executable(phase8_comprehensive_demo phase8_comprehensive_demo.c)
target_link_libraries(phase8_comprehensive_demo nimcp m)
target_include_directories(phase8_comprehensive_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# GPU Integration Test - GPU neuron API with CPU fallback
add_executable(gpu_integration_test gpu_integration_test.c)
target_link_libraries(gpu_integration_test nimcp m)
target_include_directories(gpu_integration_test PRIVATE ${NIMCP_INCLUDE_DIR})

# Install examples
install(TARGETS event_demo brain_demo ethics_demo infant_demo integrated_demo brain_probe_demo izhikevich_demo nlp_integration_test fractal_network_demo nlp_integration_demo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)

# NIMCP 2.7.0 Phase 9.0 - Neural Logic Gate Test
add_executable(test_neural_logic test_neural_logic.c)
target_link_libraries(test_neural_logic nimcp m)
target_include_directories(test_neural_logic PRIVATE ${NIMCP_INCLUDE_DIR})

# ============================================================================
# NIMCP 2.6 Copy-on-Write (COW) Examples - Memory Efficient Brain Cloning
# ============================================================================

# COW Example 1: Multi-Tenant Inference Server
# Demonstrates efficient model sharing across multiple tenants (86% memory savings)
add_executable(cow_inference_server cow_inference_server.c)
target_link_libraries(cow_inference_server nimcp m)
target_include_directories(cow_inference_server PRIVATE ${NIMCP_INCLUDE_DIR})

# COW Example 2: Snapshot Before Training
# Demonstrates instant checkpointing and rollback (99% memory savings)
add_executable(cow_snapshot_learning cow_snapshot_learning.c)
target_link_libraries(cow_snapshot_learning nimcp m)
target_include_directories(cow_snapshot_learning PRIVATE ${NIMCP_INCLUDE_DIR})

# COW Example 3: A/B Testing Training Strategies
# Demonstrates parallel strategy comparison (64% memory savings)
add_executable(cow_ab_testing cow_ab_testing.c)
target_link_libraries(cow_ab_testing nimcp m)
target_include_directories(cow_ab_testing PRIVATE ${NIMCP_INCLUDE_DIR})

# ============================================================================
# NIMCP 2.8 Distributed COW Examples - Network-Wide Brain Cloning
# ============================================================================

# Distributed COW Example: Cross-Node Brain Sharing
# Demonstrates distributed COW cloning with lazy network loading (76% cluster memory savings)
add_executable(distributed_cow_demo distributed_cow_demo.c)
target_link_libraries(distributed_cow_demo nimcp m pthread z)
target_include_directories(distributed_cow_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# ============================================================================
# Phase C2.2: Neuromodulation Integration Example
# ============================================================================

# Neuromodulation Integration: Phasic-Tonic + Receptor Subtypes
# Demonstrates integration of Enhancement #1 and #2:
# - TD error encoding via phasic bursts
# - Receptor subtype processing (D1 vs D2)
# - Regional specialization (cortex vs striatum)
# - Pharmacological modeling (D2 blockade)
add_executable(neuromodulation_integration_example neuromodulation_integration_example.c)
target_link_libraries(neuromodulation_integration_example nimcp m)
target_include_directories(neuromodulation_integration_example PRIVATE ${NIMCP_INCLUDE_DIR})

# ============================================================================
# Option 2.2: Reinforcement Learning with Eligibility Traces Demo
# ============================================================================

# RL Eligibility Demo: Demonstrates reward-based learning with eligibility traces
# Key features:
# - Temporal credit assignment via eligibility traces
# - Delayed reward propagation to earlier actions
# - Three-factor learning rule (Hebbian + Reward + Dopamine)
# - Simple cart-pole environment for RL
add_executable(rl_eligibility_demo rl_eligibility_demo.c)
target_link_libraries(rl_eligibility_demo nimcp m)
target_include_directories(rl_eligibility_demo PRIVATE ${NIMCP_INCLUDE_DIR})

# Mesh Network Bootstrap Demo
# Demonstrates the complete NIMCP mesh network integration:
# - Bootstrap creates 5 channels (system, left/right hemisphere, subcortical, GPU)
# - Registers ~260 modules across 12 categories
# - Shows endorsement policies (cognitive, motor, memory, emergency, etc.)
# - Demonstrates belief propagation via gossip
# - Shows EOV transaction flow
add_executable(mesh_bootstrap_demo mesh_bootstrap_demo.c)
target_link_libraries(mesh_bootstrap_demo nimcp m)
target_include_directories(mesh_bootstrap_demo PRIVATE ${NIMCP_INCLUDE_DIR})
