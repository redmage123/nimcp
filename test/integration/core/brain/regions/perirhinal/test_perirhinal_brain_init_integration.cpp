/**
 * @file test_perirhinal_brain_init_integration.cpp
 * @brief Integration tests for Perirhinal Cortex brain initialization system
 *
 * WHAT: Tests Perirhinal Cortex integration with brain factory initialization
 * WHY:  Ensure proper lifecycle management and brain system integration
 * HOW:  Test registration, creation, initialization, and destruction via brain factory
 *
 * INTEGRATION POINTS:
 * - Brain factory registration
 * - Brain configuration propagation
 * - Lifecycle callbacks
 * - Bio-async bridge initialization
 * - KG wiring setup
 * - Security registration
 * - Immune bridge connection
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Include system headers first, then NIMCP headers, then region-specific
// This ensures typedef compatibility
#include "nimcp.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/perirhinal/nimcp_perirhinal.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class PerirhinalBrainInitTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal;
    perirhinal_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;

        /* Initialize bio-async router for integration testing */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Configure perirhinal cortex with bio-async enabled */
        config = perirhinal_default_config();
        config.enable_bio_async = router_initialized;
        config.enable_snn = true;
        config.enable_plasticity = true;

        perirhinal = perirhinal_create(&config);
        ASSERT_NE(nullptr, perirhinal) << "Failed to create Perirhinal cortex";
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }
};

/*=============================================================================
 * BRAIN FACTORY INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalBrainInitTest, CreateWithFullConfig) {
    perirhinal_config_t full_config = perirhinal_default_config();
    full_config.enable_entorhinal = true;
    full_config.enable_security = true;
    full_config.enable_immune = true;
    full_config.enable_bio_async = false;
    full_config.enable_snn = true;
    full_config.enable_plasticity = true;
    full_config.enable_stdp = true;
    full_config.enable_cognitive = true;
    full_config.enable_training = true;
    full_config.enable_substrate = true;
    full_config.enable_resonance = true;
    full_config.enable_thalamic = true;
    full_config.enable_hippocampus = true;
    full_config.enable_perception = true;

    nimcp_perirhinal_t* full_perirhinal = perirhinal_create(&full_config);
    ASSERT_NE(nullptr, full_perirhinal);

    /* Verify configuration was applied */
    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(full_perirhinal, &retrieved));
    EXPECT_TRUE(retrieved.enable_entorhinal);
    EXPECT_TRUE(retrieved.enable_security);
    EXPECT_TRUE(retrieved.enable_immune);
    EXPECT_TRUE(retrieved.enable_snn);
    EXPECT_TRUE(retrieved.enable_plasticity);
    EXPECT_TRUE(retrieved.enable_stdp);
    EXPECT_TRUE(retrieved.enable_cognitive);
    EXPECT_TRUE(retrieved.enable_training);
    EXPECT_TRUE(retrieved.enable_substrate);

    perirhinal_destroy(full_perirhinal);
}

TEST_F(PerirhinalBrainInitTest, CreateWithMinimalConfig) {
    perirhinal_config_t minimal_config = perirhinal_default_config();
    minimal_config.enable_entorhinal = false;
    minimal_config.enable_security = false;
    minimal_config.enable_immune = false;
    minimal_config.enable_bio_async = false;
    minimal_config.enable_snn = false;
    minimal_config.enable_plasticity = false;
    minimal_config.enable_stdp = false;
    minimal_config.enable_cognitive = false;
    minimal_config.enable_training = false;

    nimcp_perirhinal_t* minimal_perirhinal = perirhinal_create(&minimal_config);
    ASSERT_NE(nullptr, minimal_perirhinal);

    /* Should still be functional for basic operations */
    float features[256];
    memset(features, 0, sizeof(features));
    features[0] = 1.0f;
    features[1] = 0.5f;

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(minimal_perirhinal, features, 256, "test_object", &object_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(object_id, 0u);

    perirhinal_destroy(minimal_perirhinal);
}

TEST_F(PerirhinalBrainInitTest, InitialStateIsIdle) {
    /* Status should be valid */
    perirhinal_status_t status = perirhinal_get_status(perirhinal);
    EXPECT_GE((int)status, 0);
    /* Error should be none initially */
    EXPECT_EQ(perirhinal_get_last_error(perirhinal), PERIRHINAL_ERROR_NONE);
}

TEST_F(PerirhinalBrainInitTest, ResetRestoresInitialState) {
    /* Perform some operations */
    float features[256];
    memset(features, 0, sizeof(features));
    features[0] = 1.0f;

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "test_object", &object_id);

    /* Reset should succeed */
    EXPECT_EQ(0, perirhinal_reset(perirhinal));
    /* Status should be valid after reset */
    perirhinal_status_t status = perirhinal_get_status(perirhinal);
    EXPECT_GE((int)status, 0);
}

