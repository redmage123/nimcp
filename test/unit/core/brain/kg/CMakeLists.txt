# ============================================================================
# KG Persistence and Evolution Unit Tests
# ============================================================================
# WHAT: CMakeLists for KG disaster recovery, analytics, schema, and temporal tests
# WHY:  Enable testing of KG persistence, analytics, schema evolution, and temporal queries
# HOW:  Build test executables linking against nimcp core
# ============================================================================

message(STATUS "  [KG] Building KG persistence and evolution tests")

# ----------------------------------------------------------------------------
# Test: KG Disaster Recovery
# ----------------------------------------------------------------------------
add_executable(test_kg_disaster_recovery
    test_kg_disaster_recovery.c
)
target_link_libraries(test_kg_disaster_recovery
    nimcp
    GTest::gtest_main
)
target_include_directories(test_kg_disaster_recovery PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_kg_disaster_recovery COMMAND test_kg_disaster_recovery)
set_tests_properties(test_kg_disaster_recovery PROPERTIES
    LABELS "unit;kg;disaster_recovery"
    TIMEOUT 60
)

# ----------------------------------------------------------------------------
# Test: KG Analytics
# ----------------------------------------------------------------------------
add_executable(test_kg_analytics
    test_kg_analytics.c
)
target_link_libraries(test_kg_analytics
    nimcp
    GTest::gtest_main
)
target_include_directories(test_kg_analytics PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_kg_analytics COMMAND test_kg_analytics)
set_tests_properties(test_kg_analytics PROPERTIES
    LABELS "unit;kg;analytics"
    TIMEOUT 60
)

# ----------------------------------------------------------------------------
# Test: KG Schema Evolution
# ----------------------------------------------------------------------------
add_executable(test_kg_schema
    test_kg_schema.c
)
target_link_libraries(test_kg_schema
    nimcp
    GTest::gtest_main
)
target_include_directories(test_kg_schema PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_kg_schema COMMAND test_kg_schema)
set_tests_properties(test_kg_schema PROPERTIES
    LABELS "unit;kg;schema"
    TIMEOUT 60
)

# ----------------------------------------------------------------------------
# Test: KG Temporal Queries
# ----------------------------------------------------------------------------
add_executable(test_kg_temporal
    test_kg_temporal.c
)
target_link_libraries(test_kg_temporal
    nimcp
    GTest::gtest_main
)
target_include_directories(test_kg_temporal PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_kg_temporal COMMAND test_kg_temporal)
set_tests_properties(test_kg_temporal PROPERTIES
    LABELS "unit;kg;temporal"
    TIMEOUT 60
)
