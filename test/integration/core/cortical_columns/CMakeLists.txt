#=============================================================================
# CMakeLists.txt - Cortical Columns Brain Integration Tests
#=============================================================================
# WHAT: Build configuration for cortical columns integration tests
# WHY:  Verify complete integration of cortical modules with brain system
# HOW:  GTest framework with comprehensive module linking
#
# @author NIMCP Development Team
# @date 2025-11-25

cmake_minimum_required(VERSION 3.14)

#=============================================================================
# Test Executable: Cortical Columns Brain Integration
#=============================================================================

add_executable(integration_cortical_columns_brain
    test_cortical_columns_brain_integration.cpp
)

#=============================================================================
# Include Directories
#=============================================================================

target_include_directories(integration_cortical_columns_brain
    PRIVATE
        # Main include directory
        ${CMAKE_SOURCE_DIR}/include

        # Source directories for internal headers
        ${CMAKE_SOURCE_DIR}/src

        # Core modules
        ${CMAKE_SOURCE_DIR}/src/core/brain
        ${CMAKE_SOURCE_DIR}/src/core/neuron
        ${CMAKE_SOURCE_DIR}/src/core/synapse
        ${CMAKE_SOURCE_DIR}/src/core/axon

        # Cortical columns modules
        ${CMAKE_SOURCE_DIR}/src/core/cortical_columns

        # Plasticity modules
        ${CMAKE_SOURCE_DIR}/src/plasticity/stdp
        ${CMAKE_SOURCE_DIR}/src/plasticity/hebbian

        # Glial modules
        ${CMAKE_SOURCE_DIR}/src/glial/astrocytes
        ${CMAKE_SOURCE_DIR}/src/glial/oligodendrocytes
        ${CMAKE_SOURCE_DIR}/src/glial/microglia
        ${CMAKE_SOURCE_DIR}/src/glial/integration

        # Utils
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/utils/platform
        ${CMAKE_SOURCE_DIR}/src/utils/validation
)

#=============================================================================
# Link Libraries
#=============================================================================

