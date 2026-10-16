/**
 * @file test_hippocampus.cpp
 * @brief Unit tests for Hippocampus
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/hippocampus/nimcp_hippocampus.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class HippocampusTest : public ::testing::Test {
protected:
    nimcp_hippocampus_t* hippo = nullptr;

    void SetUp() override {
        hippo_config_t config = hippo_default_config();
        config.num_dg_cells = 256;
        config.num_ca3_cells = 128;
        config.num_ca1_cells = 128;
        config.num_subiculum_cells = 64;
        config.num_place_cells = 64;
        config.max_episodes = 256;
        hippo = hippo_create(&config);
        ASSERT_NE(hippo, nullptr);
    }

    void TearDown() override {
        if (hippo) {
            hippo_destroy(hippo);
            hippo = nullptr;
        }
    }

    void createTestContent(float* content, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            content[i] = base_value + sinf(i * 0.1f) * 0.5f;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, CreateWithDefaultConfig) {
    nimcp_hippocampus_t* h = hippo_create(nullptr);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->status, HIPPO_STATUS_READY);
    EXPECT_EQ(h->num_dg_cells, HIPPO_DEFAULT_DG_CELLS);
    EXPECT_EQ(h->num_ca3_cells, HIPPO_DEFAULT_CA3_CELLS);
    EXPECT_EQ(h->num_ca1_cells, HIPPO_DEFAULT_CA1_CELLS);
    hippo_destroy(h);
}

TEST_F(HippocampusTest, CreateWithCustomConfig) {
    hippo_config_t config = hippo_default_config();
    config.num_dg_cells = 512;
    config.num_ca3_cells = 256;
    config.max_episodes = 512;

    nimcp_hippocampus_t* h = hippo_create(&config);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->num_dg_cells, 512u);
    EXPECT_EQ(h->num_ca3_cells, 256u);
    EXPECT_EQ(h->max_episodes, 512u);
    hippo_destroy(h);
}

TEST_F(HippocampusTest, DestroyNull) {
    hippo_destroy(nullptr);
    SUCCEED();
}

TEST_F(HippocampusTest, Reset) {
    hippo->updates_processed = 100;
    hippo->encodings_performed = 50;
    hippo->theta_phase = 1.5f;

    EXPECT_EQ(hippo_reset(hippo), 0);

    EXPECT_EQ(hippo->updates_processed, 0u);
    EXPECT_EQ(hippo->encodings_performed, 0u);
    EXPECT_FLOAT_EQ(hippo->theta_phase, 0.0f);
    EXPECT_EQ(hippo->status, HIPPO_STATUS_READY);
}

TEST_F(HippocampusTest, ResetNull) {
    EXPECT_EQ(hippo_reset(nullptr), -1);
}

TEST_F(HippocampusTest, Update) {
    EXPECT_EQ(hippo_update(hippo, 0.01f), 0);
    EXPECT_EQ(hippo->updates_processed, 1u);
}

TEST_F(HippocampusTest, UpdateNull) {
    EXPECT_EQ(hippo_update(nullptr, 0.01f), -1);
}

TEST_F(HippocampusTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(hippo_update(hippo, 0.01f), 0);
    }
    EXPECT_EQ(hippo->updates_processed, 100u);
}

/*=============================================================================
 * EPISODIC MEMORY TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, EncodeEpisode) {
    float what[128], where[3], when[32];
    createTestContent(what, 128, 0.5f);
    where[0] = 10.0f; where[1] = 20.0f; where[2] = 0.0f;
    createTestContent(when, 32, 0.3f);

    uint32_t episode_id;
    EXPECT_EQ(hippo_encode_episode(hippo, what, 128, where, 3, when, 32,
        0.5f, 0.7f, &episode_id), 0);
    EXPECT_LT(episode_id, hippo->max_episodes);
    EXPECT_EQ(hippo->num_episodes, 1u);
    EXPECT_EQ(hippo->encodings_performed, 1u);
}

TEST_F(HippocampusTest, EncodeEpisodeNull) {
    float what[128];
    uint32_t id;
    EXPECT_EQ(hippo_encode_episode(nullptr, what, 128, NULL, 0, NULL, 0,
        0.0f, 0.0f, &id), -1);
}

TEST_F(HippocampusTest, EncodeMultipleEpisodes) {
    float what[128];
    uint32_t ids[10];

    for (int i = 0; i < 10; i++) {
        createTestContent(what, 128, (float)i * 0.1f);
        EXPECT_EQ(hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0,
            (float)i * 0.1f - 0.5f, 0.5f, &ids[i]), 0);
    }

    EXPECT_EQ(hippo->num_episodes, 10u);

    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(HippocampusTest, GetEpisode) {
    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.7f, 0.8f, &episode_id);

    const nimcp_episode_t* ep = hippo_get_episode(hippo, episode_id);
    ASSERT_NE(ep, nullptr);
    EXPECT_EQ(ep->episode_id, episode_id);
    EXPECT_FLOAT_EQ(ep->emotional_valence, 0.7f);
    EXPECT_FLOAT_EQ(ep->emotional_arousal, 0.8f);
}

TEST_F(HippocampusTest, GetEpisodeInvalid) {
    EXPECT_EQ(hippo_get_episode(hippo, 99999), nullptr);
    EXPECT_EQ(hippo_get_episode(nullptr, 0), nullptr);
}

TEST_F(HippocampusTest, RetrieveEpisode) {
    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &episode_id);

    uint32_t retrieved_id;
    float confidence;
    EXPECT_EQ(hippo_retrieve_episode(hippo, what, 128, RETRIEVAL_FREE_RECALL,
        &retrieved_id, &confidence), 0);
    EXPECT_EQ(retrieved_id, episode_id);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(HippocampusTest, RetrieveEpisodeNull) {
    float what[128];
    uint32_t id;
    float conf;
    EXPECT_EQ(hippo_retrieve_episode(nullptr, what, 128, RETRIEVAL_FREE_RECALL, &id, &conf), -1);
    EXPECT_EQ(hippo_retrieve_episode(hippo, nullptr, 128, RETRIEVAL_FREE_RECALL, &id, &conf), -1);
}

TEST_F(HippocampusTest, RetrieveWithCuedRecall) {
    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &episode_id);

    /* Create partial cue */
    float cue[64];
    memcpy(cue, what, 64 * sizeof(float));

    uint32_t retrieved_id;
    float confidence;
    EXPECT_EQ(hippo_retrieve_episode(hippo, cue, 64, RETRIEVAL_CUED_RECALL,
        &retrieved_id, &confidence), 0);
}

