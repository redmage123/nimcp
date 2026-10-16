# SNN and CNN Training Unit Tests
# Tests for SNN/CNN layer construction, forward/backward pass, gradient computation

# SNN Backprop Unit Tests
# DISABLED: snn_surrogate_t and snn_surrogate_method_t have duplicate enum values
# Needs API refactoring to unify snn/nimcp_snn_types.h and training/nimcp_snn_backprop.h
message(STATUS "unit_training_test_snn_backprop: DISABLED (source file not yet created - duplicate enum definitions in headers)")
#[[
add_executable(unit_training_test_snn_backprop
    test_snn_backprop.cpp
)

target_include_directories(unit_training_test_snn_backprop PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_snn_backprop PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_snn_backprop
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_snn_backprop
         COMMAND unit_training_test_snn_backprop
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_snn_backprop
    PROPERTIES
        LABELS "unit;training;snn;backprop"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")
#]]

# CNN Backprop Unit Tests
add_executable(unit_training_test_cnn_backprop
    test_cnn_backprop.cpp
)

target_include_directories(unit_training_test_cnn_backprop PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_cnn_backprop PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_cnn_backprop
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_cnn_backprop
         COMMAND unit_training_test_cnn_backprop
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_cnn_backprop
    PROPERTIES
        LABELS "unit;training;cnn;backprop;gradient"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# CNN-Cortex Bridge Unit Tests
add_executable(unit_training_test_cnn_cortex_bridge
    test_cnn_cortex_bridge.cpp
)

target_include_directories(unit_training_test_cnn_cortex_bridge PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_cnn_cortex_bridge PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_cnn_cortex_bridge
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_cnn_cortex_bridge
         COMMAND unit_training_test_cnn_cortex_bridge
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_cnn_cortex_bridge
    PROPERTIES
        LABELS "unit;training;cnn;cortex;bridge"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Auto Architecture Unit Tests
# DISABLED: auto_arch_stats_t/lnn_config_t incomplete type issues
message(STATUS "unit_training_test_auto_architecture: DISABLED (source file not yet created - incomplete type issues)")
#[[
add_executable(unit_training_test_auto_architecture
    test_auto_architecture.cpp
)

target_include_directories(unit_training_test_auto_architecture PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_auto_architecture PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_auto_architecture
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_auto_architecture
         COMMAND unit_training_test_auto_architecture
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_auto_architecture
    PROPERTIES
        LABELS "unit;training;auto;architecture"
        TIMEOUT 180
        RESOURCE_LOCK "brain_heavy")
#]]

# Auto Architecture NAS Unit Tests
add_executable(unit_training_test_auto_architecture_nas
    test_auto_architecture_nas.cpp
)

target_include_directories(unit_training_test_auto_architecture_nas PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_auto_architecture_nas PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_auto_architecture_nas
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_auto_architecture_nas
         COMMAND unit_training_test_auto_architecture_nas
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_auto_architecture_nas
    PROPERTIES
        LABELS "unit;training;auto;architecture;nas;evolutionary;rl;darts;pruning;pareto"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy")

# Meta-Learning and HPO Unit Tests
add_executable(unit_training_test_meta_hpo
    test_training_enhancements_meta_hpo.cpp
)

target_include_directories(unit_training_test_meta_hpo PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_meta_hpo PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_meta_hpo
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_meta_hpo
         COMMAND unit_training_test_meta_hpo
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_meta_hpo
    PROPERTIES
        LABELS "unit;training;meta-learning;hpo;hyperparameter"
        TIMEOUT 180
        RESOURCE_LOCK "brain_heavy")

# Knowledge Distillation and QAT Unit Tests
add_executable(unit_training_test_kd_qat
    test_training_enhancements_kd_qat.cpp
)

target_include_directories(unit_training_test_kd_qat PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_kd_qat PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_kd_qat
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_kd_qat
         COMMAND unit_training_test_kd_qat
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_kd_qat
    PROPERTIES
        LABELS "unit;training;kd;qat;distillation;quantization"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy")

# Continual Learning and Multi-Task Learning Unit Tests
add_executable(unit_training_test_cl_mtl
    test_training_enhancements_cl_mtl.cpp
)

target_include_directories(unit_training_test_cl_mtl PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_cl_mtl PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_cl_mtl
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_cl_mtl
         COMMAND unit_training_test_cl_mtl
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_cl_mtl
    PROPERTIES
        LABELS "unit;training;continual-learning;multi-task;cl;mtl"
        TIMEOUT 180
        RESOURCE_LOCK "brain_heavy")

# Mixed Precision (AMP) and Distributed Training Unit Tests
add_executable(unit_training_test_amp_distributed
    test_training_enhancements_amp_dist.cpp
)

target_include_directories(unit_training_test_amp_distributed PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_amp_distributed PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_amp_distributed
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_amp_distributed
         COMMAND unit_training_test_amp_distributed
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_amp_distributed
    PROPERTIES
        LABELS "unit;training;amp;mixed-precision;distributed"
        TIMEOUT 180
        RESOURCE_LOCK "brain_heavy")

# Adversarial Training and Gradient Scaling Unit Tests
add_executable(unit_training_test_adv_gs
    test_training_enhancements_adv_gs.cpp
)

target_include_directories(unit_training_test_adv_gs PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_training_test_adv_gs PRIVATE cxx_std_20)

target_link_libraries(unit_training_test_adv_gs
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp_middleware
        nimcp
        Threads::Threads
        m
)

add_test(NAME unit_training_test_adv_gs
         COMMAND unit_training_test_adv_gs
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(unit_training_test_adv_gs
    PROPERTIES
        LABELS "unit;training;adversarial;gradient-scaling;adv;gs"
        TIMEOUT 180
        RESOURCE_LOCK "brain_heavy")

# =============================================================================
# Orphaned Unit Test Targets
# =============================================================================

# Quantization Ternary Unit Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_quantization_ternary.cpp)
    add_executable(unit_training_test_quantization_ternary
        test_quantization_ternary.cpp
    )
    target_include_directories(unit_training_test_quantization_ternary PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_training_test_quantization_ternary PRIVATE cxx_std_20)
    target_link_libraries(unit_training_test_quantization_ternary PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_training_test_quantization_ternary
             COMMAND unit_training_test_quantization_ternary
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_training_test_quantization_ternary
        PROPERTIES
            LABELS "unit;training;quantization;ternary"
            TIMEOUT 120
            RESOURCE_LOCK "brain_heavy")
endif()

# SNN Backprop Stubs Unit Tests
# Header enum conflict resolved via #ifdef guards in nimcp_snn_backprop.h
# 29/55 pass (26 fail because snn_backprop_create() is still a stub returning NULL)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_backprop_stubs.cpp)
    add_executable(unit_training_test_snn_backprop_stubs
        test_snn_backprop_stubs.cpp
    )
    target_include_directories(unit_training_test_snn_backprop_stubs PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_training_test_snn_backprop_stubs PRIVATE cxx_std_20)
    target_link_libraries(unit_training_test_snn_backprop_stubs PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_training_test_snn_backprop_stubs
             COMMAND unit_training_test_snn_backprop_stubs
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_training_test_snn_backprop_stubs
        PROPERTIES
            LABELS "unit;training;snn;backprop;stubs"
            TIMEOUT 120
            RESOURCE_LOCK "brain_heavy")
endif()

# Training Dispatch Unit Tests (C — libcheck)
# NOTE: Requires pkg_check_modules(CHECK REQUIRED check) in a parent CMakeLists.txt
# or FindPkgConfig + libcheck-dev installed. The check_pic/subunit targets come from
# the system libcheck package (apt install check libsubunit-dev).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_dispatch.c)
    add_executable(unit_training_test_training_dispatch
        test_training_dispatch.c
    )
    target_include_directories(unit_training_test_training_dispatch PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_training_test_training_dispatch PRIVATE c_std_11)
    target_compile_options(unit_training_test_training_dispatch PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_training_test_training_dispatch PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_training_test_training_dispatch
             COMMAND unit_training_test_training_dispatch
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_training_test_training_dispatch
        PROPERTIES
            LABELS "unit;training;dispatch"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# SNN-Primary Architecture Tests (steps 5-10)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_primary_architecture.c)
    add_executable(unit_training_test_snn_primary_architecture
        test_snn_primary_architecture.c
    )
    target_include_directories(unit_training_test_snn_primary_architecture PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_training_test_snn_primary_architecture PRIVATE c_std_11)
    target_compile_options(unit_training_test_snn_primary_architecture PRIVATE
        -Wall -Wextra
    )
    target_link_libraries(unit_training_test_snn_primary_architecture PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME unit_training_test_snn_primary_architecture
             COMMAND unit_training_test_snn_primary_architecture
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_training_test_snn_primary_architecture
        PROPERTIES
            LABELS "unit;training;snn-primary"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# Cross-Network Bridge Validation Tests (Phase D)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cross_network_bridges.cpp)
    add_executable(unit_training_test_cross_network_bridges
        test_cross_network_bridges.cpp
    )
    target_include_directories(unit_training_test_cross_network_bridges PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_training_test_cross_network_bridges PRIVATE cxx_std_20)
    target_link_libraries(unit_training_test_cross_network_bridges PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_training_test_cross_network_bridges
             COMMAND unit_training_test_cross_network_bridges
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_training_test_cross_network_bridges
        PROPERTIES
            LABELS "unit;training;bridges;gradient;validation"
            TIMEOUT 120
            RESOURCE_LOCK "brain_heavy")
endif()

# Cortex CNN Thalamic Adapter Unit Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cortex_cnn_thalamic.cpp)
    add_executable(unit_training_test_cortex_cnn_thalamic
        test_cortex_cnn_thalamic.cpp
    )
    target_include_directories(unit_training_test_cortex_cnn_thalamic PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_training_test_cortex_cnn_thalamic PRIVATE cxx_std_20)
    target_link_libraries(unit_training_test_cortex_cnn_thalamic PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_training_test_cortex_cnn_thalamic
             COMMAND unit_training_test_cortex_cnn_thalamic
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_training_test_cortex_cnn_thalamic
        PROPERTIES
            LABELS "unit;training;cnn;cortex;thalamic"
            TIMEOUT 120
            RESOURCE_LOCK "brain_heavy")
endif()

# Cortex CNN Substrate Adapter Unit Tests (Phase 3)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cortex_cnn_substrate.cpp)
    add_executable(unit_training_test_cortex_cnn_substrate
        test_cortex_cnn_substrate.cpp
    )
    target_include_directories(unit_training_test_cortex_cnn_substrate PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(unit_training_test_cortex_cnn_substrate PRIVATE cxx_std_20)
    target_link_libraries(unit_training_test_cortex_cnn_substrate PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME unit_training_test_cortex_cnn_substrate
             COMMAND unit_training_test_cortex_cnn_substrate
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(unit_training_test_cortex_cnn_substrate
        PROPERTIES
            LABELS "unit;training;cnn;cortex;substrate"
            TIMEOUT 120
            RESOURCE_LOCK "brain_heavy")
endif()

# Training Integration Hub Unit Tests
add_subdirectory(integration)

# Training Exception Handling Unit Tests
add_subdirectory(exception)
