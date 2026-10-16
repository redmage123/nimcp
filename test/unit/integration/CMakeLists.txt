# ============================================================================
# Layer Infrastructure Integration Unit Tests
# ============================================================================
# WHAT: Unit tests for the 9-layer hierarchical integration framework
# WHY:  Verify correct operation of layer coordinators, bridges, and routing
# HOW:  GTest-based tests for lifecycle, messaging, and state management
# ============================================================================

# Guard: Check GTest availability
if(NOT TARGET GTest::gtest)
    message(STATUS "  [SKIP] Layer Infrastructure tests - GTest not found")
    return()
endif()

message(STATUS "Adding layer infrastructure unit tests...")

# ============================================================================
# Core Infrastructure Tests
# ============================================================================

# Layer Registry Tests
add_executable(unit_integration_layer_registry
    core/test_layer_registry.cpp
)
target_link_libraries(unit_integration_layer_registry
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_layer_registry PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_layer_registry
    PROPERTIES LABELS "unit;integration;layer_registry")

# Layer Coordinator Tests
add_executable(unit_integration_layer_coordinator
    core/test_layer_coordinator.cpp
)
target_link_libraries(unit_integration_layer_coordinator
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_layer_coordinator PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_layer_coordinator
    PROPERTIES LABELS "unit;integration;layer_coordinator")

# Inter-Layer Router Tests
add_executable(unit_integration_inter_layer_router
    core/test_inter_layer_router.cpp
)
target_link_libraries(unit_integration_inter_layer_router
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_inter_layer_router PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_inter_layer_router
    PROPERTIES LABELS "unit;integration;inter_layer_router")

# ============================================================================
# Intra-Layer Coordinator Tests
# ============================================================================

# Physics Intra-Layer Coordinator Tests
add_executable(unit_integration_physics_intra
    intra/test_physics_intra_coordinator.cpp
)
target_link_libraries(unit_integration_physics_intra
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_physics_intra PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_physics_intra
    PROPERTIES LABELS "unit;integration;intra;physics")

# Chemistry Intra-Layer Coordinator Tests
add_executable(unit_integration_chemistry_intra
    intra/test_chemistry_intra_coordinator.cpp
)
target_link_libraries(unit_integration_chemistry_intra
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_chemistry_intra PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_chemistry_intra
    PROPERTIES LABELS "unit;integration;intra;chemistry")

# ============================================================================
# Inter-Layer Bridge Tests
# ============================================================================

# Chemistry-Biology Bridge Tests
add_executable(unit_integration_chemistry_biology_bridge
    inter/test_chemistry_biology_bridge.cpp
)
target_link_libraries(unit_integration_chemistry_biology_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_chemistry_biology_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_chemistry_biology_bridge
    PROPERTIES LABELS "unit;integration;inter;chemistry_biology")

# Neuromodulatory-Executive Bridge Tests
add_executable(unit_integration_neuromod_executive_bridge
    inter/test_neuromod_executive_bridge.cpp
)
target_link_libraries(unit_integration_neuromod_executive_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_neuromod_executive_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_neuromod_executive_bridge
    PROPERTIES LABELS "unit;integration;inter;neuromod_executive")

# Neuromodulatory-World Model Bridge Tests
if(NOT TARGET unit_integration_inter_test_neuromod_wm_bridge)
    add_executable(unit_integration_inter_test_neuromod_wm_bridge
        inter/test_neuromod_wm_bridge.cpp
    )
    target_link_libraries(unit_integration_inter_test_neuromod_wm_bridge
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
    )
    target_compile_features(unit_integration_inter_test_neuromod_wm_bridge PRIVATE cxx_std_17)
    gtest_discover_tests(unit_integration_inter_test_neuromod_wm_bridge
        PROPERTIES LABELS "unit;integration;inter;neuromod_wm")
endif()

# Neuromodulatory-Attention Bridge Tests
add_executable(unit_integration_neuromod_attention_bridge
    inter/test_neuromod_attention_bridge.cpp
)
target_link_libraries(unit_integration_neuromod_attention_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_neuromod_attention_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_neuromod_attention_bridge
    PROPERTIES LABELS "unit;integration;inter;neuromod_attention")

# Neuromodulatory-Emotion Bridge Tests
add_executable(unit_integration_neuromod_emotion_bridge
    inter/test_neuromod_emotion_bridge.cpp
)
target_link_libraries(unit_integration_neuromod_emotion_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_neuromod_emotion_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_neuromod_emotion_bridge
    PROPERTIES LABELS "unit;integration;inter;neuromod_emotion")

# Neuromodulatory-Plasticity Bridge Tests
add_executable(unit_integration_neuromod_plasticity_bridge
    inter/test_neuromod_plasticity_bridge.cpp
)
target_link_libraries(unit_integration_neuromod_plasticity_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_neuromod_plasticity_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_neuromod_plasticity_bridge
    PROPERTIES LABELS "unit;integration;inter;neuromod_plasticity")

# Neuromodulatory-Reasoning Bridge Tests
add_executable(unit_integration_neuromod_reasoning_bridge
    inter/test_neuromod_reasoning_bridge.cpp
)
target_link_libraries(unit_integration_neuromod_reasoning_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_neuromod_reasoning_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_neuromod_reasoning_bridge
    PROPERTIES LABELS "unit;integration;inter;neuromod_reasoning")

