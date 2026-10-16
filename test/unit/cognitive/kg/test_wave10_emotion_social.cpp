/**
 * @file test_wave10_emotion_social.cpp
 * @brief Unit test for KG-integration Wave W10.
 *
 * Wave W10 wires 10 affective / social cognitive modules into
 * `brain->internal_kg`:
 *   1. emotions                (cog_emotions root + state-change event)
 *   2. theory_of_mind          (cog_theory_of_mind root + belief event)
 *   3. mirror_neurons          (cog_mirror_neurons root + activation event)
 *   4. social_interaction      (cog_social_interaction root + episode event)
 *   5. collective_cognition    (cog_collective_cognition root + level event)
 *   6. personality             (cog_personality root + trait event)
 *   7. empathetic_response     (cog_empathetic_response root + response event)
 *   8. emotion_recognition     (cog_emotion_recognition root + detect event)
 *   9. grief                   (cog_grief root + stage transition event)
 *  10. shadow_emotions         (cog_shadow_emotions root + activation event)
 *
 * Each test: (a) verifies the structural root exists after brain init,
 *            (b) calls the emit helper, (c) asserts an event node prefix
 *                appears, (d) calls the query helper and confirms a
 *                plausible bias float is returned.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"

//-----------------------------------------------------------------------------
// Fixture
//-----------------------------------------------------------------------------

class Wave10AffectiveSocialTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave10_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled)
            << "internal_kg_enabled must be true post-creation";
        ASSERT_NE(brain->internal_kg, nullptr)
            << "brain->internal_kg must be allocated";

        /* Structural init is safe to re-invoke (idempotent). */
        EXPECT_EQ(nimcp_wave10_affective_kg_init(brain), 0);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    void expect_node(const char* name) {
        brain_kg_node_id_t id = brain_kg_find_node(brain->internal_kg, name);
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "expected KG node '" << name << "' to be present";
    }

    bool any_node_with_prefix(const char* prefix) {
        const size_t plen = strlen(prefix);
        for (uint32_t t = 0; t < BRAIN_KG_NODE_TYPE_COUNT; ++t) {
            brain_kg_node_list_t* list =
                brain_kg_get_nodes_by_type(brain->internal_kg,
                                           (brain_kg_node_type_t)t);
            if (!list) continue;
            bool found = false;
            for (uint32_t i = 0; i < list->count && !found; ++i) {
                const brain_kg_node_t* n = list->nodes[i];
                if (n && strncmp(n->name, prefix, plen) == 0) {
                    found = true;
                }
            }
            brain_kg_node_list_destroy(list);
            if (found) return true;
        }
        return false;
    }
};

//-----------------------------------------------------------------------------
// Structural roots: all 10 module nodes must exist after init.
//-----------------------------------------------------------------------------

TEST_F(Wave10AffectiveSocialTest, AllStructuralRootsPresent) {
    expect_node("cog_emotions");
    expect_node("cog_theory_of_mind");
    expect_node("cog_mirror_neurons");
    expect_node("cog_social_interaction");
    expect_node("cog_collective_cognition");
    expect_node("cog_personality");
    expect_node("cog_empathetic_response");
    expect_node("cog_emotion_recognition");
    expect_node("cog_grief");
    expect_node("cog_shadow_emotions");
}

TEST_F(Wave10AffectiveSocialTest, Idempotent) {
    /* Re-init must not create duplicate nodes or crash. */
    EXPECT_EQ(nimcp_wave10_affective_kg_init(brain), 0);
    EXPECT_EQ(nimcp_wave10_affective_kg_init(brain), 0);
    expect_node("cog_emotions");
    expect_node("cog_shadow_emotions");
}

//-----------------------------------------------------------------------------
// Per-module emit + query checks.
//-----------------------------------------------------------------------------

TEST_F(Wave10AffectiveSocialTest, EmotionsEmitAndQuery) {
    wave10_emotions_emit_state_change(brain, -0.3f, 0.7f, 0.8f);
    EXPECT_TRUE(any_node_with_prefix("cog_emotions_event_state_"));
    float bias = wave10_emotions_query_arousal_bias(brain);
    EXPECT_GT(bias, 0.5f) << "arousal=0.7 should raise bias above neutral";
}

TEST_F(Wave10AffectiveSocialTest, ToMEmitAndQuery) {
    wave10_tom_emit_belief_event(brain, 17u, "false_belief", 0.82f);
    EXPECT_TRUE(any_node_with_prefix("cog_tom_event_false_belief_17_"));
    float bias = wave10_tom_query_model_count_bias(brain);
    EXPECT_EQ(bias, 1.0f) << "at least one ToM model recorded";
}

TEST_F(Wave10AffectiveSocialTest, MirrorEmitAndQuery) {
    wave10_mirror_emit_activation(brain, 42u, 0.65f);
    EXPECT_TRUE(any_node_with_prefix("cog_mirror_event_activate_42_"));
    float bias = wave10_mirror_query_activity_bias(brain);
    EXPECT_NEAR(bias, 0.65f, 0.01f);
}

TEST_F(Wave10AffectiveSocialTest, SocialEmitAndQuery) {
    wave10_social_emit_episode_outcome(brain, 4u, 0.72f, 0.55f);
    EXPECT_TRUE(any_node_with_prefix("cog_social_event_episode_4_"));
    float bias = wave10_social_query_convergence_bias(brain);
    EXPECT_NEAR(bias, 0.72f, 0.01f);
}

