/**
 * @file test_wave4_peptide_and_subcortical.cpp
 * @brief Unit test for KG-integration Wave W4.
 *
 * Wave W4 adds:
 *   Part A (5 region/peptide structural + runtime event emitters):
 *     - endocannabinoid (ecb_event_retrograde_signal)
 *     - glymphatic      (glymphatic_event_clearance)
 *     - mammillary      (mammillary_event_relay)
 *     - neuropeptide    (neuropeptide_event_release)
 *     - sensory_integration (sensory_integration_event_crossmodal_bind)
 *
 *   Part B (8 per-subcortical-module runtime emit functions):
 *     - subcortical_emit_action_selected       (basal_ganglia/striatum)
 *     - subcortical_emit_inhibition            (globus_pallidus)
 *     - subcortical_emit_stop_signal           (STN)
 *     - subcortical_emit_dopamine_release      (substantia_nigra)
 *     - subcortical_emit_reward_prediction     (NAcc)
 *     - subcortical_emit_orienting_response    (superior_colliculus)
 *     - subcortical_emit_auditory_localization (inferior_colliculus)
 *     - subcortical_emit_routing_decision      (subcortical thalamus)
 *
 * Each test creates a minimal brain (internal_kg always-on + admin-token
 * populated), invokes the W4 code path, then asserts via brain_kg_find_node.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"

#include "core/brain/regions/endocannabinoid/bridges/nimcp_endocannabinoid_kg_wiring.h"
#include "core/brain/regions/glymphatic/bridges/nimcp_glymphatic_kg_wiring.h"
#include "core/brain/regions/mammillary/bridges/nimcp_mammillary_kg_wiring.h"
#include "core/brain/regions/neuropeptide/bridges/nimcp_neuropeptide_kg_wiring.h"
#include "core/brain/regions/sensory_integration/bridges/nimcp_sensory_integration_kg_wiring.h"

#include "core/brain/subcortical/bridges/nimcp_subcortical_runtime_events.h"

//-----------------------------------------------------------------------------
// Fixture
//-----------------------------------------------------------------------------

class Wave4PeptideSubcorticalTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave4_kg_test",
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

    void expect_node(const char* name) {
        brain_kg_node_id_t id = brain_kg_find_node(brain->internal_kg, name);
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "expected KG node '" << name << "' to be present";
    }

    /* Returns true if at least one node whose name starts with `prefix`
     * exists in the KG. Used for event-node prefix checks since event
     * names carry a timestamp suffix. */
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
// Part A: 5 region structural init + one runtime event each
//-----------------------------------------------------------------------------

TEST_F(Wave4PeptideSubcorticalTest, EndocannabinoidStructuralAndRuntime) {
    EXPECT_EQ(nimcp_endocannabinoid_kg_wiring_init(brain), 0);
    expect_node("endocannabinoid");
    expect_node("anandamide");
    expect_node("2_ag");
    expect_node("cb1_receptor");
    expect_node("cb2_receptor");

    /* Idempotent re-init */
    EXPECT_EQ(nimcp_endocannabinoid_kg_wiring_init(brain), 0);
    expect_node("endocannabinoid");

    /* Runtime emit */
    endocannabinoid_emit_retrograde_signal(brain, "2_ag", 0.7f);
    EXPECT_TRUE(any_node_with_prefix("ecb_event_retrograde_signal_"));
}

TEST_F(Wave4PeptideSubcorticalTest, GlymphaticStructuralAndRuntime) {
    EXPECT_EQ(nimcp_glymphatic_kg_wiring_init(brain), 0);
    expect_node("glymphatic");
    expect_node("aquaporin_4");
    expect_node("perivascular_space");
    expect_node("csf_isf_exchange");

    glymphatic_emit_clearance(brain, 0.42f);
    EXPECT_TRUE(any_node_with_prefix("glymphatic_event_clearance_"));
}

TEST_F(Wave4PeptideSubcorticalTest, MammillaryStructuralAndRuntime) {
    EXPECT_EQ(nimcp_mammillary_kg_wiring_init(brain), 0);
    expect_node("mammillary");
    expect_node("medial_mammillary_nucleus");
    expect_node("lateral_mammillary_nucleus");
    expect_node("mammillothalamic_tract");

    mammillary_emit_relay(brain, 0.5f);
    EXPECT_TRUE(any_node_with_prefix("mammillary_event_relay_"));
}