TEST_F(HippocampusTest, StrengthenEpisode) {
    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &episode_id);

    const nimcp_episode_t* ep = hippo_get_episode(hippo, episode_id);
    float initial_strength = ep->encoding_strength;

    EXPECT_EQ(hippo_strengthen_episode(hippo, episode_id, 0.2f), 0);
    EXPECT_GT(ep->encoding_strength, initial_strength);
    EXPECT_EQ(ep->retrieval_count, 1u);
}

TEST_F(HippocampusTest, StrengthenEpisodeInvalid) {
    EXPECT_EQ(hippo_strengthen_episode(hippo, 99999, 0.2f), -1);
    EXPECT_EQ(hippo_strengthen_episode(nullptr, 0, 0.2f), -1);
}

TEST_F(HippocampusTest, ForgetEpisode) {
    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &episode_id);

    EXPECT_EQ(hippo_forget_episode(hippo, episode_id), 0);
    EXPECT_EQ(hippo_get_episode(hippo, episode_id), nullptr);
}

TEST_F(HippocampusTest, FindSimilarEpisodes) {
    float what[128];

    /* Create several related episodes */
    for (int i = 0; i < 5; i++) {
        createTestContent(what, 128, 0.5f + (float)i * 0.01f);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);
    }

    /* Create unrelated episode */
    createTestContent(what, 128, 5.0f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    /* Query for similar */
    createTestContent(what, 128, 0.5f);
    uint32_t found_ids[10];
    float similarities[10];
    uint32_t num_found;

    EXPECT_EQ(hippo_find_similar_episodes(hippo, what, 128, found_ids, similarities,
        10, &num_found), 0);
    EXPECT_GT(num_found, 0u);
}

TEST_F(HippocampusTest, GetEpisodesByType) {
    float what[128], where[3];

    /* Create spatial episodes */
    for (int i = 0; i < 3; i++) {
        createTestContent(what, 128, 0.5f);
        where[0] = (float)i * 10.0f;
        where[1] = 0.0f;
        where[2] = 0.0f;
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, where, 3, NULL, 0, 0.0f, 0.5f, &id);
    }

    /* Create non-spatial episodes */
    for (int i = 0; i < 2; i++) {
        createTestContent(what, 128, 1.0f + (float)i);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);
    }

    uint32_t found_ids[10];
    uint32_t num_found;
    EXPECT_EQ(hippo_get_episodes_by_type(hippo, EPISODE_TYPE_SPATIAL, found_ids, 10, &num_found), 0);
    EXPECT_EQ(num_found, 3u);
}

TEST_F(HippocampusTest, GetRecentEpisodes) {
    float what[128];

    for (int i = 0; i < 5; i++) {
        createTestContent(what, 128, (float)i * 0.1f);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);
    }

    uint32_t found_ids[10];
    uint32_t num_found;
    EXPECT_EQ(hippo_get_recent_episodes(hippo, found_ids, 3, &num_found), 0);
    EXPECT_EQ(num_found, 3u);
}

/*=============================================================================
 * PATTERN SEPARATION/COMPLETION TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, PatternSeparate) {
    float input[128];
    createTestContent(input, 128, 0.5f);

    float output[1024];
    uint32_t output_dim = 1024;

    EXPECT_EQ(hippo_pattern_separate(hippo, input, 128, output, &output_dim), 0);
    EXPECT_GT(output_dim, 0u);
}

TEST_F(HippocampusTest, PatternSeparateNull) {
    float input[128];
    float output[1024];
    uint32_t dim = 1024;
    EXPECT_EQ(hippo_pattern_separate(nullptr, input, 128, output, &dim), -1);
    EXPECT_EQ(hippo_pattern_separate(hippo, nullptr, 128, output, &dim), -1);
}

TEST_F(HippocampusTest, PatternComplete) {
    float what[128];
    createTestContent(what, 128, 0.5f);

    /* Encode episode first */
    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &episode_id);

    /* Try to complete from partial cue */
    float partial[64];
    memcpy(partial, what, 64 * sizeof(float));

    float completed[256];
    uint32_t completed_dim = 256;
    float confidence;

    EXPECT_EQ(hippo_pattern_complete(hippo, partial, 64, completed, &completed_dim, &confidence), 0);
}

