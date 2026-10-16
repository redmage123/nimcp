# =============================================================================
# Unit Tests for Brain Module
# =============================================================================
# WHAT: Unit tests for core brain functionality
# WHY:  Verify correctness of brain module APIs
# HOW:  GTest-based unit tests
# =============================================================================

# KG Module Wiring Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_kg_module_wiring.cpp")
    add_executable(unit_brain_test_kg_module_wiring
        ${CMAKE_CURRENT_SOURCE_DIR}/test_kg_module_wiring.cpp
    )

    target_link_libraries(unit_brain_test_kg_module_wiring
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(unit_brain_test_kg_module_wiring
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/core/brain
    )

    target_compile_features(unit_brain_test_kg_module_wiring PRIVATE cxx_std_17)
    target_compile_definitions(unit_brain_test_kg_module_wiring PRIVATE NIMCP_TESTING NIMCP_UNIT_TESTING)

    gtest_discover_tests(unit_brain_test_kg_module_wiring
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;brain;kg_module_wiring"
            TIMEOUT 120
    )

    message(STATUS "  Added Unit test: unit_brain_test_kg_module_wiring")
endif()

# Brain State Manager Tests (Phase 8)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_brain_state_manager.cpp")
    add_executable(unit_brain_test_state_manager
        ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_state_manager.cpp
    )

    target_link_libraries(unit_brain_test_state_manager
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(unit_brain_test_state_manager
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/core/brain
    )

    target_compile_features(unit_brain_test_state_manager PRIVATE cxx_std_17)
    target_compile_definitions(unit_brain_test_state_manager PRIVATE NIMCP_TESTING NIMCP_UNIT_TESTING)

    gtest_discover_tests(unit_brain_test_state_manager
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;brain;state_manager;phase8"
            TIMEOUT 120
    )

    message(STATUS "  Added Unit test: unit_brain_test_state_manager")
endif()

# KG Wiring Modules Tests (10 brain regions)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_kg_wiring_modules.cpp")
    add_executable(unit_brain_test_kg_wiring_modules
        ${CMAKE_CURRENT_SOURCE_DIR}/test_kg_wiring_modules.cpp
    )

    target_link_libraries(unit_brain_test_kg_wiring_modules
        PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )

    target_include_directories(unit_brain_test_kg_wiring_modules
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/core/brain
    )

    target_compile_features(unit_brain_test_kg_wiring_modules PRIVATE cxx_std_17)
    target_compile_definitions(unit_brain_test_kg_wiring_modules PRIVATE NIMCP_TESTING NIMCP_UNIT_TESTING)

    gtest_discover_tests(unit_brain_test_kg_wiring_modules
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;brain;kg_wiring_modules"
            TIMEOUT 180
    )

    message(STATUS "  Added Unit test: unit_brain_test_kg_wiring_modules")
endif()

# Wave 8B-c: hypothalamus / entorhinal / cerebellum / basal_ganglia tick drivers
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tick/test_wave8bc_region_ticks.cpp")
    add_executable(unit_brain_test_wave8bc_region_ticks
        ${CMAKE_CURRENT_SOURCE_DIR}/tick/test_wave8bc_region_ticks.cpp
    )
    target_link_libraries(unit_brain_test_wave8bc_region_ticks
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
    target_include_directories(unit_brain_test_wave8bc_region_ticks
        PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    target_compile_features(unit_brain_test_wave8bc_region_ticks PRIVATE cxx_std_17)
    target_compile_definitions(unit_brain_test_wave8bc_region_ticks
        PRIVATE NIMCP_TESTING NIMCP_UNIT_TESTING)
    add_test(NAME unit_brain_test_wave8bc_region_ticks
             COMMAND unit_brain_test_wave8bc_region_ticks)
    set_tests_properties(unit_brain_test_wave8bc_region_ticks PROPERTIES
        LABELS "unit;brain;tick;wave8bc" TIMEOUT 60)
    message(STATUS "  Added Unit test: unit_brain_test_wave8bc_region_ticks (Wave 8B-c)")
endif()

# G6: Brain glial init/attach/destroy wiring (G1+G2 coverage)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_brain_glial_init.cpp")
    add_executable(unit_brain_test_glial_init
        ${CMAKE_CURRENT_SOURCE_DIR}/test_brain_glial_init.cpp
    )
    target_link_libraries(unit_brain_test_glial_init
        PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
    target_include_directories(unit_brain_test_glial_init
        PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    target_compile_features(unit_brain_test_glial_init PRIVATE cxx_std_17)
    add_test(NAME unit_brain_test_glial_init COMMAND unit_brain_test_glial_init)
    set_tests_properties(unit_brain_test_glial_init PROPERTIES
        LABELS "unit;brain;glial;wiring" TIMEOUT 60)
    message(STATUS "  Added Unit test: unit_brain_test_glial_init (G1/G2)")
endif()

# Wave 8A+ (2026-04-24): cochlea-bridge output-buffer wiring
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bridges/CMakeLists.txt")
    add_subdirectory(bridges)
endif()
