# Financial Module End-to-End Tests (Check Framework)
# Full system scenarios, cognitive pipelines, stress tests, real-world patterns

find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK check)

if(CHECK_FOUND)
    #=========================================================================
    # Financial E2E Tests
    # Tests full trading pipeline, cognitive pipeline, stress scenarios,
    # and real-world archetype patterns (Graham, Soros, Simons, etc.)
    #=========================================================================

    add_executable(e2e_financial_cognitive
        test_financial_e2e.c
    )
    target_include_directories(e2e_financial_cognitive PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CHECK_INCLUDE_DIRS}
    )
    target_link_libraries(e2e_financial_cognitive
        nimcp
        ${CHECK_LIBRARIES}
        pthread
        m
    )
    add_test(NAME e2e_financial_cognitive
             COMMAND e2e_financial_cognitive)
    set_tests_properties(e2e_financial_cognitive PROPERTIES
        LABELS "e2e;cognitive;parietal;financial;trading;archetype;pipeline;stress"
        TIMEOUT 600
    )

    #=========================================================================
    # Aggregate Target
    #=========================================================================

    add_custom_target(e2e_financial_cognitive_all DEPENDS
        e2e_financial_cognitive
    )

else()
    message(STATUS "Check library not found - skipping financial e2e tests")
endif()
