# Integration tests for parallel cognitive dispatch

add_executable(integration_cognitive_dispatch
    test_cognitive_dispatch_integration.cpp
)
target_link_libraries(integration_cognitive_dispatch
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(integration_cognitive_dispatch PRIVATE cxx_std_17)
target_include_directories(integration_cognitive_dispatch
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
gtest_discover_tests(integration_cognitive_dispatch
    PROPERTIES LABELS "integration;core;cognitive_dispatch"
    TIMEOUT 120
)
