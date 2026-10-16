/**
 * @file test_wave9kg_attention_wm.cpp
 * @brief Unit test for KG-integration Wave W9-kg (cognitive-control family).
 *
 * Wave W9-kg wires read-paths and/or full bidirectional KG integration into
 * 8 cognitive-control modules:
 *   1. plasticity/attention/nimcp_attention.c           (PARTIAL -> WIRED)
 *   2. cognitive/working_memory/.._part_core.c          (PARTIAL -> WIRED)
 *   3. cognitive/executive/nimcp_executive.c             (PARTIAL -> WIRED)
 *   4. cognitive/introspection/.._part_core.c            (PARTIAL -> WIRED)
 *   5. cognitive/self_model/nimcp_self_model.c           (PARTIAL -> WIRED)
 *   6. cognitive/global_workspace/.._part_core.c         (UNWIRED -> WIRED)
 *   7. cognitive/self_awareness/nimcp_self_awareness_extended.c (UNWIRED -> WIRED)
 *   8. cognitive/autobiographical_memory/nimcp_autobiographical_memory.c (UNWIRED -> WIRED)
 *
 * Strategy: create a minimal brain with internal_kg always-on, invoke
 * `w9kg_init_roots` to register the 8 module root nodes + cognitive
 * umbrella, then exercise the shared emit/query helpers and assert that
 * expected KG event nodes and edges appear.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "cognitive/executive/nimcp_w9kg_events.h"

//-----------------------------------------------------------------------------
// Fixture
//-----------------------------------------------------------------------------

class Wave9KgAttentionWmTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave9kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled)
            << "internal_kg_enabled must be true post-creation";
        ASSERT_NE(brain->internal_kg, nullptr)
            << "brain->internal_kg must be allocated";
        /* Register the brain for the shared-helper emits. Idempotent
         * with w9kg_init_roots which also registers. */
        w9kg_set_registered_brain(brain);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        w9kg_set_registered_brain(nullptr);
    }

    void expect_node(const char* name) {
        brain_kg_node_id_t id =
            brain_kg_find_node(brain->internal_kg, name);
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "expected KG node '" << name << "' to be present";
    }

    bool any_node_with_prefix(const char* prefix) {
        brain_kg_node_list_t* list =
            brain_kg_search_nodes(brain->internal_kg, prefix);
        bool found = (list && list->count > 0);
        if (list) brain_kg_node_list_destroy(list);
        return found;
    }

    uint32_t count_nodes_with_prefix(const char* prefix) {
        brain_kg_node_list_t* list =
            brain_kg_search_nodes(brain->internal_kg, prefix);
        uint32_t n = list ? list->count : 0;
        if (list) brain_kg_node_list_destroy(list);
        return n;
    }
};

//-----------------------------------------------------------------------------
// 1. Structural init — 8 root nodes + cognitive umbrella
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, StructuralRootsRegistered) {
    /* w9kg_init_roots is called during brain init; invoke again to
     * confirm idempotence. */
    EXPECT_EQ(w9kg_init_roots(brain), 0);

    expect_node("cog_attention");
    expect_node("cog_working_memory");
    expect_node("cog_executive");
    expect_node("cog_introspection");
    expect_node("cog_self_model");
    expect_node("cog_global_workspace");
    expect_node("cog_self_awareness_extended");
    expect_node("cog_autobiographical_memory");
    expect_node("cognitive");
}

TEST_F(Wave9KgAttentionWmTest, InitIsIdempotent) {
    ASSERT_EQ(w9kg_init_roots(brain), 0);
    uint32_t count_before =
        count_nodes_with_prefix("cog_");
    ASSERT_EQ(w9kg_init_roots(brain), 0);
    uint32_t count_after =
        count_nodes_with_prefix("cog_");
    EXPECT_EQ(count_before, count_after)
        << "re-running w9kg_init_roots should not add duplicate roots";
}

