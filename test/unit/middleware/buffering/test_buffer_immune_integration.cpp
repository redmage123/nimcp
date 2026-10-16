//=============================================================================
// test_buffer_immune_integration.cpp - Buffer Immune Integration Tests
//=============================================================================

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/immune/nimcp_buffer_immune.h"
#include "middleware/buffering/nimcp_circular_buffer.h"
#include "middleware/buffering/nimcp_integration_buffer.h"
#include "middleware/buffering/nimcp_phase_coded_buffer.h"
#include "middleware/buffering/nimcp_sliding_window.h"
#include "middleware/buffering/nimcp_temporal_accumulator.h"
#include "cognitive/immune/nimcp_brain_immune.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class BufferImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* brain_immune;
    buffer_immune_system_t* buffer_immune;

    void SetUp() override {
        // Create brain immune system
        brain_immune_config_t brain_config;
        brain_immune_default_config(&brain_config);
        brain_immune = brain_immune_create(&brain_config);
        ASSERT_NE(brain_immune, nullptr);
        brain_immune_start(brain_immune);

        // Create buffer immune system
        buffer_immune_config_t config;
        buffer_immune_default_config(&config);
        buffer_immune = buffer_immune_create(brain_immune, &config);
        ASSERT_NE(buffer_immune, nullptr);
    }

    void TearDown() override {
        buffer_immune_destroy(buffer_immune);
        brain_immune_destroy(brain_immune);
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================

TEST_F(BufferImmuneTest, CreateDestroy) {
    EXPECT_NE(buffer_immune, nullptr);
    EXPECT_EQ(buffer_immune_get_buffer_count(buffer_immune), 0);
}

TEST_F(BufferImmuneTest, CreateWithNullBrainImmune) {
    buffer_immune_config_t config;
    buffer_immune_default_config(&config);
    buffer_immune_system_t* sys = buffer_immune_create(nullptr, &config);
    EXPECT_EQ(sys, nullptr);
}

TEST_F(BufferImmuneTest, DefaultConfig) {
    buffer_immune_config_t config;
    int result = buffer_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_capacity_modulation);
    EXPECT_TRUE(config.enable_anomaly_detection);
    EXPECT_TRUE(config.enable_auto_immune_alert);
    EXPECT_FLOAT_EQ(config.overflow_threshold, 0.95F);
    EXPECT_FLOAT_EQ(config.coherence_min_threshold, 0.3F);
    EXPECT_EQ(config.persistent_overflow_count, 10);
}

//=============================================================================
// CIRCULAR BUFFER REGISTRATION TESTS
//=============================================================================

TEST_F(BufferImmuneTest, RegisterCircularBuffer) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer, nullptr);

    uint32_t buffer_id;
    int result = buffer_immune_register_circular(buffer_immune, buffer,
                                                 "test_circular", &buffer_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(buffer_id, 0);
    EXPECT_EQ(buffer_immune_get_buffer_count(buffer_immune), 1);

    float health_score = buffer_immune_get_health_score(buffer_immune, buffer_id);
    EXPECT_FLOAT_EQ(health_score, 1.0F);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

TEST_F(BufferImmuneTest, RegisterMultipleCircularBuffers) {
    const int NUM_BUFFERS = 5;
    circular_buffer_t* buffers[NUM_BUFFERS];
    uint32_t buffer_ids[NUM_BUFFERS];

    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffers[i] = circular_buffer_create(sizeof(float), 50, OVERFLOW_OVERWRITE);
        ASSERT_NE(buffers[i], nullptr);

        char name[32];
        snprintf(name, sizeof(name), "buffer_%d", i);
        int result = buffer_immune_register_circular(buffer_immune, buffers[i],
                                                     name, &buffer_ids[i]);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(buffer_immune_get_buffer_count(buffer_immune), NUM_BUFFERS);

    // Cleanup
    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffer_immune_unregister(buffer_immune, buffer_ids[i]);
        circular_buffer_destroy(buffers[i]);
    }
}

TEST_F(BufferImmuneTest, UnregisterCircularBuffer) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    int result = buffer_immune_unregister(buffer_immune, buffer_id);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(buffer_immune_get_buffer_count(buffer_immune), 0);

    circular_buffer_destroy(buffer);
}

//=============================================================================
// INTEGRATION BUFFER REGISTRATION TESTS
//=============================================================================

