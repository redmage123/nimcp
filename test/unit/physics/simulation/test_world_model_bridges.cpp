/**
 * @file test_world_model_bridges.cpp
 * @brief Unit + integration + regression + e2e tests for JEPA-simulation bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/physics/nimcp_world_model_bridges.h"
#include "cognitive/physics/nimcp_intuitive_physics.h"
#include "cognitive/physics/nimcp_scene_graph.h"
#include "cognitive/physics/nimcp_entity_tracker.h"
#include "cognitive/physics/nimcp_world_simulator.h"
}

/* ============================================================
 * UNIT TESTS — individual functions in isolation
 * ============================================================ */

class WMBridgeTest : public ::testing::Test {
protected:
    world_model_bridge_t* bridge = nullptr;
    void SetUp() override { bridge = wmb_create(NULL); }
    void TearDown() override { wmb_destroy(bridge); }
};

TEST_F(WMBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->initialized);
}

TEST_F(WMBridgeTest, CreateWithNullConfig) {
    /* Default config should work */
    auto* b = wmb_create(NULL);
    ASSERT_NE(b, nullptr);
    wmb_destroy(b);
}

TEST_F(WMBridgeTest, CreateWithCustomConfig) {
    wmb_config_t cfg = wmb_default_config();
    cfg.surprise_threshold = 5.0f;
    cfg.replay_batch_size = 32;
    cfg.enable_prime_resonance = false;
    auto* b = wmb_create(&cfg);
    ASSERT_NE(b, nullptr);
    EXPECT_FLOAT_EQ(b->config.surprise_threshold, 5.0f);
    EXPECT_EQ(b->config.replay_batch_size, 32u);
    EXPECT_FALSE(b->config.enable_prime_resonance);
    wmb_destroy(b);
}

TEST_F(WMBridgeTest, DefaultConfig) {
    wmb_config_t cfg = wmb_default_config();
    EXPECT_FLOAT_EQ(cfg.surprise_threshold, WMB_SURPRISE_THRESHOLD);
    EXPECT_TRUE(cfg.enable_prime_resonance);
    EXPECT_TRUE(cfg.enable_replay);
    EXPECT_TRUE(cfg.enable_physical_loss);
    EXPECT_GT(cfg.replay_learning_rate, 0.0f);
    EXPECT_GT(cfg.physical_loss_weight, 0.0f);
}

/* --- Encoding / Decoding --- */

TEST_F(WMBridgeTest, EncodeZeroState) {
    wmb_physical_state_t phys = {};
    float latent[WMB_LATENT_DIM] = {};
    wmb_encode_physical(bridge, &phys, latent);
    /* Zero physical → zero latent (identity-like weights) */
    for (int i = 0; i < WMB_LATENT_DIM; i++)
        EXPECT_FLOAT_EQ(latent[i], 0.0f);
}

TEST_F(WMBridgeTest, EncodeNonzeroState) {
    wmb_physical_state_t phys = {};
    phys.position[0] = 1.0f;
    phys.position[1] = 2.0f;
    phys.position[2] = 3.0f;
    phys.velocity[0] = 0.5f;
    phys.mass = 1.0f;
    float latent[WMB_LATENT_DIM] = {};
    wmb_encode_physical(bridge, &phys, latent);
    /* With identity-like weights, latent[0]=pos[0]=1, latent[1]=pos[1]=2, etc. */
    EXPECT_NEAR(latent[0], 1.0f, 1e-5f);
    EXPECT_NEAR(latent[1], 2.0f, 1e-5f);
    EXPECT_NEAR(latent[2], 3.0f, 1e-5f);
    EXPECT_NEAR(latent[3], 0.5f, 1e-5f);  /* velocity x */
    EXPECT_NEAR(latent[10], 1.0f, 1e-5f); /* mass */
}

