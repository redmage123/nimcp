/**
 * @file test_fep_context.cpp
 * @brief Unit tests for FEP Context module
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include "cognitive/free_energy/nimcp_fep_context.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class FEPContextTest : public ::testing::Test {
protected:
    fep_context_system_t* ctx = nullptr;
    fep_system_t* fep = nullptr;

    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;

    void SetUp() override {
        fep_context_config_t config;
        fep_context_default_config(&config);
        ctx = fep_context_create(&config);

        /* Create FEP system for integration tests */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
    }

    void TearDown() override {
        if (ctx) {
            fep_context_destroy(ctx);
            ctx = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(FEPContextTest, CreateDestroy) {
    ASSERT_NE(ctx, nullptr);
}

TEST_F(FEPContextTest, CreateWithNullConfig) {
    fep_context_system_t* sys = fep_context_create(nullptr);
    ASSERT_NE(sys, nullptr);
    fep_context_destroy(sys);
}

TEST_F(FEPContextTest, DestroyNull) {
    fep_context_destroy(nullptr);  /* Should not crash */
}

TEST_F(FEPContextTest, DefaultConfig) {
    fep_context_config_t config;
    fep_context_default_config(&config);

    EXPECT_GT(config.max_contexts, 0u);
    EXPECT_GE(config.switch_mode, CONTEXT_SWITCH_HARD);
    EXPECT_LE(config.switch_mode, CONTEXT_SWITCH_GATED);
    EXPECT_GT(config.switch_threshold, 0.0f);
    EXPECT_LE(config.switch_threshold, 1.0f);
    EXPECT_GT(config.interpolation_rate, 0.0f);
    EXPECT_LE(config.interpolation_rate, 1.0f);
}

TEST_F(FEPContextTest, CreateWithCustomConfig) {
    fep_context_config_t config;
    fep_context_default_config(&config);
    config.max_contexts = 32;
    config.switch_mode = CONTEXT_SWITCH_SOFT;
    config.switch_threshold = 0.8f;

    fep_context_system_t* custom = fep_context_create(&config);
    ASSERT_NE(custom, nullptr);
    fep_context_destroy(custom);
}

/* ============================================================================
 * Context Management Tests
 * ============================================================================ */

TEST_F(FEPContextTest, AddContext) {
    float priors[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t context_id;

    int ret = fep_context_add(ctx, "driving", priors, 8, &context_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextTest, AddMultipleContexts) {
    float priors1[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f};
    float priors2[8] = {0.4f, 0.3f, 0.2f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id1, id2;

    EXPECT_EQ(fep_context_add(ctx, "context1", priors1, 8, &id1), 0);
    EXPECT_EQ(fep_context_add(ctx, "context2", priors2, 8, &id2), 0);
    EXPECT_NE(id1, id2);
}

TEST_F(FEPContextTest, AddContextNullParams) {
    float priors[8] = {0.0f};
    uint32_t context_id;

    EXPECT_EQ(fep_context_add(nullptr, "test", priors, 8, &context_id), -1);
    EXPECT_EQ(fep_context_add(ctx, nullptr, priors, 8, &context_id), -1);
    EXPECT_EQ(fep_context_add(ctx, "test", nullptr, 8, &context_id), -1);
    EXPECT_EQ(fep_context_add(ctx, "test", priors, 0, &context_id), -1);
}

TEST_F(FEPContextTest, GetContext) {
    float priors[8] = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "test_context", priors, 8, &context_id);

    fep_context_t context;
    int ret = fep_context_get(ctx, context_id, &context);
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(context.name, "test_context");
    EXPECT_EQ(context.belief_dim, 8u);
}

TEST_F(FEPContextTest, GetContextNullParams) {
    fep_context_t context;
    EXPECT_EQ(fep_context_get(nullptr, 0, &context), -1);
    EXPECT_EQ(fep_context_get(ctx, 0, nullptr), -1);
}

TEST_F(FEPContextTest, RemoveContext) {
    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "temp", priors, 8, &context_id);

    int ret = fep_context_remove(ctx, context_id);
    EXPECT_EQ(ret, 0);

    /* Try to get removed context */
    fep_context_t context;
    EXPECT_NE(fep_context_get(ctx, context_id, &context), 0);
}

TEST_F(FEPContextTest, RemoveContextNullParams) {
    EXPECT_EQ(fep_context_remove(nullptr, 0), -1);
}

TEST_F(FEPContextTest, UpdateContext) {
    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "update_test", priors, 8, &context_id);

    float new_priors[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int ret = fep_context_update(ctx, context_id, new_priors, 8);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextTest, UpdateContextNullParams) {
    float priors[8] = {0.0f};
    EXPECT_EQ(fep_context_update(nullptr, 0, priors, 8), -1);
    EXPECT_EQ(fep_context_update(ctx, 0, nullptr, 8), -1);
}

/* ============================================================================
 * Context Switching Tests
 * ============================================================================ */

TEST_F(FEPContextTest, SwitchContext) {
    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "switch_target", priors, 8, &context_id);

    fep_context_connect(ctx, fep);
    int ret = fep_context_switch(ctx, fep, context_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextTest, SwitchContextNullParams) {
    EXPECT_EQ(fep_context_switch(nullptr, fep, 0), -1);
    EXPECT_EQ(fep_context_switch(ctx, nullptr, 0), -1);
}

TEST_F(FEPContextTest, InferContext) {
    float priors1[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float priors2[8] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id1, id2;
    fep_context_add(ctx, "ctx1", priors1, 8, &id1);
    fep_context_add(ctx, "ctx2", priors2, 8, &id2);

    fep_context_connect(ctx, fep);

    float observation[8] = {0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t inferred_id;
    float confidence;

    int ret = fep_context_infer(ctx, fep, observation, 8, &inferred_id, &confidence);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(FEPContextTest, InferContextNullParams) {
    float obs[8] = {0.0f};
    uint32_t id;
    float conf;

    EXPECT_EQ(fep_context_infer(nullptr, fep, obs, 8, &id, &conf), -1);
    EXPECT_EQ(fep_context_infer(ctx, nullptr, obs, 8, &id, &conf), -1);
    EXPECT_EQ(fep_context_infer(ctx, fep, nullptr, 8, &id, &conf), -1);
}

TEST_F(FEPContextTest, AutoSwitch) {
    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "auto_switch", priors, 8, &context_id);

    fep_context_connect(ctx, fep);

    float observation[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int ret = fep_context_auto_switch(ctx, fep, observation, 8);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextTest, AutoSwitchNullParams) {
    float obs[8] = {0.0f};
    EXPECT_EQ(fep_context_auto_switch(nullptr, fep, obs, 8), -1);
    EXPECT_EQ(fep_context_auto_switch(ctx, nullptr, obs, 8), -1);
    EXPECT_EQ(fep_context_auto_switch(ctx, fep, nullptr, 8), -1);
}

/* ============================================================================
 * Context Application Tests
 * ============================================================================ */

TEST_F(FEPContextTest, ApplyContext) {
    float priors[8] = {0.25f, 0.25f, 0.25f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "apply_test", priors, 8, &context_id);

    fep_context_connect(ctx, fep);
    int ret = fep_context_apply(ctx, fep, context_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextTest, ApplyContextNullParams) {
    EXPECT_EQ(fep_context_apply(nullptr, fep, 0), -1);
    EXPECT_EQ(fep_context_apply(ctx, nullptr, 0), -1);
}

TEST_F(FEPContextTest, BlendContexts) {
    float priors1[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float priors2[8] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id1, id2;
    fep_context_add(ctx, "blend1", priors1, 8, &id1);
    fep_context_add(ctx, "blend2", priors2, 8, &id2);

    fep_context_connect(ctx, fep);
    int ret = fep_context_blend(ctx, fep, id1, id2, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextTest, BlendContextsNullParams) {
    EXPECT_EQ(fep_context_blend(nullptr, fep, 0, 1, 0.5f), -1);
    EXPECT_EQ(fep_context_blend(ctx, nullptr, 0, 1, 0.5f), -1);
}

TEST_F(FEPContextTest, BlendContextsBoundaryFactors) {
    float priors1[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float priors2[8] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id1, id2;
    fep_context_add(ctx, "blend_a", priors1, 8, &id1);
    fep_context_add(ctx, "blend_b", priors2, 8, &id2);

    fep_context_connect(ctx, fep);

    /* Factor 0 = all context 1 */
    EXPECT_EQ(fep_context_blend(ctx, fep, id1, id2, 0.0f), 0);
    /* Factor 1 = all context 2 */
    EXPECT_EQ(fep_context_blend(ctx, fep, id1, id2, 1.0f), 0);
}

/* ============================================================================
 * Context Learning Tests
 * ============================================================================ */

TEST_F(FEPContextTest, LearnFromExperience) {
    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "learn_test", priors, 8, &context_id);

    fep_context_connect(ctx, fep);
    int ret = fep_context_learn_from_experience(ctx, fep, context_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextTest, LearnFromExperienceNullParams) {
    EXPECT_EQ(fep_context_learn_from_experience(nullptr, fep, 0), -1);
    EXPECT_EQ(fep_context_learn_from_experience(ctx, nullptr, 0), -1);
}

TEST_F(FEPContextTest, CreateFromCurrent) {
    fep_context_connect(ctx, fep);

    uint32_t new_context_id;
    int ret = fep_context_create_from_current(ctx, fep, "new_context", &new_context_id);
    EXPECT_EQ(ret, 0);

    fep_context_t context;
    EXPECT_EQ(fep_context_get(ctx, new_context_id, &context), 0);
    EXPECT_STREQ(context.name, "new_context");
}

TEST_F(FEPContextTest, CreateFromCurrentNullParams) {
    uint32_t id;
    EXPECT_EQ(fep_context_create_from_current(nullptr, fep, "test", &id), -1);
    EXPECT_EQ(fep_context_create_from_current(ctx, nullptr, "test", &id), -1);
    EXPECT_EQ(fep_context_create_from_current(ctx, fep, nullptr, &id), -1);
}

/* ============================================================================
 * State/Query Tests
 * ============================================================================ */

TEST_F(FEPContextTest, GetState) {
    fep_context_state_t state;
    int ret = fep_context_get_state(ctx, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.num_contexts, 0u);
    EXPECT_FALSE(state.switching_in_progress);
}

TEST_F(FEPContextTest, GetStateNullParams) {
    fep_context_state_t state;
    EXPECT_EQ(fep_context_get_state(nullptr, &state), -1);
    EXPECT_EQ(fep_context_get_state(ctx, nullptr), -1);
}

TEST_F(FEPContextTest, GetStateAfterAddingContexts) {
    float priors[8] = {0.0f};
    uint32_t id1, id2, id3;
    fep_context_add(ctx, "ctx1", priors, 8, &id1);
    fep_context_add(ctx, "ctx2", priors, 8, &id2);
    fep_context_add(ctx, "ctx3", priors, 8, &id3);

    fep_context_state_t state;
    fep_context_get_state(ctx, &state);
    EXPECT_EQ(state.num_contexts, 3u);
}

TEST_F(FEPContextTest, GetActive) {
    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "active_test", priors, 8, &context_id);

    fep_context_connect(ctx, fep);
    fep_context_switch(ctx, fep, context_id);

    uint32_t active_id;
    int ret = fep_context_get_active(ctx, &active_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextTest, GetActiveNullParams) {
    uint32_t id;
    EXPECT_EQ(fep_context_get_active(nullptr, &id), -1);
    EXPECT_EQ(fep_context_get_active(ctx, nullptr), -1);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(FEPContextTest, Connect) {
    int ret = fep_context_connect(ctx, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextTest, ConnectNullParams) {
    EXPECT_EQ(fep_context_connect(nullptr, fep), -1);
    EXPECT_EQ(fep_context_connect(ctx, nullptr), -1);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(FEPContextTest, BioAsyncConnectDisconnect) {
    EXPECT_FALSE(fep_context_is_bio_async_connected(ctx));

    int ret = fep_context_connect_bio_async(ctx);
    EXPECT_EQ(ret, 0);

    ret = fep_context_disconnect_bio_async(ctx);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(fep_context_is_bio_async_connected(ctx));
}

TEST_F(FEPContextTest, BioAsyncDoubleConnect) {
    fep_context_connect_bio_async(ctx);
    int ret = fep_context_connect_bio_async(ctx);
    EXPECT_EQ(ret, 0);
    fep_context_disconnect_bio_async(ctx);
}

TEST_F(FEPContextTest, BioAsyncNullParams) {
    EXPECT_EQ(fep_context_connect_bio_async(nullptr), -1);
    EXPECT_EQ(fep_context_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(fep_context_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Switching Mode Tests
 * ============================================================================ */

TEST_F(FEPContextTest, HardSwitchMode) {
    fep_context_config_t config;
    fep_context_default_config(&config);
    config.switch_mode = CONTEXT_SWITCH_HARD;

    fep_context_system_t* hard_ctx = fep_context_create(&config);
    ASSERT_NE(hard_ctx, nullptr);

    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(hard_ctx, "hard_test", priors, 8, &context_id);

    fep_context_connect(hard_ctx, fep);
    EXPECT_EQ(fep_context_switch(hard_ctx, fep, context_id), 0);

    fep_context_destroy(hard_ctx);
}

TEST_F(FEPContextTest, SoftSwitchMode) {
    fep_context_config_t config;
    fep_context_default_config(&config);
    config.switch_mode = CONTEXT_SWITCH_SOFT;

    fep_context_system_t* soft_ctx = fep_context_create(&config);
    ASSERT_NE(soft_ctx, nullptr);

    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(soft_ctx, "soft_test", priors, 8, &context_id);

    fep_context_connect(soft_ctx, fep);
    EXPECT_EQ(fep_context_switch(soft_ctx, fep, context_id), 0);

    fep_context_destroy(soft_ctx);
}

TEST_F(FEPContextTest, GatedSwitchMode) {
    fep_context_config_t config;
    fep_context_default_config(&config);
    config.switch_mode = CONTEXT_SWITCH_GATED;
    config.switch_threshold = 0.8f;

    fep_context_system_t* gated_ctx = fep_context_create(&config);
    ASSERT_NE(gated_ctx, nullptr);

    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(gated_ctx, "gated_test", priors, 8, &context_id);

    fep_context_connect(gated_ctx, fep);
    EXPECT_EQ(fep_context_switch(gated_ctx, fep, context_id), 0);

    fep_context_destroy(gated_ctx);
}

/* ============================================================================
 * Configuration Feature Tests
 * ============================================================================ */

TEST_F(FEPContextTest, DisabledLearning) {
    fep_context_config_t config;
    fep_context_default_config(&config);
    config.enable_context_learning = false;
    config.enable_context_creation = false;

    fep_context_system_t* no_learn = fep_context_create(&config);
    ASSERT_NE(no_learn, nullptr);

    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(no_learn, "no_learn", priors, 8, &context_id);

    fep_context_connect(no_learn, fep);

    /* Should return success but not update */
    int ret = fep_context_learn_from_experience(no_learn, fep, context_id);
    EXPECT_EQ(ret, 0);

    fep_context_destroy(no_learn);
}

TEST_F(FEPContextTest, MaxContextsLimit) {
    fep_context_config_t config;
    fep_context_default_config(&config);
    config.max_contexts = 3;

    fep_context_system_t* limited = fep_context_create(&config);
    ASSERT_NE(limited, nullptr);

    float priors[8] = {0.0f};
    uint32_t id;

    EXPECT_EQ(fep_context_add(limited, "ctx1", priors, 8, &id), 0);
    EXPECT_EQ(fep_context_add(limited, "ctx2", priors, 8, &id), 0);
    EXPECT_EQ(fep_context_add(limited, "ctx3", priors, 8, &id), 0);
    /* Fourth should fail */
    EXPECT_NE(fep_context_add(limited, "ctx4", priors, 8, &id), 0);

    fep_context_destroy(limited);
}

/* ============================================================================
 * Context Activation Tests
 * ============================================================================ */

TEST_F(FEPContextTest, ContextActivationTracking) {
    float priors[8] = {0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "activation_test", priors, 8, &context_id);

    fep_context_connect(ctx, fep);

    /* Switch multiple times */
    for (int i = 0; i < 5; i++) {
        fep_context_switch(ctx, fep, context_id);
    }

    fep_context_t context;
    fep_context_get(ctx, context_id, &context);
    EXPECT_GT(context.use_count, 0u);
}

/* ============================================================================
 * Long Name Tests
 * ============================================================================ */

TEST_F(FEPContextTest, LongContextName) {
    float priors[8] = {0.0f};
    uint32_t context_id;

    /* Name at max length */
    char long_name[FEP_CONTEXT_MAX_NAME_LEN];
    memset(long_name, 'a', FEP_CONTEXT_MAX_NAME_LEN - 1);
    long_name[FEP_CONTEXT_MAX_NAME_LEN - 1] = '\0';

    int ret = fep_context_add(ctx, long_name, priors, 8, &context_id);
    EXPECT_EQ(ret, 0);

    fep_context_t context;
    fep_context_get(ctx, context_id, &context);
    EXPECT_EQ(strlen(context.name), FEP_CONTEXT_MAX_NAME_LEN - 1);
}
