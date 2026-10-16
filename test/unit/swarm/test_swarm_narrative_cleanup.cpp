/**
 * @file test_swarm_narrative_cleanup.cpp
 * @brief Unit tests for narrative memory cleanup in swarm_narrative_destroy
 *
 * TEST COVERAGE:
 * - Verify swarm_narrative_destroy frees all narrative resources
 * - Test cleanup of finalized narratives (main hash table)
 * - Test cleanup of pending/incomplete narratives
 * - Test cleanup when narratives have event encodings
 * - Test cleanup with multiple narratives
 * - Test empty system cleanup
 * - Test NULL system cleanup
 *
 * WHAT WAS FIXED:
 * The swarm_narrative_destroy function had a TODO comment at line 191 where
 * narrative cleanup iteration was never implemented. When the system was
 * destroyed, all narrative_t structs, their event arrays, and the float*
 * event_encoding arrays inside each event were leaked.
 *
 * The fix adds a narrative_cleanup_iterator that hash_table_iterate calls
 * to properly free each narrative's encoding arrays, event array, and the
 * narrative struct itself before hash_table_destroy is called.
 *
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_narrative.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SwarmNarrativeCleanupTest : public ::testing::Test {
protected:
    /**
     * Helper: Create a narrative system with permissive defaults
     */
    swarm_narrative_t* create_system() {
        swarm_narrative_config_t config = {
            .max_narratives = 100,
            .max_events_per_narrative = 50,
            .coherence_threshold = 0.0f,  /* Accept all narratives */
            .enable_compression = false,
            .enable_bio_async = false
        };

        swarm_narrative_t* sn = swarm_narrative_create(&config);
        if (sn) {
            swarm_narrative_init(sn, nullptr);
        }
        return sn;
    }

    /**
     * Helper: Create a test event with heap-allocated encoding
     */
    narrative_event_t* create_test_event(uint32_t agent_id, float valence,
                                          uint32_t enc_size = 8) {
        std::vector<float> encoding(enc_size);
        for (uint32_t i = 0; i < enc_size; i++) {
            encoding[i] = 0.1f * (float)(i + 1);
        }
        return narrative_event_create(agent_id, encoding.data(), enc_size,
                                       valence, 0.8f);
    }

    /**
     * Helper: Create and finalize a narrative with N events
     * Returns the narrative_id, or 0 on failure
     */
    uint32_t create_finalized_narrative(swarm_narrative_t* sn, uint32_t teller,
                                         uint32_t num_events) {
        uint32_t narrative_id = 0;
        if (swarm_narrative_begin(sn, teller, &narrative_id) != NIMCP_SUCCESS) {
            return 0;
        }

        for (uint32_t i = 0; i < num_events; i++) {
            narrative_event_t* event = create_test_event(teller, 0.5f);
            if (!event) return 0;
            swarm_narrative_add_event(sn, narrative_id, event);
            narrative_event_destroy(event);
        }

        if (swarm_narrative_end(sn, narrative_id) != NIMCP_SUCCESS) {
            return 0;
        }

        return narrative_id;
    }
};

/* ============================================================================
 * Cleanup Tests
 * ============================================================================ */

/**
 * Destroy an empty system - should not crash
 */