TEST_F(HippocampusTest, PatternCompleteNull) {
    float input[128];
    float output[256];
    uint32_t dim = 256;
    float conf;
    EXPECT_EQ(hippo_pattern_complete(nullptr, input, 128, output, &dim, &conf), -1);
    EXPECT_EQ(hippo_pattern_complete(hippo, nullptr, 128, output, &dim, &conf), -1);
}

TEST_F(HippocampusTest, AssessNovelty) {
    float what[128];
    createTestContent(what, 128, 0.5f);

    /* First input should be novel */
    float novelty;
    bool requires_sep;
    EXPECT_EQ(hippo_assess_novelty(hippo, what, 128, &novelty, &requires_sep), 0);
    EXPECT_GT(novelty, 0.5f);
    EXPECT_TRUE(requires_sep);

    /* Encode it */
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    /* Same input should be familiar */
    EXPECT_EQ(hippo_assess_novelty(hippo, what, 128, &novelty, &requires_sep), 0);
    EXPECT_LT(novelty, 0.5f);
}

TEST_F(HippocampusTest, AssessNoveltyNull) {
    float input[128];
    float novelty;
    bool req;
    EXPECT_EQ(hippo_assess_novelty(nullptr, input, 128, &novelty, &req), -1);
    EXPECT_EQ(hippo_assess_novelty(hippo, nullptr, 128, &novelty, &req), -1);
}

/*=============================================================================
 * SPATIAL NAVIGATION TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, UpdatePosition) {
    float position[3] = {50.0f, 50.0f, 0.0f};
    EXPECT_EQ(hippo_update_position(hippo, position, 3), 0);
    EXPECT_FLOAT_EQ(hippo->current_position[0], 50.0f);
    EXPECT_FLOAT_EQ(hippo->current_position[1], 50.0f);
}

TEST_F(HippocampusTest, UpdatePositionNull) {
    float position[3] = {0};
    EXPECT_EQ(hippo_update_position(nullptr, position, 3), -1);
    EXPECT_EQ(hippo_update_position(hippo, nullptr, 3), -1);
}

TEST_F(HippocampusTest, GetActivePlaceCells) {
    float position[3] = {50.0f, 50.0f, 0.0f};
    hippo_update_position(hippo, position, 3);

    uint32_t cell_ids[100];
    float firing_rates[100];
    uint32_t num_active;
    EXPECT_EQ(hippo_get_active_place_cells(hippo, cell_ids, firing_rates, 100, &num_active), 0);
}

TEST_F(HippocampusTest, GetActivePlaceCellsNull) {
    uint32_t ids[100];
    float rates[100];
    uint32_t num;
    EXPECT_EQ(hippo_get_active_place_cells(nullptr, ids, rates, 100, &num), -1);
    EXPECT_EQ(hippo_get_active_place_cells(hippo, nullptr, rates, 100, &num), -1);
}

TEST_F(HippocampusTest, DecodePosition) {
    float position[3] = {50.0f, 50.0f, 0.0f};
    hippo_update_position(hippo, position, 3);

    float decoded[3];
    float confidence;
    EXPECT_EQ(hippo_decode_position(hippo, decoded, &confidence), 0);
}

TEST_F(HippocampusTest, DecodePositionNull) {
    float decoded[3];
    float conf;
    EXPECT_EQ(hippo_decode_position(nullptr, decoded, &conf), -1);
    EXPECT_EQ(hippo_decode_position(hippo, nullptr, &conf), -1);
}

TEST_F(HippocampusTest, CreatePlaceField) {
    float center[3] = {100.0f, 100.0f, 0.0f};
    uint32_t cell_id;
    EXPECT_EQ(hippo_create_place_field(hippo, center, 10.0f, &cell_id), 0);
}

TEST_F(HippocampusTest, LinkEpisodeToPlace) {
    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &episode_id);

    float position[3] = {75.0f, 75.0f, 0.0f};
    EXPECT_EQ(hippo_link_episode_to_place(hippo, episode_id, position), 0);

    const nimcp_episode_t* ep = hippo_get_episode(hippo, episode_id);
    EXPECT_TRUE(ep->has_spatial_context);
    EXPECT_FLOAT_EQ(ep->associated_position[0], 75.0f);
}

TEST_F(HippocampusTest, GetEpisodesAtLocation) {
    float what[128];
    createTestContent(what, 128, 0.5f);
    float where[3] = {50.0f, 50.0f, 0.0f};

    uint32_t episode_id;
    hippo_encode_episode(hippo, what, 128, where, 3, NULL, 0, 0.0f, 0.5f, &episode_id);

    float query[3] = {52.0f, 48.0f, 0.0f};
    uint32_t found_ids[10];
    uint32_t num_found;

    EXPECT_EQ(hippo_get_episodes_at_location(hippo, query, 10.0f, found_ids, 10, &num_found), 0);
    EXPECT_GE(num_found, 1u);
}

/*=============================================================================
 * OSCILLATION TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, UpdateTheta) {
    float initial = hippo->theta_phase;
    EXPECT_EQ(hippo_update_theta(hippo, 0.01f), 0);
    EXPECT_NE(hippo->theta_phase, initial);
}

TEST_F(HippocampusTest, UpdateThetaNull) {
    EXPECT_EQ(hippo_update_theta(nullptr, 0.01f), -1);
}

TEST_F(HippocampusTest, UpdateGamma) {
    float initial = hippo->gamma_phase;
    EXPECT_EQ(hippo_update_gamma(hippo, 0.01f), 0);
    EXPECT_NE(hippo->gamma_phase, initial);
}

TEST_F(HippocampusTest, UpdateGammaNull) {
    EXPECT_EQ(hippo_update_gamma(nullptr, 0.01f), -1);
}

TEST_F(HippocampusTest, GetThetaPhase) {
    hippo->theta_phase = 1.57f;
    EXPECT_FLOAT_EQ(hippo_get_theta_phase(hippo), 1.57f);
}

TEST_F(HippocampusTest, GetThetaPhaseNull) {
    EXPECT_FLOAT_EQ(hippo_get_theta_phase(nullptr), 0.0f);
}

TEST_F(HippocampusTest, GetGammaPhase) {
    hippo->gamma_phase = 3.14f;
    EXPECT_FLOAT_EQ(hippo_get_gamma_phase(hippo), 3.14f);
}

TEST_F(HippocampusTest, GetOscillationPower) {
    float theta, gamma, ripple;
    EXPECT_EQ(hippo_get_oscillation_power(hippo, &theta, &gamma, &ripple), 0);
    EXPECT_GT(theta, 0.0f);
}

TEST_F(HippocampusTest, SetOscillationState) {
    EXPECT_EQ(hippo_set_oscillation_state(hippo, OSCILLATION_SHARP_WAVE_RIPPLE), 0);
    EXPECT_EQ(hippo->oscillation_state, OSCILLATION_SHARP_WAVE_RIPPLE);
}

TEST_F(HippocampusTest, SetOscillationStateNull) {
    EXPECT_EQ(hippo_set_oscillation_state(nullptr, OSCILLATION_THETA), -1);
}

TEST_F(HippocampusTest, ThetaPhaseWraparound) {
    /* Run many updates to ensure phase wraps correctly */
    for (int i = 0; i < 1000; i++) {
        hippo_update_theta(hippo, 0.01f);
    }
    EXPECT_GE(hippo->theta_phase, 0.0f);
    EXPECT_LT(hippo->theta_phase, 2.0f * M_PI);
}

