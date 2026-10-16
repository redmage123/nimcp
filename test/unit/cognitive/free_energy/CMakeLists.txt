# ============================================================================
# Free Energy Principle Enhancement Unit Tests
# ============================================================================
# WHAT: Build configuration for FEP enhancement module unit tests
# WHY:  Each FEP enhancement module needs independent unit testing
# HOW:  Create separate test executable for each module with GTest linkage

# Unit test for FEP Learning
add_executable(unit_fep_learning test_fep_learning.cpp)
target_link_libraries(unit_fep_learning PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_learning PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_learning PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_learning PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_learning
    PROPERTIES LABELS "unit;cognitive;free_energy;learning"
)

# Unit test for FEP Curiosity
add_executable(unit_fep_curiosity test_fep_curiosity.cpp)
target_link_libraries(unit_fep_curiosity PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_curiosity PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_curiosity PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_curiosity PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_curiosity
    PROPERTIES LABELS "unit;cognitive;free_energy;curiosity"
)

# Unit test for FEP Neuromodulation
add_executable(unit_fep_neuromod test_fep_neuromod.cpp)
target_link_libraries(unit_fep_neuromod PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_neuromod PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_neuromod PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_neuromod PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_neuromod
    PROPERTIES LABELS "unit;cognitive;free_energy;neuromodulation"
)

# Unit test for FEP Immune Bridge
add_executable(unit_fep_immune_bridge test_fep_immune_bridge.cpp)
target_link_libraries(unit_fep_immune_bridge PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_immune_bridge PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_immune_bridge PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_immune_bridge PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_immune_bridge
    PROPERTIES LABELS "unit;cognitive;free_energy;immune"
)

# Unit test for FEP Consciousness
add_executable(unit_fep_consciousness test_fep_consciousness.cpp)
target_link_libraries(unit_fep_consciousness PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_consciousness PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_consciousness PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_consciousness PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_consciousness
    PROPERTIES LABELS "unit;cognitive;free_energy;consciousness"
)

# Unit test for FEP Planning
add_executable(unit_fep_planning test_fep_planning.cpp)
target_link_libraries(unit_fep_planning PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_planning PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_planning PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_planning PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_planning
    PROPERTIES LABELS "unit;cognitive;free_energy;planning"
)

# Unit test for FEP Evidence
add_executable(unit_fep_evidence test_fep_evidence.cpp)
target_link_libraries(unit_fep_evidence PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_evidence PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_evidence PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_evidence PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_evidence
    PROPERTIES LABELS "unit;cognitive;free_energy;evidence"
)

# Unit test for FEP Context
add_executable(unit_fep_context test_fep_context.cpp)
target_link_libraries(unit_fep_context PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_context PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_context PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_context PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_context
    PROPERTIES LABELS "unit;cognitive;free_energy;context"
)

# Unit test for FEP Sleep
add_executable(unit_fep_sleep test_fep_sleep.cpp)
target_link_libraries(unit_fep_sleep PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_sleep PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_sleep PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_sleep PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_sleep
    PROPERTIES LABELS "unit;cognitive;free_energy;sleep"
)

# Unit test for FEP Orchestrator
add_executable(unit_fep_orchestrator test_fep_orchestrator.cpp)
target_link_libraries(unit_fep_orchestrator PRIVATE
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
target_include_directories(unit_fep_orchestrator PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(unit_fep_orchestrator PRIVATE cxx_std_17)
target_compile_definitions(unit_fep_orchestrator PRIVATE NIMCP_TESTING)
gtest_discover_tests(unit_fep_orchestrator
    PROPERTIES LABELS "unit;cognitive;free_energy;orchestrator"
)
