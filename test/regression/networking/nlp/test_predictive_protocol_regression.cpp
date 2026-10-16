/**
 * @file test_predictive_protocol_regression.cpp
 * @brief Regression tests for Predictive Communication Protocol
 *
 * WHAT: Accuracy benchmarks and performance regression tests
 * WHY:  Ensure prediction quality doesn't degrade over time
 * HOW:  Track prediction accuracy, latency, and resource usage
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_predictive_protocol.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PredictiveProtocolRegressionTest : public ::testing::Test {
protected:
    predictive_protocol_t* protocol;

    void SetUp() override {
        predictive_protocol_config_t config;
        config.prediction_window_ms = 1000;
        config.history_buffer_size = 2048;
        config.confidence_threshold = 0.5f;
        config.enable_prefetch = true;
        config.enable_bio_async = false;

        protocol = predictive_protocol_create(&config);
        ASSERT_NE(protocol, nullptr);
    }

    void TearDown() override {
        if (protocol) {
            predictive_protocol_destroy(protocol);
            protocol = nullptr;
        }
    }

    /* Helper: get timestamp */
    uint64_t GetTimestamp() {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch());
        return ms.count();
    }

    /* Helper: train on pattern */
    void TrainPattern(const std::vector<uint32_t>& pattern, int repetitions) {
        for (int i = 0; i < repetitions; i++) {
            uint64_t base_time = GetTimestamp();

            for (size_t j = 0; j < pattern.size(); j++) {
                predictive_protocol_observe_message(
                    protocol,
                    pattern[j],
                    BIO_MODULE_BRAIN,
                    BIO_MODULE_ATTENTION,
                    base_time + j * 50);
            }
        }
    }

    /* Helper: measure prediction accuracy */
    float MeasurePredictionAccuracy(
        const std::vector<uint32_t>& test_sequence,
        int trials)
    {
        int correct = 0;
        int total = 0;

        for (int trial = 0; trial < trials; trial++) {
            for (size_t i = 0; i < test_sequence.size() - 1; i++) {
                predicted_message_t pred;
                int ret = predictive_protocol_predict_next(
                    protocol,
                    test_sequence[i],
                    &pred);

                if (ret == 0) {
                    total++;
                    if (pred.message_type == test_sequence[i + 1]) {
                        correct++;
                    }
                }
            }
        }

        if (total == 0) {
            return 0.0f;
        }

        return (float)correct / (float)total;
    }
};

//=============================================================================
// Accuracy Regression Tests
//=============================================================================

TEST_F(PredictiveProtocolRegressionTest, SimplePatternAccuracy) {
    /* Baseline: simple 2-step pattern should achieve >80% accuracy */
    std::vector<uint32_t> pattern = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    TrainPattern(pattern, 50);

    float accuracy = MeasurePredictionAccuracy(pattern, 20);

    EXPECT_GE(accuracy, 0.80f) << "Simple pattern accuracy degraded";
    EXPECT_LE(accuracy, 1.0f);
}

TEST_F(PredictiveProtocolRegressionTest, ComplexPatternAccuracy) {
    /* Complex multi-step pattern should achieve >60% accuracy */
    std::vector<uint32_t> pattern = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_VISUAL_FEATURE_DETECTED,
        BIO_MSG_SALIENCE_QUERY,
        BIO_MSG_SALIENCE_RESPONSE,
        BIO_MSG_ATTENTION_SHIFT,
        BIO_MSG_WORKING_MEMORY_STORE
    };

    TrainPattern(pattern, 30);

    float accuracy = MeasurePredictionAccuracy(pattern, 10);

    EXPECT_GE(accuracy, 0.60f) << "Complex pattern accuracy degraded";
    EXPECT_LE(accuracy, 1.0f);
}

