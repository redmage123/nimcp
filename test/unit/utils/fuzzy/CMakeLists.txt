# Fuzzy Logic Unit Tests
# Phase: Fuzzy Logic Module (types, operators, inference, bridge)
# ~185 tests across 4 executables

# --- Fuzzy Types Tests (~50 tests) ---
add_executable(unit_fuzzy_types test_fuzzy_types.cpp)
target_link_libraries(unit_fuzzy_types PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_fuzzy_types PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/test/utils)
target_compile_features(unit_fuzzy_types PRIVATE cxx_std_17)
gtest_discover_tests(unit_fuzzy_types
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;utils;fuzzy;types"
    TIMEOUT 90
)

# --- Fuzzy Operators Tests (~45 tests) ---
add_executable(unit_fuzzy_operators test_fuzzy_operators.cpp)
target_link_libraries(unit_fuzzy_operators PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_fuzzy_operators PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/test/utils)
target_compile_features(unit_fuzzy_operators PRIVATE cxx_std_17)
gtest_discover_tests(unit_fuzzy_operators
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;utils;fuzzy;operators"
    TIMEOUT 90
)

# --- Fuzzy Inference Tests (~50 tests) ---
add_executable(unit_fuzzy_inference test_fuzzy_inference.cpp)
target_link_libraries(unit_fuzzy_inference PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_fuzzy_inference PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/test/utils)
target_compile_features(unit_fuzzy_inference PRIVATE cxx_std_17)
gtest_discover_tests(unit_fuzzy_inference
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;utils;fuzzy;inference"
    TIMEOUT 90
)

# --- Fuzzy Bridge Tests (~40 tests) ---
add_executable(unit_fuzzy_bridge test_fuzzy_bridge.cpp)
target_link_libraries(unit_fuzzy_bridge PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_fuzzy_bridge PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/test/utils)
target_compile_features(unit_fuzzy_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_fuzzy_bridge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;utils;fuzzy;bridge"
    TIMEOUT 90
)

# --- Combined target for all fuzzy logic unit tests ---
add_custom_target(unit_fuzzy_logic
    DEPENDS unit_fuzzy_types unit_fuzzy_operators unit_fuzzy_inference unit_fuzzy_bridge
)
