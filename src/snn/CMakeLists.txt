# ============================================================================
# SNN (Spiking Neural Network) Module CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for the SNN module
# WHY:  Compile SNN library and link with existing infrastructure
# HOW:  Build static library, link to nimcp core and dependencies
# ============================================================================

# SNN source files
set(SNN_SOURCES
    nimcp_snn_config.c
    nimcp_snn_network.c
    nimcp_snn_encoding.c
    nimcp_snn_training.c
    nimcp_snn_bio_async.c
    nimcp_snn_immune.c
    nimcp_snn_ternary.c
)

# SNN Bridge source files - Perception
set(SNN_BRIDGE_PERCEPTION_SOURCES
    bridges/nimcp_snn_visual_bridge.c
    bridges/nimcp_snn_audio_bridge.c
    bridges/nimcp_snn_speech_bridge.c
)

# SNN Bridge source files - Plasticity
set(SNN_BRIDGE_PLASTICITY_SOURCES
    bridges/nimcp_snn_stdp_bridge.c
    bridges/nimcp_snn_bcm_bridge.c
    bridges/nimcp_snn_homeostatic_bridge.c
    bridges/nimcp_snn_scaling_bridge.c
)

# SNN Bridge source files - Cognitive
set(SNN_BRIDGE_COGNITIVE_SOURCES
    bridges/nimcp_snn_attention_bridge.c
    bridges/nimcp_snn_memory_bridge.c
    bridges/nimcp_snn_reasoning_bridge.c
    bridges/nimcp_snn_executive_bridge.c
)

# SNN Bridge source files - Memory Regions
set(SNN_BRIDGE_REGIONS_SOURCES
    bridges/nimcp_snn_hippocampus_bridge.c
    bridges/nimcp_snn_prefrontal_bridge.c
)

# SNN Bridge source files - Middleware
set(SNN_BRIDGE_MIDDLEWARE_SOURCES
    bridges/nimcp_snn_routing_bridge.c
    bridges/nimcp_snn_buffer_bridge.c
    bridges/nimcp_snn_population_bridge.c
    bridges/nimcp_snn_thalamic_bridge.c
    bridges/nimcp_snn_cortical_bridge.c
)

# SNN Bridge source files - Emotion
set(SNN_BRIDGE_EMOTION_SOURCES
    bridges/nimcp_snn_emotion_bridge.c
    bridges/nimcp_snn_grief_bridge.c
    bridges/nimcp_snn_joy_bridge.c
    bridges/nimcp_snn_love_bridge.c
    bridges/nimcp_snn_empathetic_bridge.c
)

# SNN Bridge source files - Consciousness/Metacognition
set(SNN_BRIDGE_CONSCIOUSNESS_SOURCES
    bridges/nimcp_snn_introspection_bridge.c
    bridges/nimcp_snn_global_workspace_bridge.c
    bridges/nimcp_snn_free_energy_bridge.c
    bridges/nimcp_snn_self_model_bridge.c
)

# SNN Bridge source files - Higher Cognition
set(SNN_BRIDGE_COGNITION_SOURCES
    bridges/nimcp_snn_tom_bridge.c
    bridges/nimcp_snn_curiosity_bridge.c
    bridges/nimcp_snn_meta_learning_bridge.c
)

# SNN Bridge source files - Health/Wellbeing
set(SNN_BRIDGE_HEALTH_SOURCES
    bridges/nimcp_snn_wellbeing_bridge.c
    bridges/nimcp_snn_mental_health_bridge.c
    bridges/nimcp_snn_sleep_bridge.c
)

# SNN Bridge source files - Memory Systems
set(SNN_BRIDGE_MEMORY_SOURCES
    bridges/nimcp_snn_shadow_bridge.c
    bridges/nimcp_snn_emotional_tagging_bridge.c
    bridges/nimcp_snn_autobiographical_bridge.c
)

# SNN Bridge source files - Training Integration
set(SNN_BRIDGE_TRAINING_SOURCES
    bridges/nimcp_snn_training_bridge.c
    bridges/nimcp_snn_training_integration_bridge.c
)

# SNN Bridge source files - Brainstem Integration
set(SNN_BRIDGE_BRAINSTEM_SOURCES
    bridges/nimcp_snn_medulla_bridge.c
)

# Combined bridge sources
set(SNN_BRIDGE_SOURCES
    ${SNN_BRIDGE_PERCEPTION_SOURCES}
    ${SNN_BRIDGE_PLASTICITY_SOURCES}
    ${SNN_BRIDGE_COGNITIVE_SOURCES}
    ${SNN_BRIDGE_REGIONS_SOURCES}
    ${SNN_BRIDGE_MIDDLEWARE_SOURCES}
    ${SNN_BRIDGE_EMOTION_SOURCES}
    ${SNN_BRIDGE_CONSCIOUSNESS_SOURCES}
    ${SNN_BRIDGE_COGNITION_SOURCES}
    ${SNN_BRIDGE_HEALTH_SOURCES}
    ${SNN_BRIDGE_MEMORY_SOURCES}
    ${SNN_BRIDGE_TRAINING_SOURCES}
    ${SNN_BRIDGE_BRAINSTEM_SOURCES}
)

# Create SNN static library with bridges
add_library(nimcp_snn STATIC ${SNN_SOURCES} ${SNN_BRIDGE_SOURCES})

# Include directories
target_include_directories(nimcp_snn
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/snn
)

# Link with core nimcp library (provides neuron_t, synapse_t, etc.)
target_link_libraries(nimcp_snn
    PUBLIC
        nimcp
    PRIVATE
        Threads::Threads
        m
)

# Compiler features
target_compile_features(nimcp_snn PRIVATE c_std_11)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nimcp_snn PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Export for use by other targets
set_target_properties(nimcp_snn PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
)

message(STATUS "Configured SNN module with ${CMAKE_C_COMPILER_ID}")