TEST_F(PredictiveProtocolRegressionTest, VariablePatternAccuracy) {
    /* Pattern with variation should achieve >50% accuracy */
    std::vector<uint32_t> pattern1 = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT,
        BIO_MSG_WORKING_MEMORY_STORE
    };

    std::vector<uint32_t> pattern2 = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT,
        BIO_MSG_BRAIN_STATE_QUERY
    };

    TrainPattern(pattern1, 25);
    TrainPattern(pattern2, 25);

    float acc1 = MeasurePredictionAccuracy(pattern1, 10);
    float acc2 = MeasurePredictionAccuracy(pattern2, 10);
    float avg_accuracy = (acc1 + acc2) / 2.0f;

    EXPECT_GE(avg_accuracy, 0.50f) << "Variable pattern accuracy degraded";
}

TEST_F(PredictiveProtocolRegressionTest, LongSequenceAccuracy) {
    /* Long sequence prediction should degrade gracefully */
    std::vector<uint32_t> pattern = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_VISUAL_FEATURE_DETECTED,
        BIO_MSG_SALIENCE_QUERY,
        BIO_MSG_SALIENCE_RESPONSE,
        BIO_MSG_ATTENTION_SHIFT,
        BIO_MSG_WORKING_MEMORY_STORE,
        BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MSG_BRAIN_STATE_RESPONSE,
        BIO_MSG_NEURON_ACTIVATION_REQUEST,
        BIO_MSG_NEURON_ACTIVATION_RESPONSE
    };

    TrainPattern(pattern, 20);

    /* Measure accuracy for early vs late predictions */
    std::vector<uint32_t> early = {pattern[0], pattern[1], pattern[2]};
    std::vector<uint32_t> late = {pattern[7], pattern[8], pattern[9]};

    float early_acc = MeasurePredictionAccuracy(early, 10);
    float late_acc = MeasurePredictionAccuracy(late, 10);

    /* Early predictions should be better than late */
    EXPECT_GT(early_acc, 0.5f);

    /* But both should be above minimum threshold */
    EXPECT_GE(late_acc, 0.3f) << "Late sequence predictions too poor";
}

//=============================================================================
// Confidence Calibration Tests
//=============================================================================

TEST_F(PredictiveProtocolRegressionTest, ConfidenceCalibration) {
    /* High confidence predictions should be more accurate */
    std::vector<uint32_t> pattern = {
        BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MSG_BRAIN_STATE_RESPONSE
    };

    TrainPattern(pattern, 100); /* Heavy training */

    /* Collect predictions with confidence scores */
    std::vector<float> high_conf_accuracy;
    std::vector<float> low_conf_accuracy;

    for (int trial = 0; trial < 50; trial++) {
        predicted_message_t pred;
        int ret = predictive_protocol_predict_next(
            protocol,
            BIO_MSG_BRAIN_STATE_QUERY,
            &pred);

        if (ret == 0) {
            bool correct = (pred.message_type == BIO_MSG_BRAIN_STATE_RESPONSE);

            if (pred.confidence > 0.8f) {
                high_conf_accuracy.push_back(correct ? 1.0f : 0.0f);
            } else if (pred.confidence < 0.6f) {
                low_conf_accuracy.push_back(correct ? 1.0f : 0.0f);
            }
        }
    }

    /* Calculate average accuracy for each confidence level */
    if (!high_conf_accuracy.empty() && !low_conf_accuracy.empty()) {
        float high_avg = std::accumulate(
            high_conf_accuracy.begin(),
            high_conf_accuracy.end(),
            0.0f) / high_conf_accuracy.size();

        float low_avg = std::accumulate(
            low_conf_accuracy.begin(),
            low_conf_accuracy.end(),
            0.0f) / low_conf_accuracy.size();

        /* High confidence should be more accurate */
        EXPECT_GE(high_avg, low_avg)
            << "Confidence calibration degraded";
    }
}

