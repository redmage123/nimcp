# Theory of Mind Unit Tests

# Theory of Mind SNN Bridge Unit Tests
add_executable(unit_cognitive_tom_snn_bridge
    test_tom_snn_bridge.cpp
)
target_link_libraries(unit_cognitive_tom_snn_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_tom_snn_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_tom_snn_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_tom_snn_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;tom;snn"
        TIMEOUT 120
)

# Theory of Mind Plasticity Bridge Unit Tests
add_executable(unit_cognitive_tom_plasticity_bridge
    test_tom_plasticity_bridge.cpp
)
target_link_libraries(unit_cognitive_tom_plasticity_bridge
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_tom_plasticity_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_tom_plasticity_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_tom_plasticity_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;tom;plasticity"
        TIMEOUT 120
)

# Theory of Mind Exception Handling Tests
add_executable(unit_cognitive_tom_exception_handling
    test_tom_exception_handling.cpp
)
target_link_libraries(unit_cognitive_tom_exception_handling
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)
target_include_directories(unit_cognitive_tom_exception_handling
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(unit_cognitive_tom_exception_handling PRIVATE cxx_std_17)
gtest_discover_tests(unit_cognitive_tom_exception_handling
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;tom;exception"
        TIMEOUT 120
)
