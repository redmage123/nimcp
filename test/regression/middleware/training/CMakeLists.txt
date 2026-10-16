# Training Module Regression Tests
# Phase TM-1: Security + Memory Pool Integration Regression Tests
# TODO: Prefer gtest_discover_tests over add_test for automatic test registration

add_executable(regression_middleware_training_test_training_module_regression
    test_training_module_regression.cpp
)

target_include_directories(regression_middleware_training_test_training_module_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_training_module_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_training_module_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_training_module_regression
         COMMAND regression_middleware_training_test_training_module_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_training_module_regression
    PROPERTIES
        LABELS "regression;middleware;training"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Loss Functions Regression Tests (Phase TM-2)
add_executable(regression_middleware_training_test_loss_functions_regression
    test_loss_functions_regression.cpp
)

target_include_directories(regression_middleware_training_test_loss_functions_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_loss_functions_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_loss_functions_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_loss_functions_regression
         COMMAND regression_middleware_training_test_loss_functions_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_loss_functions_regression
    PROPERTIES
        LABELS "regression;middleware;training;loss"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Optimizers Regression Tests (Phase TM-2)
add_executable(regression_middleware_training_test_optimizers_regression
    test_optimizers_regression.cpp
)

target_include_directories(regression_middleware_training_test_optimizers_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_optimizers_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_optimizers_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_optimizers_regression
         COMMAND regression_middleware_training_test_optimizers_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_optimizers_regression
    PROPERTIES
        LABELS "regression;middleware;training;optimizer"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Training-Plasticity Bridge Regression Tests (Phase TPB-1)
add_executable(regression_middleware_training_test_plasticity_bridge_regression
    test_training_plasticity_bridge_regression.cpp
)

target_include_directories(regression_middleware_training_test_plasticity_bridge_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_plasticity_bridge_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_plasticity_bridge_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_plasticity_bridge_regression
         COMMAND regression_middleware_training_test_plasticity_bridge_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_plasticity_bridge_regression
    PROPERTIES
        LABELS "regression;middleware;training;plasticity_bridge"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Event-Driven Plasticity Regression Tests (Phase EDP-1)
add_executable(regression_middleware_training_test_event_driven_plasticity_regression
    test_event_driven_plasticity_regression.cpp
)

target_include_directories(regression_middleware_training_test_event_driven_plasticity_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_event_driven_plasticity_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_event_driven_plasticity_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_event_driven_plasticity_regression
         COMMAND regression_middleware_training_test_event_driven_plasticity_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_event_driven_plasticity_regression
    PROPERTIES
        LABELS "regression;middleware;training;event_driven_plasticity"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Security Registration Regression Tests (Phase SR-1)
add_executable(regression_middleware_training_test_security_registration_regression
    test_security_registration_regression.cpp
)

target_include_directories(regression_middleware_training_test_security_registration_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_security_registration_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_security_registration_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_security_registration_regression
         COMMAND regression_middleware_training_test_security_registration_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_security_registration_regression
    PROPERTIES
        LABELS "regression;middleware;training;security_registration"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Training Callbacks Regression Tests (Phase TCB-1)
add_executable(regression_middleware_training_test_training_callbacks_regression
    test_training_callbacks_regression.cpp
)

target_include_directories(regression_middleware_training_test_training_callbacks_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_training_callbacks_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_training_callbacks_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_training_callbacks_regression
         COMMAND regression_middleware_training_test_training_callbacks_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_training_callbacks_regression
    PROPERTIES
        LABELS "regression;middleware;training;callbacks"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Portia-Training Regression Tests (Phase PORTIA-1)
# DISABLED: test_portia_training_regression.cpp has incorrect API usage
message(STATUS "  [SKIP] test_portia_training_regression.cpp - source file not yet created (API fixes needed)")

# Training-Logic Bridge Regression Tests (Phase TLB-1)
add_executable(regression_middleware_training_test_training_logic_bridge_regression
    test_training_logic_bridge_regression.cpp
)

target_include_directories(regression_middleware_training_test_training_logic_bridge_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_training_logic_bridge_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_training_logic_bridge_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_training_logic_bridge_regression
         COMMAND regression_middleware_training_test_training_logic_bridge_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_training_logic_bridge_regression
    PROPERTIES
        LABELS "regression;middleware;training;logic_bridge"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Cognitive-Training Bridge Regression Tests (Phase CTB-1)
add_executable(regression_middleware_training_test_cognitive_training_bridge_regression
    test_cognitive_training_bridge_regression.cpp
)

target_include_directories(regression_middleware_training_test_cognitive_training_bridge_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_cognitive_training_bridge_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_cognitive_training_bridge_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_cognitive_training_bridge_regression
         COMMAND regression_middleware_training_test_cognitive_training_bridge_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_cognitive_training_bridge_regression
    PROPERTIES
        LABELS "regression;middleware;training;cognitive_training"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Perception-Training Bridge Regression Tests (Phase PTB-1)
add_executable(regression_middleware_training_test_perception_training_bridge_regression
    test_perception_training_regression.cpp
)

target_include_directories(regression_middleware_training_test_perception_training_bridge_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_perception_training_bridge_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_perception_training_bridge_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_perception_training_bridge_regression
         COMMAND regression_middleware_training_test_perception_training_bridge_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_perception_training_bridge_regression
    PROPERTIES
        LABELS "regression;middleware;training;perception_training"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Cortical-Training Bridge Regression Tests (Phase CRTB-1)
add_executable(regression_middleware_training_test_cortical_training_bridge_regression
    test_cortical_training_regression.cpp
)

target_include_directories(regression_middleware_training_test_cortical_training_bridge_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(regression_middleware_training_test_cortical_training_bridge_regression PRIVATE cxx_std_20)

target_link_libraries(regression_middleware_training_test_cortical_training_bridge_regression
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME regression_middleware_training_test_cortical_training_bridge_regression
         COMMAND regression_middleware_training_test_cortical_training_bridge_regression
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(regression_middleware_training_test_cortical_training_bridge_regression
    PROPERTIES
        LABELS "regression;middleware;training;cortical_training"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# =============================================================================
# Orphaned Regression Test Targets
# =============================================================================

# Event-Driven Plasticity Bio-Async Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_event_driven_plasticity_bio_async_regression.cpp)
    add_executable(regression_middleware_training_test_edp_bio_async_regression
        test_event_driven_plasticity_bio_async_regression.cpp
    )
    target_include_directories(regression_middleware_training_test_edp_bio_async_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(regression_middleware_training_test_edp_bio_async_regression PRIVATE cxx_std_20)
    target_link_libraries(regression_middleware_training_test_edp_bio_async_regression PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME regression_middleware_training_test_edp_bio_async_regression
             COMMAND regression_middleware_training_test_edp_bio_async_regression
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_middleware_training_test_edp_bio_async_regression
        PROPERTIES
            LABELS "regression;middleware;training;event-driven-plasticity;bio-async"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# Training Adapters Bio-Async Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_adapters_bio_async_regression.cpp)
    add_executable(regression_middleware_training_test_adapters_bio_async_regression
        test_training_adapters_bio_async_regression.cpp
    )
    target_include_directories(regression_middleware_training_test_adapters_bio_async_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(regression_middleware_training_test_adapters_bio_async_regression PRIVATE cxx_std_20)
    target_link_libraries(regression_middleware_training_test_adapters_bio_async_regression PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME regression_middleware_training_test_adapters_bio_async_regression
             COMMAND regression_middleware_training_test_adapters_bio_async_regression
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_middleware_training_test_adapters_bio_async_regression
        PROPERTIES
            LABELS "regression;middleware;training;adapters;bio-async"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()