target_link_libraries(integration_cortical_columns_brain
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

target_compile_options(integration_cortical_columns_brain
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# C++ standard
set_target_properties(integration_cortical_columns_brain
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
    NAME integration_cortical_columns_brain
    COMMAND integration_cortical_columns_brain
)

# Set test properties
set_tests_properties(integration_cortical_columns_brain
    PROPERTIES
        LABELS "integration;cortical_columns;brain;visual;audio"
        TIMEOUT 300  # 5 minutes timeout
)

#=============================================================================
# Test Categories (for selective testing)
#=============================================================================

# Individual test filters can be run with:
# ./integration_cortical_columns_brain --gtest_filter="BrainIntegration*"
# ./integration_cortical_columns_brain --gtest_filter="VisualPipeline*"
# ./integration_cortical_columns_brain --gtest_filter="AudioPipeline*"
# ./integration_cortical_columns_brain --gtest_filter="CrossModule*"
# ./integration_cortical_columns_brain --gtest_filter="Learning*"
# ./integration_cortical_columns_brain --gtest_filter="EndToEnd*"

#=============================================================================
# Optional: Memory Sanitizer (for debugging)
#=============================================================================

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    target_compile_options(integration_cortical_columns_brain
        PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
    )
    target_link_options(integration_cortical_columns_brain
        PRIVATE
            -fsanitize=address
    )
endif()

#=============================================================================
# Optional: Thread Sanitizer (for concurrency testing)
#=============================================================================

option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    target_compile_options(integration_cortical_columns_brain
        PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
    )
    target_link_options(integration_cortical_columns_brain
        PRIVATE
            -fsanitize=thread
    )
endif()

#=============================================================================
# Installation (optional)
#=============================================================================

# Install test binary to test directory
install(
    TARGETS integration_cortical_columns_brain
    RUNTIME DESTINATION test/integration/cortical_columns
)

#=============================================================================
# Documentation
#=============================================================================

# Add custom target for test documentation
add_custom_target(doc_cortical_columns_integration
    COMMAND echo "Cortical Columns Brain Integration Tests"
    COMMAND echo "=========================================="
    COMMAND echo ""
    COMMAND echo "Test Categories:"
    COMMAND echo "  1. BrainIntegration - Brain creation with cortical columns"
    COMMAND echo "  2. VisualPipeline - Retinotopic, orientation, feature processing"
    COMMAND echo "  3. AudioPipeline - Tonotopic frequency mapping"
    COMMAND echo "  4. CrossModuleConnectivity - Inter-module connections"
    COMMAND echo "  5. LearningIntegration - STDP and Hebbian learning"
    COMMAND echo "  6. EndToEndProcessing - Complete sensory-to-decision pipelines"
    COMMAND echo ""
    COMMAND echo "Run all tests:"
    COMMAND echo "  ./integration_cortical_columns_brain"
    COMMAND echo ""
    COMMAND echo "Run specific category:"
    COMMAND echo "  ./integration_cortical_columns_brain --gtest_filter='VisualPipeline*'"
    COMMAND echo ""
    VERBATIM
)

#=============================================================================
# Subdirectories
#=============================================================================

# Substrate bridge integration tests
add_subdirectory(substrate)

#=============================================================================
# Additional Cortical Column Integration Tests
#=============================================================================

# Combined test executable for new cortical column tests
add_executable(integration_cortical_columns_core
    test_cortical_column_integration.cpp
    test_cortical_hierarchy_integration.cpp
    test_columnar_connectivity_integration.cpp
    test_cortical_features_integration.cpp
)

target_include_directories(integration_cortical_columns_core
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(integration_cortical_columns_core
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        Threads::Threads
        m
)

target_compile_features(integration_cortical_columns_core PRIVATE cxx_std_17)

target_compile_options(integration_cortical_columns_core
    PRIVATE
        -Wall
        -Wextra
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

add_test(
    NAME integration_cortical_columns_core
    COMMAND integration_cortical_columns_core
)

set_tests_properties(integration_cortical_columns_core
    PROPERTIES
        LABELS "integration;core;cortical_columns"
        TIMEOUT 180
)

#=============================================================================
# Individual Test Executables (for focused testing)
#=============================================================================

# Cortical column basic tests
add_executable(integration_cortical_column_basic
    test_cortical_column_integration.cpp
)

target_include_directories(integration_cortical_column_basic PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(integration_cortical_column_basic PRIVATE
    nimcp
    GTest::GTest
    GTest::Main
    Threads::Threads
    m
)

target_compile_features(integration_cortical_column_basic PRIVATE cxx_std_17)

add_test(
    NAME integration_cortical_column_basic
    COMMAND integration_cortical_column_basic
)

set_tests_properties(integration_cortical_column_basic PROPERTIES
    LABELS "integration;core;cortical_columns;basic"
    TIMEOUT 120
)

# Cortical hierarchy tests
add_executable(integration_cortical_hierarchy
    test_cortical_hierarchy_integration.cpp
)

target_include_directories(integration_cortical_hierarchy PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(integration_cortical_hierarchy PRIVATE
    nimcp
    GTest::GTest
    GTest::Main
    Threads::Threads
    m
)

target_compile_features(integration_cortical_hierarchy PRIVATE cxx_std_17)

add_test(
    NAME integration_cortical_hierarchy
    COMMAND integration_cortical_hierarchy
)

set_tests_properties(integration_cortical_hierarchy PROPERTIES
    LABELS "integration;core;cortical_columns;hierarchy"
    TIMEOUT 120
)

# Columnar connectivity tests
add_executable(integration_columnar_connectivity
    test_columnar_connectivity_integration.cpp
)

target_include_directories(integration_columnar_connectivity PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(integration_columnar_connectivity PRIVATE
    nimcp
    GTest::GTest
    GTest::Main
    Threads::Threads
    m
)

target_compile_features(integration_columnar_connectivity PRIVATE cxx_std_17)

add_test(
    NAME integration_columnar_connectivity
    COMMAND integration_columnar_connectivity
)

set_tests_properties(integration_columnar_connectivity PROPERTIES
    LABELS "integration;core;cortical_columns;connectivity"
    TIMEOUT 120
)

# Cortical features (orientation columns) tests
add_executable(integration_cortical_features
    test_cortical_features_integration.cpp
)

target_include_directories(integration_cortical_features PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(integration_cortical_features PRIVATE
    nimcp
    GTest::GTest
    GTest::Main
    Threads::Threads
    m
)

target_compile_features(integration_cortical_features PRIVATE cxx_std_17)

add_test(
    NAME integration_cortical_features
    COMMAND integration_cortical_features
)

set_tests_properties(integration_cortical_features PROPERTIES
    LABELS "integration;core;cortical_columns;features"
    TIMEOUT 120
)

#=============================================================================
# Custom Target for Running All Cortical Column Tests
#=============================================================================

add_custom_target(run_cortical_columns_integration_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L "cortical_columns" --output-on-failure
    DEPENDS integration_cortical_columns_brain
            integration_cortical_columns_core
            integration_cortical_column_basic
            integration_cortical_hierarchy
            integration_columnar_connectivity
            integration_cortical_features
    COMMENT "Running all cortical columns integration tests"
)

#=============================================================================
# Build Summary
#=============================================================================

message(STATUS "Configured cortical columns integration tests:")
message(STATUS "  - Test executable: integration_cortical_columns_brain")
message(STATUS "  - Test executable: integration_cortical_columns_core (combined)")
message(STATUS "  - Test executable: integration_cortical_column_basic (~40 tests)")
message(STATUS "  - Test executable: integration_cortical_hierarchy (~30 tests)")
message(STATUS "  - Test executable: integration_columnar_connectivity (~25 tests)")
message(STATUS "  - Test executable: integration_cortical_features (~30 tests)")
message(STATUS "  - Test categories: 6 (BrainIntegration, Visual, Audio, CrossModule, Learning, EndToEnd)")
message(STATUS "  - Test count: ~145+ comprehensive integration tests")
message(STATUS "  - Includes: Topographic maps, Orientation columns, Feature hypercolumns")
message(STATUS "  - Plasticity: STDP, Hebbian learning across columns")
message(STATUS "  - Performance: Large-scale processing benchmarks")
message(STATUS "  - Substrate: Cortical substrate bridge integration tests")
