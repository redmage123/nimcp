# =============================================================================
# NIMCP Training Integration Regression Tests
# =============================================================================
# WHAT: Regression tests guarding against training integration edge-case bugs
# WHY:  Prevent regressions in reward updates, adaptive LR, batch updates,
#       reasoning stability, and arousal bounds
# HOW:  GTest executable linked against nimcp library
# =============================================================================

add_executable(regression_training_integration_test
    test_training_integration_regression.cpp
)

target_link_libraries(regression_training_integration_test
    PRIVATE
        nimcp
        nimcp_middleware
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_training_integration_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_training_integration_test PRIVATE cxx_std_20)

gtest_discover_tests(regression_training_integration_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;training;integration"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy"
)

# =============================================================================
# NIMCP Training Pipeline Regression Tests
# =============================================================================
# WHAT: Regression tests for the full training pipeline (public API + brain_ti_*)
# WHY:  Guards against label buffer overflow, gradient NaN, sparse synapse
#       segfaults, probe validity, brain state isolation, modulation NaN, and
#       post-batch stability.
# HOW:  GTest executable linked against nimcp library
#
# REGRESSIONS COVERED:
# 1. Label buffer overflow (nimcp_part_core.c:281)
# 2. Gradient distribution correctness
# 3. Sparse synapse learning no-segfault
# 4. Probe validity after learning
# 5. Multiple brains independence
# 6. Loss NaN/Inf detection
# 7. Zero-feature training safety
# 8. Label size boundary handling
# 9. Modulation state never NaN
# 10. Post-batch update stability
# =============================================================================

add_executable(regression_training_pipeline_test
    test_training_pipeline_regression.cpp
)

