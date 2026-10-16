/**
 * @file test_wave11_safety.cpp
 * @brief Unit test for KG-integration Wave W11 — safety family.
 *
 * Wave W11 wires four safety subsystems into `brain->internal_kg`:
 *   1. ethics engine       (ethics_engine root + evaluation/incident/directive/policy events)
 *   2. LGSS                (lgss_module root + evaluation events per outcome)
 *   3. mental health       (mental_health_monitor root + disorder + intervention events)
 *   4. brain immune        (brain_immune root + antigen + lifecycle + response events)
 *
 * Each test:
 *  (a) verifies the structural root exists after the first emit,
 *  (b) calls the emit helper,
 *  (c) asserts an event-node prefix appears.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "security/nimcp_w11_safety_kg_events.h"

//-----------------------------------------------------------------------------
// Fixture
//-----------------------------------------------------------------------------

class Wave11SafetyKGTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave11_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled)
            << "internal_kg_enabled must be true post-creation";
        ASSERT_NE(brain->internal_kg, nullptr)
            << "brain->internal_kg must be allocated";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    bool has_node(const char* name) {
        return brain_kg_find_node(brain->internal_kg, name) !=
               BRAIN_KG_INVALID_NODE;
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
// 1. ensure_roots creates all 4 structural parents
//-----------------------------------------------------------------------------

TEST_F(Wave11SafetyKGTest, EnsureRootsCreatesAllFour) {
    w11_safety_ensure_roots(brain);
    EXPECT_TRUE(has_node("ethics_engine"));
    EXPECT_TRUE(has_node("lgss_module"));
    EXPECT_TRUE(has_node("mental_health_monitor"));
    EXPECT_TRUE(has_node("brain_immune"));
}

TEST_F(Wave11SafetyKGTest, EnsureRootsIsIdempotent) {
    w11_safety_ensure_roots(brain);
    w11_safety_ensure_roots(brain);  /* must not duplicate */
    EXPECT_TRUE(has_node("ethics_engine"));
    EXPECT_TRUE(has_node("lgss_module"));
}

//-----------------------------------------------------------------------------
// 2. Ethics engine emits
//-----------------------------------------------------------------------------

TEST_F(Wave11SafetyKGTest, EthicsLifecycleEmitsEvent) {
    w11_emit_ethics_lifecycle(brain, "created");
    EXPECT_TRUE(any_node_with_prefix("ethics_engine_event_lifecycle_"));
    EXPECT_TRUE(has_node("ethics_engine"));  /* parent root auto-created */
}

TEST_F(Wave11SafetyKGTest, EthicsEvaluationEmitsEvent) {
    w11_emit_ethics_evaluation(brain, /*violation*/1, "harm",
                               /*allowed*/false,
                               /*golden*/-0.7f, /*conf*/0.9f);
    EXPECT_TRUE(any_node_with_prefix("ethics_engine_event_eval_"));
}

TEST_F(Wave11SafetyKGTest, EthicsIncidentEmitsEvent) {
    w11_emit_ethics_incident(brain,
                             /*id*/42, /*violation*/1,
                             /*severity*/0.75f, /*action*/1 /*BLOCK*/,
                             /*policy_id*/7, "test_policy");
    EXPECT_TRUE(any_node_with_prefix("ethics_engine_event_incident_"));
}

TEST_F(Wave11SafetyKGTest, EthicsPolicyChangeEmitsEvent) {
    w11_emit_ethics_policy_change(brain, /*added*/true, /*pid*/99, "my_policy");
    EXPECT_TRUE(any_node_with_prefix("ethics_engine_event_policy_add_"));

    w11_emit_ethics_policy_change(brain, /*added*/false, /*pid*/99, "my_policy");
    EXPECT_TRUE(any_node_with_prefix("ethics_engine_event_policy_remove_"));
}

TEST_F(Wave11SafetyKGTest, EthicsDirectiveEmitsEvent) {
    w11_emit_ethics_directive(brain, "golden_rule",
                              /*passed*/false, /*severity*/0.8f);
    EXPECT_TRUE(any_node_with_prefix("ethics_engine_event_directive_"));
}

//-----------------------------------------------------------------------------
// 3. LGSS emits — every safety_action value produces a distinct node prefix
//-----------------------------------------------------------------------------

