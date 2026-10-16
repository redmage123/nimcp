# ============================================================================
# Glial Unit Tests
# ============================================================================

# Unit test for enhanced oligodendrocytes
add_executable(unit_glial_oligodendrocytes_enhanced
    oligodendrocytes/test_oligodendrocytes_enhanced.cpp
)

target_link_libraries(unit_glial_oligodendrocytes_enhanced
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(unit_glial_oligodendrocytes_enhanced PRIVATE cxx_std_17)

target_include_directories(unit_glial_oligodendrocytes_enhanced
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME unit_glial_oligodendrocytes_enhanced
         COMMAND unit_glial_oligodendrocytes_enhanced)

set_tests_properties(unit_glial_oligodendrocytes_enhanced
    PROPERTIES
        TIMEOUT 60
        LABELS "unit;glial;oligodendrocyte"
)

# Unit test for enhanced microglia
add_executable(unit_glial_microglia_enhanced
    microglia/test_microglia_enhanced.cpp
)

target_link_libraries(unit_glial_microglia_enhanced
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(unit_glial_microglia_enhanced PRIVATE cxx_std_17)

target_include_directories(unit_glial_microglia_enhanced
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME unit_glial_microglia_enhanced
         COMMAND unit_glial_microglia_enhanced)

set_tests_properties(unit_glial_microglia_enhanced
    PROPERTIES
        TIMEOUT 60
        LABELS "unit;glial;microglia"
)

# ============================================================================
# Unit test for myelin sheath
# ============================================================================
add_executable(unit_glial_myelin_sheath
    myelin_sheath/test_myelin_sheath.cpp
)

target_link_libraries(unit_glial_myelin_sheath
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(unit_glial_myelin_sheath PRIVATE cxx_std_17)

target_include_directories(unit_glial_myelin_sheath
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME unit_glial_myelin_sheath
         COMMAND unit_glial_myelin_sheath)

set_tests_properties(unit_glial_myelin_sheath
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;glial;myelin_sheath"
)

# ============================================================================
# Unit test for myelin math (biophysics mathematical models)
# ============================================================================
add_executable(unit_glial_myelin_math
    myelin_math/test_myelin_math.cpp
)

target_link_libraries(unit_glial_myelin_math
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(unit_glial_myelin_math PRIVATE cxx_std_17)

target_include_directories(unit_glial_myelin_math
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME unit_glial_myelin_math
         COMMAND unit_glial_myelin_math)

set_tests_properties(unit_glial_myelin_math
    PROPERTIES
        TIMEOUT 180
        LABELS "unit;glial;myelin;biophysics;math"
)

# ============================================================================
# Bio-async integration tests for glial modules
# ============================================================================
add_executable(unit_glial_test_bio_async
    test_glial_bio_async.cpp
)

target_link_libraries(unit_glial_test_bio_async
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_glial_test_bio_async
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_glial_test_bio_async PRIVATE cxx_std_17)

gtest_discover_tests(unit_glial_test_bio_async
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;glial;bio-async"
        TIMEOUT 90
)

# ============================================================================
# Glial FEP Bridge Tests
# ============================================================================
# WHAT: Unit tests for Glial-FEP bidirectional integration bridges
# WHY:  Ensure glial support integrates with FEP precision and predictions
# HOW:  Test lifecycle, effects, and bio-async for each bridge type
# ============================================================================

add_executable(unit_glial_test_fep_bridges test_glial_fep_bridges.cpp)
target_link_libraries(unit_glial_test_fep_bridges
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(unit_glial_test_fep_bridges
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_glial_test_fep_bridges PRIVATE cxx_std_17)
gtest_discover_tests(unit_glial_test_fep_bridges
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;glial;fep;bridge"
        TIMEOUT 60
)

# ============================================================================
# Astrocyte State Ops Tests (Phase 8: System-Wide Health Integration)
# ============================================================================
add_executable(unit_glial_astrocyte_state_ops
    astrocytes/test_astrocyte_state_ops.cpp
)
target_link_libraries(unit_glial_astrocyte_state_ops
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_compile_features(unit_glial_astrocyte_state_ops PRIVATE cxx_std_17)
target_include_directories(unit_glial_astrocyte_state_ops
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_glial_astrocyte_state_ops
         COMMAND unit_glial_astrocyte_state_ops)
set_tests_properties(unit_glial_astrocyte_state_ops
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;glial;astrocyte;state;fault_tolerance"
)

# ============================================================================
# Glial Exception Macros Unit Tests - DISABLED (API issues)
# Tests moved to existing exception test suite

# ============================================================================
# Glial-Immune Bridge Tests
# ============================================================================
add_subdirectory(immune)

# ============================================================================
# Glial-Sleep Bridge Tests
# ============================================================================
add_subdirectory(sleep)

# Glial-Substrate Bridge Tests — bridge deleted 2026-04-22 (dead code cleanup).
# Substrate modulation is now tested via substrate_compute_effects and the
# per-network adapter tests (test/unit/core/substrate/, test/unit/snn/, etc.).