/*=============================================================================
 * REPLAY AND CONSOLIDATION TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, TriggerReplay) {
    /* Encode some episodes first */
    float what[128];
    for (int i = 0; i < 5; i++) {
        createTestContent(what, 128, (float)i * 0.1f);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);
    }

    EXPECT_EQ(hippo_trigger_replay(hippo, REPLAY_FORWARD), 0);
    EXPECT_EQ(hippo->oscillation_state, OSCILLATION_SHARP_WAVE_RIPPLE);
    EXPECT_EQ(hippo->replays_performed, 1u);
}

TEST_F(HippocampusTest, TriggerReplayNull) {
    EXPECT_EQ(hippo_trigger_replay(nullptr, REPLAY_FORWARD), -1);
}

TEST_F(HippocampusTest, ProcessReplay) {
    float what[128];
    createTestContent(what, 128, 0.5f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    hippo_trigger_replay(hippo, REPLAY_FORWARD);
    EXPECT_EQ(hippo_process_replay(hippo), 0);
    EXPECT_EQ(hippo->oscillation_state, OSCILLATION_THETA);  /* Returns to theta after replay */
}

TEST_F(HippocampusTest, ProcessReplayNull) {
    EXPECT_EQ(hippo_process_replay(nullptr), -1);
}

TEST_F(HippocampusTest, GetLastRipple) {
    float what[128];
    createTestContent(what, 128, 0.5f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    hippo_trigger_replay(hippo, REPLAY_REVERSE);

    const nimcp_ripple_event_t* ripple = hippo_get_last_ripple(hippo);
    ASSERT_NE(ripple, nullptr);
    EXPECT_EQ(ripple->replay_direction, REPLAY_REVERSE);
}

TEST_F(HippocampusTest, GetLastRippleNull) {
    EXPECT_EQ(hippo_get_last_ripple(nullptr), nullptr);
}

TEST_F(HippocampusTest, ConsolidateMemories) {
    float what[128];
    createTestContent(what, 128, 0.5f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.8f, &id);

    float initial_consolidation = hippo->episodes[id].consolidation_level;

    for (int i = 0; i < 100; i++) {
        hippo_consolidate_memories(hippo, 0.1f);
    }

    EXPECT_GT(hippo->episodes[id].consolidation_level, initial_consolidation);
}

TEST_F(HippocampusTest, ConsolidateMemoriesNull) {
    EXPECT_EQ(hippo_consolidate_memories(nullptr, 0.1f), -1);
}

TEST_F(HippocampusTest, GetConsolidationLevel) {
    float what[128];
    createTestContent(what, 128, 0.5f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    float level = hippo_get_consolidation_level(hippo, id);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(HippocampusTest, GetConsolidationLevelInvalid) {
    EXPECT_FLOAT_EQ(hippo_get_consolidation_level(hippo, 99999), 0.0f);
    EXPECT_FLOAT_EQ(hippo_get_consolidation_level(nullptr, 0), 0.0f);
}

/*=============================================================================
 * SUBREGION ACCESS TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, GetDgPattern) {
    float pattern[1024];
    uint32_t size = 1024;
    EXPECT_EQ(hippo_get_dg_pattern(hippo, pattern, 1024, &size), 0);
    EXPECT_GT(size, 0u);
}

TEST_F(HippocampusTest, GetDgPatternNull) {
    float pattern[1024];
    uint32_t size = 1024;
    EXPECT_EQ(hippo_get_dg_pattern(nullptr, pattern, 1024, &size), -1);
    EXPECT_EQ(hippo_get_dg_pattern(hippo, nullptr, 1024, &size), -1);
}

TEST_F(HippocampusTest, GetCa3Pattern) {
    float pattern[512];
    uint32_t size = 512;
    EXPECT_EQ(hippo_get_ca3_pattern(hippo, pattern, 512, &size), 0);
    EXPECT_GT(size, 0u);
}

TEST_F(HippocampusTest, GetCa1Pattern) {
    float pattern[512];
    uint32_t size = 512;
    EXPECT_EQ(hippo_get_ca1_pattern(hippo, pattern, 512, &size), 0);
    EXPECT_GT(size, 0u);
}

TEST_F(HippocampusTest, GetSubiculumOutput) {
    float output[256];
    uint32_t size = 256;
    EXPECT_EQ(hippo_get_subiculum_output(hippo, output, 256, &size), 0);
    EXPECT_GT(size, 0u);
}

TEST_F(HippocampusTest, ActivateDg) {
    float input[128];
    createTestContent(input, 128, 0.5f);

    EXPECT_EQ(hippo_activate_dg(hippo, input, 128), 0);

    /* Check sparse activation */
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < hippo->num_dg_cells; i++) {
        if (hippo->dg_cells[i].activation > 0.1f) active_count++;
    }
    /* Should be sparse */
    EXPECT_LT(active_count, hippo->num_dg_cells / 2);
}

TEST_F(HippocampusTest, ActivateDgNull) {
    float input[128];
    EXPECT_EQ(hippo_activate_dg(nullptr, input, 128), -1);
    EXPECT_EQ(hippo_activate_dg(hippo, nullptr, 128), -1);
}

TEST_F(HippocampusTest, PropagateTrisynaptic) {
    float input[128];
    createTestContent(input, 128, 0.5f);
    hippo_activate_dg(hippo, input, 128);

    EXPECT_EQ(hippo_propagate_trisynaptic(hippo), 0);

    /* All regions should have some activity */
    float ca3_sum = 0, ca1_sum = 0, sub_sum = 0;
    for (uint32_t i = 0; i < hippo->num_ca3_cells; i++) ca3_sum += hippo->ca3_cells[i].activation;
    for (uint32_t i = 0; i < hippo->num_ca1_cells; i++) ca1_sum += hippo->ca1_cells[i].activation;
    for (uint32_t i = 0; i < hippo->num_subiculum_cells; i++) sub_sum += hippo->subiculum_cells[i].activation;

    EXPECT_GT(ca3_sum, 0.0f);
    EXPECT_GT(ca1_sum, 0.0f);
    EXPECT_GT(sub_sum, 0.0f);
}

TEST_F(HippocampusTest, PropagateTrisynapticNull) {
    EXPECT_EQ(hippo_propagate_trisynaptic(nullptr), -1);
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, ProcessIncoming) {
    EXPECT_EQ(hippo_process_incoming(hippo), 0);
}

TEST_F(HippocampusTest, ProcessIncomingNull) {
    EXPECT_EQ(hippo_process_incoming(nullptr), -1);
}

TEST_F(HippocampusTest, SendOutgoing) {
    EXPECT_EQ(hippo_send_outgoing(hippo), 0);
}

TEST_F(HippocampusTest, SendOutgoingNull) {
    EXPECT_EQ(hippo_send_outgoing(nullptr), -1);
}

TEST_F(HippocampusTest, BidirectionalUpdate) {
    EXPECT_EQ(hippo_bidirectional_update(hippo, 0.01f), 0);
}

TEST_F(HippocampusTest, BidirectionalUpdateNull) {
    EXPECT_EQ(hippo_bidirectional_update(nullptr, 0.01f), -1);
}

TEST_F(HippocampusTest, SyncEntorhinal) {
    EXPECT_EQ(hippo_sync_entorhinal(hippo), 0);
}

TEST_F(HippocampusTest, SyncPerirhinal) {
    EXPECT_EQ(hippo_sync_perirhinal(hippo), 0);
}

TEST_F(HippocampusTest, SyncParahippocampal) {
    EXPECT_EQ(hippo_sync_parahippocampal(hippo), 0);
}

TEST_F(HippocampusTest, SyncMammillary) {
    EXPECT_EQ(hippo_sync_mammillary(hippo), 0);
}

TEST_F(HippocampusTest, SyncThalamus) {
    EXPECT_EQ(hippo_sync_thalamus(hippo), 0);
}

TEST_F(HippocampusTest, SyncCortical) {
    EXPECT_EQ(hippo_sync_cortical(hippo), 0);
}

/*=============================================================================
 * BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, InitPrimeResonanceBridge) {
    EXPECT_EQ(hippo_init_prime_resonance_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->prime_resonance_bridge.initialized);
    EXPECT_FLOAT_EQ(hippo->prime_resonance_bridge.resonance_frequency, 40.0f);
}

TEST_F(HippocampusTest, InitImmuneBridge) {
    EXPECT_EQ(hippo_init_immune_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->immune_bridge.initialized);
    EXPECT_FLOAT_EQ(hippo->immune_bridge.health_score, 1.0f);
}

TEST_F(HippocampusTest, InitBioAsyncBridge) {
    EXPECT_EQ(hippo_init_bio_async_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->bio_async_bridge.initialized);
}

TEST_F(HippocampusTest, InitBrainInitBridge) {
    EXPECT_EQ(hippo_init_brain_init_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->brain_init_bridge.initialized);
    EXPECT_TRUE(hippo->brain_init_bridge.full_integration_complete);
}

TEST_F(HippocampusTest, InitSecurityBridge) {
    EXPECT_EQ(hippo_init_security_bridge(hippo, nullptr, nullptr), 0);
    EXPECT_TRUE(hippo->security_bridge.initialized);
    EXPECT_TRUE(hippo->security_bridge.integrity_checking);
}

TEST_F(HippocampusTest, InitLoggingBridge) {
    EXPECT_EQ(hippo_init_logging_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->logging_bridge.initialized);
}

TEST_F(HippocampusTest, InitCognitiveBridge) {
    EXPECT_EQ(hippo_init_cognitive_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->cognitive_bridge.initialized);
}

TEST_F(HippocampusTest, InitTrainingBridge) {
    EXPECT_EQ(hippo_init_training_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->training_bridge.initialized);
}

TEST_F(HippocampusTest, InitOmniBridge) {
    EXPECT_EQ(hippo_init_omni_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->omni_bridge.initialized);
    EXPECT_TRUE(hippo->omni_bridge.bidirectional_active);
}

TEST_F(HippocampusTest, InitHypothalamusBridge) {
    EXPECT_EQ(hippo_init_hypothalamus_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->hypothalamus_bridge.initialized);
}

TEST_F(HippocampusTest, InitSubstrateBridge) {
    EXPECT_EQ(hippo_init_substrate_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->substrate_bridge.initialized);
    EXPECT_FLOAT_EQ(hippo->substrate_bridge.atp_level, 1.0f);
}

TEST_F(HippocampusTest, InitThalamusBridge) {
    EXPECT_EQ(hippo_init_thalamus_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->thalamus_bridge.initialized);
}

TEST_F(HippocampusTest, InitPortiaBridge) {
    EXPECT_EQ(hippo_init_portia_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->portia_bridge.initialized);
    EXPECT_TRUE(hippo->portia_bridge.prospective_memory_active);
}

TEST_F(HippocampusTest, InitDragonflyBridge) {
    EXPECT_EQ(hippo_init_dragonfly_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->dragonfly_bridge.initialized);
}

TEST_F(HippocampusTest, InitPerceptionBridge) {
    EXPECT_EQ(hippo_init_perception_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->perception_bridge.initialized);
}

TEST_F(HippocampusTest, InitSnnBridge) {
    EXPECT_EQ(hippo_init_snn_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->snn_bridge.initialized);
}

TEST_F(HippocampusTest, InitPlasticityBridge) {
    EXPECT_EQ(hippo_init_plasticity_bridge(hippo, nullptr, nullptr), 0);
    EXPECT_TRUE(hippo->plasticity_bridge.initialized);
    EXPECT_TRUE(hippo->plasticity_bridge.hebbian_learning);
    EXPECT_TRUE(hippo->plasticity_bridge.stdp_enabled);
}

TEST_F(HippocampusTest, InitEntorhinalBridge) {
    EXPECT_EQ(hippo_init_entorhinal_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->entorhinal_bridge.initialized);
}

TEST_F(HippocampusTest, InitPerirhinalBridge) {
    EXPECT_EQ(hippo_init_perirhinal_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->perirhinal_bridge.initialized);
}

TEST_F(HippocampusTest, InitParahippocampalBridge) {
    EXPECT_EQ(hippo_init_parahippocampal_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->parahippocampal_bridge.initialized);
}

TEST_F(HippocampusTest, InitMammillaryBridge) {
    EXPECT_EQ(hippo_init_mammillary_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->mammillary_bridge.initialized);
}

TEST_F(HippocampusTest, InitCerebellumBridge) {
    EXPECT_EQ(hippo_init_cerebellum_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->cerebellum_bridge.initialized);
}

TEST_F(HippocampusTest, InitMedullaBridge) {
    EXPECT_EQ(hippo_init_medulla_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->medulla_bridge.initialized);
}

TEST_F(HippocampusTest, InitSwarmBridge) {
    EXPECT_EQ(hippo_init_swarm_bridge(hippo, nullptr), 0);
    EXPECT_TRUE(hippo->swarm_bridge.initialized);
}

TEST_F(HippocampusTest, InitAllBridges) {
    EXPECT_EQ(hippo_init_all_bridges(hippo, nullptr), 0);

    EXPECT_TRUE(hippo->prime_resonance_bridge.initialized);
    EXPECT_TRUE(hippo->immune_bridge.initialized);
    EXPECT_TRUE(hippo->bio_async_bridge.initialized);
    EXPECT_TRUE(hippo->security_bridge.initialized);
    EXPECT_TRUE(hippo->cognitive_bridge.initialized);
    EXPECT_TRUE(hippo->omni_bridge.initialized);
    EXPECT_TRUE(hippo->hypothalamus_bridge.initialized);
    EXPECT_TRUE(hippo->substrate_bridge.initialized);
    EXPECT_TRUE(hippo->thalamus_bridge.initialized);
    EXPECT_TRUE(hippo->portia_bridge.initialized);
    EXPECT_TRUE(hippo->dragonfly_bridge.initialized);
    EXPECT_TRUE(hippo->snn_bridge.initialized);
    EXPECT_TRUE(hippo->plasticity_bridge.initialized);
}

/*=============================================================================
 * PRIME RESONANCE INTEGRATION TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, TagWithResonance) {
    float what[128];
    createTestContent(what, 128, 0.5f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    uint64_t resonance_sig = 0x12345678;
    EXPECT_EQ(hippo_tag_with_resonance(hippo, id, resonance_sig), 0);

    const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
    EXPECT_EQ(ep->resonance_signature, resonance_sig);
}

TEST_F(HippocampusTest, TagWithResonanceInvalid) {
    EXPECT_EQ(hippo_tag_with_resonance(hippo, 99999, 0x123), -1);
    EXPECT_EQ(hippo_tag_with_resonance(nullptr, 0, 0x123), -1);
}

TEST_F(HippocampusTest, FindByResonance) {
    float what[128];
    uint64_t sig = 0xABCDEF00;

    for (int i = 0; i < 3; i++) {
        createTestContent(what, 128, (float)i * 0.1f);
        uint32_t id;
        hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);
        hippo_tag_with_resonance(hippo, id, sig);
    }

    /* Add one without the tag */
    createTestContent(what, 128, 1.0f);
    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);

    uint32_t found_ids[10];
    uint32_t num_found;
    EXPECT_EQ(hippo_find_by_resonance(hippo, sig, found_ids, 10, &num_found), 0);
    EXPECT_EQ(num_found, 3u);
}

