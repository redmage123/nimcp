# Training Module Integration Tests
# Phase TM-1: Security + Memory Pool Integration

add_executable(integration_middleware_training_test_training_module_integration
    test_training_module_integration.cpp
)

target_include_directories(integration_middleware_training_test_training_module_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_training_module_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_training_module_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_training_module_integration
         COMMAND integration_middleware_training_test_training_module_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_training_module_integration
    PROPERTIES
        LABELS "integration;middleware;training"
        TIMEOUT 60
        RESOURCE_LOCK "brain_heavy")

# Loss Functions Integration Tests (Phase TM-2)
add_executable(integration_middleware_training_test_loss_functions_integration
    test_loss_functions_integration.cpp
)

target_include_directories(integration_middleware_training_test_loss_functions_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_loss_functions_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_loss_functions_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_loss_functions_integration
         COMMAND integration_middleware_training_test_loss_functions_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_loss_functions_integration
    PROPERTIES
        LABELS "integration;middleware;training;loss"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Optimizers Integration Tests (Phase TM-2)
add_executable(integration_middleware_training_test_optimizers_integration
    test_optimizers_integration.cpp
)

target_include_directories(integration_middleware_training_test_optimizers_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_optimizers_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_optimizers_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_optimizers_integration
         COMMAND integration_middleware_training_test_optimizers_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_optimizers_integration
    PROPERTIES
        LABELS "integration;middleware;training;optimizer"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Brain Training Integration Tests (Phase TM-3)
add_executable(integration_middleware_training_test_brain_training_integration
    test_brain_training_integration.cpp
)

target_include_directories(integration_middleware_training_test_brain_training_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_brain_training_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_brain_training_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_brain_training_integration
         COMMAND integration_middleware_training_test_brain_training_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_brain_training_integration
    PROPERTIES
        LABELS "integration;middleware;training;brain"
        TIMEOUT 180
        RESOURCE_LOCK "brain_heavy")

# Training-Plasticity Bridge Integration Tests (Phase TPB-1)
add_executable(integration_middleware_training_test_plasticity_bridge_integration
    test_training_plasticity_bridge_integration.cpp
)

target_include_directories(integration_middleware_training_test_plasticity_bridge_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_plasticity_bridge_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_plasticity_bridge_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_plasticity_bridge_integration
         COMMAND integration_middleware_training_test_plasticity_bridge_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_plasticity_bridge_integration
    PROPERTIES
        LABELS "integration;middleware;training;plasticity_bridge"
        TIMEOUT 240
        RESOURCE_LOCK "brain_heavy")

# Event-Driven Plasticity Integration Tests (Phase EDP-1)
add_executable(integration_middleware_training_test_event_driven_plasticity_integration
    test_event_driven_plasticity_integration.cpp
)

target_include_directories(integration_middleware_training_test_event_driven_plasticity_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_event_driven_plasticity_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_event_driven_plasticity_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_event_driven_plasticity_integration
         COMMAND integration_middleware_training_test_event_driven_plasticity_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_event_driven_plasticity_integration
    PROPERTIES
        LABELS "integration;middleware;training;event_driven_plasticity"
        TIMEOUT 240
        RESOURCE_LOCK "brain_heavy")

# Security Registration Integration Tests (Phase SR-1)
add_executable(integration_middleware_training_test_security_registration_integration
    test_security_registration_integration.cpp
)

target_include_directories(integration_middleware_training_test_security_registration_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_security_registration_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_security_registration_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_security_registration_integration
         COMMAND integration_middleware_training_test_security_registration_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_security_registration_integration
    PROPERTIES
        LABELS "integration;middleware;training;security_registration"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Training Callbacks Integration Tests (Phase TCB-1)
add_executable(integration_middleware_training_test_training_callbacks_integration
    test_training_callbacks_integration.cpp
)

target_include_directories(integration_middleware_training_test_training_callbacks_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_training_callbacks_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_training_callbacks_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_training_callbacks_integration
         COMMAND integration_middleware_training_test_training_callbacks_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_training_callbacks_integration
    PROPERTIES
        LABELS "integration;middleware;training;callbacks"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Training Async Pipeline Integration Tests (Phase ASYNC-1)
add_executable(integration_middleware_training_test_training_async_pipeline
    test_training_async_pipeline.cpp
)

target_include_directories(integration_middleware_training_test_training_async_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_training_async_pipeline PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_training_async_pipeline
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_training_async_pipeline
         COMMAND integration_middleware_training_test_training_async_pipeline
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_training_async_pipeline
    PROPERTIES
        LABELS "integration;middleware;training;async;bio-async"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Portia-Training Integration Tests (Phase PORTIA-1)
# DISABLED: test_portia_training_integration.cpp has incorrect API usage
message(STATUS "  [SKIP] test_portia_training_integration.cpp - source file not yet created (API fixes needed)")

# Training-Logic Bridge Integration Tests (Phase TLB-1)
add_executable(integration_middleware_training_test_training_logic_bridge_integration
    test_training_logic_bridge_integration.cpp
)

target_include_directories(integration_middleware_training_test_training_logic_bridge_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_training_logic_bridge_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_training_logic_bridge_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_training_logic_bridge_integration
         COMMAND integration_middleware_training_test_training_logic_bridge_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_training_logic_bridge_integration
    PROPERTIES
        LABELS "integration;middleware;training;logic_bridge"
        TIMEOUT 240
        RESOURCE_LOCK "brain_heavy")

# Cognitive-Training Bridge Integration Tests (Phase CTB-1)
add_executable(integration_middleware_training_test_cognitive_training_bridge_integration
    test_cognitive_training_bridge_integration.cpp
)

target_include_directories(integration_middleware_training_test_cognitive_training_bridge_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_cognitive_training_bridge_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_cognitive_training_bridge_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_cognitive_training_bridge_integration
         COMMAND integration_middleware_training_test_cognitive_training_bridge_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_cognitive_training_bridge_integration
    PROPERTIES
        LABELS "integration;middleware;training;cognitive_training"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Perception-Training Bridge Integration Tests (Phase PTB-1)
add_executable(integration_middleware_training_test_perception_training_integration
    test_perception_training_integration.cpp
)

target_include_directories(integration_middleware_training_test_perception_training_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_perception_training_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_perception_training_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_perception_training_integration
         COMMAND integration_middleware_training_test_perception_training_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_perception_training_integration
    PROPERTIES
        LABELS "integration;middleware;training;perception_training"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Cortical-Training Bridge Integration Tests (Phase CRTB-1)
add_executable(integration_middleware_training_test_cortical_training_integration
    test_cortical_training_integration.cpp
)

target_include_directories(integration_middleware_training_test_cortical_training_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_cortical_training_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_cortical_training_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_cortical_training_integration
         COMMAND integration_middleware_training_test_cortical_training_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_cortical_training_integration
    PROPERTIES
        LABELS "integration;middleware;training;cortical_training"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Perception-Cognitive Cross-Bridge Integration Tests (Phase XBI-1)
add_executable(integration_middleware_training_test_perception_cognitive_integration
    test_perception_cognitive_integration.cpp
)

target_include_directories(integration_middleware_training_test_perception_cognitive_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_perception_cognitive_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_perception_cognitive_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_perception_cognitive_integration
         COMMAND integration_middleware_training_test_perception_cognitive_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_perception_cognitive_integration
    PROPERTIES
        LABELS "integration;middleware;training;cross_bridge;perception_cognitive"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Cortical-Cognitive Cross-Bridge Integration Tests (Phase XBI-2)
add_executable(integration_middleware_training_test_cortical_cognitive_integration
    test_cortical_cognitive_integration.cpp
)

target_include_directories(integration_middleware_training_test_cortical_cognitive_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_cortical_cognitive_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_cortical_cognitive_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_cortical_cognitive_integration
         COMMAND integration_middleware_training_test_cortical_cognitive_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_cortical_cognitive_integration
    PROPERTIES
        LABELS "integration;middleware;training;cross_bridge;cortical_cognitive"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Perception-Logic Cross-Bridge Integration Tests (Phase XBI-3)
add_executable(integration_middleware_training_test_perception_logic_integration
    test_perception_logic_integration.cpp
)

target_include_directories(integration_middleware_training_test_perception_logic_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_perception_logic_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_perception_logic_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_perception_logic_integration
         COMMAND integration_middleware_training_test_perception_logic_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_perception_logic_integration
    PROPERTIES
        LABELS "integration;middleware;training;cross_bridge;perception_logic"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Cortical-Logic Cross-Bridge Integration Tests (Phase XBI-4)
add_executable(integration_middleware_training_test_cortical_logic_integration
    test_cortical_logic_integration.cpp
)

target_include_directories(integration_middleware_training_test_cortical_logic_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_cortical_logic_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_cortical_logic_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_cortical_logic_integration
         COMMAND integration_middleware_training_test_cortical_logic_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_cortical_logic_integration
    PROPERTIES
        LABELS "integration;middleware;training;cross_bridge;cortical_logic"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Perception-Immune Cross-Bridge Integration Tests (Phase XBI-5)
add_executable(integration_middleware_training_test_perception_immune_integration
    test_perception_immune_integration.cpp
)

target_include_directories(integration_middleware_training_test_perception_immune_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_perception_immune_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_perception_immune_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_perception_immune_integration
         COMMAND integration_middleware_training_test_perception_immune_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_perception_immune_integration
    PROPERTIES
        LABELS "integration;middleware;training;cross_bridge;perception_immune"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Cortical-Immune Cross-Bridge Integration Tests (Phase XBI-6)
add_executable(integration_middleware_training_test_cortical_immune_integration
    test_cortical_immune_integration.cpp
)

target_include_directories(integration_middleware_training_test_cortical_immune_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_cortical_immune_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_cortical_immune_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_cortical_immune_integration
         COMMAND integration_middleware_training_test_cortical_immune_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_cortical_immune_integration
    PROPERTIES
        LABELS "integration;middleware;training;cross_bridge;cortical_immune"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Perception-Plasticity Cross-Bridge Integration Tests (Phase XBI-7)
add_executable(integration_middleware_training_test_perception_plasticity_integration
    test_perception_plasticity_integration.cpp
)

target_include_directories(integration_middleware_training_test_perception_plasticity_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_perception_plasticity_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_perception_plasticity_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_perception_plasticity_integration
         COMMAND integration_middleware_training_test_perception_plasticity_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_perception_plasticity_integration
    PROPERTIES
        LABELS "integration;middleware;training;cross_bridge;perception_plasticity"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Cortical-Plasticity Cross-Bridge Integration Tests (Phase XBI-8)
add_executable(integration_middleware_training_test_cortical_plasticity_integration
    test_cortical_plasticity_integration.cpp
)

target_include_directories(integration_middleware_training_test_cortical_plasticity_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_middleware_training_test_cortical_plasticity_integration PRIVATE cxx_std_20)

target_link_libraries(integration_middleware_training_test_cortical_plasticity_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME integration_middleware_training_test_cortical_plasticity_integration
         COMMAND integration_middleware_training_test_cortical_plasticity_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(integration_middleware_training_test_cortical_plasticity_integration
    PROPERTIES
        LABELS "integration;middleware;training;cross_bridge;cortical_plasticity"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# =============================================================================
# Bio-Async Orphaned Integration Test Targets
# =============================================================================

# Event-Driven Plasticity Bio-Async Integration Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_event_driven_plasticity_bio_async_integration.cpp)
    add_executable(integration_middleware_training_test_edp_bio_async
        test_event_driven_plasticity_bio_async_integration.cpp
    )
    target_include_directories(integration_middleware_training_test_edp_bio_async PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(integration_middleware_training_test_edp_bio_async PRIVATE cxx_std_20)
    target_link_libraries(integration_middleware_training_test_edp_bio_async PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME integration_middleware_training_test_edp_bio_async
             COMMAND integration_middleware_training_test_edp_bio_async
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_middleware_training_test_edp_bio_async
        PROPERTIES
            LABELS "integration;middleware;training;bio-async;event-driven-plasticity"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# Training Adapters Bio-Async Integration Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_adapters_bio_async_integration.cpp)
    add_executable(integration_middleware_training_test_adapters_bio_async
        test_training_adapters_bio_async_integration.cpp
    )
    target_include_directories(integration_middleware_training_test_adapters_bio_async PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(integration_middleware_training_test_adapters_bio_async PRIVATE cxx_std_20)
    target_link_libraries(integration_middleware_training_test_adapters_bio_async PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME integration_middleware_training_test_adapters_bio_async
             COMMAND integration_middleware_training_test_adapters_bio_async
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_middleware_training_test_adapters_bio_async
        PROPERTIES
            LABELS "integration;middleware;training;bio-async;adapters"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# Training Pipeline Bio-Async Integration Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_pipeline_bio_async.cpp)
    add_executable(integration_middleware_training_test_pipeline_bio_async
        test_training_pipeline_bio_async.cpp
    )
    target_include_directories(integration_middleware_training_test_pipeline_bio_async PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(integration_middleware_training_test_pipeline_bio_async PRIVATE cxx_std_20)
    target_link_libraries(integration_middleware_training_test_pipeline_bio_async PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME integration_middleware_training_test_pipeline_bio_async
             COMMAND integration_middleware_training_test_pipeline_bio_async
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(integration_middleware_training_test_pipeline_bio_async
        PROPERTIES
            LABELS "integration;middleware;training;bio-async;pipeline"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()
