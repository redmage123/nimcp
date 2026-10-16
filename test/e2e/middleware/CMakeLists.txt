# ============================================================================
# Middleware E2E Tests
# ============================================================================
# WHAT: End-to-end tests for middleware modules (circular buffer, etc.)
# WHY:  Verify complete workflows for buffering, routing, and data flow
# HOW:  GTest-based tests with realistic data patterns
# ============================================================================

message(STATUS "  Adding Middleware E2E tests...")

# ============================================================================
# Circular Buffer E2E Tests (P0-P2 Remediation)
# ============================================================================
# WHAT: End-to-end tests for circular buffer batch operations and SPSC
# WHY:  Verify batch push/pop consistency, overflow accuracy, and lock-free SPSC

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_circular_buffer_e2e.cpp)
    add_executable(e2e_middleware_test_circular_buffer_e2e
        ${CMAKE_CURRENT_SOURCE_DIR}/test_circular_buffer_e2e.cpp
    )
    target_include_directories(e2e_middleware_test_circular_buffer_e2e PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/middleware
        ${CMAKE_SOURCE_DIR}/include/middleware/buffering
    )
    target_link_libraries(e2e_middleware_test_circular_buffer_e2e PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
    )
    target_compile_features(e2e_middleware_test_circular_buffer_e2e PRIVATE cxx_std_17)
    add_test(NAME e2e_middleware_test_circular_buffer_e2e
             COMMAND e2e_middleware_test_circular_buffer_e2e)
    set_tests_properties(e2e_middleware_test_circular_buffer_e2e PROPERTIES
        LABELS "e2e;middleware;circular_buffer;batch;spsc"
        TIMEOUT 300
    )
    message(STATUS "    Added: e2e_middleware_test_circular_buffer_e2e")
endif()

message(STATUS "Added Middleware E2E tests")
