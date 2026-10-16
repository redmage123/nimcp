# ============================================================================
# NIMCP Medulla Oblongata End-to-End Tests
# ============================================================================
# WHAT: Build configuration for medulla E2E tests
# WHY:  Test complete medulla pipelines including exception handling
# HOW:  Build test executables for full medulla subsystem integration
# ============================================================================

message(STATUS "    -- Configuring Medulla Oblongata E2E Tests")

# ----------------------------------------------------------------------------
# Test: Medulla Exception E2E
# ----------------------------------------------------------------------------
set(TEST_MEDULLA_EXCEPTION_E2E_SOURCES
    test_medulla_exception_e2e.cpp
)

add_executable(test_medulla_exception_e2e ${TEST_MEDULLA_EXCEPTION_E2E_SOURCES})

target_include_directories(test_medulla_exception_e2e PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/utils
)

target_link_libraries(test_medulla_exception_e2e PRIVATE
    nimcp
    GTest::GTest
    GTest::Main
    Threads::Threads
    m
)

target_compile_features(test_medulla_exception_e2e PRIVATE cxx_std_17)

target_compile_definitions(test_medulla_exception_e2e PRIVATE
    NIMCP_TESTING
    NIMCP_E2E_TESTING
)

# Register with CTest
add_test(NAME test_medulla_exception_e2e
         COMMAND test_medulla_exception_e2e --gtest_brief=1)

# Set test timeout (E2E tests may take longer)
set_tests_properties(test_medulla_exception_e2e PROPERTIES
    TIMEOUT 300
    LABELS "e2e;core;medulla;exception"
)

message(STATUS "    -- Configured: test_medulla_exception_e2e")

# ----------------------------------------------------------------------------
# Test: Medulla Mesh E2E
# ----------------------------------------------------------------------------
set(TEST_MEDULLA_MESH_E2E_SOURCES
    test_medulla_mesh_e2e.cpp
)

add_executable(test_medulla_mesh_e2e ${TEST_MEDULLA_MESH_E2E_SOURCES})

target_include_directories(test_medulla_mesh_e2e PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/utils
)

target_link_libraries(test_medulla_mesh_e2e PRIVATE
    nimcp
    GTest::GTest
    GTest::Main
    Threads::Threads
    m
)

target_compile_features(test_medulla_mesh_e2e PRIVATE cxx_std_17)

target_compile_definitions(test_medulla_mesh_e2e PRIVATE
    NIMCP_TESTING
    NIMCP_E2E_TESTING
)

# Register with CTest
add_test(NAME test_medulla_mesh_e2e
         COMMAND test_medulla_mesh_e2e --gtest_brief=1)

# Set test timeout (E2E tests may take longer, mesh consensus especially)
set_tests_properties(test_medulla_mesh_e2e PROPERTIES
    TIMEOUT 600
    LABELS "e2e;core;medulla;mesh"
)

message(STATUS "    -- Configured: test_medulla_mesh_e2e")
