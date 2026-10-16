# ============================================================================
# VTA Unit Tests
# ============================================================================
# WHAT: Unit tests for Ventral Tegmental Area (VTA) module
# WHY:  Verify DA release, RPE, incentive salience, and adapter
# HOW:  GTest-based tests for all VTA components

message(STATUS "Adding VTA unit tests...")

# VTA Core test
add_executable(unit_vta_test_core
    test_vta_core.cpp
)

target_include_directories(unit_vta_test_core PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_vta_test_core
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_vta_test_core
         COMMAND unit_vta_test_core)

# DA Release test
add_executable(unit_vta_test_da_release
    test_da_release.cpp
)

target_include_directories(unit_vta_test_da_release PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_vta_test_da_release
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_vta_test_da_release
         COMMAND unit_vta_test_da_release)

# RPE test
add_executable(unit_vta_test_rpe
    test_rpe.cpp
)

target_include_directories(unit_vta_test_rpe PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_vta_test_rpe
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_vta_test_rpe
         COMMAND unit_vta_test_rpe)

# Incentive Salience test
add_executable(unit_vta_test_salience
    test_salience.cpp
)

target_include_directories(unit_vta_test_salience PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_vta_test_salience
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_vta_test_salience
         COMMAND unit_vta_test_salience)

# VTA Adapter test
add_executable(unit_vta_test_adapter
    test_vta_adapter.cpp
)

target_include_directories(unit_vta_test_adapter PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_vta_test_adapter
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_vta_test_adapter
         COMMAND unit_vta_test_adapter)