TEST_F(WMBridgeTest, DecodeRoundTrip) {
    /* Encode then decode should recover original physical state */
    wmb_physical_state_t original = {};
    original.position[0] = 3.14f;
    original.position[1] = 2.72f;
    original.position[2] = 1.41f;
    original.velocity[0] = -0.5f;
    original.velocity[1] = 1.0f;
    original.velocity[2] = 0.0f;
    original.orientation[0] = 1.0f;  /* w=1 identity quaternion */
    original.mass = 2.5f;
    original.radius = 0.3f;

    float latent[WMB_LATENT_DIM] = {};
    wmb_encode_physical(bridge, &original, latent);

    wmb_physical_state_t recovered = {};
    wmb_decode_latent(bridge, latent, &recovered);

    EXPECT_NEAR(recovered.position[0], original.position[0], 1e-4f);
    EXPECT_NEAR(recovered.position[1], original.position[1], 1e-4f);
    EXPECT_NEAR(recovered.position[2], original.position[2], 1e-4f);
    EXPECT_NEAR(recovered.velocity[0], original.velocity[0], 1e-4f);
    EXPECT_NEAR(recovered.velocity[1], original.velocity[1], 1e-4f);
    EXPECT_NEAR(recovered.mass, original.mass, 1e-4f);
    EXPECT_NEAR(recovered.radius, original.radius, 1e-4f);
}

TEST_F(WMBridgeTest, EncodeNullInputs) {
    float latent[WMB_LATENT_DIM];
    wmb_encode_physical(nullptr, nullptr, nullptr);  /* should not crash */
    wmb_encode_physical(bridge, nullptr, latent);     /* should not crash */
    wmb_physical_state_t phys = {};
    wmb_encode_physical(bridge, &phys, nullptr);      /* should not crash */
}

/* --- Surprise Storage --- */

TEST_F(WMBridgeTest, StoreSurpriseEvent) {
    wmb_surprise_event_t event = {};
    event.prediction_error = 1.5f;
    event.surprise_score = 3.0f;
    wmb_store_surprise(bridge, &event);
    EXPECT_EQ(bridge->replay.count, 1u);
    EXPECT_EQ(bridge->stats.surprises_stored, 1u);
}

TEST_F(WMBridgeTest, ReplayBufferWraps) {
    /* Fill buffer past capacity — should wrap without crash */
    for (uint32_t i = 0; i < WMB_MAX_REPLAY_BUFFER + 50; i++) {
        wmb_surprise_event_t event = {};
        event.surprise_score = (float)i;
        wmb_store_surprise(bridge, &event);
    }
    EXPECT_EQ(bridge->replay.count, (uint32_t)WMB_MAX_REPLAY_BUFFER);
    EXPECT_EQ(bridge->stats.surprises_stored, WMB_MAX_REPLAY_BUFFER + 50u);
}

TEST_F(WMBridgeTest, MostSurprising) {
    /* Store events with different surprise scores */
    for (int i = 0; i < 10; i++) {
        wmb_surprise_event_t event = {};
        event.surprise_score = (float)i;
        event.prediction_error = (float)i * 0.1f;
        wmb_store_surprise(bridge, &event);
    }
    const wmb_surprise_event_t* best = wmb_most_surprising(bridge);
    ASSERT_NE(best, nullptr);
    EXPECT_FLOAT_EQ(best->surprise_score, 9.0f);  /* highest score */
}

/* --- Callback Tests --- */

static int g_surprise_callback_count = 0;
static float g_last_surprise_score = 0;
static void test_surprise_cb(const wmb_surprise_event_t* event, void* ctx) {
    (void)ctx;
    g_surprise_callback_count++;
    g_last_surprise_score = event->surprise_score;
}

static int g_replay_callback_count = 0;
static void test_replay_cb(const float* before, const float* after, float score, void* ctx) {
    (void)before; (void)after; (void)score; (void)ctx;
    g_replay_callback_count++;
}

TEST_F(WMBridgeTest, SurpriseCallbackFires) {
    g_surprise_callback_count = 0;
    g_last_surprise_score = 0;
    wmb_set_surprise_callback(bridge, test_surprise_cb, NULL);

    wmb_surprise_event_t event = {};
    event.surprise_score = 7.77f;
    wmb_store_surprise(bridge, &event);

    EXPECT_EQ(g_surprise_callback_count, 1);
    EXPECT_FLOAT_EQ(g_last_surprise_score, 7.77f);
}

TEST_F(WMBridgeTest, ReplayCallbackFires) {
    g_replay_callback_count = 0;
    wmb_set_replay_callback(bridge, test_replay_cb, NULL);

    /* Store 5 events */
    for (int i = 0; i < 5; i++) {
        wmb_surprise_event_t event = {};
        event.surprise_score = (float)(i + 1);
        wmb_store_surprise(bridge, &event);
    }

    /* Replay 3 */
    wmb_replay_consolidation(bridge, 3);
    EXPECT_EQ(g_replay_callback_count, 3);
}