TEST_F(Wave10AffectiveSocialTest, CollectiveEmitAndQuery) {
    wave10_collective_emit_level_change(brain, 2, 0.42f);
    EXPECT_TRUE(any_node_with_prefix("cog_collective_event_level_2_"));
    float bias = wave10_collective_query_phi_bias(brain);
    EXPECT_NEAR(bias, 0.42f, 0.01f);
}

TEST_F(Wave10AffectiveSocialTest, PersonalityEmitAndQuery) {
    wave10_personality_emit_trait_expression(brain,
        /*O*/0.6f, /*C*/0.55f, /*E*/0.78f, /*A*/0.4f, /*N*/0.3f);
    EXPECT_TRUE(any_node_with_prefix("cog_personality_event_traits_"));
    float bias = wave10_personality_query_extraversion_bias(brain);
    EXPECT_NEAR(bias, 0.78f, 0.01f);
}

TEST_F(Wave10AffectiveSocialTest, EmpathyEmitAndQuery) {
    wave10_empathy_emit_response(brain, /*strategy*/3, /*crisis*/false, 0.9f);
    EXPECT_TRUE(any_node_with_prefix("cog_empathy_event_response_3_"));
    wave10_empathy_emit_effectiveness(brain, 0.66f);
    EXPECT_TRUE(any_node_with_prefix("cog_empathy_event_effectiveness_"));
    float bias = wave10_empathy_query_safety_bias(brain);
    EXPECT_NEAR(bias, 0.9f, 0.01f);
}

TEST_F(Wave10AffectiveSocialTest, EmoRecEmitAndQuery) {
    wave10_emorec_emit_detection(brain, "sadness", 0.83f);
    EXPECT_TRUE(any_node_with_prefix("cog_emorec_event_detect_sadness_"));
    float bias = wave10_emorec_query_detection_bias(brain);
    EXPECT_NEAR(bias, 0.83f, 0.01f);
}

TEST_F(Wave10AffectiveSocialTest, GriefEmitAndQuery) {
    wave10_grief_emit_stage_transition(brain, /*from*/1, /*to*/3, 0.47f);
    EXPECT_TRUE(any_node_with_prefix("cog_grief_event_stage_1_3_"));
    float bias = wave10_grief_query_pain_bias(brain);
    EXPECT_NEAR(bias, 0.47f, 0.01f);
}

TEST_F(Wave10AffectiveSocialTest, ShadowEmitAndQuery) {
    wave10_shadow_emit_activation(brain, "envy", 0.62f);
    EXPECT_TRUE(any_node_with_prefix("cog_shadow_event_activate_envy_"));
    float bias = wave10_shadow_query_intensity_bias(brain);
    EXPECT_NEAR(bias, 0.62f, 0.01f);
}

//-----------------------------------------------------------------------------
// Null-safety
//-----------------------------------------------------------------------------

TEST(Wave10NullSafety, AllEmitHelpersAreNullSafe) {
    /* None must crash / throw on NULL brain. */
    EXPECT_EQ(nimcp_wave10_affective_kg_init(nullptr), 0);

    wave10_emotions_emit_state_change(nullptr, 0.0f, 0.0f, 0.0f);
    wave10_tom_emit_belief_event(nullptr, 0u, nullptr, 0.0f);
    wave10_mirror_emit_activation(nullptr, 0u, 0.0f);
    wave10_social_emit_episode_outcome(nullptr, 0u, 0.0f, 0.0f);
    wave10_collective_emit_level_change(nullptr, 0, 0.0f);
    wave10_personality_emit_trait_expression(nullptr, 0, 0, 0, 0, 0);
    wave10_empathy_emit_response(nullptr, 0, false, 0.0f);
    wave10_empathy_emit_effectiveness(nullptr, 0.0f);
    wave10_emorec_emit_detection(nullptr, nullptr, 0.0f);
    wave10_grief_emit_stage_transition(nullptr, 0, 0, 0.0f);
    wave10_shadow_emit_activation(nullptr, nullptr, 0.0f);

    /* Query helpers return 0.5f neutral on NULL brain. */
    EXPECT_EQ(wave10_emotions_query_arousal_bias(nullptr), 0.5f);
    EXPECT_EQ(wave10_tom_query_model_count_bias(nullptr), 0.5f);
    EXPECT_EQ(wave10_mirror_query_activity_bias(nullptr), 0.5f);
    EXPECT_EQ(wave10_social_query_convergence_bias(nullptr), 0.5f);
    EXPECT_EQ(wave10_collective_query_phi_bias(nullptr), 0.5f);
    EXPECT_EQ(wave10_personality_query_extraversion_bias(nullptr), 0.5f);
    EXPECT_EQ(wave10_empathy_query_safety_bias(nullptr), 0.5f);
    EXPECT_EQ(wave10_emorec_query_detection_bias(nullptr), 0.5f);
    EXPECT_EQ(wave10_grief_query_pain_bias(nullptr), 0.5f);
    EXPECT_EQ(wave10_shadow_query_intensity_bias(nullptr), 0.5f);
}
