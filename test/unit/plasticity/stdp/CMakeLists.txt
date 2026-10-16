# Unit tests for STDP refactored module

add_executable(test_stdp_refactored
    test_stdp_refactored.cpp
)

target_include_directories(test_stdp_refactored PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/plasticity
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/security
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(test_stdp_refactored
    nimcp_stdp
    nimcp_async
    nimcp_logging
    nimcp_config
    nimcp_security
    nimcp_memory
    ${GTEST_LIBRARIES}
    pthread
)

add_test(NAME STDPRefactoredTests
         COMMAND test_stdp_refactored)

# Set test properties
set_tests_properties(STDPRefactoredTests PROPERTIES
    TIMEOUT 30
    LABELS "unit;plasticity;stdp;refactored"
)

# Triplet STDP Unit Tests
add_executable(test_triplet_stdp
    test_triplet_stdp.cpp
)

target_include_directories(test_triplet_stdp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/plasticity
    ${CMAKE_SOURCE_DIR}/include/cognitive
    ${CMAKE_SOURCE_DIR}/include/async
    ${CMAKE_SOURCE_DIR}/include/utils
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(test_triplet_stdp
    nimcp_triplet_stdp
    nimcp_memory
    nimcp_logging
    nimcp_platform
    ${GTEST_LIBRARIES}
    pthread
)

add_test(NAME TripletSTDPTests
         COMMAND test_triplet_stdp)

set_tests_properties(TripletSTDPTests PROPERTIES
    TIMEOUT 30
    LABELS "unit;plasticity;stdp;triplet"
)

# Quantum STDP Optimizer Unit Tests
add_executable(unit_plasticity_stdp_quantum_optimizer
    test_quantum_stdp_optimizer.cpp
)

target_include_directories(unit_plasticity_stdp_quantum_optimizer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(unit_plasticity_stdp_quantum_optimizer
    nimcp
    ${GTEST_LIBRARIES}
    pthread
    m
)

target_compile_features(unit_plasticity_stdp_quantum_optimizer PRIVATE cxx_std_17)

add_test(NAME QuantumSTDPOptimizerTests
         COMMAND unit_plasticity_stdp_quantum_optimizer)

set_tests_properties(QuantumSTDPOptimizerTests PROPERTIES
    TIMEOUT 30
    LABELS "unit;plasticity;stdp;quantum"
)

# STDP Utils Bridge Unit Tests
add_executable(unit_plasticity_stdp_utils_bridge
    test_stdp_utils_bridge.cpp
)

target_include_directories(unit_plasticity_stdp_utils_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(unit_plasticity_stdp_utils_bridge
    nimcp
    ${GTEST_LIBRARIES}
    pthread
    m
)

target_compile_features(unit_plasticity_stdp_utils_bridge PRIVATE cxx_std_17)

add_test(NAME STDPUtilsBridgeTests
         COMMAND unit_plasticity_stdp_utils_bridge)

set_tests_properties(STDPUtilsBridgeTests PROPERTIES
    TIMEOUT 60
    LABELS "unit;plasticity;stdp;utils;bridge"
)
