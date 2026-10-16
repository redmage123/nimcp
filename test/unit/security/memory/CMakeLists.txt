# ============================================================================
# Security-Memory Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Memory Bridge (Memory Protection)
# WHY:  Verify access control, encryption, classification, secure erase,
#       leakage detection, and audit for memory systems
# HOW:  Test lifecycle, connections, core security features, bidirectional effects
# ============================================================================

add_executable(unit_security_test_security_memory_bridge
    test_security_memory_bridge.cpp
)

target_link_libraries(unit_security_test_security_memory_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_include_directories(unit_security_test_security_memory_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/memory
        ${CMAKE_SOURCE_DIR}/include/cognitive/memory
        ${CMAKE_SOURCE_DIR}/include/utils/bridge
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)

target_compile_features(unit_security_test_security_memory_bridge PRIVATE cxx_std_17)

add_test(NAME unit_security_test_security_memory_bridge
         COMMAND unit_security_test_security_memory_bridge)

set_tests_properties(unit_security_test_security_memory_bridge PROPERTIES
    LABELS "unit;security;memory;bridge"
    TIMEOUT 180
)