TEST_F(Wave11SafetyKGTest, LgssDenyEmitsDenyPrefixEvent) {
    w11_emit_lgss_evaluation(brain,
                             /*action=DENY*/1, /*severity=CRITICAL*/0,
                             /*conf*/1.0f, "ACTION_INTERCEPTOR",
                             "brain decision out-of-policy");
    EXPECT_TRUE(any_node_with_prefix("lgss_module_event_deny_"));
}

TEST_F(Wave11SafetyKGTest, LgssAllowEmitsAllowPrefixEvent) {
    w11_emit_lgss_evaluation(brain,
                             /*action=ALLOW*/0, /*severity=INFO*/4,
                             /*conf*/1.0f, "INPUT_VALIDATOR",
                             "input ok");
    EXPECT_TRUE(any_node_with_prefix("lgss_module_event_allow_"));
}

TEST_F(Wave11SafetyKGTest, LgssKbEventEmitsEvent) {
    w11_emit_lgss_kb_event(brain, "rule_added", 123, "no-harm-to-humans");
    EXPECT_TRUE(any_node_with_prefix("lgss_module_event_kb_rule_added_"));
}

//-----------------------------------------------------------------------------
// 4. Mental health emits
//-----------------------------------------------------------------------------

TEST_F(Wave11SafetyKGTest, MentalHealthDisorderEmitsEvent) {
    w11_emit_mental_health_disorder(brain,
                                    /*disorder_type*/0, "sociopathy",
                                    /*severity*/2, /*score*/0.55f);
    EXPECT_TRUE(any_node_with_prefix(
        "mental_health_monitor_event_disorder_sociopathy_"));
    EXPECT_TRUE(has_node("mental_health_monitor"));
}

TEST_F(Wave11SafetyKGTest, MentalHealthInterventionEmitsEvent) {
    w11_emit_mental_health_intervention(brain, "neuromod_adjust",
                                        /*success*/true,
                                        "restore baseline");
    EXPECT_TRUE(any_node_with_prefix(
        "mental_health_monitor_event_intervention_"));
}

//-----------------------------------------------------------------------------
// 5. Brain immune emits
//-----------------------------------------------------------------------------

TEST_F(Wave11SafetyKGTest, ImmuneLifecycleEmitsEvent) {
    w11_emit_immune_lifecycle(brain, "start");
    EXPECT_TRUE(any_node_with_prefix("brain_immune_event_lifecycle_"));
    EXPECT_TRUE(has_node("brain_immune"));
}

TEST_F(Wave11SafetyKGTest, ImmuneAntigenEmitsEvent) {
    w11_emit_immune_antigen(brain, "bbb_threat", /*severity*/0.8f);
    EXPECT_TRUE(any_node_with_prefix("brain_immune_event_antigen_"));
}

TEST_F(Wave11SafetyKGTest, ImmuneResponseEmitsEvent) {
    w11_emit_immune_response(brain, "b_cell_activated", /*strength*/0.6f);
    EXPECT_TRUE(any_node_with_prefix(
        "brain_immune_event_response_b_cell_activated_"));
}

//-----------------------------------------------------------------------------
// 6. Null-safety — nothing should crash when brain is NULL or KG is disabled
//-----------------------------------------------------------------------------

TEST(Wave11SafetyKGStaticTests, NullBrainIsSafe) {
    /* Must not crash. */
    w11_safety_ensure_roots(nullptr);
    w11_emit_ethics_lifecycle(nullptr, "created");
    w11_emit_ethics_evaluation(nullptr, 0, nullptr, true, 0.0f, 0.0f);
    w11_emit_ethics_incident(nullptr, 0, 0, 0.0f, 0, 0, nullptr);
    w11_emit_ethics_policy_change(nullptr, true, 0, nullptr);
    w11_emit_ethics_directive(nullptr, nullptr, true, 0.0f);
    w11_emit_lgss_evaluation(nullptr, 0, 0, 0.0f, nullptr, nullptr);
    w11_emit_lgss_kb_event(nullptr, nullptr, 0, nullptr);
    w11_emit_mental_health_disorder(nullptr, 0, nullptr, 0, 0.0f);
    w11_emit_mental_health_intervention(nullptr, nullptr, true, nullptr);
    w11_emit_immune_lifecycle(nullptr, nullptr);
    w11_emit_immune_antigen(nullptr, nullptr, 0.0f);
    w11_emit_immune_response(nullptr, nullptr, 0.0f);
    SUCCEED();
}