TEST_F(HippocampusTest, ResonanceEnhancedEncode) {
    hippo_init_prime_resonance_bridge(hippo, nullptr);
    hippo->prime_resonance_bridge.last_resonance_tag = 0x999;

    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t id;
    EXPECT_EQ(hippo_resonance_enhanced_encode(hippo, what, 128, &id), 0);

    const nimcp_episode_t* ep = hippo_get_episode(hippo, id);
    EXPECT_GT(ep->encoding_strength, 0.5f);  /* Should be enhanced */
    EXPECT_EQ(ep->resonance_signature, 0x999ull);
}

TEST_F(HippocampusTest, ResonanceGuidedRetrieve) {
    hippo_init_prime_resonance_bridge(hippo, nullptr);

    float what[128];
    createTestContent(what, 128, 0.5f);

    uint32_t id;
    hippo_encode_episode(hippo, what, 128, NULL, 0, NULL, 0, 0.0f, 0.5f, &id);
    hippo_tag_with_resonance(hippo, id, 0x777);
    hippo->prime_resonance_bridge.last_resonance_tag = 0x777;

    uint32_t retrieved_id;
    float confidence;
    EXPECT_EQ(hippo_resonance_guided_retrieve(hippo, what, 128, &retrieved_id, &confidence), 0);
    EXPECT_EQ(retrieved_id, id);
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, GetStatus) {
    EXPECT_EQ(hippo_get_status(hippo), HIPPO_STATUS_READY);
}