//-----------------------------------------------------------------------------
// 2. Attention — emit + query salience events
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, AttentionSalienceEmitAndQuery) {
    uint32_t before =
        w9kg_query_attention_salience_count(brain);
    w9kg_emit_attention_salience(brain, /*head_idx=*/3, /*salience=*/0.7f);
    w9kg_emit_attention_salience(brain, /*head_idx=*/1, /*salience=*/0.4f);
    uint32_t after =
        w9kg_query_attention_salience_count(brain);
    EXPECT_GE(after, before + 2)
        << "two attention salience emits should be visible via KG query";
}

TEST_F(Wave9KgAttentionWmTest, AttentionFocusShiftEmit) {
    w9kg_emit_attention_focus_shift(brain, 1, 5, 0.9f);
    EXPECT_TRUE(any_node_with_prefix("cog_attention_event_shift_1_5_"))
        << "focus shift event must be present after emit";
}

//-----------------------------------------------------------------------------
// 3. Working memory — emit + read-path
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, WorkingMemoryEmitsAndQueries) {
    w9kg_emit_wm_item_stored(brain, /*item_id=*/42, /*priority=*/0.8f);
    w9kg_emit_wm_item_evicted(brain, /*item_id=*/42);
    EXPECT_TRUE(any_node_with_prefix("cog_working_memory_event_store_42_"));
    EXPECT_TRUE(any_node_with_prefix("cog_working_memory_event_evict_42_"));

    /* Semantic lookup: the "cognitive" umbrella should be findable. */
    EXPECT_TRUE(w9kg_query_wm_has_semantic(brain, "cognitive"))
        << "WM semantic query should find the cognitive umbrella node";
    EXPECT_FALSE(w9kg_query_wm_has_semantic(brain, "this_name_should_not_exist_xyz"))
        << "WM semantic query for unknown name must be false";
}

//-----------------------------------------------------------------------------
// 4. Executive — emit + ethics-context read
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, ExecutiveDecisionEmitAndEthicsQuery) {
    w9kg_emit_executive_decision(brain, /*decision_id=*/7, /*confidence=*/0.75f);
    w9kg_emit_executive_task_switch(brain, 2, 3);
    EXPECT_TRUE(any_node_with_prefix("cog_executive_event_decision_7_"));
    EXPECT_TRUE(any_node_with_prefix("cog_executive_event_task_switch_2_3_"));
    /* Ethics query: no ethics events in a fresh brain but call must not
     * crash and must return a number (likely 0 in this minimal brain). */
    uint32_t ethics =
        w9kg_query_executive_ethics_events(brain);
    (void)ethics;
    SUCCEED();
}

//-----------------------------------------------------------------------------
// 5. Introspection — wellbeing read-path + report emit
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, IntrospectionReportEmitAndWellbeingQuery) {
    w9kg_emit_introspection_report(brain, /*report_id=*/1, /*health=*/0.85f);
    EXPECT_TRUE(any_node_with_prefix("cog_introspection_event_report_1_"));
    uint32_t wellbeing =
        w9kg_query_introspection_wellbeing(brain);
    (void)wellbeing;
    SUCCEED();
}

//-----------------------------------------------------------------------------
// 6. Self-model — update emit + autobio context read
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, SelfModelUpdateEmitAndAutobioQuery) {
    w9kg_emit_self_model_update(brain, /*update_id=*/99, /*drift=*/0.2f);
    EXPECT_TRUE(any_node_with_prefix("cog_self_model_event_update_99_"));
    /* No autobio events yet in this minimal brain. */
    EXPECT_EQ(w9kg_query_self_model_autobio(brain), 0u);
    /* Add one autobio event, then query should return >= 1. */
    w9kg_emit_autobio_stored(brain, 10, 0.5f);
    EXPECT_GE(w9kg_query_self_model_autobio(brain), 1u);
}

