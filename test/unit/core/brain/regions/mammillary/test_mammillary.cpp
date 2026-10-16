/**
 * @file test_mammillary.cpp
 * @brief Unit tests for Mammillary Bodies
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/mammillary/nimcp_mammillary.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MammillaryTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mb = nullptr;

    void SetUp() override {
        mammillary_config_t config = mammillary_default_config();
        mb = mammillary_create(&config);
        ASSERT_NE(mb, nullptr);
    }

    void TearDown() override {
        if (mb) {
            mammillary_destroy(mb);
            mb = nullptr;
        }
    }

    void createTestTrace(float* trace, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            trace[i] = base_value + sinf(i * 0.1f) * 0.5f;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, CreateWithDefaultConfig) {
    nimcp_mammillary_t* m = mammillary_create(nullptr);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->status, MAMMILLARY_STATUS_READY);
    EXPECT_EQ(m->num_hd_cells, MAMMILLARY_DEFAULT_HD_CELLS);
    EXPECT_EQ(m->num_relay_cells, MAMMILLARY_DEFAULT_RELAY_CELLS);
    EXPECT_EQ(m->num_spatial_cells, MAMMILLARY_DEFAULT_SPATIAL_CELLS);
    mammillary_destroy(m);
}

TEST_F(MammillaryTest, CreateWithCustomConfig) {
    mammillary_config_t config = mammillary_default_config();
    config.num_hd_cells = 32;
    config.num_relay_cells = 64;
    config.num_spatial_cells = 32;
    config.max_memory_traces = 256;

    nimcp_mammillary_t* m = mammillary_create(&config);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->num_hd_cells, 32u);
    EXPECT_EQ(m->num_relay_cells, 64u);
    EXPECT_EQ(m->num_spatial_cells, 32u);
    EXPECT_EQ(m->max_memory_traces, 256u);
    mammillary_destroy(m);
}

TEST_F(MammillaryTest, DestroyNull) {
    mammillary_destroy(nullptr);
    SUCCEED();
}

TEST_F(MammillaryTest, Reset) {
    mb->updates_processed = 100;
    mb->relay_operations = 50;
    mb->consolidations_completed = 10;
    mb->current_heading = 1.5f;

    EXPECT_EQ(mammillary_reset(mb), 0);

    EXPECT_EQ(mb->updates_processed, 0u);
    EXPECT_EQ(mb->relay_operations, 0u);
    EXPECT_EQ(mb->consolidations_completed, 0u);
    EXPECT_FLOAT_EQ(mb->current_heading, 0.0f);
    EXPECT_EQ(mb->status, MAMMILLARY_STATUS_READY);
}

TEST_F(MammillaryTest, ResetNull) {
    EXPECT_EQ(mammillary_reset(nullptr), -1);
}

TEST_F(MammillaryTest, Update) {
    EXPECT_EQ(mammillary_update(mb, 0.01f), 0);
    EXPECT_EQ(mb->updates_processed, 1u);
}

TEST_F(MammillaryTest, UpdateNull) {
    EXPECT_EQ(mammillary_update(nullptr, 0.01f), -1);
}

TEST_F(MammillaryTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(mammillary_update(mb, 0.01f), 0);
    }
    EXPECT_EQ(mb->updates_processed, 100u);
}

/*=============================================================================
 * HEAD DIRECTION TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, UpdateHeadDirection) {
    float initial = mb->current_heading;
    EXPECT_EQ(mammillary_update_head_direction(mb, 1.0f, 0.1f), 0);
    EXPECT_GT(mb->current_heading, initial);
}

TEST_F(MammillaryTest, UpdateHeadDirectionNull) {
    EXPECT_EQ(mammillary_update_head_direction(nullptr, 1.0f, 0.1f), -1);
}

TEST_F(MammillaryTest, SetHdFromLandmark) {
    mb->current_heading = 0.0f;
    EXPECT_EQ(mammillary_set_hd_from_landmark(mb, (float)M_PI / 2.0f, 0.9f), 0);
    EXPECT_GT(mb->current_heading, 0.0f);
    EXPECT_EQ(mb->hd_corrections, 1u);
}

TEST_F(MammillaryTest, SetHdFromLandmarkNull) {
    EXPECT_EQ(mammillary_set_hd_from_landmark(nullptr, 0.0f, 0.5f), -1);
}

TEST_F(MammillaryTest, GetHeadDirection) {
    mb->current_heading = 1.57f;
    EXPECT_FLOAT_EQ(mammillary_get_head_direction(mb), 1.57f);
}

TEST_F(MammillaryTest, GetHeadDirectionNull) {
    EXPECT_FLOAT_EQ(mammillary_get_head_direction(nullptr), 0.0f);
}

TEST_F(MammillaryTest, GetHdConfidence) {
    mb->heading_confidence = 0.85f;
    EXPECT_FLOAT_EQ(mammillary_get_hd_confidence(mb), 0.85f);
}

TEST_F(MammillaryTest, GetHdConfidenceNull) {
    EXPECT_FLOAT_EQ(mammillary_get_hd_confidence(nullptr), 0.0f);
}

TEST_F(MammillaryTest, GetHdPopulationVector) {
    float vector[2];
    uint32_t dim;
    EXPECT_EQ(mammillary_get_hd_population_vector(mb, vector, &dim), 0);
    EXPECT_EQ(dim, 2u);
}

TEST_F(MammillaryTest, GetHdPopulationVectorNull) {
    float vector[2];
    uint32_t dim;
    EXPECT_EQ(mammillary_get_hd_population_vector(nullptr, vector, &dim), -1);
    EXPECT_EQ(mammillary_get_hd_population_vector(mb, nullptr, &dim), -1);
    EXPECT_EQ(mammillary_get_hd_population_vector(mb, vector, nullptr), -1);
}

TEST_F(MammillaryTest, CorrectHdDrift) {
    mb->current_heading = 0.0f;
    EXPECT_EQ(mammillary_correct_hd_drift(mb, 0.5f), 0);
    EXPECT_GT(mb->current_heading, 0.0f);
    EXPECT_EQ(mb->hd_corrections, 1u);
}

TEST_F(MammillaryTest, CorrectHdDriftNull) {
    EXPECT_EQ(mammillary_correct_hd_drift(nullptr, 0.0f), -1);
}

TEST_F(MammillaryTest, GetActiveHdCells) {
    /* Update head direction to activate some cells */
    mammillary_update_head_direction(mb, 0.0f, 0.01f);
    mammillary_update(mb, 0.01f);

    uint32_t cell_ids[100];
    float firing_rates[100];
    uint32_t num_active;
    EXPECT_EQ(mammillary_get_active_hd_cells(mb, cell_ids, firing_rates, 100, &num_active), 0);
    EXPECT_GT(num_active, 0u);
}

