# ============================================================================
# Security-Distributed Training Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Security-Distributed Training Bridge (Byzantine Defense)
# WHY:  Verify security integration with distributed/federated training systems
# HOW:  Test lifecycle, Byzantine detection, gradient validation, worker trust,
#       checkpointing, bidirectional effects, and statistics
# ============================================================================

add_executable(unit_security_test_security_distributed_training_bridge
    test_security_distributed_training_bridge.cpp
)
target_link_libraries(unit_security_test_security_distributed_training_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_security_test_security_distributed_training_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/distributed
        ${CMAKE_SOURCE_DIR}/include/training
        ${CMAKE_SOURCE_DIR}/include/async
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_security_test_security_distributed_training_bridge PRIVATE cxx_std_17)
add_test(NAME unit_security_test_security_distributed_training_bridge
         COMMAND unit_security_test_security_distributed_training_bridge)
set_tests_properties(unit_security_test_security_distributed_training_bridge PROPERTIES
    LABELS "unit;security;distributed;bridge"
    TIMEOUT 120
)
