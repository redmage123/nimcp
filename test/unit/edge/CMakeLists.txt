# Unit tests for edge modules (swarm, sensor, watchdog, drone bridges)

# Sensor Hub
add_executable(unit_edge_test_sensor_hub test_sensor_hub.cpp)
target_link_libraries(unit_edge_test_sensor_hub PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_sensor_hub PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_sensor_hub PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_sensor_hub WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;sensor" TIMEOUT 60)

# Safety Watchdog
add_executable(unit_edge_test_safety_watchdog test_safety_watchdog.cpp)
target_link_libraries(unit_edge_test_safety_watchdog PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_safety_watchdog PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_safety_watchdog PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_safety_watchdog WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;watchdog" TIMEOUT 60)

# Swarm Lifecycle
add_executable(unit_edge_test_swarm_lifecycle test_swarm_lifecycle.cpp)
target_link_libraries(unit_edge_test_swarm_lifecycle PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_swarm_lifecycle PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_swarm_lifecycle PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_swarm_lifecycle WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;swarm" TIMEOUT 60)

# Swarm Sync
add_executable(unit_edge_test_swarm_sync test_swarm_sync.cpp)
target_link_libraries(unit_edge_test_swarm_sync PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_swarm_sync PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_swarm_sync PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_swarm_sync WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;swarm" TIMEOUT 60)

# Swarm Byzantine
add_executable(unit_edge_test_swarm_byzantine test_swarm_byzantine.cpp)
target_link_libraries(unit_edge_test_swarm_byzantine PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_swarm_byzantine PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_swarm_byzantine PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_swarm_byzantine WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;swarm" TIMEOUT 60)

# Drone Bridges (MAVLink, DJI, MSP, Parrot, ROS2)
add_executable(unit_edge_test_drone_bridges test_drone_bridges.cpp)
target_link_libraries(unit_edge_test_drone_bridges PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_drone_bridges PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_drone_bridges PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_drone_bridges WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;drone" TIMEOUT 60)

# Motor Output
add_executable(unit_edge_test_motor_output test_motor_output.cpp)
target_link_libraries(unit_edge_test_motor_output PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_motor_output PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_motor_output PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_motor_output WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;motor" TIMEOUT 60)

# Sim Bridge
add_executable(unit_edge_test_sim_bridge test_sim_bridge.cpp)
target_link_libraries(unit_edge_test_sim_bridge PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_sim_bridge PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_sim_bridge PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_sim_bridge WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;sim" TIMEOUT 60)

# Embodiment (URDF)
add_executable(unit_edge_test_embodiment test_embodiment.cpp)
target_link_libraries(unit_edge_test_embodiment PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_embodiment PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_embodiment PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_embodiment WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;embodiment" TIMEOUT 60)

# Sensorimotor Loop
add_executable(unit_edge_test_sensorimotor test_sensorimotor.cpp test_linker_stubs.c)
target_link_libraries(unit_edge_test_sensorimotor PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m)
target_include_directories(unit_edge_test_sensorimotor PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
target_compile_features(unit_edge_test_sensorimotor PRIVATE cxx_std_17)
gtest_discover_tests(unit_edge_test_sensorimotor WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "unit;edge;sensorimotor" TIMEOUT 60)
