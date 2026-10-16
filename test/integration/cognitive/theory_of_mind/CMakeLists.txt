# Theory of Mind SNN-Plasticity Integration Tests

add_executable(integration_cognitive_tom_snn_plasticity
    test_tom_snn_plasticity_integration.cpp
)

target_link_libraries(integration_cognitive_tom_snn_plasticity
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_tom_snn_plasticity
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_tom_snn_plasticity PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_tom_snn_plasticity
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;tom;snn;plasticity"
        TIMEOUT 180
)

# Theory of Mind Exception Handling Integration Tests
add_executable(integration_cognitive_tom_exception
    test_tom_exception_integration.cpp
)

target_link_libraries(integration_cognitive_tom_exception
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
        m
)

target_include_directories(integration_cognitive_tom_exception
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(integration_cognitive_tom_exception PRIVATE cxx_std_17)

gtest_discover_tests(integration_cognitive_tom_exception
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "integration;cognitive;tom;exception"
        TIMEOUT 180
)
