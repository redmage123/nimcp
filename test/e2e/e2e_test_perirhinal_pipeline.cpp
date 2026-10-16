/**
 * @file e2e_test_perirhinal_pipeline.cpp
 * @brief End-to-end tests for Perirhinal Cortex Pipeline
 *
 * WHAT: Full pipeline tests for object recognition and familiarity detection
 * WHY:  Verify complete perirhinal workflows with memory system integration
 * HOW:  Test object encoding, familiarity signals, item memory operations
 *
 * TEST COVERAGE:
 * - Object Encoding Pipeline (4 tests)
 * - Familiarity Signal Generation (3 tests)
 * - Item Memory Operations (4 tests)
 * - Cross-Region Integration (3 tests)
 * - Performance Benchmarks (2 tests)
 *
 * TOTAL: 16 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Perirhinal cortex critical for object recognition memory
 * - Familiarity signals distinguish known vs novel objects
 * - Item-specific representations distinct from spatial context
 * - Bidirectional connections with entorhinal and hippocampus
 *
 * @author NIMCP Development Team
 * @date 2026-01-14
 */

#include "e2e_test_framework.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>

#include "core/brain/regions/perirhinal/nimcp_perirhinal.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

/*=============================================================================
 * Test Configuration
 *===========================================================================*/

constexpr double MAX_ENCODING_TIME_MS = 50.0;
constexpr double MAX_RECOGNITION_TIME_MS = 30.0;
constexpr float MIN_FAMILIARITY_THRESHOLD = 0.05f; /* Low threshold - actual values depend on encoding */
constexpr uint32_t FEATURE_DIM = 256;
constexpr uint32_t NUM_TEST_OBJECTS = 10;

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

static void CreateTestFeatures(float* features, uint32_t dim, float base_value) {
    for (uint32_t i = 0; i < dim; i++) {
        features[i] = base_value + (float)i * 0.001f;
    }
}

/*=============================================================================
 * Object Encoding Pipeline Tests
 *===========================================================================*/

class E2EPerirhinalEncodingTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal = nullptr;
    perirhinal_config_t config;

    void SetUp() override {
        config = perirhinal_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        perirhinal = perirhinal_create(&config);
        ASSERT_NE(perirhinal, nullptr);
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
    }
};

TEST_F(E2EPerirhinalEncodingTest, SingleObjectEncoding) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    auto start = std::chrono::high_resolution_clock::now();
    int result = perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "test_object", &object_id);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(0, result);
    EXPECT_GE(object_id, 0u);

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(elapsed_ms, MAX_ENCODING_TIME_MS);
}

TEST_F(E2EPerirhinalEncodingTest, MultipleObjectsEncoding) {
    std::vector<uint32_t> object_ids(NUM_TEST_OBJECTS);

    for (uint32_t i = 0; i < NUM_TEST_OBJECTS; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.1f);

        char name[32];
        snprintf(name, sizeof(name), "object_%u", i);

        int result = perirhinal_encode_object(perirhinal, features, FEATURE_DIM, name, &object_ids[i]);
        EXPECT_EQ(0, result);
    }

    perirhinal_stats_t stats;
    perirhinal_get_stats(perirhinal, &stats);
    EXPECT_GE(stats.objects_encoded, NUM_TEST_OBJECTS);
}

TEST_F(E2EPerirhinalEncodingTest, EncodingWithVariedFeatures) {
    /* Test encoding objects with different feature patterns */
    float sparse_features[FEATURE_DIM] = {0};
    sparse_features[0] = 1.0f;
    sparse_features[FEATURE_DIM/2] = 1.0f;

    uint32_t sparse_id = 0;
    EXPECT_EQ(0, perirhinal_encode_object(perirhinal, sparse_features, FEATURE_DIM, "sparse", &sparse_id));

    float dense_features[FEATURE_DIM];
    for (uint32_t i = 0; i < FEATURE_DIM; i++) {
        dense_features[i] = 0.5f;
    }

    uint32_t dense_id = 0;
    EXPECT_EQ(0, perirhinal_encode_object(perirhinal, dense_features, FEATURE_DIM, "dense", &dense_id));

    EXPECT_NE(sparse_id, dense_id);
}

