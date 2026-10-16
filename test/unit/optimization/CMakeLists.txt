# ============================================================================
# 40-Watt Brain Optimization Unit Tests
# ============================================================================
# Uses shared brain fixtures (one brain per test suite, not per test)
# for efficient test execution

# Active Neuron Tracking (Phase 1)
add_executable(unit_active_neuron_tracking
    test_active_neuron_tracking.cpp
)
target_link_libraries(unit_active_neuron_tracking
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_active_neuron_tracking
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_active_neuron_tracking PRIVATE cxx_std_17)
gtest_discover_tests(unit_active_neuron_tracking
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;optimization;active_neurons" TIMEOUT 120
)

# Learning Workspace (Phase 2)
add_executable(unit_learning_workspace
    test_learning_workspace.cpp
)
target_link_libraries(unit_learning_workspace
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_learning_workspace
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_learning_workspace PRIVATE cxx_std_17)
gtest_discover_tests(unit_learning_workspace
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;optimization;workspace" TIMEOUT 120
)

# Fast Math (Phase 3)
add_executable(unit_fast_math
    test_fast_math.cpp
)
target_link_libraries(unit_fast_math
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_fast_math
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fast_math PRIVATE cxx_std_17)
gtest_discover_tests(unit_fast_math
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;optimization;fast_math" TIMEOUT 120
)

# Lazy Init (Phase 4)
add_executable(unit_lazy_init
    test_lazy_init.cpp
)
target_link_libraries(unit_lazy_init
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_lazy_init
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_lazy_init PRIVATE cxx_std_17)
gtest_discover_tests(unit_lazy_init
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;optimization;lazy_init" TIMEOUT 120
)

# Synapse Fast Path (Phase 5)
add_executable(unit_synapse_fast_path
    test_synapse_fast_path.cpp
)
target_link_libraries(unit_synapse_fast_path
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_synapse_fast_path
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_synapse_fast_path PRIVATE cxx_std_17)
gtest_discover_tests(unit_synapse_fast_path
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;optimization;synapse_fast" TIMEOUT 120
)

# Hot Path Optimizations (Phase 2)
add_executable(unit_hot_path_optimizations
    test_hot_path_optimizations.cpp
)
target_link_libraries(unit_hot_path_optimizations
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(unit_hot_path_optimizations
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_hot_path_optimizations PRIVATE cxx_std_17)
gtest_discover_tests(unit_hot_path_optimizations
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;optimization;hot_path" TIMEOUT 120
)

# Resource Optimizations (Phase 3)
add_executable(unit_resource_optimizations test_resource_optimizations.cpp)
target_link_libraries(unit_resource_optimizations PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_resource_optimizations PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_resource_optimizations PRIVATE cxx_std_17)
gtest_discover_tests(unit_resource_optimizations WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "unit;optimization;resource" TIMEOUT 120)

# Biological Connectivity (Phase 3)
add_executable(unit_biological_connectivity test_biological_connectivity.cpp)
target_link_libraries(unit_biological_connectivity PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_biological_connectivity PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_biological_connectivity PRIVATE cxx_std_17)
gtest_discover_tests(unit_biological_connectivity WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "unit;optimization;biological" TIMEOUT 120)

# Neuron Substrate Optimizations (Phase 4)
add_executable(unit_neuron_substrate test_neuron_substrate_optimizations.cpp)
target_link_libraries(unit_neuron_substrate PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_neuron_substrate PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_neuron_substrate PRIVATE cxx_std_17)
gtest_discover_tests(unit_neuron_substrate WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "unit;optimization;substrate" TIMEOUT 120)

# Synapse + Glial Optimizations (Phase 4)
add_executable(unit_synapse_glial test_synapse_glial_optimizations.cpp)
target_link_libraries(unit_synapse_glial PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_synapse_glial PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_synapse_glial PRIVATE cxx_std_17)
gtest_discover_tests(unit_synapse_glial WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "unit;optimization;synapse_glial" TIMEOUT 120)
