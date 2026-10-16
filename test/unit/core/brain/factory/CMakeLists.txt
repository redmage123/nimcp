# ============================================================================
# Brain Factory Unit Tests
# ============================================================================

# Test 1: brain_create() function
add_executable(unit_core_brain_factory_test_brain_create
    test_brain_create.cpp
)
target_link_libraries(unit_core_brain_factory_test_brain_create
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_core_brain_factory_test_brain_create
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_core_brain_factory_test_brain_create
         COMMAND unit_core_brain_factory_test_brain_create)
set_tests_properties(unit_core_brain_factory_test_brain_create PROPERTIES LABELS "unit" TIMEOUT 300 RESOURCE_LOCK "brain_heavy")

# Test 2: brain_create_custom() function
add_executable(unit_core_brain_factory_test_brain_create_custom
    test_brain_create_custom.cpp
)
target_link_libraries(unit_core_brain_factory_test_brain_create_custom
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_core_brain_factory_test_brain_create_custom
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_core_brain_factory_test_brain_create_custom
         COMMAND unit_core_brain_factory_test_brain_create_custom)
set_tests_properties(unit_core_brain_factory_test_brain_create_custom PROPERTIES LABELS "unit" TIMEOUT 180 RESOURCE_LOCK "brain_heavy")

# Test 3: create_personality() helper function
add_executable(unit_core_brain_factory_test_personality
    test_personality.cpp
)
target_link_libraries(unit_core_brain_factory_test_personality
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_core_brain_factory_test_personality
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_core_brain_factory_test_personality
         COMMAND unit_core_brain_factory_test_personality)
set_tests_properties(unit_core_brain_factory_test_personality PROPERTIES LABELS "unit")

# Test 4: Coordinator init functions (6 coordinators)
add_executable(unit_core_brain_factory_init_coordinators
    init/test_brain_init_coordinators.cpp
)
target_link_libraries(unit_core_brain_factory_init_coordinators
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_core_brain_factory_init_coordinators
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_core_brain_factory_init_coordinators
         COMMAND unit_core_brain_factory_init_coordinators)
set_tests_properties(unit_core_brain_factory_init_coordinators PROPERTIES LABELS "unit")

# Test: Basal Ganglia Brain Init
add_executable(unit_core_brain_factory_init_basal_ganglia
    init/test_brain_init_basal_ganglia.cpp
)
target_link_libraries(unit_core_brain_factory_init_basal_ganglia
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_core_brain_factory_init_basal_ganglia
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_core_brain_factory_init_basal_ganglia
         COMMAND unit_core_brain_factory_init_basal_ganglia)
set_tests_properties(unit_core_brain_factory_init_basal_ganglia PROPERTIES LABELS "unit" RESOURCE_LOCK "brain_heavy")

# Test: Hypothalamus Brain Init
add_executable(unit_core_brain_factory_init_hypothalamus
    init/test_brain_init_hypothalamus.cpp
)
target_link_libraries(unit_core_brain_factory_init_hypothalamus
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_core_brain_factory_init_hypothalamus
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_core_brain_factory_init_hypothalamus
         COMMAND unit_core_brain_factory_init_hypothalamus)
set_tests_properties(unit_core_brain_factory_init_hypothalamus PROPERTIES LABELS "unit" RESOURCE_LOCK "brain_heavy")

# Test: Brain Tier Config (Opt 2, 3, 6 - enable/lazy flags)
add_executable(unit_core_brain_factory_test_brain_tier_config
    test_brain_tier_config.cpp
)
target_link_libraries(unit_core_brain_factory_test_brain_tier_config
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_core_brain_factory_test_brain_tier_config
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_core_brain_factory_test_brain_tier_config
         COMMAND unit_core_brain_factory_test_brain_tier_config)
set_tests_properties(unit_core_brain_factory_test_brain_tier_config PROPERTIES LABELS "unit" TIMEOUT 90)

# Test: BG BBB/KG Initialization Wiring
add_executable(unit_core_brain_factory_init_bg_bbb_kg
    init/test_bg_bbb_kg_initialization.cpp
)
target_link_libraries(unit_core_brain_factory_init_bg_bbb_kg
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_core_brain_factory_init_bg_bbb_kg
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_core_brain_factory_init_bg_bbb_kg
         COMMAND unit_core_brain_factory_init_bg_bbb_kg)
set_tests_properties(unit_core_brain_factory_init_bg_bbb_kg PROPERTIES LABELS "unit" RESOURCE_LOCK "brain_heavy")

# Test: Parallel Init (wave-based subsystem initialization)
add_executable(unit_core_brain_factory_test_parallel_init
    test_brain_parallel_init.cpp
)
target_link_libraries(unit_core_brain_factory_test_parallel_init
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_core_brain_factory_test_parallel_init
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_core_brain_factory_test_parallel_init
         COMMAND unit_core_brain_factory_test_parallel_init)
set_tests_properties(unit_core_brain_factory_test_parallel_init PROPERTIES
    LABELS "unit;brain;parallel_init"
    TIMEOUT 300
    RESOURCE_LOCK "brain_heavy")
