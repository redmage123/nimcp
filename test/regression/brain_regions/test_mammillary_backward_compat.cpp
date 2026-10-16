/**
 * @file test_mammillary_backward_compat.cpp
 * @brief Backward compatibility regression tests for Mammillary Bodies
 *
 * WHAT: Tests Mammillary Bodies API stability and backward compatibility
 * WHY:  Ensure existing mammillary code continues to work after updates
 * HOW:  Test core API functions, data structures, and return values
 *
 * REGRESSION FOCUS:
 * - API function signatures unchanged
 * - Return value semantics preserved
 * - Default behaviors maintained
 * - Error codes consistent
 * - Configuration defaults stable
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/mammillary/nimcp_mammillary.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MammillaryBackwardCompatTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mammillary;
    mammillary_config_t config;

    static constexpr uint32_t TRACE_DIM = 128;

    void SetUp() override {
        config = mammillary_default_config();
        config.enable_papez_circuit = true;
        config.enable_head_direction = true;
        mammillary = mammillary_create(&config);
        ASSERT_NE(nullptr, mammillary);
    }

    void TearDown() override {
        if (mammillary) {
            mammillary_destroy(mammillary);
            mammillary = nullptr;
        }
    }

    void CreateTestTrace(float* trace, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            trace[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * API FUNCTION SIGNATURE TESTS
 *===========================================================================*/

