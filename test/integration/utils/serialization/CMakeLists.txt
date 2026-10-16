# Serialization AES Integration Tests
# Tests cross-context encrypt/decrypt, file persistence round-trip,
# multi-buffer workflows, and nimcp_write_processed/nimcp_read_processed

add_executable(integration_utils_serialization_aes
    test_serialization_aes_integration.cpp
)
target_link_libraries(integration_utils_serialization_aes
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(integration_utils_serialization_aes
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(integration_utils_serialization_aes PRIVATE cxx_std_17)
gtest_discover_tests(integration_utils_serialization_aes
    PROPERTIES LABELS "integration;utils;serialization;aes")