TEST_F(Wave4PeptideSubcorticalTest, NeuropeptideStructuralAndRuntime) {
    EXPECT_EQ(nimcp_neuropeptide_kg_wiring_init(brain), 0);
    expect_node("neuropeptide");
    expect_node("oxytocin");
    expect_node("vasopressin");
    expect_node("crh");
    expect_node("substance_p");
    expect_node("orexin");
    expect_node("opioid_peptide");

    neuropeptide_emit_release(brain, "oxytocin", 0.8f);
    EXPECT_TRUE(any_node_with_prefix("neuropeptide_event_release_"));
}

TEST_F(Wave4PeptideSubcorticalTest, SensoryIntegrationStructuralAndRuntime) {
    EXPECT_EQ(nimcp_sensory_integration_kg_wiring_init(brain), 0);
    expect_node("sensory_integration");
    expect_node("crossmodal_binding");
    expect_node("multisensory_convergence");
    expect_node("temporal_binding");

    sensory_integration_emit_crossmodal_bind(brain, "visual", "audio", 0.9f);
    EXPECT_TRUE(any_node_with_prefix(
        "sensory_integration_event_crossmodal_bind_"));
}

//-----------------------------------------------------------------------------
// Part B: 8 per-subcortical-module runtime emit functions
//-----------------------------------------------------------------------------

TEST_F(Wave4PeptideSubcorticalTest, SubcorticalEmitActionSelected) {
    subcortical_emit_action_selected(brain, 7u, 0.8f);
    EXPECT_TRUE(any_node_with_prefix("striatum_event_action_7_"));
}

TEST_F(Wave4PeptideSubcorticalTest, SubcorticalEmitInhibition) {
    subcortical_emit_inhibition(brain, 0.55f);
    EXPECT_TRUE(any_node_with_prefix("gp_event_inhibit_"));
}

TEST_F(Wave4PeptideSubcorticalTest, SubcorticalEmitStopSignal) {
    subcortical_emit_stop_signal(brain);
    EXPECT_TRUE(any_node_with_prefix("stn_event_stop_"));
}

TEST_F(Wave4PeptideSubcorticalTest, SubcorticalEmitDopamineRelease) {
    subcortical_emit_dopamine_release(brain, 0.33f);
    EXPECT_TRUE(any_node_with_prefix("sn_event_da_"));
}

TEST_F(Wave4PeptideSubcorticalTest, SubcorticalEmitRewardPrediction) {
    subcortical_emit_reward_prediction(brain, 0.15f);
    EXPECT_TRUE(any_node_with_prefix("nacc_event_rpe_"));
}

TEST_F(Wave4PeptideSubcorticalTest, SubcorticalEmitOrientingResponse) {
    subcortical_emit_orienting_response(brain, 0.3f, -0.1f);
    EXPECT_TRUE(any_node_with_prefix("sc_event_orient_"));
}

TEST_F(Wave4PeptideSubcorticalTest, SubcorticalEmitAuditoryLocalization) {
    subcortical_emit_auditory_localization(brain, 45.0f, 10.0f);
    EXPECT_TRUE(any_node_with_prefix("ic_event_loc_"));
}

TEST_F(Wave4PeptideSubcorticalTest, SubcorticalEmitRoutingDecision) {
    subcortical_emit_routing_decision(brain, 2u, 5u);
    EXPECT_TRUE(any_node_with_prefix("thalamus_event_route_"));
}

//-----------------------------------------------------------------------------
// Null-safety: every W4 emit must be a no-op on NULL brain
//-----------------------------------------------------------------------------

TEST(Wave4NullSafety, AllEmittersAreNullSafe) {
    /* Should not crash or throw when brain is NULL. */
    endocannabinoid_emit_retrograde_signal(nullptr, "aea", 1.0f);
    glymphatic_emit_clearance(nullptr, 1.0f);
    mammillary_emit_relay(nullptr, 1.0f);
    neuropeptide_emit_release(nullptr, "oxytocin", 1.0f);
    sensory_integration_emit_crossmodal_bind(nullptr, "v", "a", 1.0f);

    subcortical_emit_action_selected(nullptr, 0, 0.0f);
    subcortical_emit_inhibition(nullptr, 0.0f);
    subcortical_emit_stop_signal(nullptr);
    subcortical_emit_dopamine_release(nullptr, 0.0f);
    subcortical_emit_reward_prediction(nullptr, 0.0f);
    subcortical_emit_orienting_response(nullptr, 0.0f, 0.0f);
    subcortical_emit_auditory_localization(nullptr, 0.0f, 0.0f);
    subcortical_emit_routing_decision(nullptr, 0u, 0u);

    SUCCEED();
}
