# BCM Sleep Bridge Unit Tests

add_executable(unit_plasticity_bcm_sleep_bridge
    test_bcm_sleep_bridge.cpp
)

target_link_libraries(unit_plasticity_bcm_sleep_bridge
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(unit_plasticity_bcm_sleep_bridge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(unit_plasticity_bcm_sleep_bridge)
