# ============================================================================
# 40-Watt Brain Optimization E2E Tests
# ============================================================================
add_executable(e2e_optimization
    test_optimization_e2e.cpp
)
target_link_libraries(e2e_optimization
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(e2e_optimization
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(e2e_optimization PRIVATE cxx_std_17)
gtest_discover_tests(e2e_optimization
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "e2e;optimization" TIMEOUT 300
)

# Hot Path E2E Tests
add_executable(e2e_hotpath
    test_hotpath_e2e.cpp
)
target_link_libraries(e2e_hotpath
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(e2e_hotpath
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(e2e_hotpath PRIVATE cxx_std_17)
gtest_discover_tests(e2e_hotpath
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "e2e;optimization;hot_path" TIMEOUT 300
)

# Resource E2E Tests (Phase 3)
add_executable(e2e_resource test_resource_e2e.cpp)
target_link_libraries(e2e_resource PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(e2e_resource PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(e2e_resource PRIVATE cxx_std_17)
gtest_discover_tests(e2e_resource WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "e2e;optimization;resource" TIMEOUT 300)

# Biological E2E Tests (Phase 3)
add_executable(e2e_biological test_biological_e2e.cpp)
target_link_libraries(e2e_biological PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(e2e_biological PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(e2e_biological PRIVATE cxx_std_17)
gtest_discover_tests(e2e_biological WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "e2e;optimization;biological" TIMEOUT 300)

# Substrate E2E Tests (Phase 4)
add_executable(e2e_substrate test_substrate_e2e.cpp)
target_link_libraries(e2e_substrate PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(e2e_substrate PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(e2e_substrate PRIVATE cxx_std_17)
gtest_discover_tests(e2e_substrate WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "e2e;optimization;substrate" TIMEOUT 300)
