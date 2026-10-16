/**
 * @file test_swarm_narrative.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Narrative Memory System
 *
 * TEST COVERAGE:
 * - System creation and destruction
 * - Narrative construction (begin/add/end)
 * - Narrative sharing and broadcasting
 * - Narrative retrieval (by ID, topic, popularity)
 * - Coherence calculation
 * - Bio-async integration
 * - Event creation and management
 * - Statistics and monitoring
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_narrative.h"

class SwarmNarrativeTest : public ::testing::Test {
protected:
    swarm_narrative_t* system;

    void SetUp() override {
        swarm_narrative_config_t config = {
            .max_narratives = 100,
            .max_events_per_narrative = 50,
            .coherence_threshold = 0.3f,
            .enable_compression = false,
            .enable_bio_async = false
        };

        system = swarm_narrative_create(&config);
        ASSERT_NE(system, nullptr);
        ASSERT_EQ(swarm_narrative_init(system, nullptr), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (system) {
            swarm_narrative_destroy(system);
        }
    }

    narrative_event_t* create_test_event(uint32_t agent_id, float valence) {
        float encoding[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
        return narrative_event_create(agent_id, encoding, 5, valence, 0.8f);
    }
};

/* ============================================================================
 * Creation and Destruction Tests
 * ============================================================================ */

TEST_F(SwarmNarrativeTest, CreateValidSystem) {
    EXPECT_NE(system, nullptr);
}

TEST_F(SwarmNarrativeTest, DestroyNullSystem) {
    swarm_narrative_destroy(nullptr);
    SUCCEED();
}

TEST_F(SwarmNarrativeTest, CreateWithNullConfigFails) {
    auto* sys = swarm_narrative_create(nullptr);
    EXPECT_EQ(sys, nullptr);
}

TEST_F(SwarmNarrativeTest, CreateWithCustomConfig) {
    swarm_narrative_config_t config = {
        .max_narratives = 50,
        .max_events_per_narrative = 25,
        .coherence_threshold = 0.5f,
        .enable_compression = true,
        .enable_bio_async = true
    };

    auto* sys = swarm_narrative_create(&config);
    EXPECT_NE(sys, nullptr);
    if (sys) {
        swarm_narrative_destroy(sys);
    }
}

/* ============================================================================
 * Narrative Construction Tests
 * ============================================================================ */

TEST_F(SwarmNarrativeTest, BeginNarrative) {
    uint32_t narrative_id;
    int result = swarm_narrative_begin(system, 123, &narrative_id);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(narrative_id, 0);
}

TEST_F(SwarmNarrativeTest, BeginNarrativeWithNullSystem) {
    uint32_t narrative_id;
    int result = swarm_narrative_begin(nullptr, 123, &narrative_id);

    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmNarrativeTest, BeginNarrativeWithNullIdPointer) {
    int result = swarm_narrative_begin(system, 123, nullptr);

    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

TEST_F(SwarmNarrativeTest, AddEventToNarrative) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    narrative_event_t* event = create_test_event(123, 0.5f);
    ASSERT_NE(event, nullptr);

    int result = swarm_narrative_add_event(system, narrative_id, event);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    narrative_event_destroy(event);
}

TEST_F(SwarmNarrativeTest, AddMultipleEventsToNarrative) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    for (int i = 0; i < 5; i++) {
        narrative_event_t* event = create_test_event(123, 0.5f + i * 0.1f);
        ASSERT_NE(event, nullptr);

        int result = swarm_narrative_add_event(system, narrative_id, event);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        narrative_event_destroy(event);
    }
}

TEST_F(SwarmNarrativeTest, AddEventToNonexistentNarrative) {
    narrative_event_t* event = create_test_event(123, 0.5f);
    ASSERT_NE(event, nullptr);

    int result = swarm_narrative_add_event(system, 9999, event);

    EXPECT_EQ(result, NIMCP_NOT_FOUND);

    narrative_event_destroy(event);
}

TEST_F(SwarmNarrativeTest, EndNarrative) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    narrative_event_t* event = create_test_event(123, 0.5f);
    swarm_narrative_add_event(system, narrative_id, event);
    narrative_event_destroy(event);

    int result = swarm_narrative_end(system, narrative_id);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmNarrativeTest, EndNarrativeWithNoEvents) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    int result = swarm_narrative_end(system, narrative_id);

    EXPECT_EQ(result, NIMCP_ERROR);
}