TEST_F(BufferImmuneTest, RegisterIntegrationBuffer) {
    integration_buffer_t* buffer = integration_buffer_create(10, 20, 30, 1);
    ASSERT_NE(buffer, nullptr);

    uint32_t buffer_id;
    int result = buffer_immune_register_integration(buffer_immune, buffer,
                                                    "test_integration", &buffer_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(buffer_id, 0);

    buffer_immune_unregister(buffer_immune, buffer_id);
    integration_buffer_destroy(buffer);
}

//=============================================================================
// PHASE-CODED BUFFER REGISTRATION TESTS
//=============================================================================

TEST_F(BufferImmuneTest, RegisterPhaseCodedBuffer) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    uint32_t buffer_id;
    int result = buffer_immune_register_phase_coded(buffer_immune, buffer,
                                                    "test_phase", &buffer_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(buffer_id, 0);

    buffer_immune_unregister(buffer_immune, buffer_id);
    phase_buffer_destroy(buffer);
}

//=============================================================================
// SLIDING WINDOW REGISTRATION TESTS
//=============================================================================

TEST_F(BufferImmuneTest, RegisterSlidingWindow) {
    sliding_window_t* window = sliding_window_create(50, 25);
    ASSERT_NE(window, nullptr);

    uint32_t buffer_id;
    int result = buffer_immune_register_sliding_window(buffer_immune, window,
                                                       "test_window", &buffer_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(buffer_id, 0);

    buffer_immune_unregister(buffer_immune, buffer_id);
    sliding_window_destroy(window);
}

//=============================================================================
// TEMPORAL ACCUMULATOR REGISTRATION TESTS
//=============================================================================

TEST_F(BufferImmuneTest, RegisterTemporalAccumulator) {
    temporal_accumulator_t* accumulator = temporal_accumulator_create(
        4, 0.1F, INTEGRATION_EMA);
    ASSERT_NE(accumulator, nullptr);

    uint32_t buffer_id;
    int result = buffer_immune_register_temporal_accumulator(
        buffer_immune, accumulator, "test_accumulator", &buffer_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(buffer_id, 0);

    buffer_immune_unregister(buffer_immune, buffer_id);
    temporal_accumulator_destroy(accumulator);
}

//=============================================================================
// CAPACITY MODULATION TESTS
//=============================================================================

TEST_F(BufferImmuneTest, ModulateCapacityInflammationLevels) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    // Test each inflammation level
    struct {
        brain_inflammation_level_t level;
        float expected_multiplier;
    } test_cases[] = {
        {INFLAMMATION_NONE,     1.0F},
        {INFLAMMATION_LOCAL,    0.9F},
        {INFLAMMATION_REGIONAL, 0.75F},
        {INFLAMMATION_SYSTEMIC, 0.5F},
        {INFLAMMATION_STORM,    0.25F},
    };

    for (const auto& tc : test_cases) {
        int result = buffer_immune_modulate_capacity(buffer_immune, buffer_id,
                                                     tc.level);
        EXPECT_EQ(result, 0);

        float multiplier = buffer_immune_get_capacity_multiplier(buffer_immune,
                                                                 buffer_id);
        EXPECT_FLOAT_EQ(multiplier, tc.expected_multiplier)
            << "Failed for inflammation level " << tc.level;
    }

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

TEST_F(BufferImmuneTest, ModulateAllBuffers) {
    const int NUM_BUFFERS = 3;
    circular_buffer_t* buffers[NUM_BUFFERS];
    uint32_t buffer_ids[NUM_BUFFERS];

    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffers[i] = circular_buffer_create(sizeof(float), 100, OVERFLOW_OVERWRITE);
        char name[32];
        snprintf(name, sizeof(name), "buffer_%d", i);
        buffer_immune_register_circular(buffer_immune, buffers[i], name,
                                       &buffer_ids[i]);
    }

    int result = buffer_immune_modulate_all(buffer_immune, INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(result, NUM_BUFFERS);

    // Verify all buffers modulated
    for (int i = 0; i < NUM_BUFFERS; i++) {
        float multiplier = buffer_immune_get_capacity_multiplier(buffer_immune,
                                                                 buffer_ids[i]);
        EXPECT_FLOAT_EQ(multiplier, 0.5F);
    }

    // Cleanup
    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffer_immune_unregister(buffer_immune, buffer_ids[i]);
        circular_buffer_destroy(buffers[i]);
    }
}

TEST_F(BufferImmuneTest, RestoreCapacity) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    // Modulate then restore
    buffer_immune_modulate_capacity(buffer_immune, buffer_id, INFLAMMATION_STORM);
    EXPECT_FLOAT_EQ(buffer_immune_get_capacity_multiplier(buffer_immune, buffer_id),
                    0.25F);

    int result = buffer_immune_restore_capacity(buffer_immune, buffer_id);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(buffer_immune_get_capacity_multiplier(buffer_immune, buffer_id),
                    1.0F);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

TEST_F(BufferImmuneTest, RestoreAllBuffers) {
    const int NUM_BUFFERS = 3;
    circular_buffer_t* buffers[NUM_BUFFERS];
    uint32_t buffer_ids[NUM_BUFFERS];

    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffers[i] = circular_buffer_create(sizeof(float), 100, OVERFLOW_OVERWRITE);
        char name[32];
        snprintf(name, sizeof(name), "buffer_%d", i);
        buffer_immune_register_circular(buffer_immune, buffers[i], name,
                                       &buffer_ids[i]);
    }

    // Modulate all
    buffer_immune_modulate_all(buffer_immune, INFLAMMATION_REGIONAL);

    // Restore all
    int result = buffer_immune_restore_all(buffer_immune);
    EXPECT_EQ(result, NUM_BUFFERS);

    // Verify all restored
    for (int i = 0; i < NUM_BUFFERS; i++) {
        float multiplier = buffer_immune_get_capacity_multiplier(buffer_immune,
                                                                 buffer_ids[i]);
        EXPECT_FLOAT_EQ(multiplier, 1.0F);
    }

    // Cleanup
    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffer_immune_unregister(buffer_immune, buffer_ids[i]);
        circular_buffer_destroy(buffers[i]);
    }
}