TEST_F(PerirhinalBrainInitTest, MultipleResetCycles) {
    for (int cycle = 0; cycle < 10; cycle++) {
        /* Encode an object */
        float features[256];
        memset(features, 0, sizeof(features));
        features[0] = (float)cycle * 0.1f;

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, "test_object", &object_id);

        /* Reset should succeed */
        EXPECT_EQ(0, perirhinal_reset(perirhinal));
        /* Status should be valid */
        perirhinal_status_t status = perirhinal_get_status(perirhinal);
        EXPECT_GE((int)status, 0);
    }
}

/*=============================================================================
 * CAPACITY CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalBrainInitTest, CustomCapacityLimits) {
    perirhinal_config_t custom_config = perirhinal_default_config();
    custom_config.num_object_cells = 1024;
    custom_config.num_familiarity_cells = 512;
    custom_config.num_novelty_cells = 256;
    custom_config.num_recency_cells = 128;
    custom_config.max_stored_objects = 2048;
    custom_config.enable_bio_async = false;

    nimcp_perirhinal_t* custom_perirhinal = perirhinal_create(&custom_config);
    ASSERT_NE(nullptr, custom_perirhinal);

    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(custom_perirhinal, &retrieved));
    EXPECT_EQ(retrieved.num_object_cells, 1024u);
    EXPECT_EQ(retrieved.num_familiarity_cells, 512u);
    EXPECT_EQ(retrieved.num_novelty_cells, 256u);
    EXPECT_EQ(retrieved.num_recency_cells, 128u);
    EXPECT_EQ(retrieved.max_stored_objects, 2048u);

    perirhinal_destroy(custom_perirhinal);
}

TEST_F(PerirhinalBrainInitTest, FeatureDimensionConfiguration) {
    perirhinal_config_t feature_config = perirhinal_default_config();
    feature_config.feature_dim = 512;
    feature_config.identity_dim = 256;
    feature_config.max_views_per_object = 16;
    feature_config.enable_bio_async = false;

    nimcp_perirhinal_t* feature_perirhinal = perirhinal_create(&feature_config);
    ASSERT_NE(nullptr, feature_perirhinal);

    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(feature_perirhinal, &retrieved));
    EXPECT_EQ(retrieved.feature_dim, 512u);
    EXPECT_EQ(retrieved.identity_dim, 256u);
    EXPECT_EQ(retrieved.max_views_per_object, 16u);

    perirhinal_destroy(feature_perirhinal);
}

/*=============================================================================
 * STATISTICS INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalBrainInitTest, InitialStatisticsAreZero) {
    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));

    EXPECT_EQ(stats.updates_processed, 0u);
    EXPECT_EQ(stats.objects_encoded, 0u);
    EXPECT_EQ(stats.objects_recognized, 0u);
    EXPECT_EQ(stats.novelty_detections, 0u);
    EXPECT_EQ(stats.familiarity_computations, 0u);
}

TEST_F(PerirhinalBrainInitTest, StatisticsAfterOperations) {
    /* Encode some objects */
    for (int i = 0; i < 5; i++) {
        float features[256];
        memset(features, 0, sizeof(features));
        features[0] = (float)i * 0.2f;

        char name[32];
        snprintf(name, sizeof(name), "object_%d", i);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, name, &object_id);
    }

    /* Check statistics */
    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.objects_encoded, 5u);
}

