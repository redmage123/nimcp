# Attention Substrate Bridge Unit Tests

add_executable(unit_cognitive_attention_substrate_bridge
    test_attention_substrate_bridge.cpp
)

target_link_libraries(unit_cognitive_attention_substrate_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_cognitive_attention_substrate_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(unit_cognitive_attention_substrate_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_cognitive_attention_substrate_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;cognitive;substrate;attention"
        TIMEOUT 60
)