TEST_F(HippocampusTest, GetStatusNull) {
    EXPECT_EQ(hippo_get_status(nullptr), HIPPO_STATUS_ERROR);
}

TEST_F(HippocampusTest, GetLastError) {
    EXPECT_EQ(hippo_get_last_error(hippo), HIPPO_ERROR_NONE);
}

TEST_F(HippocampusTest, GetLastErrorNull) {
    EXPECT_EQ(hippo_get_last_error(nullptr), HIPPO_ERROR_INTERNAL);
}

TEST_F(HippocampusTest, ErrorString) {
    EXPECT_STREQ(hippo_error_string(HIPPO_ERROR_NONE), "No error");
    EXPECT_STREQ(hippo_error_string(HIPPO_ERROR_MEMORY_FULL), "Memory full");
    EXPECT_STREQ(hippo_error_string(HIPPO_ERROR_ENCODING_FAILED), "Encoding failed");
    EXPECT_STREQ(hippo_error_string(HIPPO_ERROR_RETRIEVAL_FAILED), "Retrieval failed");
}

TEST_F(HippocampusTest, StatusString) {
    EXPECT_STREQ(hippo_status_string(HIPPO_STATUS_IDLE), "Idle");
    EXPECT_STREQ(hippo_status_string(HIPPO_STATUS_READY), "Ready");
    EXPECT_STREQ(hippo_status_string(HIPPO_STATUS_ENCODING), "Encoding");
    EXPECT_STREQ(hippo_status_string(HIPPO_STATUS_RETRIEVING), "Retrieving");
    EXPECT_STREQ(hippo_status_string(HIPPO_STATUS_REPLAYING), "Replaying");
}

