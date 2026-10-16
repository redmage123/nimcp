# Attention Sleep Bridge Unit Tests

add_executable(unit_cognitive_attention_sleep_bridge
    test_attention_sleep_bridge.cpp
)

target_link_libraries(unit_cognitive_attention_sleep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(unit_cognitive_attention_sleep_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(unit_cognitive_attention_sleep_bridge)
