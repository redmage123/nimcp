# ============================================================================
# Brain E2E Tests
# ============================================================================
# WHAT: End-to-end tests for brain knowledge graph module wiring
# WHY:  Verify complete module wiring workflows with KG integration
# HOW:  Test full lifecycle, multi-module subsystems, and dynamic reconfiguration
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping Brain E2E tests")
    return()
endif()

# ============================================================================
# KG Module Wiring E2E Test
# ============================================================================
# WHAT: End-to-end tests for KG module wiring with brain KG integration
# WHY:  Verify complete module wiring workflows
# HOW:  Test full lifecycle, multi-module subsystems, and dynamic reconfiguration

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_kg_module_wiring_e2e.cpp")
    message(STATUS "Adding KG module wiring E2E tests...")
    add_executable(e2e_test_kg_module_wiring
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_kg_module_wiring_e2e.cpp
    )

    target_link_libraries(e2e_test_kg_module_wiring
        PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            Threads::Threads
            m
    )

    target_include_directories(e2e_test_kg_module_wiring
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/test/e2e
    )

    target_compile_features(e2e_test_kg_module_wiring PRIVATE cxx_std_17)

    target_compile_definitions(e2e_test_kg_module_wiring PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    add_test(NAME e2e_test_kg_module_wiring COMMAND e2e_test_kg_module_wiring)

    set_tests_properties(e2e_test_kg_module_wiring PROPERTIES
        LABELS "e2e;brain;kg;wiring"
        TIMEOUT 120
        RUN_SERIAL TRUE
    )

    message(STATUS "  Added E2E test: e2e_test_kg_module_wiring")
endif()

# ============================================================================
# Hemispheric Brain E2E Tests
# ============================================================================
# WHAT: End-to-end tests for hemispheric brain layer
# WHY:  Verify bilateral brain architecture, lateralization, and split-brain
# HOW:  Test language/spatial processing, split-brain experiments, injury/recovery

set(HEMISPHERIC_E2E_DIR ${CMAKE_SOURCE_DIR}/test/e2e/core/brain/hemispheric)

if(EXISTS "${HEMISPHERIC_E2E_DIR}/test_language_lateralization_e2e.cpp")
    message(STATUS "Adding Hemispheric Brain E2E tests...")

    # Language Lateralization E2E Test
    add_executable(e2e_test_language_lateralization
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
        ${HEMISPHERIC_E2E_DIR}/test_language_lateralization_e2e.cpp
    )

    target_link_libraries(e2e_test_language_lateralization
        PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            Threads::Threads
            m
    )

    target_include_directories(e2e_test_language_lateralization
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/test
            ${CMAKE_SOURCE_DIR}/test/e2e
            ${CMAKE_SOURCE_DIR}/test/utils
    )

    target_compile_features(e2e_test_language_lateralization PRIVATE cxx_std_17)

    target_compile_definitions(e2e_test_language_lateralization PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    add_test(NAME e2e_test_language_lateralization COMMAND e2e_test_language_lateralization)

    set_tests_properties(e2e_test_language_lateralization PROPERTIES
        LABELS "e2e;hemispheric;language;lateralization"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )

    message(STATUS "  Added E2E test: e2e_test_language_lateralization")
endif()

if(EXISTS "${HEMISPHERIC_E2E_DIR}/test_spatial_processing_e2e.cpp")
    # Spatial Processing E2E Test
    add_executable(e2e_test_spatial_processing
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
        ${HEMISPHERIC_E2E_DIR}/test_spatial_processing_e2e.cpp
    )

    target_link_libraries(e2e_test_spatial_processing
        PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            Threads::Threads
            m
    )

    target_include_directories(e2e_test_spatial_processing
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/test
            ${CMAKE_SOURCE_DIR}/test/e2e
            ${CMAKE_SOURCE_DIR}/test/utils
    )

    target_compile_features(e2e_test_spatial_processing PRIVATE cxx_std_17)

    target_compile_definitions(e2e_test_spatial_processing PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    add_test(NAME e2e_test_spatial_processing COMMAND e2e_test_spatial_processing)

    set_tests_properties(e2e_test_spatial_processing PROPERTIES
        LABELS "e2e;hemispheric;spatial;processing"
        TIMEOUT 300
        RUN_SERIAL TRUE
    )

    message(STATUS "  Added E2E test: e2e_test_spatial_processing")
endif()

if(EXISTS "${HEMISPHERIC_E2E_DIR}/test_split_brain_e2e.cpp")
    # Split Brain E2E Test
    add_executable(e2e_test_split_brain
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
        ${HEMISPHERIC_E2E_DIR}/test_split_brain_e2e.cpp
    )

    target_link_libraries(e2e_test_split_brain
        PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            Threads::Threads
            m
    )

    target_include_directories(e2e_test_split_brain
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/test
            ${CMAKE_SOURCE_DIR}/test/e2e
            ${CMAKE_SOURCE_DIR}/test/utils
    )

    target_compile_features(e2e_test_split_brain PRIVATE cxx_std_17)

    target_compile_definitions(e2e_test_split_brain PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    add_test(NAME e2e_test_split_brain COMMAND e2e_test_split_brain)

    set_tests_properties(e2e_test_split_brain PROPERTIES
        LABELS "e2e;hemispheric;split_brain;callosotomy"
        TIMEOUT 600
        RUN_SERIAL TRUE
    )

    message(STATUS "  Added E2E test: e2e_test_split_brain")
endif()

if(EXISTS "${HEMISPHERIC_E2E_DIR}/test_injury_recovery_e2e.cpp")
    # Injury Recovery E2E Test
    add_executable(e2e_test_injury_recovery
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
        ${HEMISPHERIC_E2E_DIR}/test_injury_recovery_e2e.cpp
    )

    target_link_libraries(e2e_test_injury_recovery
        PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            Threads::Threads
            m
    )

    target_include_directories(e2e_test_injury_recovery
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/test
            ${CMAKE_SOURCE_DIR}/test/e2e
            ${CMAKE_SOURCE_DIR}/test/utils
    )

    target_compile_features(e2e_test_injury_recovery PRIVATE cxx_std_17)

    target_compile_definitions(e2e_test_injury_recovery PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    add_test(NAME e2e_test_injury_recovery COMMAND e2e_test_injury_recovery)

    set_tests_properties(e2e_test_injury_recovery PROPERTIES
        LABELS "e2e;hemispheric;injury;recovery;rehabilitation"
        TIMEOUT 600
        RUN_SERIAL TRUE
    )

    message(STATUS "  Added E2E test: e2e_test_injury_recovery")
endif()

# Combined hemispheric tests target
if(EXISTS "${HEMISPHERIC_E2E_DIR}/test_language_lateralization_e2e.cpp" AND
   EXISTS "${HEMISPHERIC_E2E_DIR}/test_spatial_processing_e2e.cpp" AND
   EXISTS "${HEMISPHERIC_E2E_DIR}/test_split_brain_e2e.cpp" AND
   EXISTS "${HEMISPHERIC_E2E_DIR}/test_injury_recovery_e2e.cpp")

    add_executable(e2e_test_hemispheric_all
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
        ${HEMISPHERIC_E2E_DIR}/test_language_lateralization_e2e.cpp
        ${HEMISPHERIC_E2E_DIR}/test_spatial_processing_e2e.cpp
        ${HEMISPHERIC_E2E_DIR}/test_split_brain_e2e.cpp
        ${HEMISPHERIC_E2E_DIR}/test_injury_recovery_e2e.cpp
    )

    target_link_libraries(e2e_test_hemispheric_all
        PRIVATE
            nimcp
            GTest::GTest
            GTest::Main
            Threads::Threads
            m
    )

    target_include_directories(e2e_test_hemispheric_all
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/test
            ${CMAKE_SOURCE_DIR}/test/e2e
            ${CMAKE_SOURCE_DIR}/test/utils
    )

    target_compile_features(e2e_test_hemispheric_all PRIVATE cxx_std_17)

    target_compile_definitions(e2e_test_hemispheric_all PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    add_test(NAME e2e_test_hemispheric_all COMMAND e2e_test_hemispheric_all)

    set_tests_properties(e2e_test_hemispheric_all PROPERTIES
        LABELS "e2e;hemispheric;all"
        TIMEOUT 1200
        RUN_SERIAL TRUE
    )

    message(STATUS "  Added E2E test: e2e_test_hemispheric_all (combined)")
endif()