TEST_F(MammillaryTest, GetActiveHdCellsNull) {
    uint32_t cell_ids[100];
    float firing_rates[100];
    uint32_t num_active;
    EXPECT_EQ(mammillary_get_active_hd_cells(nullptr, cell_ids, firing_rates, 100, &num_active), -1);
    EXPECT_EQ(mammillary_get_active_hd_cells(mb, nullptr, firing_rates, 100, &num_active), -1);
}

TEST_F(MammillaryTest, HeadDirectionWraparound) {
    /* Test that head direction wraps around correctly */
    mb->current_heading = 0.0f;
    for (int i = 0; i < 100; i++) {
        mammillary_update_head_direction(mb, 0.1f, 0.1f);
    }
    /* Should have wrapped around */
    float heading = mammillary_get_head_direction(mb);
    EXPECT_GE(heading, 0.0f);
    EXPECT_LT(heading, 2.0f * M_PI);
}

/*=============================================================================
 * MEMORY RELAY TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, ReceiveHippocampalInput) {
    float trace[256];
    createTestTrace(trace, 256, 0.5f);

    uint32_t trace_id;
    EXPECT_EQ(mammillary_receive_hippocampal_input(mb, trace, 256,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id), 0);
    EXPECT_LT(trace_id, mb->max_memory_traces);
    EXPECT_EQ(mb->num_memory_traces, 1u);
}

TEST_F(MammillaryTest, ReceiveHippocampalInputNull) {
    float trace[256];
    uint32_t id;
    EXPECT_EQ(mammillary_receive_hippocampal_input(nullptr, trace, 256,
        MEMORY_TRACE_EPISODIC, 0.0f, &id), -1);
    EXPECT_EQ(mammillary_receive_hippocampal_input(mb, nullptr, 256,
        MEMORY_TRACE_EPISODIC, 0.0f, &id), -1);
}

TEST_F(MammillaryTest, ReceiveHippocampalInputZeroDim) {
    float trace[256];
    uint32_t id;
    EXPECT_EQ(mammillary_receive_hippocampal_input(mb, trace, 0,
        MEMORY_TRACE_EPISODIC, 0.0f, &id), -1);
}

TEST_F(MammillaryTest, ReceiveMultipleTraces) {
    float trace[256];
    uint32_t ids[10];

    for (int i = 0; i < 10; i++) {
        createTestTrace(trace, 256, (float)i * 0.1f);
        EXPECT_EQ(mammillary_receive_hippocampal_input(mb, trace, 256,
            (memory_trace_type_t)(i % 6), (float)i * 0.1f - 0.5f, &ids[i]), 0);
    }

    EXPECT_EQ(mb->num_memory_traces, 10u);

    /* Verify unique IDs */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(MammillaryTest, RelayToThalamus) {
    float trace[256];
    createTestTrace(trace, 256, 0.5f);

    uint32_t trace_id;
    mammillary_receive_hippocampal_input(mb, trace, 256,
        MEMORY_TRACE_EPISODIC, 0.0f, &trace_id);

    EXPECT_EQ(mammillary_relay_to_thalamus(mb, trace_id), 0);
    EXPECT_GT(mb->thalamus_bridge.output_buffer_size, 0u);
    EXPECT_EQ(mb->relay_operations, 1u);
}

TEST_F(MammillaryTest, RelayToThalamusInvalid) {
    EXPECT_EQ(mammillary_relay_to_thalamus(mb, 99999), -1);
    EXPECT_EQ(mammillary_relay_to_thalamus(nullptr, 0), -1);
}