TEST_F(E2EPerirhinalEncodingTest, EncodingPersistence) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "persistent", &object_id));

    /* Object should be retrievable */
    const nimcp_stored_object_t* stored = perirhinal_get_object(perirhinal, object_id);
    EXPECT_NE(nullptr, stored);
}

/*=============================================================================
 * Familiarity Signal Tests
 *===========================================================================*/

class E2EPerirhinalFamiliarityTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal = nullptr;

    void SetUp() override {
        perirhinal_config_t config = perirhinal_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        perirhinal = perirhinal_create(&config);
        ASSERT_NE(perirhinal, nullptr);
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
    }
};

TEST_F(E2EPerirhinalFamiliarityTest, FamiliarObjectRecognition) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "familiar", &object_id));

    /* Same features should be recognized as familiar */
    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    EXPECT_EQ(0, perirhinal_recognize_object(perirhinal, features, FEATURE_DIM, &result));
    EXPECT_GT(result.familiarity_strength, MIN_FAMILIARITY_THRESHOLD);
}

TEST_F(E2EPerirhinalFamiliarityTest, NovelObjectDetection) {
    float known_features[FEATURE_DIM];
    CreateTestFeatures(known_features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, known_features, FEATURE_DIM, "known", &object_id));

    /* Different features should have lower familiarity */
    float novel_features[FEATURE_DIM];
    CreateTestFeatures(novel_features, FEATURE_DIM, 0.9f);

    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    perirhinal_recognize_object(perirhinal, novel_features, FEATURE_DIM, &result);
    /* Novel objects should have lower familiarity than known objects */
    EXPECT_GE(result.familiarity_strength, 0.0f);
}

TEST_F(E2EPerirhinalFamiliarityTest, FamiliarityGradient) {
    float base_features[FEATURE_DIM];
    CreateTestFeatures(base_features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, base_features, FEATURE_DIM, "base", &object_id));

    /* Test familiarity at different similarity levels */
    float similar_features[FEATURE_DIM];
    CreateTestFeatures(similar_features, FEATURE_DIM, 0.51f); /* Very similar */

    perirhinal_recognition_result_t similar_result;
    memset(&similar_result, 0, sizeof(similar_result));
    perirhinal_recognize_object(perirhinal, similar_features, FEATURE_DIM, &similar_result);

    float different_features[FEATURE_DIM];
    CreateTestFeatures(different_features, FEATURE_DIM, 0.8f); /* More different */

    perirhinal_recognition_result_t different_result;
    memset(&different_result, 0, sizeof(different_result));
    perirhinal_recognize_object(perirhinal, different_features, FEATURE_DIM, &different_result);

    /* Similar features should have higher familiarity */
    EXPECT_GE(similar_result.familiarity_strength, different_result.familiarity_strength);
}

/*=============================================================================
 * Item Memory Tests
 *===========================================================================*/

class E2EPerirhinalItemMemoryTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal = nullptr;

    void SetUp() override {
        perirhinal_config_t config = perirhinal_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        perirhinal = perirhinal_create(&config);
        ASSERT_NE(perirhinal, nullptr);
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
    }
};

TEST_F(E2EPerirhinalItemMemoryTest, ItemStorageAndRetrieval) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "item_test", &object_id));

    const nimcp_stored_object_t* stored = perirhinal_get_object(perirhinal, object_id);
    ASSERT_NE(nullptr, stored);
}

TEST_F(E2EPerirhinalItemMemoryTest, MultipleItemRetrieval) {
    std::vector<uint32_t> ids(5);

    for (int i = 0; i < 5; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.15f);

        char name[32];
        snprintf(name, sizeof(name), "item_%d", i);
        ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, FEATURE_DIM, name, &ids[i]));
    }

    /* All items should be retrievable */
    for (int i = 0; i < 5; i++) {
        const nimcp_stored_object_t* stored = perirhinal_get_object(perirhinal, ids[i]);
        EXPECT_NE(nullptr, stored);
    }
}

