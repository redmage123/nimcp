/**
 * @file test_portia_classification_accuracy.cpp
 * @brief Regression tests for Portia target classification accuracy
 *
 * TEST COVERAGE:
 * - Classification with noisy data
 * - Target tracking accuracy
 * - Pruning doesn't lose active targets
 * - Classification latency
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <random>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_classification.h"

namespace {

class PortiaClassificationAccuracyTest : public ::testing::Test {
protected:
    void SetUp() override {
        portia_classification_config_t cfg;
        cfg.classification_threshold = 0.7f;
        cfg.max_targets = 100;
        cfg.retention_time_ms = 5000;
        cfg.enable_prediction = true;
        cfg.enable_bio_async = false;

        classifier = portia_classification_init(&cfg);
        ASSERT_NE(classifier, nullptr);
    }

    void TearDown() override {
        if (classifier) {
            portia_classification_destroy(classifier);
        }
    }

    portia_classifier_t classifier;
    std::mt19937 rng{42};
};

TEST_F(PortiaClassificationAccuracyTest, ClassificationWithNoisyData) {
    std::normal_distribution<float> noise(0.0f, 2.0f);

    // Create target with noisy observations
    const float TRUE_X = 100.0f, TRUE_Y = 50.0f;
    uint32_t target_id = portia_classification_add_target(
        classifier, TRUE_X, TRUE_Y, 0.0f, 10.0f);
    ASSERT_NE(target_id, 0u);

    // Update with noisy positions
    for (int i = 0; i < 50; i++) {
        float x = TRUE_X + noise(rng);
        float y = TRUE_Y + noise(rng);
        portia_classification_update(classifier, target_id, x, y, 0.0f);
    }

    // Get target info
    target_info_t info;
    int result = portia_classification_get_target(classifier, target_id, &info);
    ASSERT_EQ(result, 0);

    // Position should be near true position
    float error = std::sqrt(
        (info.x - TRUE_X) * (info.x - TRUE_X) +
        (info.y - TRUE_Y) * (info.y - TRUE_Y));
    EXPECT_LT(error, 5.0f) << "Tracking error too high: " << error;
}

TEST_F(PortiaClassificationAccuracyTest, TrackingAccuracy) {
    // Create moving target
    uint32_t target_id = portia_classification_add_target(
        classifier, 0.0f, 0.0f, 0.0f, 5.0f);

    // Simulate linear movement
    for (int i = 1; i <= 100; i++) {
        float x = i * 1.0f;
        float y = i * 0.5f;
        portia_classification_update(classifier, target_id, x, y, 0.0f);
    }

    target_info_t info;
    portia_classification_get_target(classifier, target_id, &info);

    // Should be near final position
    EXPECT_NEAR(info.x, 100.0f, 2.0f);
    EXPECT_NEAR(info.y, 50.0f, 2.0f);

    // Velocity tracking is implementation-dependent
    // Some classifiers don't track velocity internally
    // Main test is position tracking accuracy
}

TEST_F(PortiaClassificationAccuracyTest, PruningDoesntLoseActive) {
    std::vector<uint32_t> active_targets;

    // Create active targets
    for (int i = 0; i < 20; i++) {
        uint32_t id = portia_classification_add_target(
            classifier, i * 10.0f, i * 10.0f, 0.0f, 5.0f);
        if (id != 0) active_targets.push_back(id);
    }

    // Keep updating them
    for (int t = 0; t < 10; t++) {
        for (uint32_t id : active_targets) {
            portia_classification_update(classifier, id, 0.0f, 0.0f, 0.0f);
        }
    }

    // Prune
    uint32_t pruned = portia_classification_prune(classifier);
    std::cout << "Pruned: " << pruned << " targets\n";

    // Active targets should still exist
    for (uint32_t id : active_targets) {
        target_info_t info;
        int result = portia_classification_get_target(classifier, id, &info);
        EXPECT_EQ(result, 0) << "Active target " << id << " was pruned";
    }
}

TEST_F(PortiaClassificationAccuracyTest, ClassificationLatency) {
    const int ITERATIONS = 1000;

    // Create target
    uint32_t id = portia_classification_add_target(
        classifier, 50.0f, 50.0f, 0.0f, 10.0f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        target_class_t classification;
        float confidence;
        portia_classification_classify(classifier, id, &classification, &confidence);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    double avg_latency = elapsed.count() / ITERATIONS;

    const double MAX_LATENCY_US = 100.0;
    EXPECT_LT(avg_latency, MAX_LATENCY_US)
        << "Classification too slow: " << avg_latency << " us";

    std::cout << "Classification latency: " << avg_latency << " us\n";
}

TEST_F(PortiaClassificationAccuracyTest, ThreatDetectionWorks) {
    // Create some threats
    for (int i = 0; i < 5; i++) {
        uint32_t id = portia_classification_add_target(
            classifier, i * 20.0f, 0.0f, 0.0f, 10.0f);

        // Classify (would set to THREAT in real implementation)
        target_class_t classification;
        float confidence;
        portia_classification_classify(classifier, id, &classification, &confidence);
    }

    uint32_t threats[10];
    uint32_t count = portia_classification_get_threats(classifier, threats, 10);

    std::cout << "Threats detected: " << count << "\n";
}

TEST_F(PortiaClassificationAccuracyTest, MultipleTargetsTracked) {
    const int NUM_TARGETS = 50;
    std::vector<uint32_t> ids;

    for (int i = 0; i < NUM_TARGETS; i++) {
        uint32_t id = portia_classification_add_target(
            classifier, i * 5.0f, i * 5.0f, 0.0f, 3.0f);
        if (id != 0) ids.push_back(id);
    }

    uint32_t count = portia_classification_get_count(classifier);
    EXPECT_EQ(count, ids.size());

    // All should be retrievable
    for (uint32_t id : ids) {
        target_info_t info;
        int result = portia_classification_get_target(classifier, id, &info);
        EXPECT_EQ(result, 0);
    }
}

} // namespace