TEST_F(MammillaryTest, GetTrace) {
    float trace[256];
    createTestTrace(trace, 256, 0.5f);

    uint32_t trace_id;
    mammillary_receive_hippocampal_input(mb, trace, 256,
        MEMORY_TRACE_SPATIAL, 0.7f, &trace_id);

    const nimcp_memory_trace_t* mt = mammillary_get_trace(mb, trace_id);
    ASSERT_NE(mt, nullptr);
    EXPECT_EQ(mt->trace_id, trace_id);
    EXPECT_EQ(mt->type, MEMORY_TRACE_SPATIAL);
    EXPECT_FLOAT_EQ(mt->emotional_intensity, 0.7f);
}

TEST_F(MammillaryTest, GetTraceInvalid) {
    EXPECT_EQ(mammillary_get_trace(mb, 99999), nullptr);
    EXPECT_EQ(mammillary_get_trace(nullptr, 0), nullptr);
}

/*=============================================================================
 * PAPEZ CIRCUIT TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, ProcessPapezCycle) {
    EXPECT_EQ(mammillary_process_papez_cycle(mb), 0);
}

TEST_F(MammillaryTest, ProcessPapezCycleNull) {
    EXPECT_EQ(mammillary_process_papez_cycle(nullptr), -1);
}

TEST_F(MammillaryTest, GetPapezPhase) {
    EXPECT_EQ(mammillary_get_papez_phase(mb), PAPEZ_PHASE_IDLE);
}

TEST_F(MammillaryTest, GetPapezPhaseNull) {
    EXPECT_EQ(mammillary_get_papez_phase(nullptr), PAPEZ_PHASE_IDLE);
}

TEST_F(MammillaryTest, GetPapezActivity) {
    float activity = mammillary_get_papez_activity(mb);
    EXPECT_GE(activity, 0.0f);
}

TEST_F(MammillaryTest, GetPapezActivityNull) {
    EXPECT_FLOAT_EQ(mammillary_get_papez_activity(nullptr), 0.0f);
}

TEST_F(MammillaryTest, AdvancePapezPhase) {
    EXPECT_EQ(mammillary_advance_papez_phase(mb), 0);
    EXPECT_EQ(mb->papez_phase, PAPEZ_PHASE_HIPPOCAMPAL_INPUT);

    EXPECT_EQ(mammillary_advance_papez_phase(mb), 0);
    EXPECT_EQ(mb->papez_phase, PAPEZ_PHASE_MAMMILLARY_RELAY);

    EXPECT_EQ(mammillary_advance_papez_phase(mb), 0);
    EXPECT_EQ(mb->papez_phase, PAPEZ_PHASE_THALAMIC_OUTPUT);

    EXPECT_EQ(mammillary_advance_papez_phase(mb), 0);
    EXPECT_EQ(mb->papez_phase, PAPEZ_PHASE_CINGULATE_FEEDBACK);

    EXPECT_EQ(mammillary_advance_papez_phase(mb), 0);
    EXPECT_EQ(mb->papez_phase, PAPEZ_PHASE_COMPLETE);

    EXPECT_EQ(mammillary_advance_papez_phase(mb), 0);
    EXPECT_EQ(mb->papez_phase, PAPEZ_PHASE_IDLE);
}

TEST_F(MammillaryTest, AdvancePapezPhaseNull) {
    EXPECT_EQ(mammillary_advance_papez_phase(nullptr), -1);
}

/*=============================================================================
 * CONSOLIDATION TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, StartConsolidation) {
    float trace[256];
    createTestTrace(trace, 256, 0.5f);

    uint32_t trace_id;
    mammillary_receive_hippocampal_input(mb, trace, 256,
        MEMORY_TRACE_EPISODIC, 0.0f, &trace_id);

    EXPECT_EQ(mammillary_start_consolidation(mb, trace_id), 0);
    EXPECT_EQ(mb->consolidation_state, CONSOLIDATION_STRENGTHENING);
}

TEST_F(MammillaryTest, StartConsolidationInvalid) {
    EXPECT_EQ(mammillary_start_consolidation(mb, 99999), -1);
    EXPECT_EQ(mammillary_start_consolidation(nullptr, 0), -1);
}

TEST_F(MammillaryTest, UpdateConsolidation) {
    float trace[256];
    createTestTrace(trace, 256, 0.5f);

    uint32_t trace_id;
    mammillary_receive_hippocampal_input(mb, trace, 256,
        MEMORY_TRACE_EPISODIC, 0.0f, &trace_id);

    /* Run consolidation updates */
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(mammillary_update_consolidation(mb, 0.1f), 0);
    }

    const nimcp_memory_trace_t* mt = mammillary_get_trace(mb, trace_id);
    EXPECT_GT(mt->strength, 0.0f);
}

TEST_F(MammillaryTest, UpdateConsolidationNull) {
    EXPECT_EQ(mammillary_update_consolidation(nullptr, 0.1f), -1);
}

TEST_F(MammillaryTest, GetConsolidationState) {
    EXPECT_EQ(mammillary_get_consolidation_state(mb), CONSOLIDATION_IDLE);
}

TEST_F(MammillaryTest, GetConsolidationStateNull) {
    EXPECT_EQ(mammillary_get_consolidation_state(nullptr), CONSOLIDATION_IDLE);
}

