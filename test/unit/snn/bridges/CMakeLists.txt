# SNN Bridge Unit Tests
#
# Tests for SNN integration bridges:
# - Perception: Visual, Audio, Speech (45 tests)
# - Plasticity: STDP, BCM, Homeostatic, Scaling (60 tests)
# - Cognitive: Attention, Memory, Reasoning, Executive (60 tests)
# - Memory Regions: Hippocampus, Prefrontal (30 tests)
# - Middleware: Routing, Buffer, Population, Thalamic, Cortical (75 tests)
# - Emotion: Emotion, Grief, Joy, Love, Empathetic (75 tests)
# - Consciousness: Introspection, Global Workspace, Free Energy, Self Model (60 tests)
# - Cognition: ToM, Curiosity, Meta Learning (45 tests)
# - Health: Wellbeing, Mental Health, Sleep (45 tests)
# - Memory Systems: Shadow, Emotional Tagging, Autobiographical (45 tests)
# - Training: Training Bridge (30 tests)
#
# Total: 570+ tests

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping SNN bridge unit tests")
    return()
endif()

# Helper function to create bridge test
function(add_snn_bridge_test name)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_${name}_bridge.cpp)
        add_executable(test_snn_${name}_bridge
            test_snn_${name}_bridge.cpp
        )
        target_link_libraries(test_snn_${name}_bridge
            PRIVATE
            nimcp_snn
            nimcp
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            m
        )
        target_include_directories(test_snn_${name}_bridge
            PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
        )
        target_compile_features(test_snn_${name}_bridge PRIVATE cxx_std_17)
        add_test(
            NAME test_snn_${name}_bridge
            COMMAND test_snn_${name}_bridge
        )
        message(STATUS "  Added SNN bridge test: test_snn_${name}_bridge")
    endif()
endfunction()

#=============================================================================
# Perception Bridges
#=============================================================================
add_snn_bridge_test(visual)
add_snn_bridge_test(audio)
add_snn_bridge_test(speech)

set_tests_properties(
    test_snn_visual_bridge
    test_snn_audio_bridge
    test_snn_speech_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;perception"
)

#=============================================================================
# Plasticity Bridges
#=============================================================================
add_snn_bridge_test(stdp)
add_snn_bridge_test(bcm)
add_snn_bridge_test(homeostatic)
add_snn_bridge_test(scaling)

set_tests_properties(
    test_snn_stdp_bridge
    test_snn_bcm_bridge
    test_snn_homeostatic_bridge
    test_snn_scaling_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;plasticity"
)

#=============================================================================
# Cognitive Bridges
#=============================================================================
add_snn_bridge_test(attention)
add_snn_bridge_test(memory)
add_snn_bridge_test(reasoning)
add_snn_bridge_test(executive)

set_tests_properties(
    test_snn_attention_bridge
    test_snn_memory_bridge
    test_snn_reasoning_bridge
    test_snn_executive_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;cognitive"
)

#=============================================================================
# Memory Region Bridges
#=============================================================================
add_snn_bridge_test(hippocampus)
add_snn_bridge_test(prefrontal)

set_tests_properties(
    test_snn_hippocampus_bridge
    test_snn_prefrontal_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;regions"
)

#=============================================================================
# Middleware Bridges
#=============================================================================
add_snn_bridge_test(routing)
add_snn_bridge_test(buffer)
add_snn_bridge_test(population)
add_snn_bridge_test(thalamic)
add_snn_bridge_test(cortical)

set_tests_properties(
    test_snn_routing_bridge
    test_snn_buffer_bridge
    test_snn_population_bridge
    test_snn_thalamic_bridge
    test_snn_cortical_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;middleware"
)

#=============================================================================
# Emotion Bridges
#=============================================================================
add_snn_bridge_test(emotion)
add_snn_bridge_test(grief)
add_snn_bridge_test(joy)
add_snn_bridge_test(love)
add_snn_bridge_test(empathetic)

set_tests_properties(
    test_snn_emotion_bridge
    test_snn_grief_bridge
    test_snn_joy_bridge
    test_snn_love_bridge
    test_snn_empathetic_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;emotion"
)

#=============================================================================
# Consciousness/Metacognition Bridges
#=============================================================================
add_snn_bridge_test(introspection)
add_snn_bridge_test(global_workspace)
add_snn_bridge_test(free_energy)
add_snn_bridge_test(self_model)

set_tests_properties(
    test_snn_introspection_bridge
    test_snn_global_workspace_bridge
    test_snn_free_energy_bridge
    test_snn_self_model_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;consciousness"
)

#=============================================================================
# Higher Cognition Bridges
#=============================================================================
add_snn_bridge_test(tom)
add_snn_bridge_test(curiosity)
add_snn_bridge_test(meta_learning)

set_tests_properties(
    test_snn_tom_bridge
    test_snn_curiosity_bridge
    test_snn_meta_learning_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;cognition"
)

#=============================================================================
# Health/Wellbeing Bridges
#=============================================================================
add_snn_bridge_test(wellbeing)
add_snn_bridge_test(mental_health)
add_snn_bridge_test(sleep)

set_tests_properties(
    test_snn_wellbeing_bridge
    test_snn_mental_health_bridge
    test_snn_sleep_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;health"
)

#=============================================================================
# Memory System Bridges
#=============================================================================
add_snn_bridge_test(shadow)
add_snn_bridge_test(emotional_tagging)
add_snn_bridge_test(autobiographical)

set_tests_properties(
    test_snn_shadow_bridge
    test_snn_emotional_tagging_bridge
    test_snn_autobiographical_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;memory"
)

#=============================================================================
# Training Integration Bridge
#=============================================================================
add_snn_bridge_test(training)
add_snn_bridge_test(training_integration)

set_tests_properties(
    test_snn_training_bridge
    test_snn_training_integration_bridge
    PROPERTIES
    TIMEOUT 90
    LABELS "snn;bridges;training"
)

#=============================================================================
# Medulla Bridge (Brainstem Integration)
#=============================================================================
add_snn_bridge_test(medulla)

set_tests_properties(
    test_snn_medulla_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;medulla;brainstem"
)

#=============================================================================
# Language Bridge (SNN ↔ Language spike-driven word-concept binding)
#=============================================================================
add_snn_bridge_test(language)

set_tests_properties(
    test_snn_language_bridge
    PROPERTIES
    TIMEOUT 60
    LABELS "snn;bridges;language"
)