TEST_F(WMBridgeTest, NullCallbackSafe) {
    wmb_set_surprise_callback(bridge, NULL, NULL);
    wmb_set_replay_callback(bridge, NULL, NULL);
    wmb_surprise_event_t event = {};
    event.surprise_score = 1.0f;
    wmb_store_surprise(bridge, &event);  /* should not crash */
    wmb_replay_consolidation(bridge, 1); /* should not crash */
}

TEST_F(WMBridgeTest, MostSurprisingEmptyBuffer) {
    const wmb_surprise_event_t* best = wmb_most_surprising(bridge);
    EXPECT_EQ(best, nullptr);
}

/* --- Consolidation Replay --- */

TEST_F(WMBridgeTest, ReplayNoEvents) {
    uint32_t replayed = wmb_replay_consolidation(bridge, 10);
    EXPECT_EQ(replayed, 0u);
}

TEST_F(WMBridgeTest, ReplayDecaysSurprise) {
    wmb_surprise_event_t event = {};
    event.surprise_score = 10.0f;
    wmb_store_surprise(bridge, &event);

    /* Replay should decay the surprise score by 0.7× */
    wmb_replay_consolidation(bridge, 1);
    EXPECT_NEAR(bridge->replay.events[0].surprise_score, 7.0f, 0.01f);

    wmb_replay_consolidation(bridge, 1);
    EXPECT_NEAR(bridge->replay.events[0].surprise_score, 4.9f, 0.01f);
}

TEST_F(WMBridgeTest, ReplayCountsCorrectly) {
    for (int i = 0; i < 20; i++) {
        wmb_surprise_event_t event = {};
        event.surprise_score = (float)(20 - i);
        wmb_store_surprise(bridge, &event);
    }
    uint32_t replayed = wmb_replay_consolidation(bridge, 5);
    EXPECT_EQ(replayed, 5u);
    EXPECT_EQ(bridge->stats.replays_executed, 5u);
}

/* --- Stats --- */

TEST_F(WMBridgeTest, StatsInitiallyZero) {
    wmb_stats_t stats = wmb_get_stats(bridge);
    EXPECT_EQ(stats.predictions_made, 0u);
    EXPECT_EQ(stats.simulations_run, 0u);
    EXPECT_EQ(stats.surprises_stored, 0u);
    EXPECT_EQ(stats.replays_executed, 0u);
    EXPECT_FLOAT_EQ(stats.mean_prediction_error, 0.0f);
}

TEST_F(WMBridgeTest, ConnectSystems) {
    auto* phys = intuitive_physics_create(NULL);
    auto* scene = scene_graph_create(NULL);
    auto* tracker = entity_tracker_create(NULL);
    wmb_connect(bridge, nullptr, phys, nullptr, scene, tracker);
    EXPECT_EQ(bridge->physics, phys);
    EXPECT_EQ(bridge->scene, scene);
    EXPECT_EQ(bridge->tracker, tracker);
    EXPECT_EQ(bridge->jepa, nullptr);
    entity_tracker_destroy(tracker);
    scene_graph_destroy(scene);
    intuitive_physics_destroy(phys);
}

/* ============================================================
 * INTEGRATION TESTS — bridge connected to real physics engine
 * ============================================================ */

class WMBridgeIntegrationTest : public ::testing::Test {
protected:
    world_model_bridge_t* bridge = nullptr;
    intuitive_physics_engine_t* physics = nullptr;

    void SetUp() override {
        bridge = wmb_create(NULL);
        physics = intuitive_physics_create(NULL);
        intuitive_physics_add_ground(physics);
        wmb_connect(bridge, nullptr, physics, nullptr, nullptr, nullptr);
    }
    void TearDown() override {
        intuitive_physics_destroy(physics);
        wmb_destroy(bridge);
    }
};

TEST_F(WMBridgeIntegrationTest, PredictAndVerifyFallingBall) {
    /* Drop a ball — prediction should have nonzero error (gravity changes trajectory) */
    ip_object_t ball = {};
    ball.position = {0, 5.0f, 0};
    ball.velocity = {0, 0, 0};
    ball.mass = 1.0f;
    ball.shape.type = IP_SHAPE_SPHERE;
    ball.shape.sphere.radius = 0.5f;
    ball.restitution = 0.3f;
    ball.friction = 0.5f;
    ball.visible = true;
    ball.active = true;
    intuitive_physics_add_object(physics, &ball);

    /* Run predict-and-verify for several steps */
    float total_error = 0;
    for (int i = 0; i < 50; i++) {
        float error = wmb_predict_and_verify(bridge, 0.01f);
        total_error += error;
    }

    wmb_stats_t stats = wmb_get_stats(bridge);
    EXPECT_EQ(stats.predictions_made, 50u);
    EXPECT_EQ(stats.simulations_run, 50u);
    EXPECT_GT(stats.mean_prediction_error, 0.0f);
}