TEST_F(MammillaryTest, StrengthenTrace) {
    float trace[256];
    createTestTrace(trace, 256, 0.5f);

    uint32_t trace_id;
    mammillary_receive_hippocampal_input(mb, trace, 256,
        MEMORY_TRACE_EPISODIC, 0.0f, &trace_id);

    const nimcp_memory_trace_t* mt = mammillary_get_trace(mb, trace_id);
    float initial_strength = mt->strength;

    EXPECT_EQ(mammillary_strengthen_trace(mb, trace_id, 0.2f), 0);
    EXPECT_GT(mt->strength, initial_strength);
    EXPECT_EQ(mt->retrieval_count, 1u);
}

TEST_F(MammillaryTest, StrengthenTraceInvalid) {
    EXPECT_EQ(mammillary_strengthen_trace(mb, 99999, 0.2f), -1);
    EXPECT_EQ(mammillary_strengthen_trace(nullptr, 0, 0.2f), -1);
}

TEST_F(MammillaryTest, GetTracesByType) {
    float trace[256];

    /* Create traces of different types */
    for (int i = 0; i < 6; i++) {
        createTestTrace(trace, 256, (float)i * 0.1f);
        uint32_t id;
        mammillary_receive_hippocampal_input(mb, trace, 256,
            (memory_trace_type_t)i, 0.0f, &id);
    }

    uint32_t found_ids[10];
    uint32_t num_found;
    EXPECT_EQ(mammillary_get_traces_by_type(mb, MEMORY_TRACE_SPATIAL,
        found_ids, 10, &num_found), 0);
    EXPECT_EQ(num_found, 1u);
}

TEST_F(MammillaryTest, GetTracesByTypeNull) {
    uint32_t found_ids[10];
    uint32_t num_found;
    EXPECT_EQ(mammillary_get_traces_by_type(nullptr, MEMORY_TRACE_SPATIAL,
        found_ids, 10, &num_found), -1);
    EXPECT_EQ(mammillary_get_traces_by_type(mb, MEMORY_TRACE_SPATIAL,
        nullptr, 10, &num_found), -1);
}

TEST_F(MammillaryTest, GetStrongestTraces) {
    float trace[256];

    /* Create traces with different strengths */
    for (int i = 0; i < 5; i++) {
        createTestTrace(trace, 256, (float)i * 0.1f);
        uint32_t id;
        mammillary_receive_hippocampal_input(mb, trace, 256,
            MEMORY_TRACE_EPISODIC, (float)i * 0.2f, &id);
        mammillary_strengthen_trace(mb, id, (float)i * 0.1f);
    }

    uint32_t trace_ids[10];
    float strengths[10];
    uint32_t num_returned;
    EXPECT_EQ(mammillary_get_strongest_traces(mb, trace_ids, strengths, 3, &num_returned), 0);
    EXPECT_EQ(num_returned, 3u);

    /* Verify sorted order (strongest first) */
    for (uint32_t i = 1; i < num_returned; i++) {
        EXPECT_GE(strengths[i - 1], strengths[i]);
    }
}

TEST_F(MammillaryTest, DecayTraces) {
    float trace[256];
    createTestTrace(trace, 256, 0.5f);

    uint32_t trace_id;
    mammillary_receive_hippocampal_input(mb, trace, 256,
        MEMORY_TRACE_EPISODIC, 0.0f, &trace_id);

    const nimcp_memory_trace_t* mt = mammillary_get_trace(mb, trace_id);
    float initial_strength = mt->strength;

    EXPECT_EQ(mammillary_decay_traces(mb, 0.1f), 0);
    EXPECT_LT(mt->strength, initial_strength);
}

TEST_F(MammillaryTest, DecayTracesNull) {
    EXPECT_EQ(mammillary_decay_traces(nullptr, 0.1f), -1);
}

TEST_F(MammillaryTest, RemoveTrace) {
    float trace[256];
    createTestTrace(trace, 256, 0.5f);

    uint32_t trace_id;
    mammillary_receive_hippocampal_input(mb, trace, 256,
        MEMORY_TRACE_EPISODIC, 0.0f, &trace_id);
    EXPECT_EQ(mb->num_memory_traces, 1u);

    EXPECT_EQ(mammillary_remove_trace(mb, trace_id), 0);
    EXPECT_EQ(mb->num_memory_traces, 0u);
    EXPECT_EQ(mammillary_get_trace(mb, trace_id), nullptr);
}

TEST_F(MammillaryTest, RemoveTraceInvalid) {
    EXPECT_EQ(mammillary_remove_trace(mb, 99999), -1);
    EXPECT_EQ(mammillary_remove_trace(nullptr, 0), -1);
}

