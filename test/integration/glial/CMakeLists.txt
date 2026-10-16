# ============================================================================
# Glial Integration Tests
# ============================================================================
# WHAT: Integration tests for glial cell modules (microglia, oligodendrocytes)
# WHY:  Verify cross-module coordination and brain system integration
# HOW:  Build test executables with all required dependencies
#
# TESTS:
# 1. test_glial_integration - Full glial system coordination
# 2. test_microglia_brain_integration - Microglia-brain interactions
# 3. test_oligodendrocyte_integration - Oligodendrocyte network coordination
# ============================================================================

# ============================================================================
# Glial System Integration Test
# ============================================================================
add_executable(integration_glial_test_glial_integration
    test_glial_integration.cpp
)

target_link_libraries(integration_glial_test_glial_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(integration_glial_test_glial_integration PRIVATE cxx_std_17)

target_include_directories(integration_glial_test_glial_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_glial_test_glial_integration
         COMMAND integration_glial_test_glial_integration)

set_tests_properties(integration_glial_test_glial_integration
    PROPERTIES
        TIMEOUT 120
        LABELS "integration;glial"
)

# ============================================================================
# Microglia-Brain Integration Test
# ============================================================================
add_executable(integration_glial_test_microglia_brain
    test_microglia_brain_integration.cpp
)

target_link_libraries(integration_glial_test_microglia_brain
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(integration_glial_test_microglia_brain PRIVATE cxx_std_17)

target_include_directories(integration_glial_test_microglia_brain
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_glial_test_microglia_brain
         COMMAND integration_glial_test_microglia_brain)

set_tests_properties(integration_glial_test_microglia_brain
    PROPERTIES
        TIMEOUT 120
        LABELS "integration;glial;microglia"
)

# ============================================================================
# Oligodendrocyte Integration Test
# ============================================================================
add_executable(integration_glial_test_oligodendrocyte
    test_oligodendrocyte_integration.cpp
)

target_link_libraries(integration_glial_test_oligodendrocyte
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(integration_glial_test_oligodendrocyte PRIVATE cxx_std_17)

target_include_directories(integration_glial_test_oligodendrocyte
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_glial_test_oligodendrocyte
         COMMAND integration_glial_test_oligodendrocyte)

set_tests_properties(integration_glial_test_oligodendrocyte
    PROPERTIES
        TIMEOUT 120
        LABELS "integration;glial;oligodendrocyte"
)

# ============================================================================
# Myelin-Axon-Oligodendrocyte Integration Test
# ============================================================================
add_executable(integration_glial_test_myelin_axon_oligo
    test_myelin_axon_oligo_integration.cpp
)

target_link_libraries(integration_glial_test_myelin_axon_oligo
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(integration_glial_test_myelin_axon_oligo PRIVATE cxx_std_17)

target_include_directories(integration_glial_test_myelin_axon_oligo
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_glial_test_myelin_axon_oligo
         COMMAND integration_glial_test_myelin_axon_oligo)

set_tests_properties(integration_glial_test_myelin_axon_oligo
    PROPERTIES
        TIMEOUT 180
        LABELS "integration;glial;myelin;axon;oligodendrocyte"
)

# ============================================================================
# Myelin Biophysics Integration Test
# ============================================================================
# WHAT: Integration tests for enhanced myelin biophysics mathematical models
# WHY:  Verify cross-module coordination of biophysics calculations
# HOW:  Test G-ratio, cable theory, saltatory conduction, metabolic models
# ============================================================================
add_executable(integration_glial_test_myelin_biophysics
    test_myelin_biophysics_integration.cpp
)

target_link_libraries(integration_glial_test_myelin_biophysics
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(integration_glial_test_myelin_biophysics PRIVATE cxx_std_17)

target_include_directories(integration_glial_test_myelin_biophysics
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME integration_glial_test_myelin_biophysics
         COMMAND integration_glial_test_myelin_biophysics)

set_tests_properties(integration_glial_test_myelin_biophysics
    PROPERTIES
        TIMEOUT 300
        LABELS "integration;glial;myelin;biophysics;math"
)

# =============================================================================
# G6: Hot-path modulation integration test (G3/G4/G5)
# =============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_glial_hot_path.cpp")
    add_executable(integration_glial_test_hot_path
        test_glial_hot_path.cpp
    )
    target_link_libraries(integration_glial_test_hot_path
        PRIVATE GTest::GTest GTest::Main nimcp pthread)
    target_compile_features(integration_glial_test_hot_path PRIVATE cxx_std_17)
    target_include_directories(integration_glial_test_hot_path
        PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME integration_glial_test_hot_path
             COMMAND integration_glial_test_hot_path)
    set_tests_properties(integration_glial_test_hot_path PROPERTIES
        LABELS "integration;glial;hot_path;G3;G4;G5" TIMEOUT 120)
    message(STATUS "  Added Integration test: integration_glial_test_hot_path (G3/G4/G5)")
endif()