TEST_F(PredictiveProtocolRegressionTest, ConfidenceRange) {
    /* Confidence values should stay in valid range */
    std::vector<uint32_t> pattern = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    TrainPattern(pattern, 20);

    /* Check confidence ranges */
    for (int i = 0; i < 100; i++) {
        predicted_message_t pred;
        int ret = predictive_protocol_predict_next(
            protocol,
            BIO_MSG_VISUAL_INPUT,
            &pred);

        if (ret == 0) {
            EXPECT_GE(pred.confidence, 0.0f);
            EXPECT_LE(pred.confidence, 1.0f);
        }
    }
}

//=============================================================================
// Prefetch Performance Tests
//=============================================================================

TEST_F(PredictiveProtocolRegressionTest, PrefetchHitRate) {
    /* Prefetch hit rate should be >70% for well-learned patterns */
    std::vector<uint32_t> pattern = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT,
        BIO_MSG_WORKING_MEMORY_STORE
    };

    TrainPattern(pattern, 40);

    /* Make predictions and prefetch */
    int prefetch_count = 0;
    for (int i = 0; i < 50; i++) {
        predicted_message_t pred;
        int ret = predictive_protocol_predict_next(
            protocol,
            BIO_MSG_VISUAL_INPUT,
            &pred);

        if (ret == 0) {
            ret = predictive_protocol_prefetch_data(protocol, &pred);
            if (ret == 0) {
                prefetch_count++;
            }
        }
    }

    /* Check cache hits */
    int hit_count = 0;
    for (int i = 0; i < prefetch_count; i++) {
        void* data = nullptr;
        uint32_t size = 0;

        int ret = predictive_protocol_check_prefetch(
            protocol,
            BIO_MSG_ATTENTION_SHIFT,
            &data,
            &size);

        if (ret == 0) {
            hit_count++;
        }
    }

    /* Calculate hit rate */
    float hit_rate = prefetch_count > 0 ?
        (float)hit_count / (float)prefetch_count : 0.0f;

    EXPECT_GE(hit_rate, 0.70f) << "Prefetch hit rate degraded";
}

TEST_F(PredictiveProtocolRegressionTest, PrefetchMemoryUsage) {
    /* Prefetch cache should not grow unbounded */
    std::vector<uint32_t> pattern = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    TrainPattern(pattern, 20);

    /* Trigger many prefetches */
    for (int i = 0; i < 1000; i++) {
        predicted_message_t pred;
        pred.message_type = BIO_MSG_VISUAL_INPUT + (i % 100);
        pred.confidence = 0.8f;

        predictive_protocol_prefetch_data(protocol, &pred);
    }

    /* Memory should be bounded (cache cleanup working) */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);

    /* Should have some misses from cleanup */
    EXPECT_GT(stats.prefetch_misses, 0)
        << "Cache cleanup not working";
}

//=============================================================================
// Latency Regression Tests
//=============================================================================

TEST_F(PredictiveProtocolRegressionTest, PredictionLatency) {
    /* Prediction latency should be <1ms for typical patterns */
    std::vector<uint32_t> pattern = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    TrainPattern(pattern, 30);

    std::vector<double> latencies;

    for (int i = 0; i < 1000; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        predicted_message_t pred;
        predictive_protocol_predict_next(
            protocol,
            BIO_MSG_VISUAL_INPUT,
            &pred);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start);

        latencies.push_back(duration.count() / 1000.0); /* Convert to ms */
    }

    /* Calculate statistics */
    double avg_latency = std::accumulate(
        latencies.begin(),
        latencies.end(),
        0.0) / latencies.size();

    std::sort(latencies.begin(), latencies.end());
    double p50_latency = latencies[latencies.size() / 2];
    double p95_latency = latencies[latencies.size() * 95 / 100];
    double p99_latency = latencies[latencies.size() * 99 / 100];

    /* Latency thresholds */
    EXPECT_LT(avg_latency, 1.0) << "Average prediction latency too high";
    EXPECT_LT(p50_latency, 1.0) << "P50 prediction latency too high";
    EXPECT_LT(p95_latency, 5.0) << "P95 prediction latency too high";
    EXPECT_LT(p99_latency, 10.0) << "P99 prediction latency too high";
}