/*=============================================================================
 * SPATIAL PROCESSING TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, UpdateSpatialCells) {
    float position[3] = {50.0f, 50.0f, 0.0f};
    EXPECT_EQ(mammillary_update_spatial_cells(mb, position, 3), 0);
}

TEST_F(MammillaryTest, UpdateSpatialCellsNull) {
    float position[3] = {0};
    EXPECT_EQ(mammillary_update_spatial_cells(nullptr, position, 3), -1);
    EXPECT_EQ(mammillary_update_spatial_cells(mb, nullptr, 3), -1);
}

TEST_F(MammillaryTest, EncodeSpatialMemory) {
    float position[3] = {100.0f, 100.0f, 0.0f};
    float context[128];
    createTestTrace(context, 128, 0.5f);

    uint32_t trace_id;
    EXPECT_EQ(mammillary_encode_spatial_memory(mb, position, 1.57f,
        context, 128, &trace_id), 0);

    const nimcp_memory_trace_t* mt = mammillary_get_trace(mb, trace_id);
    ASSERT_NE(mt, nullptr);
    EXPECT_TRUE(mt->has_spatial_context);
    EXPECT_FLOAT_EQ(mt->position[0], 100.0f);
    EXPECT_FLOAT_EQ(mt->heading, 1.57f);
}

TEST_F(MammillaryTest, EncodeSpatialMemoryNull) {
    float position[3] = {0};
    uint32_t id;
    EXPECT_EQ(mammillary_encode_spatial_memory(nullptr, position, 0.0f,
        nullptr, 0, &id), -1);
}

TEST_F(MammillaryTest, RetrieveSpatialContext) {
    /* Encode a spatial memory */
    float position[3] = {100.0f, 100.0f, 0.0f};
    float context[128];
    createTestTrace(context, 128, 0.5f);

    uint32_t trace_id;
    mammillary_encode_spatial_memory(mb, position, 1.57f, context, 128, &trace_id);

    /* Try to retrieve at nearby position */
    float query_pos[3] = {102.0f, 98.0f, 0.0f};
    float retrieved[256];
    uint32_t retrieved_dim;
    EXPECT_EQ(mammillary_retrieve_spatial_context(mb, query_pos, 3,
        retrieved, &retrieved_dim), 0);
    EXPECT_GT(retrieved_dim, 0u);
}

TEST_F(MammillaryTest, RetrieveSpatialContextNull) {
    float pos[3] = {0};
    float context[256];
    uint32_t dim;
    EXPECT_EQ(mammillary_retrieve_spatial_context(nullptr, pos, 3, context, &dim), -1);
    EXPECT_EQ(mammillary_retrieve_spatial_context(mb, nullptr, 3, context, &dim), -1);
    EXPECT_EQ(mammillary_retrieve_spatial_context(mb, pos, 3, nullptr, &dim), -1);
}

TEST_F(MammillaryTest, GetSpatialActivity) {
    float position[3] = {50.0f, 50.0f, 0.0f};
    mammillary_update_spatial_cells(mb, position, 3);

    float activity[100];
    uint32_t num_cells;
    EXPECT_EQ(mammillary_get_spatial_activity(mb, activity, 100, &num_cells), 0);
    EXPECT_GT(num_cells, 0u);
}

TEST_F(MammillaryTest, GetSpatialActivityNull) {
    float activity[100];
    uint32_t num_cells;
    EXPECT_EQ(mammillary_get_spatial_activity(nullptr, activity, 100, &num_cells), -1);
    EXPECT_EQ(mammillary_get_spatial_activity(mb, nullptr, 100, &num_cells), -1);
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, ProcessIncoming) {
    EXPECT_EQ(mammillary_process_incoming(mb), 0);
}

TEST_F(MammillaryTest, ProcessIncomingNull) {
    EXPECT_EQ(mammillary_process_incoming(nullptr), -1);
}

TEST_F(MammillaryTest, SendOutgoing) {
    EXPECT_EQ(mammillary_send_outgoing(mb), 0);
}

TEST_F(MammillaryTest, SendOutgoingNull) {
    EXPECT_EQ(mammillary_send_outgoing(nullptr), -1);
}

TEST_F(MammillaryTest, BidirectionalUpdate) {
    EXPECT_EQ(mammillary_bidirectional_update(mb, 0.01f), 0);
}

TEST_F(MammillaryTest, BidirectionalUpdateNull) {
    EXPECT_EQ(mammillary_bidirectional_update(nullptr, 0.01f), -1);
}

TEST_F(MammillaryTest, SyncHippocampus) {
    EXPECT_EQ(mammillary_sync_hippocampus(mb), 0);
}

TEST_F(MammillaryTest, SyncHippocampusNull) {
    EXPECT_EQ(mammillary_sync_hippocampus(nullptr), -1);
}

TEST_F(MammillaryTest, SyncThalamus) {
    EXPECT_EQ(mammillary_sync_thalamus(mb), 0);
}

TEST_F(MammillaryTest, SyncThalamusNull) {
    EXPECT_EQ(mammillary_sync_thalamus(nullptr), -1);
}

TEST_F(MammillaryTest, SendToThalamus) {
    EXPECT_EQ(mammillary_send_to_thalamus(mb), 0);
}

TEST_F(MammillaryTest, SendToThalamusNull) {
    EXPECT_EQ(mammillary_send_to_thalamus(nullptr), -1);
}

TEST_F(MammillaryTest, ReceiveCingulateFeedback) {
    EXPECT_EQ(mammillary_receive_cingulate_feedback(mb), 0);
}

TEST_F(MammillaryTest, ReceiveCingulateFeedbackNull) {
    EXPECT_EQ(mammillary_receive_cingulate_feedback(nullptr), -1);
}

