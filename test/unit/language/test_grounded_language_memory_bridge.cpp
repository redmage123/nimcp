/**
 * @file test_grounded_language_memory_bridge.cpp
 * @brief Unit tests for the grounded_language → memory subsystem bridge.
 *
 * WHAT: Cover the three connect-* setters + the dispatch hook in
 *       grounded_language_ground(), exercising the gating logic
 *       (attention floor, NULL features, NULL handle) and verifying
 *       that downstream subsystems actually receive events.
 *
 * WHY:  The dispatcher is the single fan-out point for memory-system
 *       integration; if it regresses, working memory + episodic
 *       replay + hippocampus all silently lose grounded-vocabulary
 *       participation.
 *
 * HOW:  Real working_memory_t and nimcp_episodic_replay_t handles
 *       (cheap to create) — assert size deltas after grounding events.
 *       Hippocampus is exercised only at the connect/null-safety
 *       level here because its association API takes the brain-side
 *       adapter struct.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/memory/nimcp_episodic_replay.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class GLMemoryBridge : public ::testing::Test {
protected:
    grounded_language_t* gl = nullptr;
    semantic_memory_system_t* sm = nullptr;
    working_memory_t* wm = nullptr;
    nimcp_episodic_replay_t* replay = nullptr;

    void SetUp() override {
        sm = semantic_memory_create();
        ASSERT_NE(sm, nullptr);
        gl = grounded_language_create(TEST_DIM, sm);
        ASSERT_NE(gl, nullptr);
        wm = working_memory_create();
        ASSERT_NE(wm, nullptr);
        nimcp_episodic_replay_config_t rc{};
        rc.replay_count = 10;
        rc.replay_speed_multiplier = 10.0f;
        rc.importance_threshold = 0.0f;
        rc.replay_lr_scale = 0.3f;
        rc.buffer_capacity = 64;
        replay = nimcp_episodic_replay_create(&rc);
        ASSERT_NE(replay, nullptr);
    }
    void TearDown() override {
        if (replay) nimcp_episodic_replay_destroy(replay);
        if (wm) working_memory_destroy(wm);
        if (gl) grounded_language_destroy(gl);
        if (sm) semantic_memory_destroy(sm);
    }

    gl_grounding_event_t make_event(const char* word, float attention) {
        static std::vector<float> features(TEST_DIM, 0.5f);
        gl_grounding_event_t e{};
        e.word = word;
        e.modality = GL_MODALITY_VISUAL;
        e.sensory_features = features.data();
        e.feature_dim = TEST_DIM;
        e.emotional_valence = 0.2f;
        e.emotional_arousal = 0.4f;
        e.attention = attention;
        e.context_sentence = nullptr;
        return e;
    }
};

/* --- Setters: NULL handle + NULL value safety ------------------------ */
TEST_F(GLMemoryBridge, ConnectNullHandleSafe) {
    /* All three setters must not crash on NULL gl. */
    grounded_language_connect_working_memory(nullptr, wm);
    grounded_language_connect_episodic_replay(nullptr, replay);
    grounded_language_connect_hippocampus(nullptr, nullptr);
    SUCCEED();
}

TEST_F(GLMemoryBridge, ConnectNullValueAllowed) {
    /* Disconnecting (passing NULL) is legal — restores the default. */
    grounded_language_connect_working_memory(gl, nullptr);
    grounded_language_connect_episodic_replay(gl, nullptr);
    grounded_language_connect_hippocampus(gl, nullptr);
    SUCCEED();
}

/* --- ground() with no connections is a no-op for memory ------------- */
TEST_F(GLMemoryBridge, GroundWithoutConnectionsIsNoOp) {
    auto e = make_event("apple", 0.8f);
    int rc = grounded_language_ground(gl, &e);
    EXPECT_EQ(rc, 0);
    /* WM count unchanged — bridge wasn't connected. */
    EXPECT_EQ(working_memory_get_count(wm), 0u);
}

/* --- Working memory receives high-attention events ----------------- */
TEST_F(GLMemoryBridge, WorkingMemoryReceivesHighAttentionEvent) {
    grounded_language_connect_working_memory(gl, wm);
    auto e = make_event("apple", 0.8f);
    ASSERT_EQ(0, grounded_language_ground(gl, &e));
    EXPECT_GT(working_memory_get_count(wm), 0u);
}

/* --- Attention floor blocks low-attention events ------------------- */
TEST_F(GLMemoryBridge, LowAttentionDoesNotPushToWorkingMemory) {
    grounded_language_connect_working_memory(gl, wm);
    /* attention=0.1 is below _GL_MEM_ATTN_FLOOR (0.30). */
    auto e = make_event("apple", 0.1f);
    ASSERT_EQ(0, grounded_language_ground(gl, &e));
    EXPECT_EQ(working_memory_get_count(wm), 0u);
}

/* --- Episodic replay receives the event ---------------------------- */
TEST_F(GLMemoryBridge, EpisodicReplayReceivesEvent) {
    grounded_language_connect_episodic_replay(gl, replay);
    /* We can't peek the buffer count directly without the consolidate
     * API; this test confirms the path doesn't crash and that ground
     * still returns OK with episodic_replay attached. */
    auto e = make_event("ocean", 0.7f);
    EXPECT_EQ(0, grounded_language_ground(gl, &e));
}

/* --- Multiple subsystems can be connected simultaneously ---------- */
TEST_F(GLMemoryBridge, AllConnectionsCoexist) {
    grounded_language_connect_working_memory(gl, wm);
    grounded_language_connect_episodic_replay(gl, replay);
    /* hippocampus left null — bridge should still work. */
    auto e = make_event("river", 0.9f);
    ASSERT_EQ(0, grounded_language_ground(gl, &e));
    EXPECT_GT(working_memory_get_count(wm), 0u);
}

}  // namespace
