# ============================================================================
# Security-Knowledge Graph Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Knowledge Graph Bridge
# WHY:  Verify security integration with knowledge graph operations
# HOW:  Test lifecycle, query validation, traversal access control,
#       node integrity, consistency enforcement, privacy isolation,
#       lockdown mode, bidirectional effects, and statistics
# ============================================================================

# Security-Knowledge Graph Bridge Unit Tests (~100 tests)
add_executable(unit_security_test_security_knowledge_graph_bridge
    test_security_knowledge_graph_bridge.cpp
)
target_link_libraries(unit_security_test_security_knowledge_graph_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_knowledge_graph_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/knowledge
        ${CMAKE_SOURCE_DIR}/include/cognitive/knowledge
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_knowledge_graph_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_knowledge_graph_bridge
         COMMAND unit_security_test_security_knowledge_graph_bridge)
set_tests_properties(unit_security_test_security_knowledge_graph_bridge PROPERTIES
    LABELS "unit;security;knowledge;bridge"
    TIMEOUT 120
)
