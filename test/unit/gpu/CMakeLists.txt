# GPU Unit Tests

macro(add_gpu_test TEST_NAME)
    add_executable(unit_gpu_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_gpu_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_gpu_${TEST_NAME}
             COMMAND unit_gpu_${TEST_NAME})
endmacro()

# GPU detection and execution tests
add_gpu_test(test_simd_detect)
add_gpu_test(test_network_detect)
add_gpu_test(test_execution_mode)
add_gpu_test(test_gpu_neuron)
add_gpu_test(test_spike_event)

# Multi-GPU bounds validation tests (P2-13 fix)
add_gpu_test(test_multigpu_bounds)

# AWS Neuron/Inferentia bridge tests (no CUDA required — stubs on non-Inferentia)
add_gpu_test(test_neuron_bridge)

# GPU stub false positive throw tests (no CUDA required)
add_gpu_test(test_gpu_stub_no_false_throws)

# GPU inference optimization unit tests (no CUDA required — GTEST_SKIP if no GPU)
add_gpu_test(test_gpu_inference_unit)

# Softmax numerical stability tests (CPU backend, no CUDA required)
add_gpu_test(test_softmax_numerical_stability)

# CUDA Kernel tests - only built when CUDA toolkit is available
# These tests require actual CUDA kernel implementations
if(CUDAToolkit_FOUND)
    message(STATUS "  Building CUDA kernel unit tests (CUDA toolkit found)")

    # GPU quantum and inference kernel tests
    add_gpu_test(test_quantum_kernels)
    # DISABLED: Test needs mc_gpu_status_string and mc_should_use_gpu implementations
    # add_gpu_test(test_qmc_gpu)  # GPU Monte Carlo and MCTS
    add_gpu_test(test_inference_kernels)

    # GPU tensor and training kernel tests
    add_gpu_test(test_tensor_kernels)
    add_gpu_test(test_training_kernels)

    # GPU SNN and LNN kernel tests
    add_gpu_test(test_snn_kernels)
    add_gpu_test(test_snn_triplet_stdp_gpu)
    add_gpu_test(test_lnn_kernels)
    add_gpu_test(test_lnn_ode_kernels)

    # GPU Swarm Intelligence kernel tests
    add_gpu_test(test_swarm_kernels)

    # GPU Neural Substrate kernel tests (axon, dendrite, myelin, neuromod, glial, metabolic)
    add_gpu_test(test_substrate_gpu)

    # GPU Axon module tests
    add_gpu_test(test_axon_gpu)

    # GPU Dendrite module tests
    add_gpu_test(test_dendrite_gpu)

    # GPU Myelin module tests
    add_gpu_test(test_myelin_gpu)

    # GPU Brain Oscillation kernel tests (PLV, PLI, PAC, coherence, FFT)
    add_gpu_test(test_oscillations_kernels)

    # GPU Memory Consolidation kernel tests (hippocampal replay, systems consolidation, engrams)
    add_gpu_test(test_memory_consolidation_kernels)

    # GPU Knowledge Graph kernel tests (BFS/DFS traversal, similarity, hyperbolic space)
    add_gpu_test(test_knowledge_graph_kernels)

    # GPU Synaptic Transmission kernel tests (Tsodyks-Markram, receptor kinetics)
    add_gpu_test(test_synapse_kernels)

    # GPU Dragonfly Vision kernel tests (target tracking, optical flow, STMD)
    add_gpu_test(test_dragonfly_vision_kernels)

    # GPU Plasticity kernel tests (STDP, BCM, homeostatic, STP, calcium dynamics)
    add_gpu_test(test_plasticity_kernels)

    # GPU Neuromodulator kernel tests (dopamine, serotonin, acetylcholine, norepinephrine)
    add_gpu_test(test_neuromodulator_kernels)

    # GPU Emotion kernel tests (amygdala, OFC, NAcc, ACC - limbic system)
    add_gpu_test(test_emotion_kernels)

    # GPU Reasoning kernel tests (logic, rule engine, CSP, causal inference)
    add_gpu_test(test_reasoning_kernels)

    # GPU Metalearning kernel tests (MAML, Reptile, ProtoNets)
    add_gpu_test(test_metalearning_kernels)

    # GPU Portia kernel tests (spider vision, attention, spatial cognition)
    add_gpu_test(test_portia_kernels)

    # GPU Sleep kernel tests (NREM/REM consolidation, homeostasis, replay)
    add_gpu_test(test_sleep_kernels)

    # GPU Brain Regions kernel tests (cortical columns, PFC, motor, parietal)
    add_gpu_test(test_regions_kernels)

    # GPU Ternary kernel tests (2-bit packing, no-multiply GEMM, quantization)
    add_gpu_test(test_ternary_kernels)

    # GPU Perception kernel tests (atan2, optical flow, STFT, batched matmul)
    add_gpu_test(test_perception_gpu)

    # GPU LNN Enhancement tests (gradient DAO, CSR weights, spectral radius)
    add_gpu_test(test_lnn_enhancements_gpu)

    # GPU Speech and Oscillations kernel tests (LPC, Levinson-Durbin, Hilbert, PAC)
    add_gpu_test(test_speech_oscillations_gpu)

    # GPU CNN Backward Pass tests (im2col, col2im, layer norm, instance norm)
    add_gpu_test(test_cnn_backward_gpu)

    # GPU Swarm Radix Sort, Tensor Axis Reduction, and Enhanced Myelin tests
    add_gpu_test(test_swarm_tensor_myelin_gpu)

    # GPU Graph kernel tests (CSR graph, BFS, clustering, centrality, DAO, GraphQL)
    add_gpu_test(test_graph_gpu)

    # GPU Occipital Bridge tests (V1-V5 visual processing, saliency, optical flow)
    add_gpu_test(test_occipital_gpu_bridge)
else()
    message(STATUS "  Skipping CUDA kernel tests (CUDA toolkit not found)")
endif()

# Include subdirectory tests
add_subdirectory(immune)
add_subdirectory(exception)

# Include cognitive subdirectory tests (Broca GPU, etc.)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cognitive/CMakeLists.txt)
    add_subdirectory(cognitive)
endif()

# Include fuzzy GPU tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fuzzy/CMakeLists.txt)
    add_subdirectory(fuzzy)
endif()

# Include financial GPU tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/financial/CMakeLists.txt)
    add_subdirectory(financial)
endif()

# Include statistics GPU tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/statistics/CMakeLists.txt)
    add_subdirectory(statistics)
endif()

# Include recovery GPU tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/recovery/CMakeLists.txt)
    add_subdirectory(recovery)
endif()

# Include perception GPU tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/perception/CMakeLists.txt)
    add_subdirectory(perception)
endif()

# Include inference GPU tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/inference/CMakeLists.txt)
    add_subdirectory(inference)
endif()

# Include neural GPU tests (SNN, LNN, CNN recovery)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/neural/CMakeLists.txt)
    add_subdirectory(neural)
endif()

# Include swarm GPU tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/swarm/CMakeLists.txt)
    add_subdirectory(swarm)
endif()

# Include hypothalamus GPU tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/hypothalamus/CMakeLists.txt)
    add_subdirectory(hypothalamus)
endif()

# Include quantum GPU tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/quantum/CMakeLists.txt)
    add_subdirectory(quantum)
endif()

# Include graph GPU recovery tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/graph/CMakeLists.txt)
    add_subdirectory(graph)
endif()

# Include misc GPU recovery integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/misc/CMakeLists.txt)
    add_subdirectory(misc)
endif()

# Include training GPU unit tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/training/CMakeLists.txt)
    add_subdirectory(training)
endif()