/*=============================================================================
 * BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, InitHippocampusBridge) {
    EXPECT_EQ(mammillary_init_hippocampus_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->hippocampus_bridge.initialized);
    EXPECT_FLOAT_EQ(mb->hippocampus_bridge.fornix_strength, 0.8f);
}

TEST_F(MammillaryTest, InitThalamusBridge) {
    EXPECT_EQ(mammillary_init_thalamus_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->thalamus_bridge.initialized);
    EXPECT_FLOAT_EQ(mb->thalamus_bridge.tract_strength, 0.8f);
}

TEST_F(MammillaryTest, InitCingulateBridge) {
    EXPECT_EQ(mammillary_init_cingulate_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->cingulate_bridge.initialized);
    EXPECT_FLOAT_EQ(mb->cingulate_bridge.feedback_strength, 0.5f);
}

TEST_F(MammillaryTest, InitHypothalamusBridge) {
    EXPECT_EQ(mammillary_init_hypothalamus_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->hypothalamus_bridge.initialized);
}

TEST_F(MammillaryTest, InitVestibularBridge) {
    EXPECT_EQ(mammillary_init_vestibular_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->vestibular_bridge.initialized);
    EXPECT_FLOAT_EQ(mb->vestibular_bridge.update_rate, 100.0f);
}

TEST_F(MammillaryTest, InitEntorhinalBridge) {
    EXPECT_EQ(mammillary_init_entorhinal_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->entorhinal_bridge.initialized);
}

TEST_F(MammillaryTest, InitSecurityBridge) {
    EXPECT_EQ(mammillary_init_security_bridge(mb, nullptr, nullptr), 0);
    EXPECT_TRUE(mb->security_bridge.initialized);
    EXPECT_EQ(mb->security_bridge.access_level, 1u);
}

TEST_F(MammillaryTest, InitImmuneBridge) {
    EXPECT_EQ(mammillary_init_immune_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->immune_bridge.initialized);
    EXPECT_FLOAT_EQ(mb->immune_bridge.health_score, 1.0f);
}

TEST_F(MammillaryTest, InitBioAsyncBridge) {
    EXPECT_EQ(mammillary_init_bio_async_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->bio_async_bridge.initialized);
}

TEST_F(MammillaryTest, InitLoggingBridge) {
    EXPECT_EQ(mammillary_init_logging_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->logging_bridge.initialized);
}

TEST_F(MammillaryTest, InitResonanceBridge) {
    EXPECT_EQ(mammillary_init_resonance_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->resonance_bridge.initialized);
    EXPECT_FLOAT_EQ(mb->resonance_bridge.frequency, 6.0f);  /* Theta band (4-8Hz) */
}

TEST_F(MammillaryTest, InitCognitiveBridge) {
    EXPECT_EQ(mammillary_init_cognitive_bridge(mb, nullptr, nullptr), 0);
    EXPECT_TRUE(mb->cognitive_bridge.initialized);
}

TEST_F(MammillaryTest, InitLogicBridge) {
    EXPECT_EQ(mammillary_init_logic_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->logic_bridge.initialized);
}

TEST_F(MammillaryTest, InitSubstrateBridge) {
    EXPECT_EQ(mammillary_init_substrate_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->substrate_bridge.initialized);
    EXPECT_FLOAT_EQ(mb->substrate_bridge.atp_level, 1.0f);
}

TEST_F(MammillaryTest, InitThalamicBridge) {
    EXPECT_EQ(mammillary_init_thalamic_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->thalamic_bridge.initialized);
}

TEST_F(MammillaryTest, InitPerceptionBridge) {
    EXPECT_EQ(mammillary_init_perception_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->perception_bridge.initialized);
}

TEST_F(MammillaryTest, InitSnnBridge) {
    EXPECT_EQ(mammillary_init_snn_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->snn_bridge.initialized);
    EXPECT_TRUE(mb->snn_bridge.plasticity_enabled);
}

TEST_F(MammillaryTest, InitSwarmBridge) {
    EXPECT_EQ(mammillary_init_swarm_bridge(mb, nullptr, nullptr), 0);
    EXPECT_TRUE(mb->swarm_bridge.initialized);
}

TEST_F(MammillaryTest, InitCerebellumBridge) {
    EXPECT_EQ(mammillary_init_cerebellum_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->cerebellum_bridge.initialized);
}

TEST_F(MammillaryTest, InitMedullaBridge) {
    EXPECT_EQ(mammillary_init_medulla_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->medulla_bridge.initialized);
}

TEST_F(MammillaryTest, InitOmniBridge) {
    EXPECT_EQ(mammillary_init_omni_bridge(mb, nullptr), 0);
    EXPECT_TRUE(mb->omni_bridge.initialized);
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, GetStatus) {
    EXPECT_EQ(mammillary_get_status(mb), MAMMILLARY_STATUS_READY);
}

TEST_F(MammillaryTest, GetStatusNull) {
    EXPECT_EQ(mammillary_get_status(nullptr), MAMMILLARY_STATUS_ERROR);
}

TEST_F(MammillaryTest, GetLastError) {
    EXPECT_EQ(mammillary_get_last_error(mb), MAMMILLARY_ERROR_NONE);
}

TEST_F(MammillaryTest, GetLastErrorNull) {
    EXPECT_EQ(mammillary_get_last_error(nullptr), MAMMILLARY_ERROR_INTERNAL);
}

