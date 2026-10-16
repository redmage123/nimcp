# Middleware Encoding Unit Tests
# Test Rate Coding
add_executable(unit_middleware_encoding_rate_coding
    test_rate_coding.cpp
)
target_link_libraries(unit_middleware_encoding_rate_coding
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_encoding_rate_coding
         COMMAND unit_middleware_encoding_rate_coding)

# Test Temporal Coding
add_executable(unit_middleware_encoding_temporal_coding
    test_temporal_coding.cpp
)
target_link_libraries(unit_middleware_encoding_temporal_coding
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_encoding_temporal_coding
         COMMAND unit_middleware_encoding_temporal_coding)

# Test Population Coding (Phase 2)
add_executable(unit_middleware_encoding_population_coding
    test_population_coding.cpp
)
target_link_libraries(unit_middleware_encoding_population_coding
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_encoding_population_coding
         COMMAND unit_middleware_encoding_population_coding)

# Test All Encoders Integration
add_executable(unit_middleware_encoding_all_encoders
    test_encoders.cpp
)
target_link_libraries(unit_middleware_encoding_all_encoders
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_encoding_all_encoders
         COMMAND unit_middleware_encoding_all_encoders)

# Test Population Coding-Immune Integration
add_executable(unit_middleware_encoding_population_immune
    test_population_coding_immune_integration.cpp
)
target_link_libraries(unit_middleware_encoding_population_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_encoding_population_immune
         COMMAND unit_middleware_encoding_population_immune)
