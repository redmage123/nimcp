# ============================================================================
# Training Integration Hub Unit Tests
# ============================================================================
# Tests for the training integration hub functionality including:
# - Hub lifecycle (create/destroy)
# - Module registration/unregistration
# - Event subscription and publishing
# - Inter-module queries
# - Statistics tracking
# - Convenience helper functions
# ============================================================================

add_executable(unit_test_training_integration_hub
    test_training_integration_hub.cpp
)

target_link_libraries(unit_test_training_integration_hub
    PRIVATE
        nimcp
        nimcp_middleware
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_test_training_integration_hub
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_test_training_integration_hub PRIVATE cxx_std_20)

gtest_discover_tests(unit_test_training_integration_hub
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;training;integration;hub"
        TIMEOUT 60
        RESOURCE_LOCK "brain_heavy"
)

# ============================================================================
# Training Symbolic Logic Hub Bridge Unit Tests
# ============================================================================
# Tests for the symbolic logic - training hub bridge functionality including:
# - Bridge lifecycle (create/destroy)
# - Rule management (add, remove, get)
# - Metrics updates
# - Rule evaluation
# - Rule learning
# - Query APIs (LR, difficulty, early stop)
# - Hub connection
# ============================================================================

add_executable(unit_test_training_symbolic_logic_hub_bridge
    test_training_symbolic_logic_hub_bridge.cpp
)

target_link_libraries(unit_test_training_symbolic_logic_hub_bridge
    PRIVATE
        nimcp
        nimcp_middleware
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_test_training_symbolic_logic_hub_bridge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(unit_test_training_symbolic_logic_hub_bridge PRIVATE cxx_std_20)

gtest_discover_tests(unit_test_training_symbolic_logic_hub_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;training;integration;symbolic-logic;hub-bridge"
        TIMEOUT 120
        RESOURCE_LOCK "brain_heavy"
)
