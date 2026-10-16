# ============================================================================
# Cognitive Memory Integration Tests
# ============================================================================
# Memory Integration Testing

# ============================================================================
# PR Memory Exception Handling Integration Tests
# ============================================================================
# Tests exception handling across PR memory modules (pr_memory_node, procedural, prospective)

add_executable(integration_cognitive_memory_pr_exception
    test_pr_memory_exception_integration.cpp
)

target_link_libraries(integration_cognitive_memory_pr_exception
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_memory_pr_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_memory_pr_exception PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_memory_pr_exception
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;memory;pr_memory;exception"
        TIMEOUT 120
)

# ============================================================================
# Working Memory - SNN - Plasticity Integration Testing

# ============================================================================
# Working Memory SNN+Plasticity Integration Tests
# ============================================================================
# Tests bidirectional dataflow between working memory, SNN, and plasticity

add_executable(integration_cognitive_memory_snn_plasticity
    test_working_memory_snn_plasticity_integration.cpp
)

target_link_libraries(integration_cognitive_memory_snn_plasticity
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_memory_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/time
        ${CMAKE_SOURCE_DIR}/src/cognitive/memory
        ${CMAKE_SOURCE_DIR}/src/snn
        ${CMAKE_SOURCE_DIR}/src/plasticity
)

target_compile_features(integration_cognitive_memory_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_memory_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;memory;snn;plasticity"
        TIMEOUT 180
)

# ============================================================================
# Engram Integration Tests (existing)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_engram_integration.cpp")
    add_executable(integration_cognitive_memory_engram
        test_engram_integration.cpp
    )

    target_link_libraries(integration_cognitive_memory_engram
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            m
    )

    target_include_directories(integration_cognitive_memory_engram
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    target_compile_features(integration_cognitive_memory_engram PRIVATE cxx_std_17)

    gtest_discover_tests(integration_cognitive_memory_engram
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;cognitive;memory;engram"
            TIMEOUT 120
    )
endif()

# ============================================================================
# Semantic Memory Integration Tests (existing)
# ============================================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_semantic_memory_integration.cpp")
    add_executable(integration_cognitive_memory_semantic
        test_semantic_memory_integration.cpp
    )

    target_link_libraries(integration_cognitive_memory_semantic
        PRIVATE
            nimcp
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            m
    )

    target_include_directories(integration_cognitive_memory_semantic
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    target_compile_features(integration_cognitive_memory_semantic PRIVATE cxx_std_17)

    gtest_discover_tests(integration_cognitive_memory_semantic
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "integration;cognitive;memory;semantic"
            TIMEOUT 120
    )
endif()