# Neuromodulatory-GameTheory Bridge Tests
add_executable(unit_integration_neuromod_gametheory_bridge
    inter/test_neuromod_gametheory_bridge.cpp
)
target_link_libraries(unit_integration_neuromod_gametheory_bridge
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_neuromod_gametheory_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_neuromod_gametheory_bridge
    PROPERTIES LABELS "unit;integration;inter;neuromod_gametheory")

# ============================================================================
# Module Adapter Tests
# ============================================================================

# --- Physics Layer Adapters ---

add_executable(unit_integration_hh_dynamics_adapter
    adapters/physics/test_hh_dynamics_adapter.cpp
)
target_link_libraries(unit_integration_hh_dynamics_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_hh_dynamics_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_hh_dynamics_adapter
    PROPERTIES LABELS "unit;integration;adapters;physics")

add_executable(unit_integration_ephaptic_adapter
    adapters/physics/test_ephaptic_coupling_adapter.cpp
)
target_link_libraries(unit_integration_ephaptic_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_ephaptic_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_ephaptic_adapter
    PROPERTIES LABELS "unit;integration;adapters;physics")

add_executable(unit_integration_thermodynamics_adapter
    adapters/physics/test_thermodynamics_adapter.cpp
)
target_link_libraries(unit_integration_thermodynamics_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_thermodynamics_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_thermodynamics_adapter
    PROPERTIES LABELS "unit;integration;adapters;physics")

# --- Chemistry Layer Adapters ---

add_executable(unit_integration_calcium_adapter
    adapters/chemistry/test_calcium_adapter.cpp
)
target_link_libraries(unit_integration_calcium_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_calcium_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_calcium_adapter
    PROPERTIES LABELS "unit;integration;adapters;chemistry")

add_executable(unit_integration_metabolic_adapter
    adapters/chemistry/test_metabolic_adapter.cpp
)
target_link_libraries(unit_integration_metabolic_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_metabolic_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_metabolic_adapter
    PROPERTIES LABELS "unit;integration;adapters;chemistry")

add_executable(unit_integration_neurotransmitter_adapter
    adapters/chemistry/test_neurotransmitter_adapter.cpp
)
target_link_libraries(unit_integration_neurotransmitter_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_neurotransmitter_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_neurotransmitter_adapter
    PROPERTIES LABELS "unit;integration;adapters;chemistry")

# --- Neuromodulatory Layer Adapters ---

add_executable(unit_integration_neuromod_pool_adapter
    adapters/neuromod/test_neuromod_pool_adapter.cpp
)
target_link_libraries(unit_integration_neuromod_pool_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_neuromod_pool_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_neuromod_pool_adapter
    PROPERTIES LABELS "unit;integration;adapters;neuromod")

add_executable(unit_integration_neuromod_nucleus_adapter
    adapters/neuromod/test_neuromod_nucleus_adapter.cpp
)
target_link_libraries(unit_integration_neuromod_nucleus_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_neuromod_nucleus_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_neuromod_nucleus_adapter
    PROPERTIES LABELS "unit;integration;adapters;neuromod")

# --- Sensory Layer Adapters ---

add_executable(unit_integration_visual_adapter
    adapters/sensory/test_visual_adapter.cpp
)
target_link_libraries(unit_integration_visual_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_visual_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_visual_adapter
    PROPERTIES LABELS "unit;integration;adapters;sensory")

add_executable(unit_integration_auditory_adapter
    adapters/sensory/test_auditory_adapter.cpp
)
target_link_libraries(unit_integration_auditory_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_auditory_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_auditory_adapter
    PROPERTIES LABELS "unit;integration;adapters;sensory")

# --- Memory Layer Adapters ---

add_executable(unit_integration_pr_memory_adapter
    adapters/memory/test_pr_memory_adapter.cpp
)
target_link_libraries(unit_integration_pr_memory_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_pr_memory_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_pr_memory_adapter
    PROPERTIES LABELS "unit;integration;adapters;memory")

add_executable(unit_integration_hippocampus_adapter
    adapters/memory/test_hippocampus_adapter.cpp
)
target_link_libraries(unit_integration_hippocampus_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_hippocampus_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_hippocampus_adapter
    PROPERTIES LABELS "unit;integration;adapters;memory")

# --- Executive Layer Adapters ---

add_executable(unit_integration_basal_ganglia_adapter
    adapters/executive/test_basal_ganglia_adapter.cpp
)
target_link_libraries(unit_integration_basal_ganglia_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_basal_ganglia_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_basal_ganglia_adapter
    PROPERTIES LABELS "unit;integration;adapters;executive")

add_executable(unit_integration_pfc_adapter
    adapters/executive/test_pfc_adapter.cpp
)
target_link_libraries(unit_integration_pfc_adapter
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_integration_pfc_adapter PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_pfc_adapter
    PROPERTIES LABELS "unit;integration;adapters;executive")

# Inter-Layer Router Queue Depth Tests (Tier 4 fix)
add_executable(unit_integration_inter_layer_router_queue
    test_inter_layer_router_queue.cpp
)
target_link_libraries(unit_integration_inter_layer_router_queue
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_integration_inter_layer_router_queue
    PRIVATE ${CMAKE_SOURCE_DIR}/test
)
target_compile_features(unit_integration_inter_layer_router_queue PRIVATE cxx_std_17)
gtest_discover_tests(unit_integration_inter_layer_router_queue
    PROPERTIES LABELS "unit;integration;inter_layer_router")

message(STATUS "  Added 8 layer infrastructure unit tests")
message(STATUS "  Added 14 module adapter unit tests")