//=============================================================================
// ANOMALY DETECTION TESTS
//=============================================================================

TEST_F(BufferImmuneTest, ReportOverflow) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    int result = buffer_immune_report_overflow(buffer_immune, buffer_id, 0.96F);
    EXPECT_EQ(result, 0);

    buffer_immune_stats_t stats;
    buffer_immune_get_stats(buffer_immune, &stats);
    EXPECT_EQ(stats.overflows_reported, 1);
    EXPECT_EQ(stats.anomalies_detected, 1);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

TEST_F(BufferImmuneTest, PersistentOverflowTriggersImmuneAlert) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    // Report persistent overflows
    for (int i = 0; i < 10; i++) {
        buffer_immune_report_overflow(buffer_immune, buffer_id, 0.97F);
    }

    buffer_immune_stats_t stats;
    buffer_immune_get_stats(buffer_immune, &stats);
    EXPECT_GT(stats.immune_alerts_sent, 0);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

TEST_F(BufferImmuneTest, ReportCorruption) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    uint8_t corruption_sig[] = {0xDE, 0xAD, 0xBE, 0xEF};
    int result = buffer_immune_report_corruption(buffer_immune, buffer_id,
                                                 corruption_sig, sizeof(corruption_sig));
    EXPECT_EQ(result, 0);

    buffer_immune_stats_t stats;
    buffer_immune_get_stats(buffer_immune, &stats);
    EXPECT_EQ(stats.anomalies_detected, 1);
    EXPECT_GT(stats.immune_alerts_sent, 0);

    buffer_health_t health;
    buffer_immune_check_health(buffer_immune, buffer_id, &health);
    EXPECT_EQ(health, BUFFER_HEALTH_CRITICAL);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

TEST_F(BufferImmuneTest, ReportCoherenceLoss) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    uint32_t buffer_id;
    buffer_immune_register_phase_coded(buffer_immune, buffer, "test", &buffer_id);

    // Report low coherence
    int result = buffer_immune_report_coherence_loss(buffer_immune, buffer_id, 0.2F);
    EXPECT_EQ(result, 0);

    buffer_immune_stats_t stats;
    buffer_immune_get_stats(buffer_immune, &stats);
    EXPECT_EQ(stats.anomalies_detected, 1);

    buffer_immune_unregister(buffer_immune, buffer_id);
    phase_buffer_destroy(buffer);
}

