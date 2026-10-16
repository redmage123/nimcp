# Middleware Normalization Unit Tests
# Test Adaptive Normalizer
add_executable(unit_middleware_normalization_adaptive_normalizer
    test_adaptive_normalizer.cpp
)
target_link_libraries(unit_middleware_normalization_adaptive_normalizer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_normalization_adaptive_normalizer
         COMMAND unit_middleware_normalization_adaptive_normalizer)
# Test Homeostatic Normalizer
add_executable(unit_middleware_normalization_homeostatic_normalizer
    test_homeostatic_normalizer.cpp
)
target_link_libraries(unit_middleware_normalization_homeostatic_normalizer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_normalization_homeostatic_normalizer
         COMMAND unit_middleware_normalization_homeostatic_normalizer)
# Test Min Max Normalizer
add_executable(unit_middleware_normalization_min_max_normalizer
    test_min_max_normalizer.cpp
)
target_link_libraries(unit_middleware_normalization_min_max_normalizer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_normalization_min_max_normalizer
         COMMAND unit_middleware_normalization_min_max_normalizer)
# Test Zscore Normalizer
add_executable(unit_middleware_normalization_zscore_normalizer
    test_zscore_normalizer.cpp
)
target_link_libraries(unit_middleware_normalization_zscore_normalizer
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_normalization_zscore_normalizer
         COMMAND unit_middleware_normalization_zscore_normalizer)
# Test All Normalizers Integration
add_executable(unit_middleware_normalization_all_normalizers
    test_normalizers.cpp
)
target_link_libraries(unit_middleware_normalization_all_normalizers
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_normalization_all_normalizers
         COMMAND unit_middleware_normalization_all_normalizers)

# Test Normalization Immune Integration
add_executable(unit_middleware_normalization_immune
    test_normalization_immune.cpp
)
target_link_libraries(unit_middleware_normalization_immune
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME unit_middleware_normalization_immune
         COMMAND unit_middleware_normalization_immune)

# Test Normalizer Thread Safety (Bug H13)
add_executable(unit_middleware_normalization_thread_safety
    test_normalizer_thread_safety.cpp
)
target_link_libraries(unit_middleware_normalization_thread_safety
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
        Threads::Threads
    )
add_test(NAME unit_middleware_normalization_thread_safety
         COMMAND unit_middleware_normalization_thread_safety)
set_tests_properties(unit_middleware_normalization_thread_safety
    PROPERTIES
        LABELS "unit;middleware;normalization;thread_safety"
        TIMEOUT 60)
