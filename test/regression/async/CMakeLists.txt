# Regression tests for async module (bio-async and traditional futures)

# =============================================================================
# Traditional Future Regression Tests
# =============================================================================
add_executable(regression_async_test_future_regression
    test_future_regression.cpp
)

target_link_libraries(regression_async_test_future_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_async_test_future_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_future_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_future_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async"
        TIMEOUT 120
)

# =============================================================================
# Bio-Async Regression Tests
# =============================================================================
add_executable(regression_async_test_bio_async_regression
    test_bio_async_regression.cpp
)

target_link_libraries(regression_async_test_bio_async_regression
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_async_test_bio_async_regression
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_bio_async_regression PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_bio_async_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async"
        TIMEOUT 120
)

# =============================================================================
# Wiring Diagram Regression Tests
# =============================================================================
add_executable(regression_async_test_wiring_diagram
    test_wiring_diagram_regression.cpp
)

target_link_libraries(regression_async_test_wiring_diagram
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_async_test_wiring_diagram
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_wiring_diagram PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_wiring_diagram
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async;wiring"
        TIMEOUT 120
)

# =============================================================================
# KG Dispatch Regression Tests (Phase 7: Runtime Message Orchestration)
# =============================================================================
add_executable(regression_async_test_bio_router_kg_dispatch
    test_bio_router_kg_dispatch_regression.cpp
)

target_link_libraries(regression_async_test_bio_router_kg_dispatch
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_async_test_bio_router_kg_dispatch
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_bio_router_kg_dispatch PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_bio_router_kg_dispatch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async;router;kg;dispatch"
        TIMEOUT 120
)

# =============================================================================
# Wiring Diagram KG Sync Regression Tests (Phase 8: Wiring-KG Bidirectional Sync)
# =============================================================================
add_executable(regression_async_test_wiring_diagram_kg_sync
    test_wiring_diagram_kg_sync_regression.cpp
)

target_link_libraries(regression_async_test_wiring_diagram_kg_sync
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs
)

target_include_directories(regression_async_test_wiring_diagram_kg_sync
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_wiring_diagram_kg_sync PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_wiring_diagram_kg_sync
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async;wiring;kg;sync"
        TIMEOUT 120
)

# =============================================================================
# Wiring Diagram Validation Regression Tests (Phase 9: Validation & Dependency Resolution)
# =============================================================================
add_executable(regression_async_test_wiring_diagram_validation
    test_wiring_diagram_validation_regression.cpp
)

target_link_libraries(regression_async_test_wiring_diagram_validation
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs
)

target_include_directories(regression_async_test_wiring_diagram_validation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_wiring_diagram_validation PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_wiring_diagram_validation
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async;wiring;validation;dependency"
        TIMEOUT 120
)

# =============================================================================
# Self-Assembly Regression Tests (Phase 10: Automatic Self-Assembly)
# =============================================================================
add_executable(regression_async_test_self_assembly
    test_self_assembly_regression.cpp
)

target_link_libraries(regression_async_test_self_assembly
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
        stdc++fs
)

target_include_directories(regression_async_test_self_assembly
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_self_assembly PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_self_assembly
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async;orchestrator;self-assembly"
        TIMEOUT 120
)

# =============================================================================
# Async & Events Walkthrough Fix Regression Tests (P1/P2/P3 fixes 2026-02-08)
# =============================================================================
add_executable(regression_async_test_walkthrough_fixes
    test_async_events_walkthrough_fixes.cpp
)

target_link_libraries(regression_async_test_walkthrough_fixes
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_async_test_walkthrough_fixes
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_walkthrough_fixes PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_walkthrough_fixes
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async;events;walkthrough"
        TIMEOUT 120
)

# =============================================================================
# Async & Middleware Pass3 Fix Regression Tests (P1/P2/P3 fixes 2026-02-08)
# =============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_async_middleware_pass3_fixes.cpp")
add_executable(regression_async_test_middleware_pass3_fixes
    test_async_middleware_pass3_fixes.cpp
)

target_link_libraries(regression_async_test_middleware_pass3_fixes
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_async_test_middleware_pass3_fixes
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_middleware_pass3_fixes PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_middleware_pass3_fixes
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async;middleware;pass3"
        TIMEOUT 120
)
endif()

# =============================================================================
# Bio-Router TOCTOU Race Condition Regression Tests
# =============================================================================
add_executable(regression_async_test_bio_router_toctou
    test_bio_router_toctou_regression.cpp
)

target_link_libraries(regression_async_test_bio_router_toctou
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(regression_async_test_bio_router_toctou
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(regression_async_test_bio_router_toctou PRIVATE cxx_std_17)

gtest_discover_tests(regression_async_test_bio_router_toctou
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;async;router;toctou;thread-safety"
        TIMEOUT 120
)