target_link_libraries(regression_training_pipeline_test
    PRIVATE
        nimcp
        nimcp_middleware
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_training_pipeline_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(regression_training_pipeline_test PRIVATE cxx_std_20)

gtest_discover_tests(regression_training_pipeline_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;training;pipeline;modulation"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy"
)

# =============================================================================
# =============================================================================
# Per-Cortex CNN Processor Tests
# =============================================================================

# Cortex CNN Unit Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cortex_cnn_unit.cpp)
    add_executable(regression_cortex_cnn_unit_test
        test_cortex_cnn_unit.cpp
    )
    target_include_directories(regression_cortex_cnn_unit_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_cortex_cnn_unit_test PRIVATE cxx_std_20)
    target_link_libraries(regression_cortex_cnn_unit_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_cortex_cnn_unit_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;cortex;cnn;unit"
            TIMEOUT 120
    )
endif()

# Cortex CNN Integration Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cortex_cnn_integration.cpp)
    add_executable(regression_cortex_cnn_integration_test
        test_cortex_cnn_integration.cpp
    )
    target_include_directories(regression_cortex_cnn_integration_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_cortex_cnn_integration_test PRIVATE cxx_std_20)
    target_link_libraries(regression_cortex_cnn_integration_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_cortex_cnn_integration_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;cortex;cnn;integration"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# Cortex CNN Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cortex_cnn_regression.cpp)
    add_executable(regression_cortex_cnn_regression_test
        test_cortex_cnn_regression.cpp
    )
    target_include_directories(regression_cortex_cnn_regression_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_cortex_cnn_regression_test PRIVATE cxx_std_20)
    target_link_libraries(regression_cortex_cnn_regression_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_cortex_cnn_regression_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;cortex;cnn"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# Cortex CNN E2E Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cortex_cnn_e2e.cpp)
    add_executable(regression_cortex_cnn_e2e_test
        test_cortex_cnn_e2e.cpp
    )
    target_include_directories(regression_cortex_cnn_e2e_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_cortex_cnn_e2e_test PRIVATE cxx_std_20)
    target_link_libraries(regression_cortex_cnn_e2e_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_cortex_cnn_e2e_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;cortex;cnn;e2e"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# Orphaned Regression Test Targets
# =============================================================================

# CNN Backprop Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cnn_backprop_regression.cpp)
    add_executable(regression_training_cnn_backprop_test
        test_cnn_backprop_regression.cpp
    )
    target_include_directories(regression_training_cnn_backprop_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_cnn_backprop_test PRIVATE cxx_std_20)
    target_link_libraries(regression_training_cnn_backprop_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_training_cnn_backprop_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;cnn;backprop"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# CNN-Cortex Bridge Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cnn_cortex_bridge_regression.cpp)
    add_executable(regression_training_cnn_cortex_bridge_test
        test_cnn_cortex_bridge_regression.cpp
    )
    target_include_directories(regression_training_cnn_cortex_bridge_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_cnn_cortex_bridge_test PRIVATE cxx_std_20)
    target_link_libraries(regression_training_cnn_cortex_bridge_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_training_cnn_cortex_bridge_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;cnn;cortex;bridge"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# NAS Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_nas_regression.cpp)
    add_executable(regression_training_nas_test
        test_nas_regression.cpp
    )
    target_include_directories(regression_training_nas_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_nas_test PRIVATE cxx_std_20)
    target_link_libraries(regression_training_nas_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_training_nas_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;nas;architecture"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# SNN Backprop Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_backprop_regression.cpp)
    add_executable(regression_training_snn_backprop_test
        test_snn_backprop_regression.cpp
    )
    target_include_directories(regression_training_snn_backprop_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_snn_backprop_test PRIVATE cxx_std_20)
    target_link_libraries(regression_training_snn_backprop_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_training_snn_backprop_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;snn;backprop"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# SNN Backprop Stubs Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_backprop_stubs_regression.cpp)
    add_executable(regression_training_snn_backprop_stubs_test
        test_snn_backprop_stubs_regression.cpp
    )
    target_include_directories(regression_training_snn_backprop_stubs_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_snn_backprop_stubs_test PRIVATE cxx_std_20)
    target_link_libraries(regression_training_snn_backprop_stubs_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_training_snn_backprop_stubs_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;snn;backprop;stubs"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# Training Portia Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_portia_regression.cpp)
    add_executable(regression_training_portia_test
        test_training_portia_regression.cpp
    )
    target_include_directories(regression_training_portia_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_portia_test PRIVATE cxx_std_20)
    target_link_libraries(regression_training_portia_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_training_portia_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;portia"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# Training Stability Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_stability_regression.cpp)
    add_executable(regression_training_stability_test
        test_training_stability_regression.cpp
    )
    target_include_directories(regression_training_stability_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_stability_test PRIVATE cxx_std_20)
    target_link_libraries(regression_training_stability_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_training_stability_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;stability"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# Training Symbolic Logic Hub Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_symbolic_logic_hub_regression.cpp)
    add_executable(regression_training_symbolic_logic_hub_test
        test_training_symbolic_logic_hub_regression.cpp
    )
    target_include_directories(regression_training_symbolic_logic_hub_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_symbolic_logic_hub_test PRIVATE cxx_std_20)
    target_link_libraries(regression_training_symbolic_logic_hub_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    gtest_discover_tests(regression_training_symbolic_logic_hub_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "regression;training;symbolic-logic;hub"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy"
    )
endif()

# Training Dispatch Regression Tests (C — libcheck)
# NOTE: Requires pkg_check_modules(CHECK REQUIRED check) in a parent CMakeLists.txt
# or FindPkgConfig + libcheck-dev installed. The check_pic/subunit targets come from
# the system libcheck package (apt install check libsubunit-dev).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_training_dispatch_regression.c)
    add_executable(regression_training_dispatch_test
        test_training_dispatch_regression.c
    )
    target_include_directories(regression_training_dispatch_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_dispatch_test PRIVATE c_std_11)
    target_link_libraries(regression_training_dispatch_test PRIVATE
        nimcp_middleware nimcp Threads::Threads check_pic subunit rt m
    )
    add_test(NAME regression_training_dispatch_test
             COMMAND regression_training_dispatch_test
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_training_dispatch_test
        PROPERTIES
            LABELS "regression;training;dispatch"
            TIMEOUT 300
            RESOURCE_LOCK "brain_heavy")
endif()

# Architecture Phase 2 Fixes Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_architecture_phase2_fixes.cpp)
    add_executable(regression_training_architecture_phase2_fixes
        test_architecture_phase2_fixes.cpp
    )
    target_include_directories(regression_training_architecture_phase2_fixes PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_architecture_phase2_fixes PRIVATE cxx_std_20)
    target_link_libraries(regression_training_architecture_phase2_fixes PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME regression_training_architecture_phase2_fixes
             COMMAND regression_training_architecture_phase2_fixes
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_training_architecture_phase2_fixes
        PROPERTIES
            LABELS "regression;training;architecture-phase2"
            TIMEOUT 60)
endif()

# Architecture Evaluation Fixes Regression Tests (Session 58-59)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_architecture_eval_fixes.cpp)
    add_executable(regression_training_architecture_eval_fixes
        test_architecture_eval_fixes.cpp
    )
    target_include_directories(regression_training_architecture_eval_fixes PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_architecture_eval_fixes PRIVATE cxx_std_20)
    target_link_libraries(regression_training_architecture_eval_fixes PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME regression_training_architecture_eval_fixes
             COMMAND regression_training_architecture_eval_fixes
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_training_architecture_eval_fixes
        PROPERTIES
            LABELS "regression;training;architecture-eval"
            TIMEOUT 60)
endif()

# Architecture Phase 3 Fixes Regression Tests (Post-eval audit)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_architecture_phase3_fixes.cpp)
    add_executable(regression_training_architecture_phase3_fixes
        test_architecture_phase3_fixes.cpp
    )
    target_include_directories(regression_training_architecture_phase3_fixes PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_architecture_phase3_fixes PRIVATE cxx_std_20)
    target_link_libraries(regression_training_architecture_phase3_fixes PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME regression_training_architecture_phase3_fixes
             COMMAND regression_training_architecture_phase3_fixes
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_training_architecture_phase3_fixes
        PROPERTIES
            LABELS "regression;training;architecture-phase3"
            TIMEOUT 60)
endif()

# Architecture Phase 4 Fixes Regression Tests (Post-eval audit #3)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_architecture_phase4_fixes.cpp)
    add_executable(regression_training_architecture_phase4_fixes
        test_architecture_phase4_fixes.cpp
    )
    target_include_directories(regression_training_architecture_phase4_fixes PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_architecture_phase4_fixes PRIVATE cxx_std_20)
    target_link_libraries(regression_training_architecture_phase4_fixes PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME regression_training_architecture_phase4_fixes
             COMMAND regression_training_architecture_phase4_fixes
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_training_architecture_phase4_fixes
        PROPERTIES
            LABELS "regression;training;architecture-phase4"
            TIMEOUT 60)
endif()

# SNN BPTT Regression Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_bptt.cpp)
    add_executable(regression_training_snn_bptt_test
        test_snn_bptt.cpp
    )
    target_include_directories(regression_training_snn_bptt_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_snn_bptt_test PRIVATE cxx_std_20)
    target_link_libraries(regression_training_snn_bptt_test PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME regression_training_snn_bptt_test
             COMMAND regression_training_snn_bptt_test
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_training_snn_bptt_test
        PROPERTIES
            LABELS "regression;training;snn;bptt"
            TIMEOUT 60)
endif()

# Architecture Restructuring TDD Tests (dual-pathway, cross-network bridges)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_architecture_restructuring.cpp)
    add_executable(regression_training_architecture_restructuring
        test_architecture_restructuring.cpp
    )
    target_include_directories(regression_training_architecture_restructuring PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_architecture_restructuring PRIVATE cxx_std_20)
    target_link_libraries(regression_training_architecture_restructuring PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME regression_training_architecture_restructuring
             COMMAND regression_training_architecture_restructuring
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_training_architecture_restructuring
        PROPERTIES
            LABELS "regression;training;architecture-restructuring"
            TIMEOUT 120)
endif()

# Architecture Audit Fixes Regression Tests (Session 57-58)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_architecture_audit_fixes.cpp)
    add_executable(regression_training_architecture_audit_fixes
        test_architecture_audit_fixes.cpp
    )
    target_include_directories(regression_training_architecture_audit_fixes PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(regression_training_architecture_audit_fixes PRIVATE cxx_std_20)
    target_link_libraries(regression_training_architecture_audit_fixes PRIVATE
        GTest::gtest GTest::gtest_main nimcp_middleware nimcp Threads::Threads m
    )
    add_test(NAME regression_training_architecture_audit_fixes
             COMMAND regression_training_architecture_audit_fixes
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(regression_training_architecture_audit_fixes
        PROPERTIES
            LABELS "regression;training;architecture-audit"
            TIMEOUT 60)
endif()

# Hamiltonian LNN + FNO unit tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_hamiltonian_fno_unit.cpp")
    add_executable(regression_hamiltonian_fno_unit_test
        test_hamiltonian_fno_unit.cpp
    )
    target_link_libraries(regression_hamiltonian_fno_unit_test
        PRIVATE
            nimcp
            nimcp_middleware
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
    )
    target_include_directories(regression_hamiltonian_fno_unit_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    set_target_properties(regression_hamiltonian_fno_unit_test PROPERTIES
        LABELS "regression;training;hamiltonian;fno;unit"
        TIMEOUT 120
    )
    gtest_discover_tests(regression_hamiltonian_fno_unit_test
        PROPERTIES LABELS "regression;training;hamiltonian;fno;unit"
        PROPERTIES TIMEOUT 120
    )
endif()
