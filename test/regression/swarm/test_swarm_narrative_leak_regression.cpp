/**
 * @file test_swarm_narrative_leak_regression.cpp
 * @brief Regression test for narrative memory cleanup leak
 *
 * REGRESSION: Narrative cleanup not implemented (memory leak)
 *
 * ORIGINAL BUG:
 * In nimcp_swarm_narrative.c, swarm_narrative_destroy() had a TODO comment at
 * line 191: "Free narratives (TODO: implement narrative cleanup iteration)".
 * The hash tables were destroyed but the narrative_t* values inside them
 * (and their owned event_encoding float arrays) were never freed.
 *
 * WHAT LEAKED:
 * - narrative_t struct for each narrative
 * - narrative_event_t array for each narrative
 * - float* event_encoding for each event in each narrative
 *
 * FIX:
 * Added narrative_cleanup_iterator() callback and free_narrative() helper.
 * swarm_narrative_destroy() now calls hash_table_iterate() on both the
 * narratives and pending_narratives hash tables before destroying them,
 * properly freeing all narrative resources.
 *
 * TEST APPROACH:
 * We create narratives with known memory patterns and verify that destroy
 * completes without crashing. Since we cannot directly verify heap state
 * from within the test (without ASAN), we exercise the cleanup paths
 * thoroughly and rely on ASAN/Valgrind in CI to detect actual leaks.
 *
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_narrative.h"

/* ============================================================================
 * Regression Test Fixture
 * ============================================================================ */

class NarrativeLeakRegressionTest : public ::testing::Test {
protected:
    narrative_event_t* make_event(uint32_t agent, float valence, uint32_t enc_size) {
        std::vector<float> enc(enc_size);
        for (uint32_t i = 0; i < enc_size; i++) {
            enc[i] = (float)(i + 1) * 0.01f;
        }
        return narrative_event_create(agent, enc.data(), enc_size, valence, 0.5f);
    }

    swarm_narrative_t* make_system(float threshold = 0.0f) {
        swarm_narrative_config_t config = {
            .max_narratives = 200,
            .max_events_per_narrative = 100,
            .coherence_threshold = threshold,
            .enable_compression = false,
            .enable_bio_async = false
        };
        swarm_narrative_t* sn = swarm_narrative_create(&config);
        if (sn) {
            swarm_narrative_init(sn, nullptr);
        }
        return sn;
    }
};

/* ============================================================================
 * Regression Tests
 * ============================================================================ */

/**
 * REGRESSION: Single finalized narrative with events must be freed on destroy.
 * Before the fix, the narrative_t, events array, and event encodings leaked.
 */