TEST_F(MammillaryBackwardCompatTest, API_mammillary_default_config_exists) {
    mammillary_config_t cfg = mammillary_default_config();
    EXPECT_TRUE(true);  /* Compilation success = function exists */
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_create_exists) {
    mammillary_config_t cfg = mammillary_default_config();
    nimcp_mammillary_t* test = mammillary_create(&cfg);
    ASSERT_NE(nullptr, test);
    mammillary_destroy(test);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_destroy_exists) {
    mammillary_config_t cfg = mammillary_default_config();
    nimcp_mammillary_t* test = mammillary_create(&cfg);
    mammillary_destroy(test);
    mammillary_destroy(nullptr);  /* Should handle NULL safely */
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_receive_hippocampal_input_exists) {
    float trace[TRACE_DIM];
    CreateTestTrace(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_relay_to_thalamus_exists) {
    float trace[TRACE_DIM];
    CreateTestTrace(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);

    int result = mammillary_relay_to_thalamus(mammillary, trace_id);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_process_papez_cycle_exists) {
    int result = mammillary_process_papez_cycle(mammillary);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_get_papez_phase_exists) {
    papez_phase_t phase = mammillary_get_papez_phase(mammillary);
    EXPECT_GE((int)phase, 0);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_advance_papez_phase_exists) {
    mammillary_advance_papez_phase(mammillary);
    EXPECT_TRUE(true);  /* Compilation success */
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_update_head_direction_exists) {
    int result = mammillary_update_head_direction(mammillary, 0.5f, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_get_head_direction_exists) {
    mammillary_update_head_direction(mammillary, 0.5f, 10.0f);
    float heading = mammillary_get_head_direction(mammillary);
    EXPECT_GE(heading, -M_PI);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_reset_exists) {
    mammillary_reset(mammillary);
    EXPECT_TRUE(true);  /* Compilation success */
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_get_status_exists) {
    mammillary_status_t status = mammillary_get_status(mammillary);
    EXPECT_GE((int)status, 0);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_get_config_exists) {
    mammillary_config_t retrieved;
    int result = mammillary_get_config(mammillary, &retrieved);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_get_stats_exists) {
    mammillary_stats_t stats;
    int result = mammillary_get_stats(mammillary, &stats);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_start_consolidation_exists) {
    float trace[TRACE_DIM];
    CreateTestTrace(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);

    int result = mammillary_start_consolidation(mammillary, trace_id);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBackwardCompatTest, API_mammillary_strengthen_trace_exists) {
    float trace[TRACE_DIM];
    CreateTestTrace(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);

    int result = mammillary_strengthen_trace(mammillary, trace_id, 0.2f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * RETURN VALUE SEMANTICS TESTS
 *===========================================================================*/

TEST_F(MammillaryBackwardCompatTest, ReturnSemantics_ReceiveReturnsZeroOnSuccess) {
    float trace[TRACE_DIM];
    CreateTestTrace(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    EXPECT_EQ(0, mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id));
}

TEST_F(MammillaryBackwardCompatTest, ReturnSemantics_NullHandledGracefully) {
    float trace[TRACE_DIM];
    CreateTestTrace(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    int result = mammillary_receive_hippocampal_input(nullptr, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    EXPECT_NE(0, result);
}

/*=============================================================================
 * DEFAULT BEHAVIOR TESTS
 *===========================================================================*/

TEST_F(MammillaryBackwardCompatTest, DefaultConfig_HasReasonableValues) {
    mammillary_config_t cfg = mammillary_default_config();

    EXPECT_GT(cfg.max_memory_traces, 0u);
}

TEST_F(MammillaryBackwardCompatTest, DefaultStats_AreZero) {
    mammillary_stats_t stats;
    mammillary_get_stats(mammillary, &stats);

    EXPECT_EQ(0u, stats.total_memory_traces);
    EXPECT_EQ(0u, stats.relay_operations);
}

/*=============================================================================
 * MEMORY TRACE TYPE TESTS
 *===========================================================================*/

TEST_F(MammillaryBackwardCompatTest, TraceTypes_AllTypesAccepted) {
    memory_trace_type_t types[] = {
        MEMORY_TRACE_EPISODIC,
        MEMORY_TRACE_SPATIAL,
        MEMORY_TRACE_CONTEXTUAL,
        MEMORY_TRACE_EMOTIONAL
    };

    for (int i = 0; i < 4; i++) {
        float trace[TRACE_DIM];
        CreateTestTrace(trace, TRACE_DIM, (float)i * 0.2f);

        uint32_t trace_id = 0;
        int result = mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
            types[i], 0.5f, &trace_id);
        EXPECT_EQ(0, result);
    }
}

/*=============================================================================
 * STRUCTURE SIZE TESTS
 *===========================================================================*/

TEST_F(MammillaryBackwardCompatTest, StructSize_mammillary_config_t) {
    EXPECT_GT(sizeof(mammillary_config_t), sizeof(uint32_t) * 2);
}

TEST_F(MammillaryBackwardCompatTest, StructSize_mammillary_stats_t) {
    EXPECT_GT(sizeof(mammillary_stats_t), sizeof(uint32_t) * 2);
}

/*=============================================================================
 * LIFECYCLE COMPATIBILITY TESTS
 *===========================================================================*/

TEST_F(MammillaryBackwardCompatTest, Lifecycle_CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        mammillary_config_t cfg = mammillary_default_config();
        nimcp_mammillary_t* test = mammillary_create(&cfg);
        EXPECT_NE(nullptr, test);
        mammillary_destroy(test);
    }
}

TEST_F(MammillaryBackwardCompatTest, Lifecycle_ResetMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        float trace[TRACE_DIM];
        CreateTestTrace(trace, TRACE_DIM, (float)i * 0.1f);

        uint32_t trace_id = 0;
        mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
            MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
        mammillary_reset(mammillary);
    }
}

TEST_F(MammillaryBackwardCompatTest, Lifecycle_OperationsAfterReset) {
    float trace[TRACE_DIM];
    CreateTestTrace(trace, TRACE_DIM, 0.5f);

    uint32_t id1 = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &id1);
    mammillary_reset(mammillary);

    uint32_t id2 = 0;
    int result = mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &id2);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * PAPEZ CIRCUIT TESTS
 *===========================================================================*/

TEST_F(MammillaryBackwardCompatTest, PapezCircuit_MultiplePhaseAdvances) {
    for (int i = 0; i < 10; i++) {
        mammillary_advance_papez_phase(mammillary);
    }
    papez_phase_t phase = mammillary_get_papez_phase(mammillary);
    EXPECT_GE((int)phase, 0);
}

TEST_F(MammillaryBackwardCompatTest, PapezCircuit_MultipleCycles) {
    for (int i = 0; i < 5; i++) {
        int result = mammillary_process_papez_cycle(mammillary);
        EXPECT_EQ(0, result);
    }
}

/*=============================================================================
 * HEAD DIRECTION TESTS
 *===========================================================================*/

TEST_F(MammillaryBackwardCompatTest, HeadDirection_MultipleUpdates) {
    for (int i = 0; i < 20; i++) {
        int result = mammillary_update_head_direction(mammillary, 0.1f, 10.0f);
        EXPECT_EQ(0, result);
    }

    float heading = mammillary_get_head_direction(mammillary);
    EXPECT_GE(heading, -2 * M_PI);
    EXPECT_LE(heading, 2 * M_PI);
}

TEST_F(MammillaryBackwardCompatTest, HeadDirection_LandmarkCorrection) {
    /* Create drift */
    for (int i = 0; i < 10; i++) {
        mammillary_update_head_direction(mammillary, 0.1f, 10.0f);
    }

    /* Correct with landmark */
    int result = mammillary_set_hd_from_landmark(mammillary, 0.0f, 0.9f);
    EXPECT_EQ(0, result);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