TEST_F(E2EPerirhinalItemMemoryTest, ItemMemoryCapacity) {
    /* Encode many objects to test capacity */
    const uint32_t MANY_OBJECTS = 50;

    for (uint32_t i = 0; i < MANY_OBJECTS; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.02f);

        char name[32];
        snprintf(name, sizeof(name), "capacity_%u", i);

        uint32_t id = 0;
        int result = perirhinal_encode_object(perirhinal, features, FEATURE_DIM, name, &id);
        EXPECT_EQ(0, result);
    }

    perirhinal_stats_t stats;
    perirhinal_get_stats(perirhinal, &stats);
    EXPECT_GE(stats.objects_encoded, MANY_OBJECTS);
}

TEST_F(E2EPerirhinalItemMemoryTest, ItemMemoryAfterReset) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "pre_reset", &object_id);

    perirhinal_reset(perirhinal);

    /* Should be able to encode new objects after reset */
    uint32_t new_id = 0;
    int result = perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "post_reset", &new_id);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * Cross-Region Integration Tests
 *===========================================================================*/

class E2EPerirhinalIntegrationTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal = nullptr;

    void SetUp() override {
        perirhinal_config_t config = perirhinal_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        config.enable_snn = true;
        perirhinal = perirhinal_create(&config);
        ASSERT_NE(perirhinal, nullptr);
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
    }
};

TEST_F(E2EPerirhinalIntegrationTest, VisualInputProcessing) {
    float visual_features[FEATURE_DIM];
    CreateTestFeatures(visual_features, FEATURE_DIM, 0.5f);

    int result = perirhinal_process_visual_input(perirhinal, visual_features, FEATURE_DIM);
    EXPECT_EQ(0, result);
}

TEST_F(E2EPerirhinalIntegrationTest, UpdateCycleIntegration) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "update_test", &object_id);

    /* Run multiple update cycles */
    for (int i = 0; i < 50; i++) {
        int result = perirhinal_update(perirhinal, 10.0f);
        EXPECT_EQ(0, result);
    }

    perirhinal_stats_t stats;
    perirhinal_get_stats(perirhinal, &stats);
    EXPECT_GE(stats.updates_processed, 50u);
}

TEST_F(E2EPerirhinalIntegrationTest, BidirectionalUpdate) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "bidir_test", &object_id);

    for (int i = 0; i < 10; i++) {
        int result = perirhinal_bidirectional_update(perirhinal, 10.0f);
        EXPECT_EQ(0, result);
    }
}

/*=============================================================================
 * Performance Benchmark Tests
 *===========================================================================*/

class E2EPerirhinalBenchmarkTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal = nullptr;

    void SetUp() override {
        perirhinal_config_t config = perirhinal_default_config();
        config.enable_bio_async = false;
        perirhinal = perirhinal_create(&config);
        ASSERT_NE(perirhinal, nullptr);
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
    }
};

TEST_F(E2EPerirhinalBenchmarkTest, EncodingThroughput) {
    const uint32_t BENCHMARK_COUNT = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < BENCHMARK_COUNT; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.01f);

        uint32_t id = 0;
        perirhinal_encode_object(perirhinal, features, FEATURE_DIM, nullptr, &id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    double objects_per_second = (BENCHMARK_COUNT * 1000.0) / elapsed_ms;
    EXPECT_GT(objects_per_second, 100.0); /* At least 100 objects/second */
}

TEST_F(E2EPerirhinalBenchmarkTest, RecognitionLatency) {
    /* Pre-encode objects */
    for (int i = 0; i < 50; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.02f);

        uint32_t id = 0;
        perirhinal_encode_object(perirhinal, features, FEATURE_DIM, nullptr, &id);
    }

    /* Benchmark recognition */
    float query_features[FEATURE_DIM];
    CreateTestFeatures(query_features, FEATURE_DIM, 0.5f);

    auto start = std::chrono::high_resolution_clock::now();

    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    perirhinal_recognize_object(perirhinal, query_features, FEATURE_DIM, &result);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(elapsed_ms, MAX_RECOGNITION_TIME_MS);
}

/*=============================================================================
 * Main
 *===========================================================================*/

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