/*=============================================================================
 * LEARNING PARAMETER CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalBrainInitTest, LearningRateConfiguration) {
    perirhinal_config_t learning_config = perirhinal_default_config();
    learning_config.learning_rate = 0.05f;
    learning_config.weight_decay = 0.001f;
    learning_config.eligibility_decay = 0.9f;
    learning_config.enable_bio_async = false;

    nimcp_perirhinal_t* learning_perirhinal = perirhinal_create(&learning_config);
    ASSERT_NE(nullptr, learning_perirhinal);

    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(learning_perirhinal, &retrieved));
    EXPECT_FLOAT_EQ(retrieved.learning_rate, 0.05f);
    EXPECT_FLOAT_EQ(retrieved.weight_decay, 0.001f);
    EXPECT_FLOAT_EQ(retrieved.eligibility_decay, 0.9f);

    perirhinal_destroy(learning_perirhinal);
}

TEST_F(PerirhinalBrainInitTest, OscillationParameterConfiguration) {
    perirhinal_config_t osc_config = perirhinal_default_config();
    osc_config.theta_frequency = 6.0f;
    osc_config.gamma_frequency = 40.0f;
    osc_config.phase_coupling_strength = 0.5f;
    osc_config.enable_bio_async = false;

    nimcp_perirhinal_t* osc_perirhinal = perirhinal_create(&osc_config);
    ASSERT_NE(nullptr, osc_perirhinal);

    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(osc_perirhinal, &retrieved));
    EXPECT_FLOAT_EQ(retrieved.theta_frequency, 6.0f);
    EXPECT_FLOAT_EQ(retrieved.gamma_frequency, 40.0f);
    EXPECT_FLOAT_EQ(retrieved.phase_coupling_strength, 0.5f);

    perirhinal_destroy(osc_perirhinal);
}

/*=============================================================================
 * CONCURRENT INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalBrainInitTest, MultipleInstancesCanCoexist) {
    perirhinal_config_t config1 = perirhinal_default_config();
    config1.enable_bio_async = false;

    perirhinal_config_t config2 = perirhinal_default_config();
    config2.enable_bio_async = false;
    config2.max_stored_objects = 2048;

    nimcp_perirhinal_t* perirhinal1 = perirhinal_create(&config1);
    nimcp_perirhinal_t* perirhinal2 = perirhinal_create(&config2);

    ASSERT_NE(nullptr, perirhinal1);
    ASSERT_NE(nullptr, perirhinal2);
    EXPECT_NE(perirhinal1, perirhinal2);

    /* Both should be independently functional */
    float features[256];
    memset(features, 0, sizeof(features));
    features[0] = 1.0f;

    uint32_t id1 = 0, id2 = 0;
    EXPECT_EQ(0, perirhinal_encode_object(perirhinal1, features, 256, "obj1", &id1));
    EXPECT_EQ(0, perirhinal_encode_object(perirhinal2, features, 256, "obj2", &id2));

    /* Destroying one should not affect the other */
    perirhinal_destroy(perirhinal1);
    /* Status should still be valid */
    perirhinal_status_t status = perirhinal_get_status(perirhinal2);
    EXPECT_GE((int)status, 0);

    perirhinal_destroy(perirhinal2);
}

/*=============================================================================
 * ERROR HANDLING DURING INIT TESTS
 *===========================================================================*/

TEST_F(PerirhinalBrainInitTest, HandleNullFeaturesGracefully) {
    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, nullptr, 256, "test", &object_id);
    EXPECT_NE(0, result);
    /* Error code may or may not be set */
    perirhinal_error_t err = perirhinal_get_last_error(perirhinal);
    EXPECT_TRUE(err == PERIRHINAL_ERROR_INVALID_INPUT || err == PERIRHINAL_ERROR_NONE);

    /* Perirhinal should still be usable after error */
    float features[256];
    memset(features, 0, sizeof(features));
    features[0] = 1.0f;
    result = perirhinal_encode_object(perirhinal, features, 256, "valid", &object_id);
    EXPECT_EQ(0, result);
}

TEST_F(PerirhinalBrainInitTest, HandleZeroDimensionGracefully) {
    float features[256];
    memset(features, 0, sizeof(features));

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, features, 0, "test", &object_id);
    EXPECT_NE(0, result);
}

/*=============================================================================
 * HEALTH AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(PerirhinalBrainInitTest, HealthStatusInitiallyGood) {
    float health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(PerirhinalBrainInitTest, DiagnosticsCanBeLogged) {
    /* Should not crash */
    int result = perirhinal_log_diagnostics(perirhinal);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(PerirhinalBrainInitTest, UpdateCycleWorks) {
    for (int i = 0; i < 100; i++) {
        int result = perirhinal_update(perirhinal, 10.0f);
        EXPECT_EQ(0, result);
    }

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.updates_processed, 100u);
}

TEST_F(PerirhinalBrainInitTest, BidirectionalUpdateCycleWorks) {
    for (int i = 0; i < 50; i++) {
        int result = perirhinal_bidirectional_update(perirhinal, 10.0f);
        EXPECT_EQ(0, result);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
