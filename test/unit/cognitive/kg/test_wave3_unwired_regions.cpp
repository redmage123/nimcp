/**
 * @file test_wave3_unwired_regions.cpp
 * @brief Unit test for KG-integration Wave W3 (unwired brain regions).
 *
 * Wave W3 added fresh `*_kg_wiring.c` bridges for 10 brain regions that
 * previously had zero KG references:
 *   occipital, somatosensory, motor, broca, gustatory, olfactory,
 *   brainstem, raphe, parahippocampal, perirhinal.
 *
 * Each bridge exposes:
 *   int  nimcp_<region>_kg_wiring_init(brain_t brain);
 *   void nimcp_<region>_kg_emit_event(brain_t, kind, intensity, ts_us);
 *
 * This test verifies:
 *   (a) The structural sub-region nodes are present after the brain
 *       factory has run Wave 31 (region_kg_bridges). A second call to each
 *       wiring_init is idempotent.
 *   (b) Each emit_event produces a new event-node linked to its region
 *       root (or that the event node landed in the KG under the
 *       conventional name `<region>_event_<kind>_<ts_us>`).
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <string>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"

#include "core/brain/regions/occipital/bridges/nimcp_occipital_kg_wiring.h"
#include "core/brain/regions/somatosensory/bridges/nimcp_somatosensory_kg_wiring.h"
#include "core/brain/regions/motor/bridges/nimcp_motor_kg_wiring.h"
#include "core/brain/regions/broca/bridges/nimcp_broca_kg_wiring.h"
#include "core/brain/regions/gustatory/bridges/nimcp_gustatory_kg_wiring.h"
#include "core/brain/regions/olfactory/bridges/nimcp_olfactory_kg_wiring.h"
#include "core/brain/regions/brainstem/bridges/nimcp_brainstem_kg_wiring.h"
#include "core/brain/regions/raphe/bridges/nimcp_raphe_kg_wiring.h"
#include "core/brain/regions/parahippocampal/bridges/nimcp_parahippocampal_kg_wiring.h"
#include "core/brain/regions/perirhinal/bridges/nimcp_perirhinal_kg_wiring.h"

// --------------------------------------------------------------------------
// Fixture
// --------------------------------------------------------------------------

class Wave3UnwiredRegionsTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave3_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled);
        ASSERT_NE(brain->internal_kg, nullptr);
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
};

// --------------------------------------------------------------------------
// Structural nodes — each region_init is idempotent (Wave 31 already ran,
// a second call must not error and must not remove existing nodes).
// --------------------------------------------------------------------------

TEST_F(Wave3UnwiredRegionsTest, OccipitalStructuralNodes) {
    EXPECT_EQ(nimcp_occipital_kg_wiring_init(brain), 0);
    expect_node("occipital_cortex");
    expect_node("occipital_v1");
    expect_node("occipital_v2");
    expect_node("occipital_v4");
    expect_node("occipital_it");
}

TEST_F(Wave3UnwiredRegionsTest, SomatosensoryStructuralNodes) {
    EXPECT_EQ(nimcp_somatosensory_kg_wiring_init(brain), 0);
    expect_node("somatosensory_cortex");
    expect_node("somatosensory_s1");
    expect_node("somatosensory_s2");
}

TEST_F(Wave3UnwiredRegionsTest, MotorStructuralNodes) {
    EXPECT_EQ(nimcp_motor_kg_wiring_init(brain), 0);
    expect_node("motor_cortex");
    expect_node("motor_m1");
    expect_node("motor_sma");
    expect_node("motor_pmc");
}

TEST_F(Wave3UnwiredRegionsTest, BrocaStructuralNodes) {
    EXPECT_EQ(nimcp_broca_kg_wiring_init(brain), 0);
    expect_node("broca_area");
    expect_node("broca_ba44");
    expect_node("broca_ba45");
    expect_node("broca_dorsal_stream");
    expect_node("broca_ventral_stream");
}

TEST_F(Wave3UnwiredRegionsTest, GustatoryStructuralNodes) {
    EXPECT_EQ(nimcp_gustatory_kg_wiring_init(brain), 0);
    expect_node("gustatory_cortex");
    expect_node("gustatory_primary");
    expect_node("gustatory_secondary");
}

TEST_F(Wave3UnwiredRegionsTest, OlfactoryStructuralNodes) {
    EXPECT_EQ(nimcp_olfactory_kg_wiring_init(brain), 0);
    expect_node("olfactory_system");
    expect_node("olfactory_bulb");
    expect_node("olfactory_piriform");
}

TEST_F(Wave3UnwiredRegionsTest, BrainstemStructuralNodes) {
    EXPECT_EQ(nimcp_brainstem_kg_wiring_init(brain), 0);
    expect_node("brainstem");
    expect_node("brainstem_vestibular");
    expect_node("brainstem_cardiopulmonary");
}

TEST_F(Wave3UnwiredRegionsTest, RapheStructuralNodes) {
    EXPECT_EQ(nimcp_raphe_kg_wiring_init(brain), 0);
    expect_node("raphe_nuclei");
    expect_node("raphe_dorsal");
    expect_node("raphe_median");
}

TEST_F(Wave3UnwiredRegionsTest, ParahippocampalStructuralNodes) {
    EXPECT_EQ(nimcp_parahippocampal_kg_wiring_init(brain), 0);
    expect_node("parahippocampal_cortex");
    expect_node("parahippocampal_posterior");
    expect_node("parahippocampal_anterior");
}

TEST_F(Wave3UnwiredRegionsTest, PerirhinalStructuralNodes) {
    EXPECT_EQ(nimcp_perirhinal_kg_wiring_init(brain), 0);
    expect_node("perirhinal_cortex");
    expect_node("perirhinal_area35");
    expect_node("perirhinal_area36");
}

// --------------------------------------------------------------------------
// Runtime events — call each emit_event once with a unique timestamp and
// assert the corresponding node-name ends up in the KG.
// --------------------------------------------------------------------------

TEST_F(Wave3UnwiredRegionsTest, AllRegionsEmitRuntimeEvents) {
    struct EmitCase {
        const char* region;
        const char* kind;
        void (*emit)(brain_t, const char*, float, uint64_t);
    };
    const EmitCase cases[] = {
        { "occipital",       "edge_detect",  nimcp_occipital_kg_emit_event },
        { "somatosensory",   "touch_light",  nimcp_somatosensory_kg_emit_event },
        { "motor",           "reach",        nimcp_motor_kg_emit_event },
        { "broca",           "sentence",     nimcp_broca_kg_emit_event },
        { "gustatory",       "sweet",        nimcp_gustatory_kg_emit_event },
        { "olfactory",       "floral",       nimcp_olfactory_kg_emit_event },
        { "brainstem",       "tachycardia",  nimcp_brainstem_kg_emit_event },
        { "raphe",           "calm",         nimcp_raphe_kg_emit_event },
        { "parahippocampal", "scene",        nimcp_parahippocampal_kg_emit_event },
        { "perirhinal",      "seen_before",  nimcp_perirhinal_kg_emit_event },
    };

    uint64_t ts = 1714089600000000ULL;
    for (const auto& c : cases) {
        ASSERT_NE(c.emit, nullptr);
        c.emit(brain, c.kind, 0.75f, ts);

        std::string expected = std::string(c.region) + "_event_"
                             + c.kind + "_" + std::to_string(ts);
        brain_kg_node_id_t id = brain_kg_find_node(brain->internal_kg,
                                                   expected.c_str());
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "emit_event for region '" << c.region << "' did not create node '"
            << expected << "'";
        ++ts;  /* unique ts per event */
    }
}
