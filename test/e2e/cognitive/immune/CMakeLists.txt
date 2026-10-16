# ============================================================================
# Brain Immune System E2E Tests
# ============================================================================
# WHAT: End-to-end tests for brain immune tick orchestrator
# WHY:  Verify complete immune workflows with production-like scenarios
# HOW:  Check framework tests with realistic workloads
# ============================================================================

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK check)

if(CHECK_FOUND)
    # Brain Immune Tick E2E Tests (basic tick orchestrator)
    add_executable(e2e_cognitive_immune_tick
        test_brain_immune_tick_e2e.c
    )
    target_include_directories(e2e_cognitive_immune_tick PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(e2e_cognitive_immune_tick
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME e2e_cognitive_immune_tick
             COMMAND e2e_cognitive_immune_tick)
    set_tests_properties(e2e_cognitive_immune_tick PROPERTIES
        LABELS "e2e;cognitive;immune;tick"
        TIMEOUT 300
    )

    # Brain Immune Tick Enhanced E2E Tests (MC, QA, pattern memory, integration)
    add_executable(e2e_cognitive_immune_tick_enhanced
        test_brain_immune_tick_enhanced_e2e.c
    )
    target_include_directories(e2e_cognitive_immune_tick_enhanced PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(e2e_cognitive_immune_tick_enhanced
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME e2e_cognitive_immune_tick_enhanced
             COMMAND e2e_cognitive_immune_tick_enhanced)
    set_tests_properties(e2e_cognitive_immune_tick_enhanced PROPERTIES
        LABELS "e2e;cognitive;immune;tick;enhanced;mc;qa;pattern;stress"
        TIMEOUT 600
    )
else()
    message(STATUS "Check library not found - skipping brain immune E2E tests")
endif()
