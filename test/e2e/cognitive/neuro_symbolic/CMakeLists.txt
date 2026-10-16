# ============================================================================
# Neuro-Symbolic E2E Tests - Mathesis Genius Mathematics System
# ============================================================================
# WHAT: End-to-end tests for the genius-level mathematics system
# WHY:  Verify complete mathematical reasoning workflows including:
#       - Pattern discovery and conjecture generation (Gauss mode)
#       - Evolutionary proof search with energy consistency
#       - Classical vs quantum planning comparison
#       - Multi-genius collaboration
#       - Full orchestrator workflows
# HOW:  GTest-based tests with extended timeouts for longer-running operations
# ============================================================================

# Find test sources
file(GLOB NEURO_SYMBOLIC_E2E_SOURCES "*.cpp")

# Guard: Check if test sources exist
if(NOT NEURO_SYMBOLIC_E2E_SOURCES)
    message(STATUS "No neuro-symbolic E2E tests found")
    return()
endif()

# Create test executable for each test file
foreach(TEST_FILE ${NEURO_SYMBOLIC_E2E_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(TARGET_NAME "e2e_neuro_symbolic_${TEST_NAME}")

    add_executable(${TARGET_NAME} ${TEST_FILE})

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            Threads::Threads
            m
    )

    # Enable C++17 for structured bindings and other features
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)

    # Enable test-only APIs
    target_compile_definitions(${TARGET_NAME} PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    # Register test with CTest - extended timeout for mathematical proofs
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_tests_properties(${TARGET_NAME} PROPERTIES
        LABELS "e2e;cognitive;neuro_symbolic;mathesis"
        TIMEOUT 600
    )
endforeach()

message(STATUS "Added neuro-symbolic E2E tests (Mathesis genius mathematics)")