TEST_F(MammillaryTest, ErrorString) {
    EXPECT_STREQ(mammillary_error_string(MAMMILLARY_ERROR_NONE), "No error");
    EXPECT_STREQ(mammillary_error_string(MAMMILLARY_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(mammillary_error_string(MAMMILLARY_ERROR_MEMORY_FULL), "Memory full");
    EXPECT_STREQ(mammillary_error_string(MAMMILLARY_ERROR_RELAY_FAILED), "Relay failed");
    EXPECT_STREQ(mammillary_error_string(MAMMILLARY_ERROR_HD_DRIFT), "Head direction drift");
    EXPECT_STREQ(mammillary_error_string(MAMMILLARY_ERROR_CONSOLIDATION_FAILED), "Consolidation failed");
    EXPECT_STREQ(mammillary_error_string(MAMMILLARY_ERROR_CIRCUIT_BROKEN), "Circuit broken");
    EXPECT_STREQ(mammillary_error_string(MAMMILLARY_ERROR_INTERNAL), "Internal error");
}

TEST_F(MammillaryTest, StatusString) {
    EXPECT_STREQ(mammillary_status_string(MAMMILLARY_STATUS_IDLE), "Idle");
    EXPECT_STREQ(mammillary_status_string(MAMMILLARY_STATUS_READY), "Ready");
    EXPECT_STREQ(mammillary_status_string(MAMMILLARY_STATUS_RELAYING), "Relaying");
    EXPECT_STREQ(mammillary_status_string(MAMMILLARY_STATUS_CONSOLIDATING), "Consolidating");
    EXPECT_STREQ(mammillary_status_string(MAMMILLARY_STATUS_HD_PROCESSING), "HD processing");
    EXPECT_STREQ(mammillary_status_string(MAMMILLARY_STATUS_SPATIAL_ENCODING), "Spatial encoding");
    EXPECT_STREQ(mammillary_status_string(MAMMILLARY_STATUS_ERROR), "Error");
}

TEST_F(MammillaryTest, GetStats) {
    mammillary_stats_t stats;
    EXPECT_EQ(mammillary_get_stats(mb, &stats), 0);
    EXPECT_EQ(stats.total_memory_traces, 0u);
}

TEST_F(MammillaryTest, GetStatsNull) {
    mammillary_stats_t stats;
    EXPECT_EQ(mammillary_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(mammillary_get_stats(mb, nullptr), -1);
}

TEST_F(MammillaryTest, GetConfig) {
    mammillary_config_t config;
    EXPECT_EQ(mammillary_get_config(mb, &config), 0);
    EXPECT_EQ(config.num_hd_cells, MAMMILLARY_DEFAULT_HD_CELLS);
}

TEST_F(MammillaryTest, GetConfigNull) {
    mammillary_config_t config;
    EXPECT_EQ(mammillary_get_config(nullptr, &config), -1);
    EXPECT_EQ(mammillary_get_config(mb, nullptr), -1);
}

TEST_F(MammillaryTest, GetHealthStatus) {
    float health = mammillary_get_health_status(mb);
    EXPECT_GT(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(MammillaryTest, GetHealthStatusNull) {
    EXPECT_FLOAT_EQ(mammillary_get_health_status(nullptr), 0.0f);
}

TEST_F(MammillaryTest, GetCircuitIntegrity) {
    float integrity = mammillary_get_circuit_integrity(mb);
    EXPECT_GT(integrity, 0.0f);
    EXPECT_LE(integrity, 1.0f);
}

TEST_F(MammillaryTest, GetCircuitIntegrityNull) {
    EXPECT_FLOAT_EQ(mammillary_get_circuit_integrity(nullptr), 0.0f);
}

TEST_F(MammillaryTest, LogDiagnostics) {
    EXPECT_EQ(mammillary_log_diagnostics(mb), 0);
}

TEST_F(MammillaryTest, LogDiagnosticsNull) {
    EXPECT_EQ(mammillary_log_diagnostics(nullptr), -1);
}

/*=============================================================================
 * CELL ACTIVITY TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, GetHdCellActivity) {
    float activity[100];
    size_t count = mammillary_get_hd_cell_activity(mb, activity, 100);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, 100u);
}

TEST_F(MammillaryTest, GetHdCellActivityNull) {
    float activity[100];
    EXPECT_EQ(mammillary_get_hd_cell_activity(nullptr, activity, 100), 0u);
    EXPECT_EQ(mammillary_get_hd_cell_activity(mb, nullptr, 100), 0u);
}

TEST_F(MammillaryTest, GetRelayCellActivity) {
    float activity[200];
    size_t count = mammillary_get_relay_cell_activity(mb, activity, 200);
    EXPECT_GT(count, 0u);
}

TEST_F(MammillaryTest, GetRelayCellActivityNull) {
    float activity[100];
    EXPECT_EQ(mammillary_get_relay_cell_activity(nullptr, activity, 100), 0u);
}

TEST_F(MammillaryTest, GetSpatialCellActivity) {
    float activity[100];
    size_t count = mammillary_get_spatial_cell_activity(mb, activity, 100);
    EXPECT_GT(count, 0u);
}

TEST_F(MammillaryTest, GetSpatialCellActivityNull) {
    float activity[100];
    EXPECT_EQ(mammillary_get_spatial_cell_activity(nullptr, activity, 100), 0u);
}

/*=============================================================================
 * SERIALIZATION TESTS
 *===========================================================================*/

TEST_F(MammillaryTest, GetSerializationSize) {
    size_t size = mammillary_get_serialization_size(mb);
    EXPECT_GT(size, 0u);
}

TEST_F(MammillaryTest, GetSerializationSizeNull) {
    EXPECT_EQ(mammillary_get_serialization_size(nullptr), 0u);
}

TEST_F(MammillaryTest, Serialize) {
    size_t size = mammillary_get_serialization_size(mb);
    uint8_t* buffer = new uint8_t[size];
    size_t written;

    EXPECT_EQ(mammillary_serialize(mb, buffer, size, &written), 0);
    EXPECT_GT(written, 0u);

    delete[] buffer;
}

TEST_F(MammillaryTest, SerializeNull) {
    uint8_t buffer[1024];
    size_t written;
    EXPECT_EQ(mammillary_serialize(nullptr, buffer, 1024, &written), -1);
    EXPECT_EQ(mammillary_serialize(mb, nullptr, 1024, &written), -1);
    EXPECT_EQ(mammillary_serialize(mb, buffer, 1024, nullptr), -1);
}

TEST_F(MammillaryTest, Deserialize) {
    /* Set some state */
    mb->current_heading = 1.5f;
    mb->heading_confidence = 0.9f;

    /* Serialize */
    size_t size = mammillary_get_serialization_size(mb);
    uint8_t* buffer = new uint8_t[size];
    size_t written;
    mammillary_serialize(mb, buffer, size, &written);

    /* Deserialize */
    size_t bytes_read;
    nimcp_mammillary_t* restored = mammillary_deserialize(buffer, size, &bytes_read);

    ASSERT_NE(restored, nullptr);
    EXPECT_FLOAT_EQ(restored->current_heading, 1.5f);
    EXPECT_FLOAT_EQ(restored->heading_confidence, 0.9f);

    mammillary_destroy(restored);
    delete[] buffer;
}

TEST_F(MammillaryTest, DeserializeNull) {
    size_t bytes_read;
    EXPECT_EQ(mammillary_deserialize(nullptr, 100, &bytes_read), nullptr);
}

/*=============================================================================
 * INTEGRATION SCENARIOS
 *===========================================================================*/

TEST_F(MammillaryTest, FullMemoryRelayCycle) {
    /* Initialize bridges */
    mammillary_init_hippocampus_bridge(mb, nullptr);
    mammillary_init_thalamus_bridge(mb, nullptr);
    mammillary_init_cingulate_bridge(mb, nullptr);

    /* Receive memory from hippocampus */
    float trace[256];
    createTestTrace(trace, 256, 0.5f);

    uint32_t trace_id;
    mammillary_receive_hippocampal_input(mb, trace, 256,
        MEMORY_TRACE_EPISODIC, 0.7f, &trace_id);

    /* Start Papez circuit */
    mammillary_advance_papez_phase(mb);
    EXPECT_EQ(mammillary_get_papez_phase(mb), PAPEZ_PHASE_HIPPOCAMPAL_INPUT);

    /* Relay to thalamus */
    mammillary_relay_to_thalamus(mb, trace_id);

    /* Complete circuit */
    for (int i = 0; i < 5; i++) {
        mammillary_advance_papez_phase(mb);
    }
    EXPECT_EQ(mammillary_get_papez_phase(mb), PAPEZ_PHASE_IDLE);

    /* Run consolidation */
    mammillary_start_consolidation(mb, trace_id);
    for (int i = 0; i < 200; i++) {
        mammillary_update_consolidation(mb, 0.05f);
    }

    const nimcp_memory_trace_t* mt = mammillary_get_trace(mb, trace_id);
    EXPECT_EQ(mt->state, CONSOLIDATION_COMPLETE);
}

TEST_F(MammillaryTest, HeadDirectionNavigation) {
    /* Initialize vestibular bridge */
    mammillary_init_vestibular_bridge(mb, nullptr);

    /* Simulate turning 360 degrees */
    float total_turn = 0.0f;
    float angular_vel = 0.5f;  /* rad/s */
    float dt = 0.01f;

    while (total_turn < 2 * M_PI) {
        mammillary_update_head_direction(mb, angular_vel, dt);
        total_turn += angular_vel * dt;
    }

    /* Should be back near start */
    float final_heading = mammillary_get_head_direction(mb);
    float error = fabsf(final_heading - mb->config.hd_drift_correction_rate);
    /* Allow some drift */
    EXPECT_LT(error, 0.5f);
}

TEST_F(MammillaryTest, SpatialMemoryAssociation) {
    /* Encode multiple spatial memories */
    float positions[5][3] = {
        {0, 0, 0},
        {100, 0, 0},
        {100, 100, 0},
        {0, 100, 0},
        {50, 50, 0}
    };

    float context[128];
    uint32_t trace_ids[5];

    for (int i = 0; i < 5; i++) {
        createTestTrace(context, 128, (float)i * 0.2f);
        mammillary_encode_spatial_memory(mb, positions[i], (float)i * 0.5f,
            context, 128, &trace_ids[i]);
    }

    /* Retrieve from near each position */
    for (int i = 0; i < 5; i++) {
        float query[3] = {
            positions[i][0] + 2.0f,
            positions[i][1] + 2.0f,
            positions[i][2]
        };
        float retrieved[256];
        uint32_t dim;
        EXPECT_EQ(mammillary_retrieve_spatial_context(mb, query, 3, retrieved, &dim), 0);
        EXPECT_GT(dim, 0u);
    }
}