TEST_F(PredictiveProtocolRegressionTest, ObservationLatency) {
    /* Message observation should be <0.1ms */
    std::vector<double> latencies;

    for (int i = 0; i < 10000; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        predictive_protocol_observe_message(
            protocol,
            BIO_MSG_VISUAL_INPUT,
            BIO_MODULE_BRAIN,
            BIO_MODULE_ATTENTION,
            GetTimestamp());

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start);

        latencies.push_back(duration.count() / 1000.0);
    }

    double avg_latency = std::accumulate(
        latencies.begin(),
        latencies.end(),
        0.0) / latencies.size();

    EXPECT_LT(avg_latency, 0.1) << "Observation latency too high";
}

//=============================================================================
// Scalability Tests
//=============================================================================

TEST_F(PredictiveProtocolRegressionTest, ManyPatternsScalability) {
    /* Should handle hundreds of different patterns */
    const int PATTERN_COUNT = 500;

    for (int p = 0; p < PATTERN_COUNT; p++) {
        uint32_t type1 = BIO_MSG_VISUAL_INPUT + (p % 100);
        uint32_t type2 = BIO_MSG_ATTENTION_SHIFT + (p % 100);

        std::vector<uint32_t> pattern = {type1, type2};
        TrainPattern(pattern, 2);
    }

    /* Should still make predictions */
    predicted_message_t pred;
    int ret = predictive_protocol_predict_next(
        protocol,
        BIO_MSG_VISUAL_INPUT,
        &pred);

    /* May or may not succeed, but shouldn't crash */
    EXPECT_TRUE(ret == 0 || ret < 0);
}

TEST_F(PredictiveProtocolRegressionTest, HighFrequencyUpdates) {
    /* Should handle high message frequency */
    const int MESSAGE_COUNT = 50000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < MESSAGE_COUNT; i++) {
        uint32_t msg_type = BIO_MSG_VISUAL_INPUT + (i % 10);

        predictive_protocol_observe_message(
            protocol,
            msg_type,
            BIO_MODULE_BRAIN,
            BIO_MODULE_ATTENTION,
            GetTimestamp());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start);

    double throughput = MESSAGE_COUNT / (duration.count() / 1000.0);

    /* Should process >10k messages/second */
    EXPECT_GT(throughput, 10000.0)
        << "Message processing throughput degraded";
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(PredictiveProtocolRegressionTest, MemoryStability) {
    /* Memory usage should stabilize after warmup */
    std::vector<uint32_t> pattern = {
        BIO_MSG_VISUAL_INPUT,
        BIO_MSG_ATTENTION_SHIFT
    };

    /* Run for extended period */
    for (int epoch = 0; epoch < 1000; epoch++) {
        TrainPattern(pattern, 1);

        predicted_message_t pred;
        predictive_protocol_predict_next(
            protocol,
            BIO_MSG_VISUAL_INPUT,
            &pred);
    }

    /* Should complete without memory issues */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    EXPECT_GT(stats.predictions_made, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "====================================\n";
    std::cout << "Predictive Protocol Regression Tests\n";
    std::cout << "====================================\n\n";
    std::cout << "Benchmarks:\n";
    std::cout << "- Simple pattern accuracy: >80%\n";
    std::cout << "- Complex pattern accuracy: >60%\n";
    std::cout << "- Prefetch hit rate: >70%\n";
    std::cout << "- Prediction latency: <1ms (avg)\n";
    std::cout << "- Observation latency: <0.1ms (avg)\n";
    std::cout << "- Throughput: >10k messages/sec\n\n";

    int result = RUN_ALL_TESTS();

    std::cout << "\n====================================\n";
    std::cout << "Regression tests completed\n";
    std::cout << "====================================\n";

    return result;
}
