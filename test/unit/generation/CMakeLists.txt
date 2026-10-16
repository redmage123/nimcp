# =============================================================================
# NIMCP Generation Unit Tests
# =============================================================================
# WHAT: Unit tests for tokenizer, embedding, and language generator
# WHY:  These components form the language generation pipeline — each must be
#       individually verified before integration testing
# HOW:  GTest executables linked against nimcp library
# =============================================================================

# =============================================================================
# Tokenizer Unit Tests
# =============================================================================

add_executable(unit_generation_test_tokenizer
    test_tokenizer.cpp
)

target_link_libraries(unit_generation_test_tokenizer
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_generation_test_tokenizer
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_generation_test_tokenizer PRIVATE cxx_std_17)

gtest_discover_tests(unit_generation_test_tokenizer
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;generation;tokenizer"
        TIMEOUT 120
)

# =============================================================================
# Embedding Unit Tests
# =============================================================================

add_executable(unit_generation_test_embedding
    test_embedding.cpp
)

target_link_libraries(unit_generation_test_embedding
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_generation_test_embedding
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_generation_test_embedding PRIVATE cxx_std_17)

gtest_discover_tests(unit_generation_test_embedding
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;generation;embedding"
        TIMEOUT 120
)

# =============================================================================
# Language Generator Unit Tests
# =============================================================================

add_executable(unit_generation_test_language_generator
    test_language_generator.cpp
)

target_link_libraries(unit_generation_test_language_generator
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_generation_test_language_generator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_generation_test_language_generator PRIVATE cxx_std_17)

gtest_discover_tests(unit_generation_test_language_generator
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;generation;language_generator"
        TIMEOUT 180
)

# =============================================================================
# Embedding Thread Safety Unit Tests (Bug H20)
# =============================================================================

add_executable(unit_generation_test_embedding_thread_safety
    test_embedding_thread_safety.cpp
)

target_link_libraries(unit_generation_test_embedding_thread_safety
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_include_directories(unit_generation_test_embedding_thread_safety
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

target_compile_features(unit_generation_test_embedding_thread_safety PRIVATE cxx_std_17)

gtest_discover_tests(unit_generation_test_embedding_thread_safety
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;generation;embedding;thread_safety"
        TIMEOUT 120
)
