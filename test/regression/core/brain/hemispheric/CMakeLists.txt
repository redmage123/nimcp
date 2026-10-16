# Hemispheric Brain Regression Tests
# Tests for hemispheric brain, corpus callosum, lateralization, and contralateral mapping

# Hemispheric Brain Regression Test
add_executable(regression_hemispheric_brain_regression
    test_hemispheric_brain_regression.cpp
)
target_link_libraries(regression_hemispheric_brain_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(regression_hemispheric_brain_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)
add_test(NAME regression_hemispheric_brain_regression
         COMMAND regression_hemispheric_brain_regression)
set_tests_properties(regression_hemispheric_brain_regression
    PROPERTIES
        LABELS "regression"
        TIMEOUT 600
        RESOURCE_LOCK "brain_heavy"
)

# Corpus Callosum Regression Test
add_executable(regression_corpus_callosum_regression
    test_corpus_callosum_regression.cpp
)
target_link_libraries(regression_corpus_callosum_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(regression_corpus_callosum_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)
add_test(NAME regression_corpus_callosum_regression
         COMMAND regression_corpus_callosum_regression)
set_tests_properties(regression_corpus_callosum_regression
    PROPERTIES
        LABELS "regression"
        TIMEOUT 300
        RESOURCE_LOCK "brain_heavy"
)

# Lateralization Regression Test
add_executable(regression_lateralization_regression
    test_lateralization_regression.cpp
)
target_link_libraries(regression_lateralization_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(regression_lateralization_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)
add_test(NAME regression_lateralization_regression
         COMMAND regression_lateralization_regression)
set_tests_properties(regression_lateralization_regression
    PROPERTIES
        LABELS "regression"
        TIMEOUT 180
)

# Contralateral Mapping Regression Test
add_executable(regression_contralateral_mapping_regression
    test_contralateral_mapping_regression.cpp
)
target_link_libraries(regression_contralateral_mapping_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(regression_contralateral_mapping_regression
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)
add_test(NAME regression_contralateral_mapping_regression
         COMMAND regression_contralateral_mapping_regression)
set_tests_properties(regression_contralateral_mapping_regression
    PROPERTIES
        LABELS "regression"
        TIMEOUT 180
)

# Hemispheric Memory Optimization Regression Test (Opt 5+7)
add_executable(regression_hemispheric_memory_opt
    test_hemispheric_memory_opt_regression.cpp
)
target_link_libraries(regression_hemispheric_memory_opt
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(regression_hemispheric_memory_opt
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)
add_test(NAME regression_hemispheric_memory_opt
         COMMAND regression_hemispheric_memory_opt)
set_tests_properties(regression_hemispheric_memory_opt
    PROPERTIES
        LABELS "regression"
        TIMEOUT 600
        RESOURCE_LOCK "brain_heavy"
)
