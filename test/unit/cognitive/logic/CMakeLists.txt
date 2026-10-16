# ============================================================================
# Symbolic Logic Unit Tests
# ============================================================================

# Test 1: Symbolic Logic
add_test_binary(
    test_symbolic_logic
    ${CMAKE_CURRENT_SOURCE_DIR}/test_symbolic_logic.cpp
    unit
)

# Test 2: Symbolic Logic Real
add_test_binary(
    test_symbolic_logic_real
    ${CMAKE_CURRENT_SOURCE_DIR}/test_symbolic_logic_real.cpp
    unit
)

# Test 3: Neural Logic Real
add_test_binary(
    test_neural_logic_real
    ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_logic_real.cpp
    unit
)

# Test 4: Sensory-Logic Bridges (Visual, Somatosensory, Audio)
add_test_binary(
    test_sensory_logic_bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sensory_logic_bridges.cpp
    unit
)

# Test 5: Logic Sleep Bridge
add_executable(unit_cognitive_logic_test_logic_sleep_bridge
    test_logic_sleep_bridge.cpp
)
target_link_libraries(unit_cognitive_logic_test_logic_sleep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_cognitive_logic_test_logic_sleep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_logic_test_logic_sleep_bridge
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;cognitive;logic;sleep"
)
