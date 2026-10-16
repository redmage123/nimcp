# Habenula unit tests

# Test helper include
include_directories(${CMAKE_SOURCE_DIR}/test/utils)

# Core test
add_executable(unit_habenula_test_core
    test_habenula_core.cpp
)
target_link_libraries(unit_habenula_test_core
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)
add_test(NAME unit_habenula_test_core COMMAND unit_habenula_test_core)

# Adapter test with training integration
add_executable(unit_habenula_test_adapter
    test_habenula_adapter.cpp
)
target_link_libraries(unit_habenula_test_adapter
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)
add_test(NAME unit_habenula_test_adapter COMMAND unit_habenula_test_adapter)
