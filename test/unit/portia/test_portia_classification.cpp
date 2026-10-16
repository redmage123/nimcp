/**
 * @file test_portia_classification.cpp
 * @brief Unit tests for Portia Target Classification System
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_classification.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"

class PortiaClassificationTest : public ::testing::Test {
protected:
    portia_classifier_t classifier;
    portia_classification_config_t config;

    void SetUp() override {
        // Initialize configuration
        config.classification_threshold = 0.5f;
        config.max_targets = 100;
        config.retention_time_ms = 5000;
        config.enable_prediction = true;
        config.enable_bio_async = false;  // Disabled for unit tests

        // Create classifier
        classifier = portia_classification_init(&config);
        ASSERT_NE(classifier, nullptr) << "Failed to create classifier";
    }

    void TearDown() override {
        if (classifier) {
            portia_classification_destroy(classifier);
            classifier = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(PortiaClassificationTest, InitializeWithValidConfig) {
    // Classifier already initialized in SetUp
    EXPECT_NE(classifier, nullptr);
    EXPECT_EQ(portia_classification_get_count(classifier), 0);
}

TEST_F(PortiaClassificationTest, InitializeWithInvalidConfig) {
    portia_classification_config_t bad_config = config;
    bad_config.max_targets = 0;  // Invalid

    portia_classifier_t bad = portia_classification_init(&bad_config);
    EXPECT_EQ(bad, nullptr);
}

TEST_F(PortiaClassificationTest, InitializeWithNullConfig) {
    portia_classifier_t bad = portia_classification_init(nullptr);
    EXPECT_EQ(bad, nullptr);
}

TEST_F(PortiaClassificationTest, DestroyNullClassifier) {
    // Should not crash
    portia_classification_destroy(nullptr);
}

//=============================================================================
// Target Management Tests
//=============================================================================

TEST_F(PortiaClassificationTest, AddSingleTarget) {
    uint32_t id = portia_classification_add_target(classifier, 1.0f, 2.0f, 3.0f, 0.5f);
    EXPECT_GT(id, 0);
    EXPECT_EQ(portia_classification_get_count(classifier), 1);
}

TEST_F(PortiaClassificationTest, AddMultipleTargets) {
    uint32_t id1 = portia_classification_add_target(classifier, 1.0f, 1.0f, 1.0f, 0.5f);
    uint32_t id2 = portia_classification_add_target(classifier, 2.0f, 2.0f, 2.0f, 0.6f);
    uint32_t id3 = portia_classification_add_target(classifier, 3.0f, 3.0f, 3.0f, 0.7f);

    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_GT(id3, 0);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_EQ(portia_classification_get_count(classifier), 3);
}

TEST_F(PortiaClassificationTest, AddTargetToFullRegistry) {
    // Fill registry
    for (uint32_t i = 0; i < config.max_targets; i++) {
        uint32_t id = portia_classification_add_target(
            classifier, (float)i, (float)i, (float)i, 0.5f);
        EXPECT_GT(id, 0);
    }

    EXPECT_EQ(portia_classification_get_count(classifier), config.max_targets);

    // Try to add one more
    uint32_t overflow_id = portia_classification_add_target(
        classifier, 100.0f, 100.0f, 100.0f, 0.5f);
    EXPECT_EQ(overflow_id, 0);
}

TEST_F(PortiaClassificationTest, AddTargetWithInvalidPosition) {
    uint32_t id = portia_classification_add_target(
        classifier, NAN, 2.0f, 3.0f, 0.5f);
    EXPECT_EQ(id, 0);
}

TEST_F(PortiaClassificationTest, UpdateTarget) {
    uint32_t id = portia_classification_add_target(classifier, 0.0f, 0.0f, 0.0f, 0.5f);
    ASSERT_GT(id, 0);

    // Wait a bit for time delta
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Update position
    int result = portia_classification_update(classifier, id, 1.0f, 1.0f, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Get target and check velocity was computed
    target_info_t info;
    result = portia_classification_get_target(classifier, id, &info);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(info.x, 1.0f);
    EXPECT_FLOAT_EQ(info.y, 1.0f);
    EXPECT_FLOAT_EQ(info.z, 1.0f);
    EXPECT_EQ(info.observation_count, 2);
}

TEST_F(PortiaClassificationTest, UpdateNonexistentTarget) {
    int result = portia_classification_update(classifier, 999, 1.0f, 1.0f, 1.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(PortiaClassificationTest, GetTarget) {
    uint32_t id = portia_classification_add_target(classifier, 1.0f, 2.0f, 3.0f, 0.8f);
    ASSERT_GT(id, 0);

    target_info_t info;
    int result = portia_classification_get_target(classifier, id, &info);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(info.id, id);
    EXPECT_FLOAT_EQ(info.x, 1.0f);
    EXPECT_FLOAT_EQ(info.y, 2.0f);
    EXPECT_FLOAT_EQ(info.z, 3.0f);
    EXPECT_FLOAT_EQ(info.size, 0.8f);
    EXPECT_TRUE(info.active);
}

TEST_F(PortiaClassificationTest, GetNonexistentTarget) {
    target_info_t info;
    int result = portia_classification_get_target(classifier, 999, &info);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
}

//=============================================================================
// Classification Tests
//=============================================================================

TEST_F(PortiaClassificationTest, ClassifyNewTarget) {
    uint32_t id = portia_classification_add_target(classifier, 0.0f, 0.0f, 0.0f, 0.5f);
    ASSERT_GT(id, 0);

    target_class_t classification;
    float confidence;
    int result = portia_classification_classify(classifier, id, &classification, &confidence);

    ASSERT_EQ(result, NIMCP_SUCCESS);
    // New target with few observations should be UNKNOWN
    EXPECT_EQ(classification, TARGET_CLASS_UNKNOWN);
    EXPECT_LT(confidence, 0.5f);
}

TEST_F(PortiaClassificationTest, ClassifyFastMovingSmallTarget) {
    // Create target
    uint32_t id = portia_classification_add_target(classifier, 0.0f, 0.0f, 0.0f, 0.3f);
    ASSERT_GT(id, 0);

    // Simulate fast movement over multiple updates
    for (int i = 1; i <= 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        portia_classification_update(classifier, id,
            (float)i * 1.0f, (float)i * 1.0f, 0.0f);
    }

    // Classify - should be PREY (small, fast)
    target_class_t classification;
    float confidence;
    int result = portia_classification_classify(classifier, id, &classification, &confidence);

    ASSERT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(classification, TARGET_CLASS_PREY);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(PortiaClassificationTest, ClassifyFastMovingLargeTarget) {
    // Create large target
    uint32_t id = portia_classification_add_target(classifier, 0.0f, 0.0f, 0.0f, 2.0f);
    ASSERT_GT(id, 0);

    // Simulate fast movement
    for (int i = 1; i <= 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        portia_classification_update(classifier, id,
            (float)i * 1.0f, (float)i * 1.0f, 0.0f);
    }

    // Classify - should be THREAT (large, fast)
    target_class_t classification;
    float confidence;
    int result = portia_classification_classify(classifier, id, &classification, &confidence);

    ASSERT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(classification, TARGET_CLASS_THREAT);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(PortiaClassificationTest, ClassifyStationarySmallTarget) {
    // Create small target
    uint32_t id = portia_classification_add_target(classifier, 0.0f, 0.0f, 0.0f, 0.3f);
    ASSERT_GT(id, 0);

    // Update position (barely moving)
    for (int i = 1; i <= 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        portia_classification_update(classifier, id, 0.01f, 0.01f, 0.0f);
    }

    // Classify - should be NEUTRAL (small, stationary)
    target_class_t classification;
    float confidence;
    int result = portia_classification_classify(classifier, id, &classification, &confidence);

    ASSERT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(classification, TARGET_CLASS_NEUTRAL);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(PortiaClassificationTest, ClassifyStationaryLargeTarget) {
    // Create large target
    uint32_t id = portia_classification_add_target(classifier, 0.0f, 0.0f, 0.0f, 2.0f);
    ASSERT_GT(id, 0);

    // Update position (stationary)
    for (int i = 1; i <= 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        portia_classification_update(classifier, id, 0.0f, 0.0f, 0.0f);
    }

    // Classify - should be OBSTACLE (large, stationary)
    target_class_t classification;
    float confidence;
    int result = portia_classification_classify(classifier, id, &classification, &confidence);

    ASSERT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(classification, TARGET_CLASS_OBSTACLE);
    EXPECT_GT(confidence, 0.5f);
}

//=============================================================================
// Threat Assessment Tests
//=============================================================================

TEST_F(PortiaClassificationTest, GetThreatsEmpty) {
    uint32_t threats[10];
    uint32_t count = portia_classification_get_threats(classifier, threats, 10);
    EXPECT_EQ(count, 0);
}

TEST_F(PortiaClassificationTest, GetThreatsWithMultipleTargets) {
    // Add threat target
    uint32_t threat_id = portia_classification_add_target(classifier, 0.0f, 0.0f, 0.0f, 2.0f);
    ASSERT_GT(threat_id, 0);

    // Make it move fast
    for (int i = 1; i <= 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        portia_classification_update(classifier, threat_id,
            (float)i * 1.0f, (float)i * 1.0f, 0.0f);
    }

    // Classify as threat
    target_class_t classification;
    float confidence;
    portia_classification_classify(classifier, threat_id, &classification, &confidence);

    // Add neutral target
    uint32_t neutral_id = portia_classification_add_target(classifier, 5.0f, 5.0f, 0.0f, 0.2f);
    ASSERT_GT(neutral_id, 0);

    // Get threats
    uint32_t threats[10];
    uint32_t count = portia_classification_get_threats(classifier, threats, 10);

    EXPECT_EQ(count, 1);
    EXPECT_EQ(threats[0], threat_id);
}

//=============================================================================
// Pruning Tests
//=============================================================================

TEST_F(PortiaClassificationTest, PruneNoStaleTargets) {
    // Add target
    uint32_t id = portia_classification_add_target(classifier, 0.0f, 0.0f, 0.0f, 0.5f);
    ASSERT_GT(id, 0);

    // Prune immediately - should not remove
    uint32_t pruned = portia_classification_prune(classifier);
    EXPECT_EQ(pruned, 0);
    EXPECT_EQ(portia_classification_get_count(classifier), 1);
}

TEST_F(PortiaClassificationTest, PruneStaleTargets) {
    // Create classifier with short retention
    portia_classification_config_t short_config = config;
    short_config.retention_time_ms = 100;  // 100ms retention
    portia_classifier_t short_classifier = portia_classification_init(&short_config);
    ASSERT_NE(short_classifier, nullptr);

    // Add target
    uint32_t id = portia_classification_add_target(short_classifier, 0.0f, 0.0f, 0.0f, 0.5f);
    ASSERT_GT(id, 0);

    // Wait for retention time
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Prune - should remove stale target
    uint32_t pruned = portia_classification_prune(short_classifier);
    EXPECT_EQ(pruned, 1);
    EXPECT_EQ(portia_classification_get_count(short_classifier), 0);

    portia_classification_destroy(short_classifier);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(PortiaClassificationTest, ConcurrentAddTargets) {
    const int num_threads = 4;
    const int targets_per_thread = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, targets_per_thread]() {
            for (int i = 0; i < targets_per_thread; i++) {
                float pos = (float)(t * targets_per_thread + i);
                uint32_t id = portia_classification_add_target(
                    classifier, pos, pos, pos, 0.5f);
                EXPECT_GT(id, 0);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(portia_classification_get_count(classifier),
              num_threads * targets_per_thread);
}

TEST_F(PortiaClassificationTest, ConcurrentUpdateAndClassify) {
    // Add targets
    std::vector<uint32_t> target_ids;
    for (int i = 0; i < 10; i++) {
        uint32_t id = portia_classification_add_target(
            classifier, (float)i, (float)i, 0.0f, 0.5f);
        ASSERT_GT(id, 0);
        target_ids.push_back(id);
    }

    // Concurrent updates and classifications
    std::vector<std::thread> threads;

    // Update thread
    threads.emplace_back([this, &target_ids]() {
        for (int iter = 0; iter < 10; iter++) {
            for (uint32_t id : target_ids) {
                portia_classification_update(
                    classifier, id, (float)iter, (float)iter, 0.0f);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Classify thread
    threads.emplace_back([this, &target_ids]() {
        for (int iter = 0; iter < 10; iter++) {
            for (uint32_t id : target_ids) {
                target_class_t classification;
                float confidence;
                portia_classification_classify(classifier, id, &classification, &confidence);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    for (auto& thread : threads) {
        thread.join();
    }

    // All targets should still be active
    EXPECT_EQ(portia_classification_get_count(classifier), target_ids.size());
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PortiaClassificationTest, ClassifyNonexistentTarget) {
    target_class_t classification;
    float confidence;
    int result = portia_classification_classify(classifier, 999, &classification, &confidence);
    EXPECT_EQ(result, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(PortiaClassificationTest, GetCountWithNullClassifier) {
    uint32_t count = portia_classification_get_count(nullptr);
    EXPECT_EQ(count, 0);
}

TEST_F(PortiaClassificationTest, AddTargetWithNullClassifier) {
    uint32_t id = portia_classification_add_target(nullptr, 0.0f, 0.0f, 0.0f, 0.5f);
    EXPECT_EQ(id, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
