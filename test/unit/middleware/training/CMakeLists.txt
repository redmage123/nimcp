# Training Module Unit Tests
# Phase TM-1: Security + Memory Pool Integration Tests
# TODO: Prefer gtest_discover_tests over add_test for automatic test registration

# =============================================================================
# Orphan detection policy: Source files without a corresponding add_executable
# are wrapped in if(EXISTS) guards at the bottom of this file. These will
# automatically activate when the source file is created.
# =============================================================================

add_executable(unit_middleware_training_test_training_module
    test_training_module.cpp
)

target_include_directories(unit_middleware_training_test_training_module PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_training_module PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_training_module
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_training_module
         COMMAND unit_middleware_training_test_training_module
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Label for filtering
set_tests_properties(unit_middleware_training_test_training_module
    PROPERTIES LABELS "unit;middleware;training" TIMEOUT 120)

# Loss Functions Unit Tests (Phase TM-2)
add_executable(unit_middleware_training_test_loss_functions
    test_loss_functions.cpp
)

target_include_directories(unit_middleware_training_test_loss_functions PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_loss_functions PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_loss_functions
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_loss_functions
         COMMAND unit_middleware_training_test_loss_functions
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_loss_functions
    PROPERTIES
        LABELS "unit;middleware;training;loss"
        TIMEOUT 60)

# Optimizers Unit Tests (Phase TM-2)
add_executable(unit_middleware_training_test_optimizers
    test_optimizers.cpp
)

target_include_directories(unit_middleware_training_test_optimizers PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_optimizers PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_optimizers
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_optimizers
         COMMAND unit_middleware_training_test_optimizers
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_optimizers
    PROPERTIES
        LABELS "unit;middleware;training;optimizer"
        TIMEOUT 60)

# Learning Rate Scheduler Unit Tests (Phase TM-4)
add_executable(unit_middleware_training_test_lr_scheduler
    test_lr_scheduler.cpp
)

target_include_directories(unit_middleware_training_test_lr_scheduler PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_lr_scheduler PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_lr_scheduler
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_lr_scheduler
         COMMAND unit_middleware_training_test_lr_scheduler
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_lr_scheduler
    PROPERTIES
        LABELS "unit;middleware;training;lr_scheduler"
        TIMEOUT 60)

# Regularization Unit Tests (Phase TM-5)
add_executable(unit_middleware_training_test_regularization
    test_regularization.cpp
)

target_include_directories(unit_middleware_training_test_regularization PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_regularization PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_regularization
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_regularization
         COMMAND unit_middleware_training_test_regularization
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_regularization
    PROPERTIES
        LABELS "unit;middleware;training;regularization"
        TIMEOUT 60)

# Gradient Management Unit Tests (Phase TM-6)
add_executable(unit_middleware_training_test_gradient_manager
    test_gradient_manager.cpp
)

target_include_directories(unit_middleware_training_test_gradient_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_gradient_manager PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_gradient_manager
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_gradient_manager
         COMMAND unit_middleware_training_test_gradient_manager
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_gradient_manager
    PROPERTIES
        LABELS "unit;middleware;training;gradient"
        TIMEOUT 60)

# Training-Plasticity Bridge Unit Tests (Phase TPB-1)
add_executable(unit_middleware_training_test_plasticity_bridge
    test_training_plasticity_bridge.cpp
)

target_include_directories(unit_middleware_training_test_plasticity_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_plasticity_bridge PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_plasticity_bridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_plasticity_bridge
         COMMAND unit_middleware_training_test_plasticity_bridge
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_plasticity_bridge
    PROPERTIES
        LABELS "unit;middleware;training;plasticity_bridge"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Event-Driven Plasticity Unit Tests (Phase EDP-1)
add_executable(unit_middleware_training_test_event_driven_plasticity
    test_event_driven_plasticity.cpp
)

target_include_directories(unit_middleware_training_test_event_driven_plasticity PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_event_driven_plasticity PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_event_driven_plasticity
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_event_driven_plasticity
         COMMAND unit_middleware_training_test_event_driven_plasticity
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_event_driven_plasticity
    PROPERTIES
        LABELS "unit;middleware;training;event_driven_plasticity"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Security Registration Unit Tests (Phase SR-1)
add_executable(unit_middleware_training_test_security_registration
    test_security_registration.cpp
)

target_include_directories(unit_middleware_training_test_security_registration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_security_registration PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_security_registration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_security_registration
         COMMAND unit_middleware_training_test_security_registration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_security_registration
    PROPERTIES
        LABELS "unit;middleware;training;security_registration"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Training Callbacks Unit Tests (Phase TCB-1)
add_executable(unit_middleware_training_test_training_callbacks
    test_training_callbacks.cpp
)

target_include_directories(unit_middleware_training_test_training_callbacks PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_training_callbacks PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_training_callbacks
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_training_callbacks
         COMMAND unit_middleware_training_test_training_callbacks
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_training_callbacks
    PROPERTIES
        LABELS "unit;middleware;training;callbacks"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Training Plasticity Bridge Bio-Async Integration Tests (Phase TPB-BIO-1)
add_executable(unit_middleware_training_test_tpb_bio_async
    test_tpb_bio_async.cpp
)

target_include_directories(unit_middleware_training_test_tpb_bio_async PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_tpb_bio_async PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_tpb_bio_async
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

gtest_discover_tests(unit_middleware_training_test_tpb_bio_async
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;middleware;training;tpb;bio-async"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy"
)

# Training Adapters Bio-Async Integration Tests (Phase TA-BIO-1)
add_executable(unit_middleware_training_test_training_adapters_bio_async
    test_training_adapters_bio_async.cpp
)

target_include_directories(unit_middleware_training_test_training_adapters_bio_async PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_training_adapters_bio_async PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_training_adapters_bio_async
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

gtest_discover_tests(unit_middleware_training_test_training_adapters_bio_async
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;middleware;training;adapters;bio-async"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy"
)

# Portia-Training Integration Tests (Phase PORTIA-1)
# DISABLED: test_portia_training_integration.cpp has incorrect API usage
# TODO: Fix and re-enable after API review
# The test uses:
#   - bio_router_context_t (doesn't exist - use bio_router_config_t)
#   - default_inbox_capacity (should be inbox_capacity)
#   - enable_priority_channels (doesn't exist)
#   - worker_thread_count (should be worker_threads)
message(STATUS "  [SKIP] test_portia_training_integration.cpp - source file not yet created (API fixes needed)")

# Training-Logic Bridge Unit Tests (Phase TLB-1)
add_executable(unit_middleware_training_test_training_logic_bridge
    test_training_logic_bridge.cpp
)

target_include_directories(unit_middleware_training_test_training_logic_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_training_logic_bridge PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_training_logic_bridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_training_logic_bridge
         COMMAND unit_middleware_training_test_training_logic_bridge
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_training_logic_bridge
    PROPERTIES
        LABELS "unit;middleware;training;logic_bridge"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Cognitive-Training Bridge Unit Tests (Phase CTB-1)
add_executable(unit_middleware_training_test_cognitive_training_bridge
    test_cognitive_training_bridge.cpp
)

target_include_directories(unit_middleware_training_test_cognitive_training_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_cognitive_training_bridge PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_cognitive_training_bridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_cognitive_training_bridge
         COMMAND unit_middleware_training_test_cognitive_training_bridge
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_cognitive_training_bridge
    PROPERTIES
        LABELS "unit;middleware;training;cognitive_training"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Perception-Training Bridge Unit Tests (Phase PTB-1)
add_executable(unit_middleware_training_test_perception_training_bridge
    test_perception_training_bridge.cpp
)

target_include_directories(unit_middleware_training_test_perception_training_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_perception_training_bridge PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_perception_training_bridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_perception_training_bridge
         COMMAND unit_middleware_training_test_perception_training_bridge
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_perception_training_bridge
    PROPERTIES
        LABELS "unit;middleware;training;perception_training"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Cortical-Training Bridge Unit Tests (Phase CRTB-1)
add_executable(unit_middleware_training_test_cortical_training_bridge
    test_cortical_training_bridge.cpp
)

target_include_directories(unit_middleware_training_test_cortical_training_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_cortical_training_bridge PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_cortical_training_bridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_cortical_training_bridge
         COMMAND unit_middleware_training_test_cortical_training_bridge
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_cortical_training_bridge
    PROPERTIES
        LABELS "unit;middleware;training;cortical_training"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Occipital-Training Bridge Unit Tests (Phase OTB-1)
add_executable(unit_middleware_training_test_occipital_training_bridge
    test_nimcp_occipital_training_bridge.cpp
)

target_include_directories(unit_middleware_training_test_occipital_training_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_occipital_training_bridge PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_occipital_training_bridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_occipital_training_bridge
         COMMAND unit_middleware_training_test_occipital_training_bridge
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_occipital_training_bridge
    PROPERTIES
        LABELS "unit;middleware;training;occipital_training"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Convergent Training Decision Unit Tests (Layer 1)
add_executable(unit_middleware_training_test_training_convergent_decision
    test_training_convergent_decision.cpp
)

target_include_directories(unit_middleware_training_test_training_convergent_decision PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_training_convergent_decision PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_training_convergent_decision
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_training_convergent_decision
         COMMAND unit_middleware_training_test_training_convergent_decision
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_training_convergent_decision
    PROPERTIES
        LABELS "unit;middleware;training;convergent_decision"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Training Causal Model Unit Tests (Layer 2: Causal DAG of training dynamics)
add_executable(unit_middleware_training_test_training_causal_model
    test_training_causal_model.cpp
)

target_include_directories(unit_middleware_training_test_training_causal_model PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_training_causal_model PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_training_causal_model
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_training_causal_model
         COMMAND unit_middleware_training_test_training_causal_model
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_training_causal_model
    PROPERTIES
        LABELS "unit;middleware;training;causal_model"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Abductive Training Diagnosis Unit Tests (Layer 3)
add_executable(unit_middleware_training_test_training_diagnosis
    test_training_diagnosis.cpp
)

target_include_directories(unit_middleware_training_test_training_diagnosis PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_training_diagnosis PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_training_diagnosis
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_training_diagnosis
         COMMAND unit_middleware_training_test_training_diagnosis
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_training_diagnosis
    PROPERTIES
        LABELS "unit;middleware;training;diagnosis"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Training Integration Fixes Unit Tests (Bug fixes: Portia, inflammation, buffer, modulation)
add_executable(unit_middleware_training_test_training_integration_fixes
    test_training_integration_fixes.cpp
)

target_include_directories(unit_middleware_training_test_training_integration_fixes PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_training_integration_fixes PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_training_integration_fixes
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_training_integration_fixes
         COMMAND unit_middleware_training_test_training_integration_fixes
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_training_integration_fixes
    PROPERTIES
        LABELS "unit;middleware;training;integration_fixes"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Gradient Manager Destroy Race Tests (Bug H12)
add_executable(unit_middleware_training_test_gradient_manager_destroy_race
    test_gradient_manager_destroy_race.cpp
)

target_include_directories(unit_middleware_training_test_gradient_manager_destroy_race PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(unit_middleware_training_test_gradient_manager_destroy_race PRIVATE cxx_std_20)

target_link_libraries(unit_middleware_training_test_gradient_manager_destroy_race
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_middleware_training_test_gradient_manager_destroy_race
         COMMAND unit_middleware_training_test_gradient_manager_destroy_race
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_middleware_training_test_gradient_manager_destroy_race
    PROPERTIES
        LABELS "unit;middleware;training;gradient;thread_safety"
        TIMEOUT 60)

# =============================================================================
# Orphaned Unit Test Targets
# =============================================================================

# Brain Training Bio-Async Unit Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_training_bio_async.cpp)
    add_executable(unit_middleware_training_test_brain_training_bio_async
        test_brain_training_bio_async.cpp
    )
    target_include_directories(unit_middleware_training_test_brain_training_bio_async PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(unit_middleware_training_test_brain_training_bio_async PRIVATE cxx_std_20)
    target_link_libraries(unit_middleware_training_test_brain_training_bio_async PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_middleware_training_test_brain_training_bio_async
             COMMAND unit_middleware_training_test_brain_training_bio_async
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_middleware_training_test_brain_training_bio_async
        PROPERTIES
            LABELS "unit;middleware;training;bio-async;brain"
            TIMEOUT 120)
endif()

# Event-Driven Plasticity Bio-Async Unit Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_event_driven_plasticity_bio_async.cpp)
    add_executable(unit_middleware_training_test_event_driven_plasticity_bio_async
        test_event_driven_plasticity_bio_async.cpp
    )
    target_include_directories(unit_middleware_training_test_event_driven_plasticity_bio_async PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(unit_middleware_training_test_event_driven_plasticity_bio_async PRIVATE cxx_std_20)
    target_link_libraries(unit_middleware_training_test_event_driven_plasticity_bio_async PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_middleware_training_test_event_driven_plasticity_bio_async
             COMMAND unit_middleware_training_test_event_driven_plasticity_bio_async
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_middleware_training_test_event_driven_plasticity_bio_async
        PROPERTIES
            LABELS "unit;middleware;training;event-driven-plasticity;bio-async"
            TIMEOUT 120)
endif()

# Learning Signal Adapter Unit Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_learning_signal_adapter.cpp)
    add_executable(unit_middleware_training_test_learning_signal_adapter
        test_learning_signal_adapter.cpp
    )
    target_include_directories(unit_middleware_training_test_learning_signal_adapter PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(unit_middleware_training_test_learning_signal_adapter PRIVATE cxx_std_20)
    target_link_libraries(unit_middleware_training_test_learning_signal_adapter PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_middleware_training_test_learning_signal_adapter
             COMMAND unit_middleware_training_test_learning_signal_adapter
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_middleware_training_test_learning_signal_adapter
        PROPERTIES
            LABELS "unit;middleware;training;learning-signal;adapter"
            TIMEOUT 120)
endif()

# Optimizer Bio-Async Unit Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_optimizer_bio_async.cpp)
    add_executable(unit_middleware_training_test_optimizer_bio_async
        test_optimizer_bio_async.cpp
    )
    target_include_directories(unit_middleware_training_test_optimizer_bio_async PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(unit_middleware_training_test_optimizer_bio_async PRIVATE cxx_std_20)
    target_link_libraries(unit_middleware_training_test_optimizer_bio_async PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_middleware_training_test_optimizer_bio_async
             COMMAND unit_middleware_training_test_optimizer_bio_async
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_middleware_training_test_optimizer_bio_async
        PROPERTIES
            LABELS "unit;middleware;training;optimizer;bio-async"
            TIMEOUT 120)
endif()

# Training Event Manager Unit Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_event_manager.cpp)
    add_executable(unit_middleware_training_test_training_event_manager
        test_training_event_manager.cpp
    )
    target_include_directories(unit_middleware_training_test_training_event_manager PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(unit_middleware_training_test_training_event_manager PRIVATE cxx_std_20)
    target_link_libraries(unit_middleware_training_test_training_event_manager PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_middleware_training_test_training_event_manager
             COMMAND unit_middleware_training_test_training_event_manager
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_middleware_training_test_training_event_manager
        PROPERTIES
            LABELS "unit;middleware;training;event-manager"
            TIMEOUT 120)
endif()

# Weight Update Router Unit Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_weight_update_router.cpp)
    add_executable(unit_middleware_training_test_weight_update_router
        test_weight_update_router.cpp
    )
    target_include_directories(unit_middleware_training_test_weight_update_router PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(unit_middleware_training_test_weight_update_router PRIVATE cxx_std_20)
    target_link_libraries(unit_middleware_training_test_weight_update_router PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_middleware_training_test_weight_update_router
             COMMAND unit_middleware_training_test_weight_update_router
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_middleware_training_test_weight_update_router
        PROPERTIES
            LABELS "unit;middleware;training;weight-update;router"
            TIMEOUT 120)
endif()
