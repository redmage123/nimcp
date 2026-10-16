# NOTE: This file is NOT processed by the build system.
# test/CMakeLists.txt uses discover_category_tests(regression) which
# auto-discovers files but skips directories with CMakeLists.txt.
# Test targets should be defined in test/CMakeLists.txt directly.

# =============================================================================
# Brain KG Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/brain/CMakeLists.txt)
    message(STATUS "Adding Brain KG Regression tests...")
    add_subdirectory(brain)
endif()

# =============================================================================
# Exception Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt)
    message(STATUS "Adding Exception Regression tests...")
    add_subdirectory(exception)
endif()

# =============================================================================
# Bio-Async Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bio_async/CMakeLists.txt)
    message(STATUS "Adding Bio-Async Regression tests...")
    add_subdirectory(bio_async)
endif()

# =============================================================================
# Fault Tolerance Regression Tests (Phase 8)
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fault_tolerance/CMakeLists.txt)
    message(STATUS "Adding Fault Tolerance Regression tests...")
    add_subdirectory(fault_tolerance)
endif()

# =============================================================================
# Statistics Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/statistics/CMakeLists.txt)
    message(STATUS "Adding Statistics Regression tests...")
    add_subdirectory(statistics)
endif()

# =============================================================================
# GPU Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gpu/CMakeLists.txt)
    message(STATUS "Adding GPU Regression tests...")
    add_subdirectory(gpu)
endif()

# =============================================================================
# Genius Profiles Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/genius/CMakeLists.txt)
    message(STATUS "Adding Genius Profiles Regression tests...")
    add_subdirectory(genius)
endif()

# =============================================================================
# Cross-Module Walkthrough Fixes Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cross_module/CMakeLists.txt)
    message(STATUS "Adding Cross-Module Walkthrough Fixes Regression tests...")
    add_subdirectory(cross_module)
endif()

# =============================================================================
# Neuralnet Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/core/neuralnet/CMakeLists.txt)
    message(STATUS "Adding Neuralnet Regression tests...")
    add_subdirectory(core/neuralnet)
endif()

# =============================================================================
# Reasoning-Generation Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/reasoning_generation/CMakeLists.txt)
    message(STATUS "Adding Reasoning-Generation Regression tests...")
    add_subdirectory(reasoning_generation)
endif()

# =============================================================================
# Training Integration Regression Tests
# =============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/training/CMakeLists.txt)
    message(STATUS "Adding Training Integration Regression tests...")
    add_subdirectory(training)
endif()

# =============================================================================
# Backprop Kernel Backward Compatibility Regression Tests
# =============================================================================
# WHAT: Regression tests for backprop kernel API changes (max_grad_norm, out_grad_norm)
# WHY:  Athena 95% accuracy plan added gradient clipping params; verify backward compat
# HOW:  Exercise API with default/disabled/extreme clipping values, verify convergence
# =============================================================================
add_executable(regression_test_backprop_kernel_compat
    test_backprop_kernel_backward_compat.cpp
)
target_link_libraries(regression_test_backprop_kernel_compat
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(regression_test_backprop_kernel_compat PRIVATE cxx_std_17)
gtest_discover_tests(regression_test_backprop_kernel_compat
    PROPERTIES LABELS "regression;plasticity;backprop;backward_compat"
    TIMEOUT 120
)