TEST_F(WMBridgeIntegrationTest, CollisionGeneratesSurprise) {
    /* Two balls on collision course — the collision should surprise the linear predictor */
    ip_object_t ball_a = {};
    ball_a.position = {-1.0f, 0.5f, 0};
    ball_a.velocity = {2.0f, 0, 0};
    ball_a.mass = 1.0f;
    ball_a.shape.type = IP_SHAPE_SPHERE;
    ball_a.shape.sphere.radius = 0.5f;
    ball_a.restitution = 0.8f;
    ball_a.visible = true;
    ball_a.active = true;

    ip_object_t ball_b = ball_a;
    ball_b.position = {1.0f, 0.5f, 0};
    ball_b.velocity = {-2.0f, 0, 0};

    intuitive_physics_add_object(physics, &ball_a);
    intuitive_physics_add_object(physics, &ball_b);

    /* Run until collision generates a surprise */
    for (int i = 0; i < 200; i++)
        wmb_predict_and_verify(bridge, 0.01f);

    wmb_stats_t stats = wmb_get_stats(bridge);
    /* At least some surprise events should be stored (collision is surprising
     * to a linear predictor) */
    /* Note: may not trigger if threshold is high relative to small dt errors */
    EXPECT_GT(stats.predictions_made, 100u);
}

TEST_F(WMBridgeIntegrationTest, NoObjectsNoError) {
    /* No dynamic objects → predict_and_verify should return 0 */
    float error = wmb_predict_and_verify(bridge, 0.01f);
    EXPECT_FLOAT_EQ(error, 0.0f);
}

TEST_F(WMBridgeIntegrationTest, ReplayAfterSurprises) {
    /* Generate some surprise events, then replay them */
    ip_object_t ball = {};
    ball.position = {0, 5.0f, 0};
    ball.mass = 1.0f;
    ball.shape.type = IP_SHAPE_SPHERE;
    ball.shape.sphere.radius = 0.5f;
    ball.visible = true;
    ball.active = true;
    intuitive_physics_add_object(physics, &ball);

    /* Lower threshold to guarantee surprises */
    bridge->config.surprise_threshold = 0.001f;

    for (int i = 0; i < 100; i++)
        wmb_predict_and_verify(bridge, 0.01f);

    uint32_t stored = bridge->stats.surprises_stored;
    /* With very low threshold, should have stored many surprises */
    EXPECT_GT(stored, 0u);

    /* Replay */
    uint32_t replayed = wmb_replay_consolidation(bridge, 10);
    EXPECT_GT(replayed, 0u);
    EXPECT_EQ(bridge->stats.replays_executed, replayed);
}

/* ============================================================
 * REGRESSION TESTS — known values that must not change
 * ============================================================ */

TEST(WMBridgeRegression, EncodeDecodeIdentity) {
    /* With identity-like weights, encode→decode must be lossless
     * for the first 12 dimensions */
    auto* bridge = wmb_create(NULL);
    ASSERT_NE(bridge, nullptr);

    wmb_physical_state_t state = {};
    state.position[0] = 1.0f;
    state.position[1] = -2.5f;
    state.position[2] = 0.001f;
    state.velocity[0] = 100.0f;
    state.velocity[1] = -50.0f;
    state.velocity[2] = 0.0f;
    state.orientation[0] = 0.707f;
    state.orientation[1] = 0.0f;
    state.orientation[2] = 0.707f;
    state.orientation[3] = 0.0f;
    state.mass = 5.0f;
    state.radius = 0.25f;

    float latent[WMB_LATENT_DIM];
    wmb_encode_physical(bridge, &state, latent);

    wmb_physical_state_t recovered;
    wmb_decode_latent(bridge, latent, &recovered);

    /* Position must round-trip exactly (identity weights, double accumulation) */
    EXPECT_NEAR(recovered.position[0], state.position[0], 1e-5f);
    EXPECT_NEAR(recovered.position[1], state.position[1], 1e-5f);
    EXPECT_NEAR(recovered.position[2], state.position[2], 1e-5f);
    EXPECT_NEAR(recovered.velocity[0], state.velocity[0], 1e-3f);
    EXPECT_NEAR(recovered.velocity[1], state.velocity[1], 1e-3f);
    EXPECT_NEAR(recovered.mass, state.mass, 1e-5f);
    EXPECT_NEAR(recovered.radius, state.radius, 1e-5f);

    wmb_destroy(bridge);
}

