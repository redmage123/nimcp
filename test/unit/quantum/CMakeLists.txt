# ============================================================================
# Quantum Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for quantum-enhanced cognitive bridges
# WHY:  Validate Grover search, SAT solving, quantum reasoning, superposition
# HOW:  Test quantum primitives and brain integration
# ============================================================================

add_executable(unit_quantum_bridges
    test_quantum_bridges.cpp
)

target_include_directories(unit_quantum_bridges PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_quantum_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)

target_compile_features(unit_quantum_bridges PRIVATE cxx_std_17)

add_test(NAME unit_quantum_bridges COMMAND unit_quantum_bridges)

# ============================================================================
# Broca Quantum Bridge Tests
# ============================================================================
add_executable(unit_quantum_test_broca_quantum_bridge
    test_broca_quantum_bridge.cpp
)

target_include_directories(unit_quantum_test_broca_quantum_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(unit_quantum_test_broca_quantum_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_quantum_test_broca_quantum_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_quantum_test_broca_quantum_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;quantum;broca"
        TIMEOUT 120
)

# ============================================================================
# Grover Search Tests
# ============================================================================
add_executable(unit_quantum_test_grover_search
    test_grover_search.cpp
)

target_include_directories(unit_quantum_test_grover_search PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(unit_quantum_test_grover_search
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_quantum_test_grover_search PRIVATE cxx_std_17)

gtest_discover_tests(unit_quantum_test_grover_search
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;quantum;grover"
        TIMEOUT 120
)

# ============================================================================
# Quantum Brain Integration Tests
# ============================================================================
# DISABLED: Uses outdated qreason_cnf_* API that has been refactored
message(STATUS "unit_quantum_test_quantum_brain_integration: DISABLED (outdated qreason_cnf_* API)")
#[[
add_executable(unit_quantum_test_quantum_brain_integration
    test_quantum_brain_integration.cpp
)

target_include_directories(unit_quantum_test_quantum_brain_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(unit_quantum_test_quantum_brain_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_quantum_test_quantum_brain_integration PRIVATE cxx_std_17)

gtest_discover_tests(unit_quantum_test_quantum_brain_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;quantum;brain;integration"
        TIMEOUT 180
)
#]]

# ============================================================================
# Quantum Bridge Integration Tests
# ============================================================================
add_executable(unit_quantum_test_quantum_bridge_integration
    test_quantum_bridge_integration.cpp
)

target_include_directories(unit_quantum_test_quantum_bridge_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(unit_quantum_test_quantum_bridge_integration
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_quantum_test_quantum_bridge_integration PRIVATE cxx_std_17)

gtest_discover_tests(unit_quantum_test_quantum_bridge_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;quantum;bridge;integration"
        TIMEOUT 120
)

# ============================================================================
# Quantum Reasoning Tests
# ============================================================================
add_executable(unit_quantum_test_quantum_reasoning
    test_quantum_reasoning.cpp
)

target_include_directories(unit_quantum_test_quantum_reasoning PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(unit_quantum_test_quantum_reasoning
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_quantum_test_quantum_reasoning PRIVATE cxx_std_17)

gtest_discover_tests(unit_quantum_test_quantum_reasoning
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;quantum;reasoning"
        TIMEOUT 120
)

# ============================================================================
# Quantum SAT Solver Tests
# ============================================================================
add_executable(unit_quantum_test_quantum_sat
    test_quantum_sat.cpp
)

target_include_directories(unit_quantum_test_quantum_sat PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(unit_quantum_test_quantum_sat
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_quantum_test_quantum_sat PRIVATE cxx_std_17)

gtest_discover_tests(unit_quantum_test_quantum_sat
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;quantum;sat"
        TIMEOUT 120
)

# ============================================================================
# Quantum Superposition Tests
# ============================================================================
add_executable(unit_quantum_test_quantum_superposition
    test_quantum_superposition.cpp
)

target_include_directories(unit_quantum_test_quantum_superposition PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(unit_quantum_test_quantum_superposition
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_quantum_test_quantum_superposition PRIVATE cxx_std_17)

gtest_discover_tests(unit_quantum_test_quantum_superposition
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;quantum;superposition"
        TIMEOUT 120
)

# Exception handling tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()
