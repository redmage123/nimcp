# Middleware Subsystems
add_library(nimcp_middleware STATIC
    # Buffering
    buffering/nimcp_circular_buffer.c
    buffering/nimcp_sliding_window.c
    buffering/nimcp_temporal_accumulator.c
    buffering/nimcp_integration_buffer.c
    buffering/nimcp_phase_coded_buffer.c

    # Normalization
    normalization/nimcp_zscore_normalizer.c
    normalization/nimcp_min_max_normalizer.c
    normalization/nimcp_adaptive_normalizer.c
    normalization/nimcp_homeostatic_normalizer.c
    normalization/nimcp_normalization_immune.c

    # Encoding (Phase 1 & Phase 2)
    encoding/nimcp_rate_coding.c
    encoding/nimcp_temporal_coding.c
    encoding/nimcp_population_coding.c

    # Feature Extraction (Phase 2)
    features/nimcp_feature_extractor.c

    # Brain Integration
    brain_integration.c

    # Events module removed - using core event bus (core/events/nimcp_event_bus.h)

    # Pattern Recognition
    patterns/nimcp_synchrony_detector.c
    patterns/nimcp_sequence_detector.c
    patterns/nimcp_oscillation_detector.c
    patterns/nimcp_pattern_library.c
    patterns/nimcp_pattern_cow.c

    # Thalamic Routing
    routing/nimcp_signal_wrapper.c
    routing/nimcp_thalamic_router.c
    routing/nimcp_attention_gate.c
    routing/nimcp_routing_table.c

    # Phase 1.5.1: Shannon Monitor & Flow Tracker
    integration/nimcp_shannon_monitor.c
    integration/nimcp_flow_tracker.c

    # Phase 1.5.2: Executive Integration & Quantum Command Propagation
    integration/nimcp_quantum_command_propagator.c
    integration/nimcp_executive_middleware_adapter.c

    # Cognitive Adapters
    cognitive/nimcp_cognitive_adapters.c
    cognitive/nimcp_working_memory_adapter.c

    # Training Adapters
    training/nimcp_training_adapters.c

    # Training Pipeline (Phase TM-2)
    training/nimcp_loss_functions.c
    training/nimcp_optimizers.c

    # Learning Rate Schedulers (Phase TM-4)
    training/nimcp_lr_scheduler.c

    # Regularization (Phase TM-5)
    training/nimcp_regularization.c

    # Gradient Management (Phase TM-6)
    training/nimcp_gradient_manager.c

    # Training-Plasticity Bridge (Phase TPB-1)
    training/nimcp_training_plasticity_bridge.c

    # Event-Driven Plasticity (Phase EDP-1)
    training/nimcp_event_driven_plasticity.c

    # Training Callbacks (Phase TCB-1)
    training/nimcp_training_callbacks.c

    # Training-Logic Bridge (Phase TLB-1)
    training/nimcp_training_logic_bridge.c

    # Cognitive-Training Bridge (Phase CTB-1)
    training/nimcp_cognitive_training_bridge.c

    # Perception-Training Bridge (Phase PTB-1)
    training/nimcp_perception_training_bridge.c

    # Cortical-Training Bridge (Phase CRTB-1)
    training/nimcp_cortical_training_bridge.c

    # Convergent Training Decision (Layer 1: multi-module evidence accumulation)
    training/nimcp_training_convergent_decision.c

    # Abductive Training Diagnosis (Layer 3: Root Cause Analysis)
    training/nimcp_training_diagnosis.c

    # Cloud Inference Bridge (Edge-Cloud Hybrid)
    cloud/nimcp_cloud_inference.c
)

target_include_directories(nimcp_middleware PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(nimcp_middleware PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(nimcp_middleware
    PRIVATE
        m
)

install(TARGETS nimcp_middleware
    EXPORT nimcp-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# Install middleware headers from include/middleware
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/middleware
    DESTINATION include/nimcp
    FILES_MATCHING PATTERN "*.h"
)
