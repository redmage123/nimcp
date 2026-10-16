add_executable(unit_core_brain_bridges_hyperledger
    test_hyperledger_bridge.cpp
)
target_link_libraries(unit_core_brain_bridges_hyperledger
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_core_brain_bridges_hyperledger PRIVATE cxx_std_17)
target_include_directories(unit_core_brain_bridges_hyperledger PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
gtest_discover_tests(unit_core_brain_bridges_hyperledger
    PROPERTIES LABELS "unit;core;brain;bridges;hyperledger"
    TIMEOUT 120
)