TEST_F(SwarmNarrativeTest, EndNonexistentNarrative) {
    int result = swarm_narrative_end(system, 9999);

    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

/* ============================================================================
 * Coherence Calculation Tests
 * ============================================================================ */

TEST_F(SwarmNarrativeTest, CalculateCoherenceForSingleEvent) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    narrative_event_t* event = create_test_event(123, 0.5f);
    swarm_narrative_add_event(system, narrative_id, event);
    narrative_event_destroy(event);

    swarm_narrative_end(system, narrative_id);

    narrative_t* narrative;
    int result = swarm_narrative_get(system, narrative_id, &narrative);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    if (result == NIMCP_SUCCESS) {
        EXPECT_GE(narrative->coherence_score, 0.0f);
        EXPECT_LE(narrative->coherence_score, 1.0f);
    }
}

TEST_F(SwarmNarrativeTest, CalculateCoherenceForMultipleEvents) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    for (int i = 0; i < 3; i++) {
        narrative_event_t* event = create_test_event(123, 0.5f);
        swarm_narrative_add_event(system, narrative_id, event);
        narrative_event_destroy(event);
    }

    swarm_narrative_end(system, narrative_id);

    narrative_t* narrative;
    swarm_narrative_get(system, narrative_id, &narrative);

    EXPECT_GE(narrative->coherence_score, 0.0f);
    EXPECT_LE(narrative->coherence_score, 1.0f);
}

/* ============================================================================
 * Narrative Retrieval Tests
 * ============================================================================ */

TEST_F(SwarmNarrativeTest, GetNarrativeById) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    narrative_event_t* event = create_test_event(123, 0.5f);
    swarm_narrative_add_event(system, narrative_id, event);
    narrative_event_destroy(event);

    swarm_narrative_end(system, narrative_id);

    narrative_t* narrative;
    int result = swarm_narrative_get(system, narrative_id, &narrative);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    if (result == NIMCP_SUCCESS) {
        EXPECT_EQ(narrative->narrative_id, narrative_id);
        EXPECT_EQ(narrative->teller_agent_id, 123);
        EXPECT_EQ(narrative->num_events, 1);
    }
}

TEST_F(SwarmNarrativeTest, GetNonexistentNarrative) {
    narrative_t* narrative;
    int result = swarm_narrative_get(system, 9999, &narrative);

    EXPECT_EQ(result, NIMCP_NOT_FOUND);
}

TEST_F(SwarmNarrativeTest, GetNarrativeWithNullResult) {
    int result = swarm_narrative_get(system, 1, nullptr);

    EXPECT_EQ(result, NIMCP_INVALID_PARAM);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(SwarmNarrativeTest, GetStatistics) {
    uint32_t total_narratives, total_events, total_shares;
    float avg_coherence;

    int result = swarm_narrative_get_stats(system, &total_narratives,
                                            &total_events, &avg_coherence,
                                            &total_shares);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(total_narratives, 0);
    EXPECT_EQ(total_events, 0);
    EXPECT_EQ(total_shares, 0);
}

TEST_F(SwarmNarrativeTest, GetStatisticsAfterCreatingNarrative) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    narrative_event_t* event = create_test_event(123, 0.5f);
    swarm_narrative_add_event(system, narrative_id, event);
    narrative_event_destroy(event);

    swarm_narrative_end(system, narrative_id);

    uint32_t total_narratives, total_events, total_shares;
    float avg_coherence;

    swarm_narrative_get_stats(system, &total_narratives,
                              &total_events, &avg_coherence,
                              &total_shares);

    EXPECT_EQ(total_narratives, 1);
    EXPECT_EQ(total_events, 1);
}

TEST_F(SwarmNarrativeTest, PrintStatus) {
    swarm_narrative_print_status(system, false);
    swarm_narrative_print_status(system, true);
    SUCCEED();
}

TEST_F(SwarmNarrativeTest, PrintStatusWithNullSystem) {
    swarm_narrative_print_status(nullptr, false);
    SUCCEED();
}

