# Cortical Substrate Bridge Unit Tests
# Tests for cortical column - neural substrate integration

add_executable(unit_core_cortical_columns_substrate_bridge
    test_cortical_substrate_bridge.cpp
)

target_link_libraries(unit_core_cortical_columns_substrate_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_core_cortical_columns_substrate_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(unit_core_cortical_columns_substrate_bridge PRIVATE cxx_std_17)

gtest_discover_tests(unit_core_cortical_columns_substrate_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;core;cortical;substrate"
        TIMEOUT 60
)
