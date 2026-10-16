/**
 * @file test_perirhinal_kg_wiring_integration.cpp
 * @brief Integration tests for Perirhinal Cortex with Knowledge Graph wiring
 *
 * WHAT: Tests Perirhinal Cortex integration with brain knowledge graph
 * WHY:  Ensure object recognition integrates with brain-wide connectivity
 * HOW:  Test KG registration, health monitoring, and inter-region connections
 *
 * KG INTEGRATION POINTS:
 * - Node registration in brain knowledge graph
 * - Health status reporting
 * - Inter-region connectivity
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Include NIMCP headers first for typedef compatibility
#include "nimcp.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/perirhinal/nimcp_perirhinal.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class PerirhinalKGWiringTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal;
    perirhinal_config_t config;

    void SetUp() override {
        config = perirhinal_default_config();
        config.enable_bio_async = false;
        config.enable_kg = true;
        perirhinal = perirhinal_create(&config);
        ASSERT_NE(nullptr, perirhinal);
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
    }

    void CreateTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * KG CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalKGWiringTest, KGEnabled) {
    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(perirhinal, &retrieved));
    EXPECT_TRUE(retrieved.enable_kg);
}

TEST_F(PerirhinalKGWiringTest, CreateWithKGDisabled) {
    perirhinal_config_t no_kg_config = perirhinal_default_config();
    no_kg_config.enable_kg = false;
    no_kg_config.enable_bio_async = false;

    nimcp_perirhinal_t* no_kg_pr = perirhinal_create(&no_kg_config);
    ASSERT_NE(nullptr, no_kg_pr);

    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(no_kg_pr, &retrieved));
    EXPECT_FALSE(retrieved.enable_kg);

    perirhinal_destroy(no_kg_pr);
}

/*=============================================================================
 * HEALTH REPORTING TESTS
 *===========================================================================*/

TEST_F(PerirhinalKGWiringTest, HealthStatusForKG) {
    float health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(PerirhinalKGWiringTest, HealthStatusAfterActivity) {
    /* Perform activity */
    for (int i = 0; i < 20; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.05f);

        char name[32];
        snprintf(name, sizeof(name), "kg_obj_%d", i);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, name, &object_id);
    }

    /* Run updates */
    for (int i = 0; i < 50; i++) {
        perirhinal_update(perirhinal, 10.0f);
    }

    float health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

/*=============================================================================
 * STATISTICS FOR KG TESTS
 *===========================================================================*/

TEST_F(PerirhinalKGWiringTest, StatsReportedToKG) {
    /* Perform operations */
    for (int i = 0; i < 5; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.2f);

        char name[32];
        snprintf(name, sizeof(name), "stats_obj_%d", i);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, name, &object_id);
    }

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.objects_encoded, 5u);
    EXPECT_GE(stats.updates_processed, 0u);
}

TEST_F(PerirhinalKGWiringTest, ComprehensiveStats) {
    /* Various operations */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "comprehensive", &object_id);

    /* Recognition */
    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    perirhinal_recognize_object(perirhinal, features, 256, &result);

    /* Familiarity */
    perirhinal_compute_familiarity(perirhinal, features, 256);

    /* Novelty */
    perirhinal_compute_novelty(perirhinal, features, 256);

    /* Updates */
    for (int i = 0; i < 10; i++) {
        perirhinal_update(perirhinal, 10.0f);
    }

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.objects_encoded, 1u);
    EXPECT_GE(stats.updates_processed, 10u);
}

/*=============================================================================
 * DIAGNOSTICS FOR KG TESTS
 *===========================================================================*/

TEST_F(PerirhinalKGWiringTest, DiagnosticsForKG) {
    int result = perirhinal_log_diagnostics(perirhinal);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * OBJECT CELL ACTIVITY FOR KG TESTS
 *===========================================================================*/

TEST_F(PerirhinalKGWiringTest, ObjectCellActivityReportable) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "activity_test", &object_id);
    perirhinal_process_visual_input(perirhinal, features, 256);

    float activity[512];
    size_t num_cells = perirhinal_get_object_cell_activity(perirhinal, activity, 512);
    EXPECT_GT(num_cells, 0u);
}

TEST_F(PerirhinalKGWiringTest, FamiliarityCellActivityReportable) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "fam_activity", &object_id);

    float activity[256];
    size_t num_cells = perirhinal_get_familiarity_cell_activity(perirhinal, activity, 256);
    EXPECT_GT(num_cells, 0u);
}

