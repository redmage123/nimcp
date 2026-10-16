# ============================================================================
# Cognitive E2E Tests
# ============================================================================
# WHAT: End-to-end tests for cognitive workflows
# WHY:  Verify complete cognitive workflows: training loops, prediction
#       pipelines, counterfactual reasoning, exception handling, and policy evaluation
# HOW:  GTest-based tests for realistic scenarios
#
# TESTS INCLUDED:
# - World model workflows (DreamerV3/JEPA inspired)
# - ToM exception handling
# - WM bridges exception handling
# - Predictive bridge exception handling
# ============================================================================

# Find test sources
file(GLOB WM_E2E_SOURCES "*.cpp")

# Exclude test_cognitive_integration_e2e.cpp - it has a dedicated target definition below
list(FILTER WM_E2E_SOURCES EXCLUDE REGEX "test_cognitive_integration_e2e\\.cpp$")

# Guard: Check if test sources exist
if(NOT WM_E2E_SOURCES)
    message(STATUS "No world model E2E tests found")
    return()
endif()

# Create test executable for each test file
foreach(TEST_FILE ${WM_E2E_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(TARGET_NAME "e2e_cognitive_${TEST_NAME}")

    add_executable(${TARGET_NAME} ${TEST_FILE})

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/e2e
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

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)
    target_compile_definitions(${TARGET_NAME} PRIVATE NIMCP_TESTING)

    # Register test with CTest
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_tests_properties(${TARGET_NAME} PROPERTIES
        LABELS "e2e;cognitive;world_model"
        TIMEOUT 300
    )
endforeach()

message(STATUS "Added world model E2E tests")

# ============================================================================
# Cognitive Integration E2E Test (GTest C++)
# ============================================================================
# WHAT: End-to-end tests for cognitive module integration
# WHY:  Verify working memory, executive functions, ToM, meta-cognition integration
# HOW:  Tests full cognitive pipeline with emotional tagging and stress testing
#
# TESTS:
# 1. WorkingMemoryBasicOperations - WM create, store, retrieve, destroy
# 2. WorkingMemoryCapacityEnforcement - Miller's 7+/-2 capacity with eviction
# 3. WorkingMemoryDecay - Temporal decay testing
# 4. WorkingMemoryRefresh - Memory refresh mechanism
# 5. ExecutiveBasicOperations - Task management and completion
# 6. ExecutiveTaskSwitching - Task switching and costs
# 7. ExecutiveInhibitoryControl - Response inhibition
# 8. ExecutiveCognitiveLoad - Load monitoring and adaptation
# 9. WMExecutiveCoordination - Working memory to executive coordination
# 10. EmotionalTaggingIntegration - Emotional enhancement of memory
# 11. TOMBasicInference - Theory of mind inference
# 12. TOMFalseBeliefDetection - False belief detection
# 13. TOMEmpathyResponse - Empathy response generation
# 14. MetaCognitionReflection - Meta-cognitive monitoring
# 15. CognitiveStress - High-volume processing
# 16. CompleteCognitivePipeline - Complete cognitive processing pipeline
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_integration_e2e.cpp)
    add_executable(e2e_test_cognitive_integration
        ${CMAKE_CURRENT_SOURCE_DIR}/test_cognitive_integration_e2e.cpp
        ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
    )
    target_include_directories(e2e_test_cognitive_integration PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/test/e2e
        ${CMAKE_SOURCE_DIR}/src/lib
    )
    target_link_libraries(e2e_test_cognitive_integration PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
    )
    target_compile_features(e2e_test_cognitive_integration PRIVATE cxx_std_17)
    target_compile_definitions(e2e_test_cognitive_integration PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )
    add_test(NAME e2e_test_cognitive_integration COMMAND e2e_test_cognitive_integration)
    set_tests_properties(e2e_test_cognitive_integration PROPERTIES
        LABELS "e2e;cognitive;integration;working_memory;executive;tom"
        TIMEOUT 300
    )
    message(STATUS "    Added: e2e_test_cognitive_integration (GTest C++)")
endif()

# ============================================================================
# Community Cache E2E Tests
# ============================================================================
add_executable(e2e_cognitive_community_cache
    test_community_cache_e2e.cpp
)

target_link_libraries(e2e_cognitive_community_cache
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(e2e_cognitive_community_cache
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(e2e_cognitive_community_cache PRIVATE cxx_std_17)

gtest_discover_tests(e2e_cognitive_community_cache
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;cognitive;consolidation;community_cache"
        TIMEOUT 120
)

# ============================================================================
# Include subdirectories
# ============================================================================
add_subdirectory(attention)
add_subdirectory(memory)
add_subdirectory(neuro_symbolic)
add_subdirectory(immune)
add_subdirectory(parietal)
add_subdirectory(fault_tolerance)
add_subdirectory(inner_dialogue)

# ============================================================================
# Cognitive E2E Pipeline Tests
# ============================================================================

add_executable(e2e_cognitive_pipeline
    test_cognitive_e2e.cpp
)
target_link_libraries(e2e_cognitive_pipeline
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(e2e_cognitive_pipeline
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(e2e_cognitive_pipeline PRIVATE cxx_std_17)
gtest_discover_tests(e2e_cognitive_pipeline
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "e2e;cognitive;pipeline"
        TIMEOUT 180
)
