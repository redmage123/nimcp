# ============================================================================
# Mesh Brain Region Integration Unit Tests
# ============================================================================
# WHAT: CMakeLists for mesh integration tests (amygdala, basal ganglia, hippocampus, thalamus)
# WHY:  Enable testing of brain region mesh network integration
# HOW:  Build test executables linking against nimcp and mesh modules
# ============================================================================

message(STATUS "  [MESH] Building mesh brain region integration tests")

# ----------------------------------------------------------------------------
# Test: Mesh Amygdala Integration
# ----------------------------------------------------------------------------
add_executable(test_mesh_amygdala_integration
    test_mesh_amygdala_integration.c
)
target_link_libraries(test_mesh_amygdala_integration
    nimcp
    GTest::gtest_main
)
target_include_directories(test_mesh_amygdala_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_mesh_amygdala_integration COMMAND test_mesh_amygdala_integration)
set_tests_properties(test_mesh_amygdala_integration PROPERTIES
    LABELS "unit;mesh;amygdala;integration"
    TIMEOUT 60
)

# ----------------------------------------------------------------------------
# Test: Mesh Basal Ganglia Integration
# ----------------------------------------------------------------------------
add_executable(test_mesh_basal_ganglia_integration
    test_mesh_basal_ganglia_integration.c
)
target_link_libraries(test_mesh_basal_ganglia_integration
    nimcp
    GTest::gtest_main
)
target_include_directories(test_mesh_basal_ganglia_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_mesh_basal_ganglia_integration COMMAND test_mesh_basal_ganglia_integration)
set_tests_properties(test_mesh_basal_ganglia_integration PROPERTIES
    LABELS "unit;mesh;basal_ganglia;integration"
    TIMEOUT 60
)

# ----------------------------------------------------------------------------
# Test: Mesh Hippocampus Integration
# ----------------------------------------------------------------------------
add_executable(test_mesh_hippocampus_integration
    test_mesh_hippocampus_integration.c
)
target_link_libraries(test_mesh_hippocampus_integration
    nimcp
    GTest::gtest_main
)
target_include_directories(test_mesh_hippocampus_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_mesh_hippocampus_integration COMMAND test_mesh_hippocampus_integration)
set_tests_properties(test_mesh_hippocampus_integration PROPERTIES
    LABELS "unit;mesh;hippocampus;integration"
    TIMEOUT 60
)

# ----------------------------------------------------------------------------
# Test: Mesh Thalamus Integration
# ----------------------------------------------------------------------------
add_executable(test_mesh_thalamus_integration
    test_mesh_thalamus_integration.c
)
target_link_libraries(test_mesh_thalamus_integration
    nimcp
    GTest::gtest_main
)
target_include_directories(test_mesh_thalamus_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_mesh_thalamus_integration COMMAND test_mesh_thalamus_integration)
set_tests_properties(test_mesh_thalamus_integration PROPERTIES
    LABELS "unit;mesh;thalamus;integration"
    TIMEOUT 60
)
