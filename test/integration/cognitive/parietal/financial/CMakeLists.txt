# Financial Module Cognitive Integration Tests (Check Framework)
# Cross-module integration tests for FM Integration Plan

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK check)

if(CHECK_FOUND)
    #=========================================================================
    # Financial Integration Tests
    # Tests BBB/Immune pipeline, KG wiring, health agent, bio-async,
    # financial pipeline, and cognitive layer integration
    #=========================================================================

    add_executable(integration_financial_cognitive
        test_financial_integration.c
    )
    target_include_directories(integration_financial_cognitive PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(integration_financial_cognitive
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME integration_financial_cognitive
             COMMAND integration_financial_cognitive)
    set_tests_properties(integration_financial_cognitive PROPERTIES
        LABELS "integration;cognitive;parietal;financial;bbb;immune;kg;health;bio-async;pipeline"
        TIMEOUT 300
    )

    #=========================================================================
    # Aggregate Target
    #=========================================================================

    add_custom_target(integration_financial_cognitive_all DEPENDS
        integration_financial_cognitive
    )

else()
    message(STATUS "Check library not found - skipping financial integration tests")
endif()