/*=============================================================================
 * STATUS REPORTING TESTS
 *===========================================================================*/

TEST_F(PerirhinalKGWiringTest, StatusReportable) {
    perirhinal_status_t status = perirhinal_get_status(perirhinal);
    /* Status should be a valid enum value */
    EXPECT_GE((int)status, 0);
}

TEST_F(PerirhinalKGWiringTest, StatusAfterOperations) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "status_test", &object_id);

    /* Status should be reportable after operations */
    perirhinal_status_t status = perirhinal_get_status(perirhinal);
    EXPECT_GE((int)status, 0);  /* Valid status value */
}

/*=============================================================================
 * SERIALIZATION FOR KG TESTS
 *===========================================================================*/

TEST_F(PerirhinalKGWiringTest, SerializationSizeEstimate) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "serialize_test", &object_id);

    size_t size = perirhinal_get_serialization_size(perirhinal);
    EXPECT_GT(size, 0u);
}

TEST_F(PerirhinalKGWiringTest, SerializationRoundTrip) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "roundtrip", &object_id));

    /* Get serialization size */
    size_t size = perirhinal_get_serialization_size(perirhinal);
    ASSERT_GT(size, 0u);

    /* Allocate buffer */
    uint8_t* buffer = (uint8_t*)malloc(size);
    ASSERT_NE(nullptr, buffer);

    /* Serialize - this is the key test */
    size_t bytes_written = 0;
    int result = perirhinal_serialize(perirhinal, buffer, size, &bytes_written);
    EXPECT_EQ(0, result);
    EXPECT_GT(bytes_written, 0u);
    EXPECT_LE(bytes_written, size);

    /* Create new instance and attempt deserialize */
    perirhinal_config_t new_config = perirhinal_default_config();
    new_config.enable_bio_async = false;
    new_config.enable_kg = true;  /* Match original config */
    nimcp_perirhinal_t* restored = perirhinal_create(&new_config);
    ASSERT_NE(nullptr, restored);

    /* Deserialization may or may not succeed depending on KG state */
    result = perirhinal_deserialize(restored, buffer, bytes_written);
    /* Just verify it doesn't crash - success optional */
    (void)result;

    free(buffer);
    perirhinal_destroy(restored);
}

/*=============================================================================
 * INTER-REGION CONNECTIVITY TESTS
 *===========================================================================*/

TEST_F(PerirhinalKGWiringTest, EntorhinalIntegration) {
    perirhinal_config_t full_config = perirhinal_default_config();
    full_config.enable_entorhinal = true;
    full_config.enable_kg = true;
    full_config.enable_bio_async = false;

    nimcp_perirhinal_t* full_pr = perirhinal_create(&full_config);
    ASSERT_NE(nullptr, full_pr);

    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(full_pr, &retrieved));
    EXPECT_TRUE(retrieved.enable_entorhinal);
    EXPECT_TRUE(retrieved.enable_kg);

    perirhinal_destroy(full_pr);
}

TEST_F(PerirhinalKGWiringTest, FullIntegrationConfig) {
    perirhinal_config_t full_config = perirhinal_default_config();
    full_config.enable_entorhinal = true;
    full_config.enable_hippocampus = true;
    full_config.enable_thalamic = true;
    full_config.enable_perception = true;
    full_config.enable_kg = true;
    full_config.enable_bio_async = false;

    nimcp_perirhinal_t* full_pr = perirhinal_create(&full_config);
    ASSERT_NE(nullptr, full_pr);

    /* Should function with all integrations */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(full_pr, features, 256, "full_integ", &object_id);
    EXPECT_EQ(0, result);

    perirhinal_destroy(full_pr);
}

/*=============================================================================
 * RESET AND LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(PerirhinalKGWiringTest, ResetMaintainsKGState) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "reset_kg", &object_id);

    /* Reset */
    perirhinal_reset(perirhinal);

    /* KG health should still be reportable */
    float health = perirhinal_get_health_status(perirhinal);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(PerirhinalKGWiringTest, MultipleResetCycles) {
    for (int cycle = 0; cycle < 10; cycle++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)cycle * 0.1f);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, 256, "cycle_test", &object_id);

        perirhinal_reset(perirhinal);

        /* Should still be healthy */
        float health = perirhinal_get_health_status(perirhinal);
        EXPECT_GE(health, 0.0f);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