TEST_F(SwarmNarrativeCleanupTest, DestroyEmptySystem) {
    swarm_narrative_t* sn = create_system();
    ASSERT_NE(sn, nullptr);

    /* No narratives added - just destroy */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * Destroy NULL system - should not crash
 */
TEST_F(SwarmNarrativeCleanupTest, DestroyNullSystem) {
    swarm_narrative_destroy(nullptr);
    SUCCEED();
}

/**
 * Destroy system with one finalized narrative containing events
 * with heap-allocated encodings. Before the fix, this would leak:
 * - The narrative_t struct
 * - The narrative_event_t array
 * - Each event's float* event_encoding
 */
TEST_F(SwarmNarrativeCleanupTest, DestroyWithFinalizedNarrative) {
    swarm_narrative_t* sn = create_system();
    ASSERT_NE(sn, nullptr);

    uint32_t nid = create_finalized_narrative(sn, 100, 3);
    ASSERT_GT(nid, (uint32_t)0);

    /* Verify it was stored */
    uint32_t total = 0;
    swarm_narrative_get_stats(sn, &total, nullptr, nullptr, nullptr);
    EXPECT_EQ(total, 1u);

    /* Destroy should free all narrative resources without leaking */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * Destroy system with multiple finalized narratives
 */
TEST_F(SwarmNarrativeCleanupTest, DestroyWithMultipleFinalizedNarratives) {
    swarm_narrative_t* sn = create_system();
    ASSERT_NE(sn, nullptr);

    /* Create 5 narratives with varying event counts */
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t nid = create_finalized_narrative(sn, 100 + i, 2 + i);
        ASSERT_GT(nid, (uint32_t)0) << "Failed to create narrative " << i;
    }

    uint32_t total = 0;
    swarm_narrative_get_stats(sn, &total, nullptr, nullptr, nullptr);
    EXPECT_EQ(total, 5u);

    /* Destroy should free all 5 narratives and their events */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * Destroy system with pending (not yet finalized) narratives.
 * These live in the pending_narratives hash table and must also be freed.
 */
TEST_F(SwarmNarrativeCleanupTest, DestroyWithPendingNarratives) {
    swarm_narrative_t* sn = create_system();
    ASSERT_NE(sn, nullptr);

    /* Begin narratives but don't finalize them */
    uint32_t nid1 = 0, nid2 = 0;
    ASSERT_EQ(swarm_narrative_begin(sn, 200, &nid1), NIMCP_SUCCESS);
    ASSERT_EQ(swarm_narrative_begin(sn, 201, &nid2), NIMCP_SUCCESS);

    /* Add events to them (but don't end) */
    narrative_event_t* event1 = create_test_event(200, 0.5f);
    narrative_event_t* event2 = create_test_event(201, -0.3f);
    ASSERT_NE(event1, nullptr);
    ASSERT_NE(event2, nullptr);

    swarm_narrative_add_event(sn, nid1, event1);
    swarm_narrative_add_event(sn, nid2, event2);

    narrative_event_destroy(event1);
    narrative_event_destroy(event2);

    /* These narratives are in pending table, not finalized.
     * Before the fix, their event encodings and event arrays would leak. */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * Destroy system with both finalized and pending narratives
 */
TEST_F(SwarmNarrativeCleanupTest, DestroyWithMixedNarratives) {
    swarm_narrative_t* sn = create_system();
    ASSERT_NE(sn, nullptr);

    /* Create finalized narratives */
    create_finalized_narrative(sn, 100, 2);
    create_finalized_narrative(sn, 101, 3);

    /* Create pending narratives */
    uint32_t pending_id = 0;
    swarm_narrative_begin(sn, 200, &pending_id);
    narrative_event_t* event = create_test_event(200, 0.7f);
    swarm_narrative_add_event(sn, pending_id, event);
    narrative_event_destroy(event);

    /* Destroy all at once */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * Destroy system with narratives that have large encodings.
 * Each event has a separately allocated float* encoding.
 */
TEST_F(SwarmNarrativeCleanupTest, DestroyWithLargeEncodings) {
    swarm_narrative_t* sn = create_system();
    ASSERT_NE(sn, nullptr);

    uint32_t nid = 0;
    ASSERT_EQ(swarm_narrative_begin(sn, 300, &nid), NIMCP_SUCCESS);

    /* Add events with large encoding vectors (256 floats each = 1KB per encoding) */
    for (int i = 0; i < 10; i++) {
        narrative_event_t* event = create_test_event(300, 0.5f, 256);
        ASSERT_NE(event, nullptr);
        swarm_narrative_add_event(sn, nid, event);
        narrative_event_destroy(event);
    }

    swarm_narrative_end(sn, nid);

    /* 10 events * 256 floats * 4 bytes = 10KB of encoding data that must be freed */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * Stress test: create many narratives, destroy the system.
 * Verifies no crash under heavier load.
 */
TEST_F(SwarmNarrativeCleanupTest, DestroyManyNarratives) {
    swarm_narrative_t* sn = create_system();
    ASSERT_NE(sn, nullptr);

    /* Create 50 finalized narratives */
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t nid = create_finalized_narrative(sn, i, 3);
        EXPECT_GT(nid, (uint32_t)0);
    }

    uint32_t total = 0;
    swarm_narrative_get_stats(sn, &total, nullptr, nullptr, nullptr);
    EXPECT_EQ(total, 50u);

    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * Test: narrative_event_create and narrative_event_destroy are symmetric.
 * Ensures standalone event creation/destruction doesn't leak.
 */
TEST_F(SwarmNarrativeCleanupTest, EventCreateDestroySymmetric) {
    for (int i = 0; i < 100; i++) {
        narrative_event_t* event = create_test_event(1, 0.5f, 32);
        ASSERT_NE(event, nullptr);
        EXPECT_NE(event->event_encoding, nullptr);
        EXPECT_EQ(event->encoding_size, 32u);
        narrative_event_destroy(event);
    }
    SUCCEED();
}

/**
 * Verify stats are correct before and after narrative finalization,
 * then verify clean destroy.
 */
TEST_F(SwarmNarrativeCleanupTest, StatsAccuracyThenCleanDestroy) {
    swarm_narrative_t* sn = create_system();
    ASSERT_NE(sn, nullptr);

    uint32_t total_n = 0, total_e = 0;
    float avg_c = 0.0f;

    /* Initially empty */
    swarm_narrative_get_stats(sn, &total_n, &total_e, &avg_c, nullptr);
    EXPECT_EQ(total_n, 0u);
    EXPECT_EQ(total_e, 0u);

    /* Add 3 narratives with 4 events each */
    for (uint32_t i = 0; i < 3; i++) {
        create_finalized_narrative(sn, i, 4);
    }

    swarm_narrative_get_stats(sn, &total_n, &total_e, &avg_c, nullptr);
    EXPECT_EQ(total_n, 3u);
    EXPECT_EQ(total_e, 12u);

    swarm_narrative_destroy(sn);
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