TEST_F(NarrativeLeakRegressionTest, SingleFinalizedNarrativeFreed) {
    swarm_narrative_t* sn = make_system();
    ASSERT_NE(sn, nullptr);

    uint32_t nid = 0;
    ASSERT_EQ(swarm_narrative_begin(sn, 1, &nid), NIMCP_SUCCESS);

    for (int i = 0; i < 5; i++) {
        narrative_event_t* ev = make_event(1, 0.5f, 16);
        ASSERT_NE(ev, nullptr);
        swarm_narrative_add_event(sn, nid, ev);
        narrative_event_destroy(ev);
    }

    ASSERT_EQ(swarm_narrative_end(sn, nid), NIMCP_SUCCESS);

    /* Before fix: this leaked 5 event encodings + events array + narrative struct */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * REGRESSION: Pending narratives (never finalized) must also be freed on destroy.
 * These sit in the pending_narratives hash table, not the main narratives table.
 */
TEST_F(NarrativeLeakRegressionTest, PendingNarrativeFreed) {
    swarm_narrative_t* sn = make_system();
    ASSERT_NE(sn, nullptr);

    uint32_t nid = 0;
    ASSERT_EQ(swarm_narrative_begin(sn, 2, &nid), NIMCP_SUCCESS);

    /* Add events but never call swarm_narrative_end */
    narrative_event_t* ev = make_event(2, 0.3f, 32);
    ASSERT_NE(ev, nullptr);
    swarm_narrative_add_event(sn, nid, ev);
    narrative_event_destroy(ev);

    /* Before fix: pending narrative, its events, and encodings leaked */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * REGRESSION: Narrative with zero events (begun but no events added).
 * The events array is still allocated (capacity-sized), must be freed.
 */
TEST_F(NarrativeLeakRegressionTest, EmptyPendingNarrativeFreed) {
    swarm_narrative_t* sn = make_system();
    ASSERT_NE(sn, nullptr);

    uint32_t nid = 0;
    ASSERT_EQ(swarm_narrative_begin(sn, 3, &nid), NIMCP_SUCCESS);

    /* No events added, never finalized - narrative struct + events array leaked */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * REGRESSION: Mix of finalized and pending narratives, all must be cleaned up.
 */
TEST_F(NarrativeLeakRegressionTest, MixedFinalizedAndPendingFreed) {
    swarm_narrative_t* sn = make_system();
    ASSERT_NE(sn, nullptr);

    /* Create 3 finalized narratives */
    for (int n = 0; n < 3; n++) {
        uint32_t nid = 0;
        swarm_narrative_begin(sn, (uint32_t)(10 + n), &nid);
        for (int e = 0; e < 4; e++) {
            narrative_event_t* ev = make_event((uint32_t)(10 + n), 0.5f, 8);
            swarm_narrative_add_event(sn, nid, ev);
            narrative_event_destroy(ev);
        }
        swarm_narrative_end(sn, nid);
    }

    /* Create 2 pending narratives */
    for (int n = 0; n < 2; n++) {
        uint32_t nid = 0;
        swarm_narrative_begin(sn, (uint32_t)(20 + n), &nid);
        narrative_event_t* ev = make_event((uint32_t)(20 + n), -0.5f, 8);
        swarm_narrative_add_event(sn, nid, ev);
        narrative_event_destroy(ev);
    }

    uint32_t total = 0;
    swarm_narrative_get_stats(sn, &total, nullptr, nullptr, nullptr);
    EXPECT_EQ(total, 3u);  /* Only finalized count in stats */

    /* Before fix: 3 finalized + 2 pending narratives leaked */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * REGRESSION: Large batch of narratives with large encodings.
 * Under ASAN, this would report significant leak bytes before the fix.
 */
TEST_F(NarrativeLeakRegressionTest, ManyNarrativesLargeEncodings) {
    swarm_narrative_t* sn = make_system();
    ASSERT_NE(sn, nullptr);

    /* 20 narratives, each with 10 events having 128-float encodings
     * = 20 * 10 * 128 * 4 = 102,400 bytes of encoding data */
    for (int n = 0; n < 20; n++) {
        uint32_t nid = 0;
        swarm_narrative_begin(sn, (uint32_t)n, &nid);
        for (int e = 0; e < 10; e++) {
            narrative_event_t* ev = make_event((uint32_t)n, 0.5f, 128);
            swarm_narrative_add_event(sn, nid, ev);
            narrative_event_destroy(ev);
        }
        swarm_narrative_end(sn, nid);
    }

    uint32_t total = 0;
    swarm_narrative_get_stats(sn, &total, nullptr, nullptr, nullptr);
    EXPECT_EQ(total, 20u);

    /* All 100KB+ of encoding data must be freed */
    swarm_narrative_destroy(sn);
    SUCCEED();
}

/**
 * REGRESSION: Create/destroy cycle. Verify no accumulation across cycles.
 */
TEST_F(NarrativeLeakRegressionTest, CreateDestroyCycle) {
    for (int cycle = 0; cycle < 10; cycle++) {
        swarm_narrative_t* sn = make_system();
        ASSERT_NE(sn, nullptr);

        uint32_t nid = 0;
        swarm_narrative_begin(sn, 1, &nid);
        narrative_event_t* ev = make_event(1, 0.5f, 16);
        swarm_narrative_add_event(sn, nid, ev);
        narrative_event_destroy(ev);
        swarm_narrative_end(sn, nid);

        swarm_narrative_destroy(sn);
    }
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