TEST_F(HippocampusTest, GetStats) {
    hippo_stats_t stats;
    EXPECT_EQ(hippo_get_stats(hippo, &stats), 0);
    EXPECT_EQ(stats.total_episodes, 0u);
}

TEST_F(HippocampusTest, GetStatsNull) {
    hippo_stats_t stats;
    EXPECT_EQ(hippo_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(hippo_get_stats(hippo, nullptr), -1);
}

TEST_F(HippocampusTest, GetConfig) {
    hippo_config_t config;
    EXPECT_EQ(hippo_get_config(hippo, &config), 0);
}

TEST_F(HippocampusTest, GetConfigNull) {
    hippo_config_t config;
    EXPECT_EQ(hippo_get_config(nullptr, &config), -1);
    EXPECT_EQ(hippo_get_config(hippo, nullptr), -1);
}

TEST_F(HippocampusTest, GetHealthStatus) {
    float health = hippo_get_health_status(hippo);
    EXPECT_GT(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(HippocampusTest, GetHealthStatusNull) {
    EXPECT_FLOAT_EQ(hippo_get_health_status(nullptr), 0.0f);
}

TEST_F(HippocampusTest, LogDiagnostics) {
    EXPECT_EQ(hippo_log_diagnostics(hippo), 0);
}

TEST_F(HippocampusTest, LogDiagnosticsNull) {
    EXPECT_EQ(hippo_log_diagnostics(nullptr), -1);
}

/*=============================================================================
 * CELL ACTIVITY TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, GetDgCellActivity) {
    float activity[1024];
    size_t count = hippo_get_dg_cell_activity(hippo, activity, 1024);
    EXPECT_GT(count, 0u);
}

TEST_F(HippocampusTest, GetDgCellActivityNull) {
    float activity[1024];
    EXPECT_EQ(hippo_get_dg_cell_activity(nullptr, activity, 1024), 0u);
    EXPECT_EQ(hippo_get_dg_cell_activity(hippo, nullptr, 1024), 0u);
}

TEST_F(HippocampusTest, GetCa3CellActivity) {
    float activity[512];
    size_t count = hippo_get_ca3_cell_activity(hippo, activity, 512);
    EXPECT_GT(count, 0u);
}

TEST_F(HippocampusTest, GetCa1CellActivity) {
    float activity[512];
    size_t count = hippo_get_ca1_cell_activity(hippo, activity, 512);
    EXPECT_GT(count, 0u);
}

TEST_F(HippocampusTest, GetSubiculumActivity) {
    float activity[256];
    size_t count = hippo_get_subiculum_activity(hippo, activity, 256);
    EXPECT_GT(count, 0u);
}

/*=============================================================================
 * SERIALIZATION TESTS
 *===========================================================================*/

TEST_F(HippocampusTest, GetSerializationSize) {
    size_t size = hippo_get_serialization_size(hippo);
    EXPECT_GT(size, 0u);
}

TEST_F(HippocampusTest, GetSerializationSizeNull) {
    EXPECT_EQ(hippo_get_serialization_size(nullptr), 0u);
}

TEST_F(HippocampusTest, Serialize) {
    size_t size = hippo_get_serialization_size(hippo);
    uint8_t* buffer = new uint8_t[size];
    size_t written;

    EXPECT_EQ(hippo_serialize(hippo, buffer, size, &written), 0);
    EXPECT_GT(written, 0u);

    delete[] buffer;
}

TEST_F(HippocampusTest, SerializeNull) {
    uint8_t buffer[1024];
    size_t written;
    EXPECT_EQ(hippo_serialize(nullptr, buffer, 1024, &written), -1);
    EXPECT_EQ(hippo_serialize(hippo, nullptr, 1024, &written), -1);
    EXPECT_EQ(hippo_serialize(hippo, buffer, 1024, nullptr), -1);
}

TEST_F(HippocampusTest, Deserialize) {
    hippo->theta_phase = 1.5f;
    hippo->gamma_phase = 2.5f;
    hippo->current_position[0] = 50.0f;

    size_t size = hippo_get_serialization_size(hippo);
    uint8_t* buffer = new uint8_t[size];
    size_t written;
    hippo_serialize(hippo, buffer, size, &written);

    size_t bytes_read;
    nimcp_hippocampus_t* restored = hippo_deserialize(buffer, size, &bytes_read);

    ASSERT_NE(restored, nullptr);
    EXPECT_FLOAT_EQ(restored->theta_phase, 1.5f);
    EXPECT_FLOAT_EQ(restored->gamma_phase, 2.5f);
    EXPECT_FLOAT_EQ(restored->current_position[0], 50.0f);

    hippo_destroy(restored);
    delete[] buffer;
}

TEST_F(HippocampusTest, DeserializeNull) {
    size_t bytes_read;
    EXPECT_EQ(hippo_deserialize(nullptr, 100, &bytes_read), nullptr);
}