TEST_F(BufferImmuneTest, CheckBufferHealth) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    buffer_health_t health;
    int result = buffer_immune_check_health(buffer_immune, buffer_id, &health);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(health, BUFFER_HEALTH_OPTIMAL);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

TEST_F(BufferImmuneTest, CheckAllHealth) {
    const int NUM_BUFFERS = 3;
    circular_buffer_t* buffers[NUM_BUFFERS];
    uint32_t buffer_ids[NUM_BUFFERS];

    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffers[i] = circular_buffer_create(sizeof(float), 100, OVERFLOW_OVERWRITE);
        char name[32];
        snprintf(name, sizeof(name), "buffer_%d", i);
        buffer_immune_register_circular(buffer_immune, buffers[i], name,
                                       &buffer_ids[i]);
    }

    buffer_health_t worst_health;
    int unhealthy = buffer_immune_check_all_health(buffer_immune, &worst_health);
    EXPECT_EQ(unhealthy, 0);
    EXPECT_EQ(worst_health, BUFFER_HEALTH_OPTIMAL);

    // Cleanup
    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffer_immune_unregister(buffer_immune, buffer_ids[i]);
        circular_buffer_destroy(buffers[i]);
    }
}

//=============================================================================
// HEALTH SCORE TESTS
//=============================================================================

TEST_F(BufferImmuneTest, HealthScoreOptimal) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    float score = buffer_immune_get_health_score(buffer_immune, buffer_id);
    EXPECT_FLOAT_EQ(score, 1.0F);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

TEST_F(BufferImmuneTest, HealthScoreDegraded) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    // Modulate capacity
    buffer_immune_modulate_capacity(buffer_immune, buffer_id, INFLAMMATION_SYSTEMIC);

    float score = buffer_immune_get_health_score(buffer_immune, buffer_id);
    EXPECT_LT(score, 1.0F);
    EXPECT_GT(score, 0.0F);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

TEST_F(BufferImmuneTest, GetStatistics) {
    buffer_immune_stats_t stats;
    int result = buffer_immune_get_stats(buffer_immune, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.anomalies_detected, 0);
    EXPECT_EQ(stats.overflows_reported, 0);
    EXPECT_EQ(stats.immune_alerts_sent, 0);
    EXPECT_FLOAT_EQ(stats.avg_health_score, 1.0F);
}

TEST_F(BufferImmuneTest, StatisticsAggregation) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    // Generate some anomalies
    buffer_immune_report_overflow(buffer_immune, buffer_id, 0.96F);
    buffer_immune_report_overflow(buffer_immune, buffer_id, 0.97F);

    buffer_immune_stats_t stats;
    buffer_immune_get_stats(buffer_immune, &stats);
    EXPECT_EQ(stats.overflows_reported, 2);
    EXPECT_EQ(stats.anomalies_detected, 2);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

//=============================================================================
// CALLBACK TESTS
//=============================================================================

static bool anomaly_callback_fired = false;
static buffer_anomaly_t callback_anomaly_type;

static void test_anomaly_callback(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    buffer_anomaly_t anomaly,
    void* user_data
) {
    anomaly_callback_fired = true;
    callback_anomaly_type = anomaly;
}

