# ============================================================================
# Reasoning Unit Tests
# ============================================================================

# Test 1: Symbolic Logic Brain Integration
# DISABLED: Functions are inside #if 0 blocks in source
# add_test_binary(
#     test_symbolic_logic_brain_integration
#     ${CMAKE_CURRENT_SOURCE_DIR}/test_symbolic_logic_brain_integration.cpp
#     unit
# )

# Test 2: Symbolic Logic Attachment
add_test_binary(
    test_symbolic_logic_attachment
    ${CMAKE_CURRENT_SOURCE_DIR}/test_symbolic_logic_attachment.cpp
    unit
)

# Test 3: Reasoning Factory
add_test_binary(
    test_reasoning_factory
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_factory.cpp
    unit
)

# Test 4: Forward Chaining
add_test_binary(
    test_forward_chaining
    ${CMAKE_CURRENT_SOURCE_DIR}/test_forward_chaining.cpp
    unit
)

# Test 5: Backward Chaining
add_test_binary(
    test_backward_chaining
    ${CMAKE_CURRENT_SOURCE_DIR}/test_backward_chaining.cpp
    unit
)

# Test 6: Unification Engine
add_test_binary(
    test_unification_engine
    ${CMAKE_CURRENT_SOURCE_DIR}/test_unification_engine.cpp
    unit
)

# Test 7: Knowledge Base Interface
add_test_binary(
    test_knowledge_base_interface
    ${CMAKE_CURRENT_SOURCE_DIR}/test_knowledge_base_interface.cpp
    unit
)

# Test 8: Reasoning Unit Integration
add_test_binary(
    test_reasoning_unit_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reasoning_integration.cpp
    unit
)

# Test 9: Reasoning Attention Integration
add_test_binary(
    test_reasoning_attention
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/test_reasoning_attention.cpp
    unit
)

# Test 10: Quantum Reasoning
add_test_binary(
    test_quantum_reasoning
    ${CMAKE_CURRENT_SOURCE_DIR}/test_quantum_reasoning.cpp
    unit
)

# Test 11: Reasoning Sleep Bridge
add_executable(unit_cognitive_reasoning_test_reasoning_sleep_bridge
    test_reasoning_sleep_bridge.cpp
)
target_link_libraries(unit_cognitive_reasoning_test_reasoning_sleep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_cognitive_reasoning_test_reasoning_sleep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_reasoning_test_reasoning_sleep_bridge
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;cognitive;reasoning;sleep"
)

# Substrate bridge tests
add_subdirectory(substrate)

# Reasoning SNN Bridge Unit Tests
add_executable(unit_cognitive_reasoning_snn_bridge
    test_reasoning_snn_bridge.cpp
)
target_link_libraries(unit_cognitive_reasoning_snn_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_reasoning_snn_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_reasoning_snn_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_reasoning_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning;snn"
        TIMEOUT 120
)

# Hypergraph Init Unit Tests (mutex initialization fix)
add_executable(unit_cognitive_reasoning_hypergraph_init
    test_hypergraph_init.cpp
)
target_link_libraries(unit_cognitive_reasoning_hypergraph_init
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_reasoning_hypergraph_init
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_reasoning_hypergraph_init PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_reasoning_hypergraph_init
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning;hypergraph"
        TIMEOUT 120
)

# KB Persistence Unit Tests
add_executable(unit_cognitive_reasoning_kb_persistence
    test_reasoning_kb_persistence.cpp
)
target_link_libraries(unit_cognitive_reasoning_kb_persistence
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_reasoning_kb_persistence
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_reasoning_kb_persistence PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_reasoning_kb_persistence
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning;persistence"
        TIMEOUT 120
)

# Reasoning Plasticity Bridge Unit Tests
add_executable(unit_cognitive_reasoning_plasticity_bridge
    test_reasoning_plasticity_bridge.cpp
)
target_link_libraries(unit_cognitive_reasoning_plasticity_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_reasoning_plasticity_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_reasoning_plasticity_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_reasoning_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;reasoning;plasticity"
        TIMEOUT 120
)
