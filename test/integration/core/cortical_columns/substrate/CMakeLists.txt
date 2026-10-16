#=============================================================================
# CMakeLists.txt - Cortical Substrate Bridge Integration Tests
#=============================================================================
# WHAT: Build configuration for cortical substrate bridge integration tests
# WHY:  Verify complete integration of cortical substrate bridge with neural
#       substrate, bio-async messaging, and multi-module coordination
# HOW:  GTest framework with comprehensive module linking
#
# TEST COVERAGE:
# - Brain-level integration (substrate + bridge)
# - Cross-module communication (bio-async)
# - State propagation (ATP/temperature → cortical effects)
# - Concurrent access from multiple threads
# - Recovery scenarios (ATP replenishment, thermal regulation)
# - End-to-end sensory processing with substrate modulation
#
# @author NIMCP Development Team
# @date 2025-12-19

cmake_minimum_required(VERSION 3.14)

#=============================================================================
# Test Executable: Cortical Substrate Bridge Integration
#=============================================================================

add_executable(integration_core_cortical_columns_substrate_bridge
    test_cortical_substrate_bridge_integration.cpp
)

#=============================================================================
# Include Directories
#=============================================================================

target_include_directories(integration_core_cortical_columns_substrate_bridge
    PRIVATE
        # Main include directory
        ${CMAKE_SOURCE_DIR}/include

        # Source directories for internal headers
        ${CMAKE_SOURCE_DIR}/src

        # Core modules
        ${CMAKE_SOURCE_DIR}/src/core/cortical_columns
        ${CMAKE_SOURCE_DIR}/src/core/neural_substrate
        ${CMAKE_SOURCE_DIR}/src/core/neuron
        ${CMAKE_SOURCE_DIR}/src/core/synapse

        # Async/messaging
        ${CMAKE_SOURCE_DIR}/src/async

        # Utils
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/utils/thread
        ${CMAKE_SOURCE_DIR}/src/utils/platform
        ${CMAKE_SOURCE_DIR}/src/utils/validation
)

#=============================================================================
# Link Libraries
#=============================================================================

target_link_libraries(integration_core_cortical_columns_substrate_bridge
    PRIVATE
        # GTest framework
        GTest::GTest
        GTest::Main

        # NIMCP library (contains all modules)
        nimcp

        # System libraries
        m           # Math library
        pthread     # Threading
)

#=============================================================================
# Compiler Options
#=============================================================================

target_compile_options(integration_core_cortical_columns_substrate_bridge
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# C++ standard
set_target_properties(integration_core_cortical_columns_substrate_bridge
    PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

#=============================================================================
# Test Registration
#=============================================================================

# Add test to CTest
add_test(
    NAME integration_core_cortical_columns_substrate_bridge
    COMMAND integration_core_cortical_columns_substrate_bridge
)

# Set test properties
set_tests_properties(integration_core_cortical_columns_substrate_bridge
    PROPERTIES
        LABELS "integration;cortical_columns;substrate;neural_substrate;bio_async"
        TIMEOUT 300  # 5 minutes timeout
)

#=============================================================================
# Test Categories (for selective testing)
#=============================================================================

# Individual test filters can be run with:
# ./integration_core_cortical_columns_substrate_bridge --gtest_filter="CorticalSubstrateBridgeIntegrationTest*"
# ./integration_core_cortical_columns_substrate_bridge --gtest_filter="*BrainLevel*"
# ./integration_core_cortical_columns_substrate_bridge --gtest_filter="*BioAsync*"
# ./integration_core_cortical_columns_substrate_bridge --gtest_filter="*StatePropagation*"
# ./integration_core_cortical_columns_substrate_bridge --gtest_filter="*Concurrent*"
# ./integration_core_cortical_columns_substrate_bridge --gtest_filter="*Recovery*"
# ./integration_core_cortical_columns_substrate_bridge --gtest_filter="*EndToEnd*"

#=============================================================================
# Optional: Memory Sanitizer (for debugging)
#=============================================================================

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    target_compile_options(integration_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
    )
    target_link_options(integration_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=address
    )
endif()

#=============================================================================
# Optional: Thread Sanitizer (for concurrency testing)
#=============================================================================

option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    target_compile_options(integration_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
    )
    target_link_options(integration_core_cortical_columns_substrate_bridge
        PRIVATE
            -fsanitize=thread
    )
endif()

#=============================================================================
# Installation (optional)
#=============================================================================

# Install test binary to test directory
install(
    TARGETS integration_core_cortical_columns_substrate_bridge
    RUNTIME DESTINATION test/integration/core/cortical_columns/substrate
)

#=============================================================================
# Documentation
#=============================================================================

# Add custom target for test documentation
add_custom_target(doc_cortical_substrate_integration
    COMMAND echo "Cortical Substrate Bridge Integration Tests"
    COMMAND echo "============================================="
    COMMAND echo ""
    COMMAND echo "Test Categories:"
    COMMAND echo "  1. BrainLevelIntegration - Substrate + bridge integration"
    COMMAND echo "  2. CrossModuleCommunication - Bio-async messaging"
    COMMAND echo "  3. StatePropagation - ATP/temp → cortical effects"
    COMMAND echo "  4. ConcurrentAccess - Multi-threaded safety"
    COMMAND echo "  5. RecoveryScenarios - ATP/thermal recovery"
    COMMAND echo "  6. EndToEndProcessing - Sensory processing pipelines"
    COMMAND echo ""
    COMMAND echo "Run all tests:"
    COMMAND echo "  ./integration_core_cortical_columns_substrate_bridge"
    COMMAND echo ""
    COMMAND echo "Run specific category:"
    COMMAND echo "  ./integration_core_cortical_columns_substrate_bridge --gtest_filter='*Recovery*'"
    COMMAND echo ""
    COMMAND echo "Brief output:"
    COMMAND echo "  ./integration_core_cortical_columns_substrate_bridge --gtest_brief=1"
    COMMAND echo ""
    VERBATIM
)

#=============================================================================
# Build Summary
#=============================================================================

message(STATUS "Configured cortical substrate bridge integration tests:")
message(STATUS "  - Test executable: integration_core_cortical_columns_substrate_bridge")
message(STATUS "  - Test categories: 6 (Brain, BioAsync, State, Concurrent, Recovery, EndToEnd)")
message(STATUS "  - Test count: ~20 comprehensive integration tests")
message(STATUS "  - Includes: ATP depletion, hyperthermia, recovery, concurrent access")
message(STATUS "  - Integration: Neural substrate, bio-async messaging, multi-threading")
message(STATUS "  - Performance: Long-running stability tests")