TEST_F(BufferImmuneTest, AnomalyCallback) {
    anomaly_callback_fired = false;

    buffer_immune_set_anomaly_callback(buffer_immune, test_anomaly_callback, nullptr);

    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    buffer_immune_report_overflow(buffer_immune, buffer_id, 0.96F);

    EXPECT_TRUE(anomaly_callback_fired);
    EXPECT_EQ(callback_anomaly_type, BUFFER_ANOMALY_OVERFLOW);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

//=============================================================================
// UPDATE TESTS
//=============================================================================

TEST_F(BufferImmuneTest, UpdateSystem) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    int result = buffer_immune_update(buffer_immune, 100);
    EXPECT_EQ(result, 0);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

//=============================================================================
// UTILITY FUNCTION TESTS
//=============================================================================

TEST_F(BufferImmuneTest, BufferTypeToString) {
    EXPECT_STREQ(buffer_immune_buffer_type_to_string(BUFFER_TYPE_CIRCULAR),
                 "circular");
    EXPECT_STREQ(buffer_immune_buffer_type_to_string(BUFFER_TYPE_INTEGRATION),
                 "integration");
    EXPECT_STREQ(buffer_immune_buffer_type_to_string(BUFFER_TYPE_PHASE_CODED),
                 "phase_coded");
    EXPECT_STREQ(buffer_immune_buffer_type_to_string(BUFFER_TYPE_SLIDING_WINDOW),
                 "sliding_window");
    EXPECT_STREQ(buffer_immune_buffer_type_to_string(BUFFER_TYPE_TEMPORAL_ACCUMULATOR),
                 "temporal_accumulator");
}

TEST_F(BufferImmuneTest, HealthToString) {
    EXPECT_STREQ(buffer_immune_health_to_string(BUFFER_HEALTH_OPTIMAL), "optimal");
    EXPECT_STREQ(buffer_immune_health_to_string(BUFFER_HEALTH_STRESSED), "stressed");
    EXPECT_STREQ(buffer_immune_health_to_string(BUFFER_HEALTH_DEGRADED), "degraded");
    EXPECT_STREQ(buffer_immune_health_to_string(BUFFER_HEALTH_CRITICAL), "critical");
    EXPECT_STREQ(buffer_immune_health_to_string(BUFFER_HEALTH_FAILED), "failed");
}

TEST_F(BufferImmuneTest, AnomalyToString) {
    EXPECT_STREQ(buffer_immune_anomaly_to_string(BUFFER_ANOMALY_NONE), "none");
    EXPECT_STREQ(buffer_immune_anomaly_to_string(BUFFER_ANOMALY_OVERFLOW), "overflow");
    EXPECT_STREQ(buffer_immune_anomaly_to_string(BUFFER_ANOMALY_CORRUPTION), "corruption");
    EXPECT_STREQ(buffer_immune_anomaly_to_string(BUFFER_ANOMALY_COHERENCE_LOSS),
                 "coherence_loss");
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

TEST_F(BufferImmuneTest, NullPointerHandling) {
    EXPECT_EQ(buffer_immune_get_buffer_count(nullptr), 0);
    EXPECT_FLOAT_EQ(buffer_immune_get_health_score(nullptr, 1), 0.0F);
    EXPECT_FLOAT_EQ(buffer_immune_get_capacity_multiplier(nullptr, 1), 1.0F);

    buffer_immune_config_t config;
    EXPECT_EQ(buffer_immune_default_config(nullptr), -1);
    EXPECT_EQ(buffer_immune_default_config(&config), 0);
}

TEST_F(BufferImmuneTest, InvalidBufferId) {
    float score = buffer_immune_get_health_score(buffer_immune, 9999);
    EXPECT_FLOAT_EQ(score, 0.0F);

    int result = buffer_immune_modulate_capacity(buffer_immune, 9999,
                                                 INFLAMMATION_LOCAL);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// INTEGRATION TEST: FULL IMMUNE CYCLE
//=============================================================================

TEST_F(BufferImmuneTest, FullImmuneCycle) {
    // Create and register buffer
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 100,
                                                        OVERFLOW_OVERWRITE);
    uint32_t buffer_id;
    buffer_immune_register_circular(buffer_immune, buffer, "test", &buffer_id);

    // Initially optimal
    EXPECT_FLOAT_EQ(buffer_immune_get_health_score(buffer_immune, buffer_id), 1.0F);

    // Simulate inflammation - capacity reduced
    buffer_immune_modulate_capacity(buffer_immune, buffer_id, INFLAMMATION_REGIONAL);
    EXPECT_FLOAT_EQ(buffer_immune_get_capacity_multiplier(buffer_immune, buffer_id),
                    0.75F);

    // Simulate buffer stress - overflow
    buffer_immune_report_overflow(buffer_immune, buffer_id, 0.96F);
    float stressed_score = buffer_immune_get_health_score(buffer_immune, buffer_id);
    EXPECT_LT(stressed_score, 1.0F);

    // Resolution - restore capacity
    buffer_immune_restore_capacity(buffer_immune, buffer_id);
    EXPECT_FLOAT_EQ(buffer_immune_get_capacity_multiplier(buffer_immune, buffer_id),
                    1.0F);

    // Health should improve
    float restored_score = buffer_immune_get_health_score(buffer_immune, buffer_id);
    EXPECT_GT(restored_score, stressed_score);

    buffer_immune_unregister(buffer_immune, buffer_id);
    circular_buffer_destroy(buffer);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
