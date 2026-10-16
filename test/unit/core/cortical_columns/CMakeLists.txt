# CMakeLists.txt for cortical column unit tests

# Test executable for cortical column module
add_executable(test_cortical_column
    test_cortical_column.cpp
)

# Link libraries
target_link_libraries(test_cortical_column
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

# C++17 standard
target_compile_features(test_cortical_column PRIVATE cxx_std_17)

# Include directories
target_include_directories(test_cortical_column
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core/cortical_columns
)

# Add to test suite
add_test(
    NAME test_cortical_column
    COMMAND test_cortical_column
)

# Set test properties
set_tests_properties(test_cortical_column PROPERTIES
    TIMEOUT 60
    LABELS "unit;core;cortical_columns"
)

# Test executable for columnar connectivity module - DISABLED: Uses incomplete laminar_structure_t type
#add_executable(test_columnar_connectivity
#    test_columnar_connectivity.cpp
#)
#target_link_libraries(test_columnar_connectivity
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp
#        pthread
#)
#target_compile_features(test_columnar_connectivity PRIVATE cxx_std_17)
#target_include_directories(test_columnar_connectivity
#    PRIVATE
#        ${CMAKE_SOURCE_DIR}/include
#        ${CMAKE_SOURCE_DIR}/src
#)
#add_test(NAME test_columnar_connectivity
#         COMMAND test_columnar_connectivity)
#set_tests_properties(test_columnar_connectivity
#    PROPERTIES
#        TIMEOUT 120
#        LABELS "unit;core;cortical_columns;connectivity"
#)

# Test executable for feature hypercolumns module - DISABLED: Uses incomplete feature_hypercolumn_t type
#add_executable(unit_core_cortical_columns_test_feature_hypercolumns
#    test_feature_hypercolumns.cpp
#    ${CMAKE_SOURCE_DIR}/src/core/cortical_columns/nimcp_feature_hypercolumns.c
#)
#target_link_libraries(unit_core_cortical_columns_test_feature_hypercolumns
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp
#        pthread
#        m
#)
#target_compile_features(unit_core_cortical_columns_test_feature_hypercolumns PRIVATE cxx_std_17)
#target_include_directories(unit_core_cortical_columns_test_feature_hypercolumns
#    PRIVATE
#        ${CMAKE_SOURCE_DIR}/include
#        ${CMAKE_SOURCE_DIR}/src
#)
#add_test(NAME unit_core_cortical_columns_test_feature_hypercolumns
#         COMMAND unit_core_cortical_columns_test_feature_hypercolumns)
#set_tests_properties(unit_core_cortical_columns_test_feature_hypercolumns
#    PROPERTIES
#        TIMEOUT 120
#        LABELS "unit;core;cortical_columns;feature_hypercolumns"
#)

# Test executable for cortical layers module
add_executable(test_cortical_layers
    test_cortical_layers.cpp
    ${CMAKE_SOURCE_DIR}/src/core/cortical_columns/nimcp_cortical_layers.c
)

# Link libraries
target_link_libraries(test_cortical_layers
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

# C++20 for designated initializers
target_compile_features(test_cortical_layers PRIVATE cxx_std_20)

# Include directories
target_include_directories(test_cortical_layers
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Add to test suite
add_test(NAME test_cortical_layers
         COMMAND test_cortical_layers)

# Set test properties
set_tests_properties(test_cortical_layers
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;layers"
)

# Test executable for cortical immune integration
add_executable(test_cortical_immune
    test_cortical_immune.cpp
    ${CMAKE_SOURCE_DIR}/src/core/cortical_columns/nimcp_cortical_immune.c
)

# Link libraries
target_link_libraries(test_cortical_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

# C++17 standard
target_compile_features(test_cortical_immune PRIVATE cxx_std_17)

# Include directories
target_include_directories(test_cortical_immune
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Add to test suite
add_test(NAME test_cortical_immune
         COMMAND test_cortical_immune)

# Set test properties
set_tests_properties(test_cortical_immune
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;immune"
)

# ============================================================================
# Cortical Column Enhancement Tests
# ============================================================================

# Test executable for cortical attention gain
add_executable(test_cortical_attention_gain
    test_cortical_attention_gain.cpp
)

target_link_libraries(test_cortical_attention_gain
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(test_cortical_attention_gain PRIVATE cxx_std_17)

target_include_directories(test_cortical_attention_gain
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_cortical_attention_gain
         COMMAND test_cortical_attention_gain)

set_tests_properties(test_cortical_attention_gain
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;attention_gain"
)

# Test executable for cortical predictive coding
add_executable(test_cortical_predictive_coding
    test_cortical_predictive_coding.cpp
)

target_link_libraries(test_cortical_predictive_coding
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(test_cortical_predictive_coding PRIVATE cxx_std_17)

target_include_directories(test_cortical_predictive_coding
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_cortical_predictive_coding
         COMMAND test_cortical_predictive_coding)

set_tests_properties(test_cortical_predictive_coding
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;predictive_coding"
)

# Test executable for cortical sparse coding
add_executable(test_cortical_sparse_coding
    test_cortical_sparse_coding.cpp
)

target_link_libraries(test_cortical_sparse_coding
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(test_cortical_sparse_coding PRIVATE cxx_std_17)

target_include_directories(test_cortical_sparse_coding
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_cortical_sparse_coding
         COMMAND test_cortical_sparse_coding)

set_tests_properties(test_cortical_sparse_coding
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;sparse_coding"
)

# Test executable for cortical temporal
add_executable(test_cortical_temporal
    test_cortical_temporal.cpp
)

target_link_libraries(test_cortical_temporal
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(test_cortical_temporal PRIVATE cxx_std_17)

target_include_directories(test_cortical_temporal
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_cortical_temporal
         COMMAND test_cortical_temporal)

set_tests_properties(test_cortical_temporal
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;temporal"
)

# Test executable for cortical hierarchy
add_executable(test_cortical_hierarchy
    test_cortical_hierarchy.cpp
)

target_link_libraries(test_cortical_hierarchy
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(test_cortical_hierarchy PRIVATE cxx_std_17)

target_include_directories(test_cortical_hierarchy
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_cortical_hierarchy
         COMMAND test_cortical_hierarchy)

set_tests_properties(test_cortical_hierarchy
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;hierarchy"
)

# Test executable for cortical neuromodulation
add_executable(test_cortical_neuromodulation
    test_cortical_neuromodulation.cpp
)

target_link_libraries(test_cortical_neuromodulation
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(test_cortical_neuromodulation PRIVATE cxx_std_17)

target_include_directories(test_cortical_neuromodulation
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_cortical_neuromodulation
         COMMAND test_cortical_neuromodulation)

set_tests_properties(test_cortical_neuromodulation
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;neuromodulation"
)

# Test executable for cortical oscillations integration
# DISABLED: Test uses wrong API types and non-existent functions
#           (cortical_oscillation_system_t vs cortical_oscillation_integration_t,
#            cortical_oscillation_update vs cortical_oscillation_integrate_step)
# TODO: Rewrite test to match actual API in nimcp_cortical_oscillations_integration.h
# add_executable(test_cortical_oscillations_integration
#     test_cortical_oscillations_integration.cpp
# )
# target_link_libraries(test_cortical_oscillations_integration
#     PRIVATE GTest::GTest GTest::Main nimcp pthread m)
# target_compile_features(test_cortical_oscillations_integration PRIVATE cxx_std_17)
# target_include_directories(test_cortical_oscillations_integration
#     PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# add_test(NAME test_cortical_oscillations_integration COMMAND test_cortical_oscillations_integration)
# set_tests_properties(test_cortical_oscillations_integration PROPERTIES TIMEOUT 120 LABELS "unit;core;cortical_columns;oscillations_integration")

# Test executable for cortical plasticity bridge
# DISABLED: Test uses wrong API types and non-existent functions
# TODO: Rewrite test to match actual API
# add_executable(test_cortical_plasticity_bridge
#     test_cortical_plasticity_bridge.cpp
# )
# target_link_libraries(test_cortical_plasticity_bridge
#     PRIVATE GTest::GTest GTest::Main nimcp pthread m)
# target_compile_features(test_cortical_plasticity_bridge PRIVATE cxx_std_17)
# target_include_directories(test_cortical_plasticity_bridge
#     PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# add_test(NAME test_cortical_plasticity_bridge COMMAND test_cortical_plasticity_bridge)
# set_tests_properties(test_cortical_plasticity_bridge PROPERTIES TIMEOUT 120 LABELS "unit;core;cortical_columns;plasticity_bridge")

# Test executable for cortical dendritic
add_executable(test_cortical_dendritic
    test_cortical_dendritic.cpp
)

target_link_libraries(test_cortical_dendritic
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(test_cortical_dendritic PRIVATE cxx_std_17)

target_include_directories(test_cortical_dendritic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_cortical_dendritic
         COMMAND test_cortical_dendritic)

set_tests_properties(test_cortical_dendritic
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;dendritic"
)

# Test executable for cortical surround
# DISABLED: Test uses wrong API types (cortical_surround_system_t vs cortical_surround_t,
#           cortical_surround_config_t vs surround_config_t) and non-existent functions
#           (cortical_surround_compute_suppression vs cortical_surround_compute_iso_suppression)
# TODO: Rewrite test to match actual API in nimcp_cortical_surround.h
# add_executable(test_cortical_surround
#     test_cortical_surround.cpp
# )
#
# target_link_libraries(test_cortical_surround
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
#         pthread
#         m
# )
#
# target_compile_features(test_cortical_surround PRIVATE cxx_std_17)
#
# target_include_directories(test_cortical_surround
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/src
# )
#
# add_test(NAME test_cortical_surround
#          COMMAND test_cortical_surround)
#
# set_tests_properties(test_cortical_surround
#     PROPERTIES
#         TIMEOUT 120
#         LABELS "unit;core;cortical_columns;surround"
# )

# Test executable for sparse coding integration (K-WTA in brain_decide)
add_executable(test_sparse_coding_integration
    test_sparse_coding_integration.cpp
)

target_link_libraries(test_sparse_coding_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(test_sparse_coding_integration PRIVATE cxx_std_17)

target_include_directories(test_sparse_coding_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_sparse_coding_integration
         COMMAND test_sparse_coding_integration)

set_tests_properties(test_sparse_coding_integration
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;sparse_coding;integration"
)

# ============================================================================
# Thousand Brains Tests (Hawkins: ref frames, voting, sequences, integration)
# ============================================================================
add_executable(test_thousand_brains
    test_thousand_brains.cpp
)

target_link_libraries(test_thousand_brains
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(test_thousand_brains PRIVATE cxx_std_17)

target_include_directories(test_thousand_brains
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core/cortical_columns
)

add_test(NAME test_thousand_brains
         COMMAND test_thousand_brains)

set_tests_properties(test_thousand_brains
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;cortical_columns;thousand_brains"
)

# ============================================================================
# Cortical Substrate Bridge Tests
# ============================================================================
add_subdirectory(substrate)
