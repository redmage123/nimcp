# ============================================================================
# Glial-Immune Bridge Unit Tests (Phase 1)
# ============================================================================
# WHAT: Unit tests for glial-immune bridge modules
# WHY:  Ensure glial cells correctly respond to immune/inflammation signals
# HOW:  Test astrocytes, oligodendrocytes, myelin immune bridges
# ============================================================================

# Glial Immune Bridges Unit Tests (Astrocyte + Microglia) - Original
add_executable(unit_glial_immune_bridges
    test_glial_immune_bridges.cpp
)
target_link_libraries(unit_glial_immune_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_glial_immune_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/glial
)
add_test(NAME unit_glial_immune_bridges
         COMMAND unit_glial_immune_bridges)
set_tests_properties(unit_glial_immune_bridges PROPERTIES LABELS "unit")

# ============================================================================
# Astrocytes-Immune Bridge Tests (A1/A2 Phenotypes)
# ============================================================================
add_executable(unit_glial_astrocytes_immune_bridge
    test_astrocytes_immune_bridge.cpp
)
target_link_libraries(unit_glial_astrocytes_immune_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_glial_astrocytes_immune_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_glial_astrocytes_immune_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_glial_astrocytes_immune_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;glial;immune;astrocytes"
        TIMEOUT 120
)

# ============================================================================
# Oligodendrocytes-Immune Bridge Tests (Demyelination/Remyelination)
# ============================================================================
add_executable(unit_glial_oligodendrocytes_immune_bridge
    test_oligodendrocytes_immune_bridge.cpp
)
target_link_libraries(unit_glial_oligodendrocytes_immune_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_glial_oligodendrocytes_immune_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_glial_oligodendrocytes_immune_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_glial_oligodendrocytes_immune_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;glial;immune;oligodendrocytes"
        TIMEOUT 120
)

# ============================================================================
# Myelin-Immune Bridge Tests (MS Pathophysiology Model)
# ============================================================================
add_executable(unit_glial_myelin_immune_bridge
    test_myelin_immune_bridge.cpp
)
target_link_libraries(unit_glial_myelin_immune_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_glial_myelin_immune_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_glial_myelin_immune_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_glial_myelin_immune_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;glial;immune;myelin"
        TIMEOUT 120
)
