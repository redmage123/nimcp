# Bio-Async Regression Tests
# WHAT: Regression tests for bio-async self-repair pipeline integration
# WHY:  Ensure backward compatibility and API contract stability
# HOW:  Test module IDs, message types, and API contracts remain unchanged

#=============================================================================
# Self-Repair Bio-Async Regression Tests
#=============================================================================

add_executable(regression_bio_async_self_repair
    test_self_repair_bio_async_regression.cpp
)

target_link_libraries(regression_bio_async_self_repair
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)

target_include_directories(regression_bio_async_self_repair
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME regression_bio_async_self_repair
         COMMAND regression_bio_async_self_repair)

set_tests_properties(regression_bio_async_self_repair PROPERTIES
    LABELS "regression;bio-async;self-repair"
    TIMEOUT 120
)