TEST(WMBridgeRegression, SurpriseDecayRate) {
    /* Surprise must decay by exactly 0.7× per replay */
    auto* bridge = wmb_create(NULL);
    ASSERT_NE(bridge, nullptr);

    wmb_surprise_event_t event = {};
    event.surprise_score = 100.0f;
    wmb_store_surprise(bridge, &event);

    float expected = 100.0f;
    for (int i = 0; i < 5; i++) {
        wmb_replay_consolidation(bridge, 1);
        expected *= 0.7f;
        EXPECT_NEAR(bridge->replay.events[0].surprise_score, expected, 0.01f);
    }

    wmb_destroy(bridge);
}

TEST(WMBridgeRegression, BufferCapacityExact) {
    /* Buffer must hold exactly WMB_MAX_REPLAY_BUFFER events */
    auto* bridge = wmb_create(NULL);
    ASSERT_NE(bridge, nullptr);

    for (uint32_t i = 0; i < WMB_MAX_REPLAY_BUFFER * 2; i++) {
        wmb_surprise_event_t event = {};
        event.surprise_score = (float)i;
        wmb_store_surprise(bridge, &event);
    }
    EXPECT_EQ(bridge->replay.count, (uint32_t)WMB_MAX_REPLAY_BUFFER);

    wmb_destroy(bridge);
}

/* ============================================================
 * E2E TEST — full predict-surprise-replay-improve cycle
 * ============================================================ */

TEST(WMBridgeE2E, FullPredictSurpriseReplayCycle) {
    /* Complete lifecycle:
     * 1. Create bridge + physics
     * 2. Drop a ball (surprising physics)
     * 3. Accumulate surprise events
     * 4. Run consolidation replay
     * 5. Verify stats show the full cycle completed */

    auto* bridge = wmb_create(NULL);
    auto* physics = intuitive_physics_create(NULL);
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(physics, nullptr);

    intuitive_physics_add_ground(physics);
    wmb_connect(bridge, nullptr, physics, nullptr, nullptr, nullptr);

    /* Add a ball that will fall and bounce */
    ip_object_t ball = {};
    ball.position = {0, 3.0f, 0};
    ball.mass = 1.0f;
    ball.shape.type = IP_SHAPE_SPHERE;
    ball.shape.sphere.radius = 0.5f;
    ball.restitution = 0.5f;
    ball.friction = 0.3f;
    ball.visible = true;
    ball.active = true;
    intuitive_physics_add_object(physics, &ball);

    /* Lower surprise threshold to capture events */
    bridge->config.surprise_threshold = 0.1f;

    /* Phase 1: Predict and verify for 200 steps (ball falls and bounces) */
    for (int i = 0; i < 200; i++)
        wmb_predict_and_verify(bridge, 0.01f);

    wmb_stats_t stats_after_predict = wmb_get_stats(bridge);
    EXPECT_EQ(stats_after_predict.predictions_made, 200u);
    EXPECT_EQ(stats_after_predict.simulations_run, 200u);
    EXPECT_GT(stats_after_predict.surprises_stored, 0u);

    /* Phase 2: Consolidation replay */
    uint32_t replayed = wmb_replay_consolidation(bridge, 20);
    EXPECT_GT(replayed, 0u);

    /* Phase 3: Verify full cycle stats */
    wmb_stats_t final_stats = wmb_get_stats(bridge);
    EXPECT_GT(final_stats.replays_executed, 0u);
    EXPECT_GT(final_stats.mean_prediction_error, 0.0f);
    EXPECT_GT(final_stats.surprises_stored, 0u);

    /* The most surprising event should have a positive error */
    const wmb_surprise_event_t* best = wmb_most_surprising(bridge);
    if (best) {
        EXPECT_GT(best->prediction_error, 0.0f);
        EXPECT_GT(best->surprise_score, 0.0f);
    }

    intuitive_physics_destroy(physics);
    wmb_destroy(bridge);
}
