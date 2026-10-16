# Serialization AES End-to-End Tests
# Tests full workflow: context create -> set key -> encrypt -> persist ->
# load -> decrypt -> verify. Key rotation simulation, stress tests.

add_executable(e2e_utils_serialization_aes
    test_serialization_aes_e2e.cpp
)
target_link_libraries(e2e_utils_serialization_aes
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(e2e_utils_serialization_aes
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(e2e_utils_serialization_aes PRIVATE cxx_std_17)
gtest_discover_tests(e2e_utils_serialization_aes
    PROPERTIES LABELS "e2e;utils;serialization;aes")