//-----------------------------------------------------------------------------
// 7. Global workspace — broadcast emit + winner query
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, GlobalWorkspaceBroadcastAndCompetition) {
    w9kg_emit_gws_broadcast(brain, /*winner=*/2, /*strength=*/0.7f,
                            /*broadcast_id=*/11);
    w9kg_emit_gws_competition(brain, /*num_competitors=*/3);
    EXPECT_TRUE(any_node_with_prefix("cog_global_workspace_event_broadcast_11_"));
    EXPECT_TRUE(any_node_with_prefix("cog_global_workspace_event_competition_3_"));
    /* Winners by salience: each broadcast creates an outgoing edge from
     * the GWS root via produced_by, so the count should be >= 1 after
     * the broadcast emit. */
    EXPECT_GE(w9kg_query_gws_winners_by_salience(brain), 1u);
}

//-----------------------------------------------------------------------------
// 8. Self-awareness extended — reflection emit + events read
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, SelfAwarenessReflectionEmitAndEventsQuery) {
    uint32_t before = w9kg_query_self_awareness_events(brain);
    /* Self-awareness read-path pulls introspection_event_* nodes — so
     * write an introspection report and re-count. */
    w9kg_emit_introspection_report(brain, /*report_id=*/2, /*health=*/0.5f);
    w9kg_emit_self_awareness_reflection(brain, /*reflection_id=*/5,
                                        /*confidence=*/0.6f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_self_awareness_extended_event_reflection_5_"));
    EXPECT_GE(w9kg_query_self_awareness_events(brain), before + 1u);
}

//-----------------------------------------------------------------------------
// 9. Autobiographical memory — store/retrieve emit + event-count read
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, AutobiographicalStoreRetrieveEmits) {
    uint32_t before = w9kg_query_autobio_event_count(brain);
    w9kg_emit_autobio_stored(brain, /*memory_id=*/1001, /*importance=*/0.6f);
    w9kg_emit_autobio_retrieved(brain, /*memory_id=*/1001);
    EXPECT_TRUE(any_node_with_prefix("cog_autobiographical_memory_event_store_1001_"));
    EXPECT_TRUE(any_node_with_prefix("cog_autobiographical_memory_event_retrieve_1001_"));
    uint32_t after = w9kg_query_autobio_event_count(brain);
    EXPECT_GE(after, before + 2u);
}

//-----------------------------------------------------------------------------
// 10. Null-safety — helpers must no-op cleanly on NULL brain
//-----------------------------------------------------------------------------

TEST_F(Wave9KgAttentionWmTest, HelpersAreNullSafe) {
    /* All emits must no-op gracefully on NULL brain. */
    w9kg_emit_attention_salience(nullptr, 0, 1.0f);
    w9kg_emit_wm_item_stored(nullptr, 0, 1.0f);
    w9kg_emit_executive_decision(nullptr, 0, 1.0f);
    w9kg_emit_introspection_report(nullptr, 0, 1.0f);
    w9kg_emit_self_model_update(nullptr, 0, 1.0f);
    w9kg_emit_gws_broadcast(nullptr, 0, 1.0f, 0);
    w9kg_emit_self_awareness_reflection(nullptr, 0, 1.0f);
    w9kg_emit_autobio_stored(nullptr, 0, 1.0f);

    /* Queries on NULL brain return 0 / false. */
    EXPECT_EQ(w9kg_query_attention_salience_count(nullptr), 0u);
    EXPECT_FALSE(w9kg_query_wm_has_semantic(nullptr, "x"));
    EXPECT_EQ(w9kg_query_executive_ethics_events(nullptr), 0u);
    EXPECT_EQ(w9kg_query_introspection_wellbeing(nullptr), 0u);
    EXPECT_EQ(w9kg_query_self_model_autobio(nullptr), 0u);
    EXPECT_EQ(w9kg_query_gws_winners_by_salience(nullptr), 0u);
    EXPECT_EQ(w9kg_query_self_awareness_events(nullptr), 0u);
    EXPECT_EQ(w9kg_query_autobio_event_count(nullptr), 0u);
    SUCCEED();
}
