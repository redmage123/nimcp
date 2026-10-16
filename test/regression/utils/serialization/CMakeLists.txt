# Serialization AES Regression Tests
# Tests invariants: ciphertext size formula, IV randomness, bit-flip auth,
# large payloads, key length edge cases, empty payload handling

add_executable(regression_utils_serialization_aes
    test_serialization_aes_regression.cpp
)
target_link_libraries(regression_utils_serialization_aes
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_include_directories(regression_utils_serialization_aes
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(regression_utils_serialization_aes PRIVATE cxx_std_17)
gtest_discover_tests(regression_utils_serialization_aes
    PROPERTIES LABELS "regression;utils;serialization;aes")
