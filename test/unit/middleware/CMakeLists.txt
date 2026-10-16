# Middleware Integration Tests
add_executable(unit_middleware_integration
    test_middleware_integration.cpp
)
target_link_libraries(unit_middleware_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_integration
         COMMAND unit_middleware_integration)

# Brain Spike Feature Extraction Tests
add_executable(unit_brain_spike_features
    test_brain_spike_features.cpp
)
target_link_libraries(unit_brain_spike_features
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_brain_spike_features
         COMMAND unit_brain_spike_features)

# Brain Population Coding Tests
add_executable(unit_brain_population_coding
    test_brain_population_coding.cpp
)
target_link_libraries(unit_brain_population_coding
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_brain_population_coding
         COMMAND unit_brain_population_coding)

# Brain Integration Tests
add_executable(unit_brain_integration
    test_brain_integration.cpp
)
target_link_libraries(unit_brain_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_brain_integration
         COMMAND unit_brain_integration)
set_tests_properties(unit_brain_integration PROPERTIES TIMEOUT 90)

add_subdirectory(brain_integration)
add_subdirectory(buffering)
add_subdirectory(encoding)
add_subdirectory(events)
add_subdirectory(features)
add_subdirectory(immune)
add_subdirectory(integration)
add_subdirectory(normalization)
add_subdirectory(patterns)
add_subdirectory(pipeline)
add_subdirectory(routing)
add_subdirectory(training)
