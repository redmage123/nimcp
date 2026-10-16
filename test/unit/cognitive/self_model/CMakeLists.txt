# ============================================================================
# Self Model SNN/Plasticity Bridge Unit Tests
# ============================================================================
# WHAT: Unit tests for Self Model SNN and Plasticity bridges
# WHY:  Validate self-model dimension encoding, insight decoding, and learning
# HOW:  Test lifecycle, encoding, simulation, detection, callbacks, protection
# ============================================================================

# Self Model FEP Bridge Unit Tests
add_executable(unit_cognitive_self_model_fep_bridge
    test_self_model_fep_bridge.cpp
)
target_link_libraries(unit_cognitive_self_model_fep_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_self_model_fep_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_self_model_fep_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_self_model_fep_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;self_model;fep"
        TIMEOUT 120
)

# Self Model SNN Bridge Unit Tests
add_executable(unit_cognitive_self_model_snn_bridge
    test_self_model_snn_bridge.cpp
)
target_link_libraries(unit_cognitive_self_model_snn_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_self_model_snn_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_self_model_snn_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_self_model_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;self_model;snn"
        TIMEOUT 120
)

# Self Model Plasticity Bridge Unit Tests
add_executable(unit_cognitive_self_model_plasticity_bridge
    test_self_model_plasticity_bridge.cpp
)
target_link_libraries(unit_cognitive_self_model_plasticity_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_self_model_plasticity_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_self_model_plasticity_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_self_model_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;self_model;plasticity"
        TIMEOUT 120
)
