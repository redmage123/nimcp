# Brain Integration Unit Tests
add_executable(unit_middleware_brain_integration_test_brain_integration
    test_brain_integration.cpp
)
target_link_libraries(unit_middleware_brain_integration_test_brain_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_brain_integration_test_brain_integration
         COMMAND unit_middleware_brain_integration_test_brain_integration)