/* ============================================================================
 * Event Utility Tests
 * ============================================================================ */

TEST_F(SwarmNarrativeTest, CreateEvent) {
    float encoding[] = {0.1f, 0.2f, 0.3f};
    narrative_event_t* event = narrative_event_create(123, encoding, 3, 0.5f, 0.8f);

    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->agent_id, 123);
    EXPECT_EQ(event->encoding_size, 3);
    EXPECT_FLOAT_EQ(event->emotional_valence, 0.5f);
    EXPECT_FLOAT_EQ(event->importance, 0.8f);
    EXPECT_NE(event->event_encoding, nullptr);

    narrative_event_destroy(event);
}

TEST_F(SwarmNarrativeTest, DestroyNullEvent) {
    narrative_event_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Sharing Tests (with bio-async disabled)
 * ============================================================================ */

TEST_F(SwarmNarrativeTest, ShareNarrativeWithBioAsyncDisabled) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    narrative_event_t* event = create_test_event(123, 0.5f);
    swarm_narrative_add_event(system, narrative_id, event);
    narrative_event_destroy(event);

    swarm_narrative_end(system, narrative_id);

    uint32_t targets[] = {456, 789};
    int result = swarm_narrative_share(system, narrative_id, targets, 2);

    // Should fail or return error since bio-async is disabled
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(SwarmNarrativeTest, BroadcastNarrativeWithBioAsyncDisabled) {
    uint32_t narrative_id;
    swarm_narrative_begin(system, 123, &narrative_id);

    narrative_event_t* event = create_test_event(123, 0.5f);
    swarm_narrative_add_event(system, narrative_id, event);
    narrative_event_destroy(event);

    swarm_narrative_end(system, narrative_id);

    int result = swarm_narrative_broadcast(system, narrative_id);

    // Should fail or return error since bio-async is disabled
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(SwarmNarrativeTest, NullPointerHandling) {
    uint32_t id;
    narrative_event_t* event = create_test_event(123, 0.5f);

    EXPECT_EQ(swarm_narrative_begin(nullptr, 123, &id), NIMCP_INVALID_PARAM);
    EXPECT_EQ(swarm_narrative_add_event(nullptr, 1, event), NIMCP_INVALID_PARAM);
    EXPECT_EQ(swarm_narrative_end(nullptr, 1), NIMCP_INVALID_PARAM);
    EXPECT_EQ(swarm_narrative_get(nullptr, 1, nullptr), NIMCP_INVALID_PARAM);

    narrative_event_destroy(event);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(SwarmNarrativeTest, CreateManyNarratives) {
    for (int i = 0; i < 10; i++) {
        uint32_t narrative_id;
        swarm_narrative_begin(system, 123, &narrative_id);

        narrative_event_t* event = create_test_event(123, 0.5f);
        swarm_narrative_add_event(system, narrative_id, event);
        narrative_event_destroy(event);

        swarm_narrative_end(system, narrative_id);
    }

    uint32_t total_narratives;
    swarm_narrative_get_stats(system, &total_narratives, nullptr, nullptr, nullptr);

    EXPECT_EQ(total_narratives, 10);
}

TEST_F(SwarmNarrativeTest, LowCoherenceNarrativeRejection) {
    // Create system with high coherence threshold
    swarm_narrative_config_t config = {
        .max_narratives = 100,
        .max_events_per_narrative = 50,
        .coherence_threshold = 0.95f,  // Very high threshold
        .enable_compression = false,
        .enable_bio_async = false
    };

    auto* high_threshold_system = swarm_narrative_create(&config);
    swarm_narrative_init(high_threshold_system, nullptr);

    uint32_t narrative_id;
    swarm_narrative_begin(high_threshold_system, 123, &narrative_id);

    // Add single event (likely low coherence)
    narrative_event_t* event = create_test_event(123, 0.5f);
    swarm_narrative_add_event(high_threshold_system, narrative_id, event);
    narrative_event_destroy(event);

    // Should be rejected due to low coherence
    int result = swarm_narrative_end(high_threshold_system, narrative_id);
    EXPECT_EQ(result, NIMCP_ERROR);

    swarm_narrative_destroy(high_threshold_system);
}
