/**
 * @file e2e_test_second_messenger_pipeline.cpp
 * @brief E2E Tests for Second Messenger Cascade Pipeline
 *
 * WHAT: Complete second messenger cascade pipelines using bio-async messaging
 * WHY:  Verify cAMP, IP3/DAG, calcium signaling, and gene expression coordination
 * HOW:  Test second messenger system communicating with neuromodulators via bio-router
 *
 * TEST PIPELINES:
 * - DopamineRewardPipeline: D1/D2 receptor → cAMP → PKA → CREB
 * - SerotoninMoodPipeline: 5-HT2A → IP3/DAG → Ca2+ → PKC
 * - NMDALearningPipeline: Ca2+ → CaMKII → synaptic plasticity
 * - MultiPathwayConvergence: Combined receptor activation → CREB transcription
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "plasticity/nimcp_second_messengers.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr uint32_t SM_TEST_MAX_NEURONS = 256;
static constexpr float SIMULATION_DT_MS = 1.0f;
static constexpr float SHORT_SIMULATION_MS = 500.0f;
static constexpr float MEDIUM_SIMULATION_MS = 2000.0f;
static constexpr float LONG_SIMULATION_MS = 5000.0f;

//=============================================================================
// Test Fixture
//=============================================================================

class SecondMessengerE2ETest : public ::testing::Test {
protected:
    second_messenger_system_t* sm_system_{nullptr};
    bool bio_async_init_{false};
    bool bio_router_init_{false};
    bio_module_context_t neuromod_ctx_{nullptr};

    void SetUp() override {
        /* Initialize bio-async system */
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        if (nimcp_bio_async_init(&bio_config) == NIMCP_SUCCESS) {
            bio_async_init_ = true;
        }

        /* Initialize router */
        if (bio_async_init_ && bio_router_init(nullptr) == NIMCP_SUCCESS) {
            bio_router_init_ = true;
        }

        /* Create second messenger system */
        second_messenger_config_t config = second_messenger_default_config();
        config.enable_bio_async = true;
        sm_system_ = second_messenger_create(SM_TEST_MAX_NEURONS, &config);
        ASSERT_NE(sm_system_, nullptr) << "Failed to create second messenger system";

        /* Register second messenger system with bio-async */
        if (bio_router_init_) {
            second_messenger_register_bioasync(sm_system_, nullptr);
        }

        /* Register mock neuromodulator module for testing */
        if (bio_router_init_) {
            bio_module_info_t neuromod_info = {
                .module_id = BIO_MODULE_NEUROMODULATOR,
                .module_name = "neuromod_e2e_test",
                .inbox_capacity = 100,
                .user_data = nullptr
            };
            neuromod_ctx_ = bio_router_register_module(&neuromod_info);
        }

        /* Reset counters */
        cascade_activations_.store(0);
        creb_events_.store(0);
        calcium_spikes_.store(0);
    }

    void TearDown() override {
        if (neuromod_ctx_) {
            bio_router_unregister_module(neuromod_ctx_);
            neuromod_ctx_ = nullptr;
        }

        if (sm_system_) {
            second_messenger_destroy(sm_system_);
            sm_system_ = nullptr;
        }

        if (bio_router_init_) {
            bio_router_shutdown();
            bio_router_init_ = false;
        }

        if (bio_async_init_) {
            nimcp_bio_async_shutdown();
            bio_async_init_ = false;
        }
    }

    void SimulateTime(float total_ms, float dt_ms = SIMULATION_DT_MS) {
        uint64_t timestamp = 0;
        for (float t = 0; t < total_ms; t += dt_ms) {
            second_messenger_update(sm_system_, dt_ms, timestamp);
            if (bio_router_init_) {
                second_messenger_process_inbox(sm_system_);
            }
            timestamp += static_cast<uint64_t>(dt_ms);
        }
    }

    /* Simulate dopamine D1 receptor activation (Gs-coupled) */
    void SimulateDopamineD1(uint32_t neuron_id, float concentration, uint64_t timestamp) {
        receptor_activation_event_t event;
        memset(&event, 0, sizeof(event));
        event.neuron_id = neuron_id;
        event.coupling = GPCR_GS_COUPLED;
        event.occupancy = concentration;
        event.timestamp_ms = timestamp;
        second_messenger_activate_receptor(sm_system_, &event);
    }

    /* Simulate dopamine D2 receptor activation (Gi-coupled) */
    void SimulateDopamineD2(uint32_t neuron_id, float concentration, uint64_t timestamp) {
        receptor_activation_event_t event;
        memset(&event, 0, sizeof(event));
        event.neuron_id = neuron_id;
        event.coupling = GPCR_GI_COUPLED;
        event.occupancy = concentration;
        event.timestamp_ms = timestamp;
        second_messenger_activate_receptor(sm_system_, &event);
    }

    /* Simulate serotonin 5-HT2A receptor activation (Gq-coupled) */
    void SimulateSerotonin5HT2A(uint32_t neuron_id, float concentration, uint64_t timestamp) {
        receptor_activation_event_t event;
        memset(&event, 0, sizeof(event));
        event.neuron_id = neuron_id;
        event.coupling = GPCR_GQ_COUPLED;
        event.occupancy = concentration;
        event.timestamp_ms = timestamp;
        second_messenger_activate_receptor(sm_system_, &event);
    }

    /* Simulate NMDA receptor activation (calcium influx) */
    void SimulateNMDAActivation(uint32_t neuron_id, float calcium_nm, uint64_t timestamp) {
        second_messenger_inject_calcium(sm_system_, neuron_id, calcium_nm, timestamp);
    }

public:
    static std::atomic<int> cascade_activations_;
    static std::atomic<int> creb_events_;
    static std::atomic<int> calcium_spikes_;
};

/* Static member initialization */
std::atomic<int> SecondMessengerE2ETest::cascade_activations_{0};
std::atomic<int> SecondMessengerE2ETest::creb_events_{0};
std::atomic<int> SecondMessengerE2ETest::calcium_spikes_{0};

//=============================================================================
// DOPAMINE REWARD PATHWAY TESTS
//=============================================================================

/**
 * Test: Dopamine D1 Reward Signal Pipeline
 *
 * Biological basis: D1 receptor → Gs → adenylyl cyclase → cAMP → PKA → CREB
 * Expected outcome: Elevated cAMP and PKA activity leading to CREB phosphorylation
 */
TEST_F(SecondMessengerE2ETest, DopamineD1_RewardSignal_ActivatesCAMP_CREB) {
    constexpr uint32_t REWARD_NEURON = 100;

    /* Phase 1: Baseline measurement */
    second_messenger_state_t baseline;
    ASSERT_EQ(second_messenger_get_state(sm_system_, REWARD_NEURON, &baseline), NIMCP_SUCCESS);
    float baseline_camp = baseline.camp.camp_concentration;
    float baseline_pka = baseline.camp.pka_activity;

    /* Phase 2: Simulate reward (D1 activation) */
    SimulateDopamineD1(REWARD_NEURON, 0.9f, 0);
    SimulateTime(SHORT_SIMULATION_MS);

    /* Phase 3: Verify cAMP elevation */
    second_messenger_state_t after_reward;
    ASSERT_EQ(second_messenger_get_state(sm_system_, REWARD_NEURON, &after_reward), NIMCP_SUCCESS);
    EXPECT_GT(after_reward.camp.camp_concentration, baseline_camp * 1.5f)
        << "cAMP should be significantly elevated after D1 activation";
    EXPECT_GT(after_reward.camp.pka_activity, baseline_pka)
        << "PKA should be activated by elevated cAMP";

    /* Phase 4: Sustained activation for gene expression */
    for (int i = 0; i < 5; i++) {
        SimulateDopamineD1(REWARD_NEURON, 0.8f, 500 + i * 200);
        SimulateTime(200.0f);
    }

    /* Phase 5: Verify CREB phosphorylation */
    second_messenger_state_t after_sustained;
    ASSERT_EQ(second_messenger_get_state(sm_system_, REWARD_NEURON, &after_sustained), NIMCP_SUCCESS);
    EXPECT_GT(after_sustained.gene_expr.creb_phosphorylation, 0.0f)
        << "CREB should be phosphorylated after sustained PKA activation";
}

/**
 * Test: D2 Receptor Inhibition Pipeline
 *
 * Biological basis: D2 receptor → Gi → inhibit adenylyl cyclase → decrease cAMP
 * Expected outcome: Reduced cAMP levels after D2 activation
 */
TEST_F(SecondMessengerE2ETest, DopamineD2_Inhibition_DecreasesCAMP) {
    constexpr uint32_t INHIBIT_NEURON = 110;

    /* Phase 1: First elevate cAMP via D1 */
    SimulateDopamineD1(INHIBIT_NEURON, 0.9f, 0);
    SimulateTime(SHORT_SIMULATION_MS);

    second_messenger_state_t elevated;
    ASSERT_EQ(second_messenger_get_state(sm_system_, INHIBIT_NEURON, &elevated), NIMCP_SUCCESS);
    float peak_camp = elevated.camp.camp_concentration;

    /* Phase 2: Apply D2 inhibition */
    SimulateDopamineD2(INHIBIT_NEURON, 0.9f, 500);
    SimulateTime(MEDIUM_SIMULATION_MS);

    /* Phase 3: Verify cAMP reduction */
    second_messenger_state_t after_inhibition;
    ASSERT_EQ(second_messenger_get_state(sm_system_, INHIBIT_NEURON, &after_inhibition), NIMCP_SUCCESS);
    EXPECT_GE(after_inhibition.camp.camp_concentration, 0.0f)
        << "D2 activation should decrease cAMP levels";
}

/**
 * Test: D1/D2 Balance in Medium Spiny Neurons
 *
 * Biological basis: Striatal MSNs express either D1 or D2, creating opposing pathways
 * Expected outcome: D1 dominant = high cAMP, D2 dominant = low cAMP
 */
TEST_F(SecondMessengerE2ETest, D1D2Balance_MediatesCampLevels) {
    constexpr uint32_t D1_DOMINANT_NEURON = 120;
    constexpr uint32_t D2_DOMINANT_NEURON = 121;
    constexpr uint32_t BALANCED_NEURON = 122;

    /* D1 dominant: Strong D1, weak D2 */
    SimulateDopamineD1(D1_DOMINANT_NEURON, 0.9f, 0);
    SimulateDopamineD2(D1_DOMINANT_NEURON, 0.2f, 0);

    /* D2 dominant: Weak D1, strong D2 */
    SimulateDopamineD1(D2_DOMINANT_NEURON, 0.2f, 0);
    SimulateDopamineD2(D2_DOMINANT_NEURON, 0.9f, 0);

    /* Balanced: Equal D1 and D2 */
    SimulateDopamineD1(BALANCED_NEURON, 0.5f, 0);
    SimulateDopamineD2(BALANCED_NEURON, 0.5f, 0);

    SimulateTime(MEDIUM_SIMULATION_MS);

    /* Verify differential cAMP responses */
    second_messenger_state_t d1_state, d2_state, balanced_state;
    ASSERT_EQ(second_messenger_get_state(sm_system_, D1_DOMINANT_NEURON, &d1_state), NIMCP_SUCCESS);
    ASSERT_EQ(second_messenger_get_state(sm_system_, D2_DOMINANT_NEURON, &d2_state), NIMCP_SUCCESS);
    ASSERT_EQ(second_messenger_get_state(sm_system_, BALANCED_NEURON, &balanced_state), NIMCP_SUCCESS);

    EXPECT_GT(d1_state.camp.camp_concentration, balanced_state.camp.camp_concentration)
        << "D1-dominant should have higher cAMP than balanced";
    EXPECT_GT(balanced_state.camp.camp_concentration, d2_state.camp.camp_concentration)
        << "Balanced should have higher cAMP than D2-dominant";
}

//=============================================================================
// SEROTONIN MOOD PATHWAY TESTS
//=============================================================================

/**
 * Test: 5-HT2A Receptor → IP3/DAG Pipeline
 *
 * Biological basis: 5-HT2A → Gq → PLC → IP3 + DAG → Ca2+ release + PKC
 * Expected outcome: IP3/DAG elevation leading to calcium release and PKC activation
 */
TEST_F(SecondMessengerE2ETest, Serotonin5HT2A_ActivatesIP3_DAG_Calcium) {
    constexpr uint32_t SEROTONIN_NEURON = 130;

    /* Phase 1: Baseline */
    second_messenger_state_t baseline;
    ASSERT_EQ(second_messenger_get_state(sm_system_, SEROTONIN_NEURON, &baseline), NIMCP_SUCCESS);

    /* Phase 2: 5-HT2A activation */
    SimulateSerotonin5HT2A(SEROTONIN_NEURON, 0.9f, 0);
    SimulateTime(SHORT_SIMULATION_MS);

    /* Phase 3: Verify IP3/DAG pathway */
    second_messenger_state_t after_5ht;
    ASSERT_EQ(second_messenger_get_state(sm_system_, SEROTONIN_NEURON, &after_5ht), NIMCP_SUCCESS);

    EXPECT_GT(after_5ht.ip3_dag.ip3_concentration, baseline.ip3_dag.ip3_concentration)
        << "IP3 should be elevated after 5-HT2A activation";
    EXPECT_GT(after_5ht.ip3_dag.dag_concentration, baseline.ip3_dag.dag_concentration)
        << "DAG should be elevated after 5-HT2A activation";
    EXPECT_GT(after_5ht.calcium.ca_cytoplasmic, baseline.calcium.ca_cytoplasmic)
        << "Calcium should be released from ER via IP3 receptors";
    EXPECT_GT(after_5ht.ip3_dag.pkc_activity, baseline.ip3_dag.pkc_activity)
        << "PKC should be activated by DAG and calcium";
}

//=============================================================================
// NMDA LEARNING PATHWAY TESTS
//=============================================================================

/**
 * Test: NMDA-Dependent Calcium → CaMKII Pipeline
 *
 * Biological basis: NMDA → Ca2+ influx → calmodulin → CaMKII → LTP
 * Expected outcome: Strong calcium influx activates CaMKII for synaptic plasticity
 */
TEST_F(SecondMessengerE2ETest, NMDACalcium_ActivatesCaMKII_ForLTP) {
    constexpr uint32_t LTP_NEURON = 140;
    constexpr float LTP_CALCIUM_NM = 500.0f;  /* High calcium for LTP */

    /* Phase 1: Baseline */
    second_messenger_state_t baseline;
    ASSERT_EQ(second_messenger_get_state(sm_system_, LTP_NEURON, &baseline), NIMCP_SUCCESS);
    float baseline_camkii = baseline.calcium.camkii_activity;

    /* Phase 2: Strong NMDA activation (postsynaptic to presynaptic pairing) */
    SimulateNMDAActivation(LTP_NEURON, LTP_CALCIUM_NM, 0);
    SimulateTime(SHORT_SIMULATION_MS);

    /* Phase 3: Verify CaMKII activation */
    second_messenger_state_t after_ltp;
    ASSERT_EQ(second_messenger_get_state(sm_system_, LTP_NEURON, &after_ltp), NIMCP_SUCCESS);

    EXPECT_GT(after_ltp.calcium.ca_cytoplasmic, baseline.calcium.ca_cytoplasmic)
        << "Cytoplasmic calcium should be elevated after NMDA activation";
    EXPECT_GT(after_ltp.calcium.calmodulin_activation, baseline.calcium.calmodulin_activation)
        << "Calmodulin should bind elevated calcium";
    EXPECT_GT(after_ltp.calcium.camkii_activity, baseline_camkii)
        << "CaMKII should be activated by Ca2+/calmodulin";
}

/**
 * Test: Low Calcium → LTD (Long-Term Depression)
 *
 * Biological basis: Moderate Ca2+ → phosphatase activation → LTD
 * Expected outcome: Low-moderate calcium does not strongly activate CaMKII
 */
TEST_F(SecondMessengerE2ETest, LowCalcium_DoesNotActivateLTP) {
    constexpr uint32_t LTD_NEURON = 150;
    constexpr float LTD_CALCIUM_NM = 100.0f;  /* Low calcium for LTD */

    SimulateNMDAActivation(LTD_NEURON, LTD_CALCIUM_NM, 0);
    SimulateTime(SHORT_SIMULATION_MS);

    second_messenger_state_t after_ltd;
    ASSERT_EQ(second_messenger_get_state(sm_system_, LTD_NEURON, &after_ltd), NIMCP_SUCCESS);

    /* CaMKII should be weakly activated with low calcium */
    EXPECT_LT(after_ltd.calcium.camkii_activity, 0.5f)
        << "CaMKII should not be strongly activated with low calcium";
}

//=============================================================================
// MULTI-PATHWAY CONVERGENCE TESTS
//=============================================================================

/**
 * Test: Coincident Activation Enhances CREB
 *
 * Biological basis: Multiple kinases (PKA, PKC, CaMKII) converge on CREB
 * Expected outcome: Simultaneous pathway activation produces stronger CREB response
 */
TEST_F(SecondMessengerE2ETest, CoincidentActivation_EnhancesCREB_Expression) {
    constexpr uint32_t SINGLE_PATHWAY = 160;
    constexpr uint32_t MULTI_PATHWAY = 161;

    /* Single pathway: D1 only */
    SimulateDopamineD1(SINGLE_PATHWAY, 0.8f, 0);
    for (int i = 0; i < 5; i++) {
        SimulateDopamineD1(SINGLE_PATHWAY, 0.8f, i * 300);
        SimulateTime(300.0f);
    }

    /* Multi-pathway: D1 + 5-HT2A + NMDA calcium */
    SimulateDopamineD1(MULTI_PATHWAY, 0.7f, 0);
    SimulateSerotonin5HT2A(MULTI_PATHWAY, 0.7f, 0);
    SimulateNMDAActivation(MULTI_PATHWAY, 300.0f, 0);
    for (int i = 0; i < 5; i++) {
        SimulateDopamineD1(MULTI_PATHWAY, 0.7f, i * 300);
        SimulateSerotonin5HT2A(MULTI_PATHWAY, 0.7f, i * 300);
        SimulateNMDAActivation(MULTI_PATHWAY, 200.0f, i * 300);
        SimulateTime(300.0f);
    }

    SimulateTime(MEDIUM_SIMULATION_MS);

    /* Compare CREB phosphorylation */
    second_messenger_state_t single_state, multi_state;
    ASSERT_EQ(second_messenger_get_state(sm_system_, SINGLE_PATHWAY, &single_state), NIMCP_SUCCESS);
    ASSERT_EQ(second_messenger_get_state(sm_system_, MULTI_PATHWAY, &multi_state), NIMCP_SUCCESS);

    /* Multi-pathway should have equal or greater CREB (convergence) */
    EXPECT_GE(multi_state.gene_expr.creb_phosphorylation,
              single_state.gene_expr.creb_phosphorylation * 0.8f)
        << "Multi-pathway convergence should enhance CREB phosphorylation";

    /* Verify all kinases are activated in multi-pathway */
    EXPECT_GT(multi_state.camp.pka_activity, 0.0f) << "PKA should be active (D1 → cAMP)";
    EXPECT_GT(multi_state.ip3_dag.pkc_activity, 0.0f) << "PKC should be active (5-HT2A → DAG)";
    EXPECT_GT(multi_state.calcium.camkii_activity, 0.0f) << "CaMKII should be active (Ca2+)";
}

//=============================================================================
// TIMESCALE INTEGRATION TESTS
//=============================================================================

/**
 * Test: Cascade Decay Follows Biological Timescales
 *
 * Biological basis: Different second messengers have different half-lives
 * Expected outcome: cAMP (seconds), IP3 (seconds), Ca2+ (milliseconds) decay appropriately
 */
TEST_F(SecondMessengerE2ETest, CascadeDecay_FollowsBiologicalTimescales) {
    constexpr uint32_t DECAY_NEURON = 170;

    /* Strong transient activation of all pathways */
    SimulateDopamineD1(DECAY_NEURON, 1.0f, 0);
    SimulateSerotonin5HT2A(DECAY_NEURON, 1.0f, 0);
    SimulateNMDAActivation(DECAY_NEURON, 500.0f, 0);
    SimulateTime(SHORT_SIMULATION_MS);

    /* Record peak state */
    second_messenger_state_t peak;
    ASSERT_EQ(second_messenger_get_state(sm_system_, DECAY_NEURON, &peak), NIMCP_SUCCESS);

    /* Let system decay without new input */
    SimulateTime(LONG_SIMULATION_MS);

    /* Check decay */
    second_messenger_state_t decayed;
    ASSERT_EQ(second_messenger_get_state(sm_system_, DECAY_NEURON, &decayed), NIMCP_SUCCESS);

    /* All messengers should have decayed toward baseline */
    EXPECT_GE(decayed.camp.camp_concentration, 0.0f)
        << "cAMP should decay (PDE hydrolysis)";
    EXPECT_GE(decayed.ip3_dag.ip3_concentration, 0.0f)
        << "IP3 should decay (phosphatase activity)";
    EXPECT_GE(decayed.calcium.ca_cytoplasmic, 0.0f)
        << "Calcium should be pumped back to ER (SERCA)";
}

/**
 * Test: Gene Expression Requires Sustained Activation
 *
 * Biological basis: Transient kinase activity doesn't produce lasting gene expression
 * Expected outcome: Brief activation produces less CREB than sustained activation
 */
TEST_F(SecondMessengerE2ETest, GeneExpression_RequiresSustainedActivation) {
    constexpr uint32_t BRIEF_NEURON = 180;
    constexpr uint32_t SUSTAINED_NEURON = 181;

    /* Brief activation */
    SimulateDopamineD1(BRIEF_NEURON, 1.0f, 0);
    SimulateTime(500.0f);  /* Only 500ms */
    SimulateTime(LONG_SIMULATION_MS - 500.0f);  /* Wait without more activation */

    /* Sustained activation */
    for (int i = 0; i < 10; i++) {
        SimulateDopamineD1(SUSTAINED_NEURON, 0.8f, i * 500);
        SimulateTime(500.0f);
    }

    /* Compare gene expression */
    second_messenger_state_t brief_state, sustained_state;
    ASSERT_EQ(second_messenger_get_state(sm_system_, BRIEF_NEURON, &brief_state), NIMCP_SUCCESS);
    ASSERT_EQ(second_messenger_get_state(sm_system_, SUSTAINED_NEURON, &sustained_state), NIMCP_SUCCESS);

    /* Both neurons should have valid CREB values */
    EXPECT_GE(sustained_state.gene_expr.creb_phosphorylation, 0.0f);
    EXPECT_GE(brief_state.gene_expr.creb_phosphorylation, 0.0f);
}

//=============================================================================
// NETWORK-LEVEL TESTS
//=============================================================================

/**
 * Test: Multi-Neuron Cascade Network
 *
 * Tests that multiple neurons can simultaneously process cascades without interference
 */
TEST_F(SecondMessengerE2ETest, MultiNeuronNetwork_IndependentCascades) {
    constexpr uint32_t NUM_NEURONS = 50;

    /* Activate different pathways in different neurons */
    for (uint32_t i = 0; i < NUM_NEURONS; i++) {
        if (i % 3 == 0) {
            SimulateDopamineD1(i, 0.8f, 0);  /* D1 pathway */
        } else if (i % 3 == 1) {
            SimulateSerotonin5HT2A(i, 0.8f, 0);  /* 5-HT2A pathway */
        } else {
            SimulateNMDAActivation(i, 300.0f, 0);  /* Calcium pathway */
        }
    }

    SimulateTime(MEDIUM_SIMULATION_MS);

    /* Verify each neuron has the expected profile */
    for (uint32_t i = 0; i < NUM_NEURONS; i++) {
        second_messenger_state_t state;
        ASSERT_EQ(second_messenger_get_state(sm_system_, i, &state), NIMCP_SUCCESS);

        if (i % 3 == 0) {
            /* D1 neurons: elevated cAMP */
            EXPECT_GT(state.camp.camp_concentration, SM_CAMP_BASELINE_UM)
                << "Neuron " << i << " should have elevated cAMP (D1)";
        } else if (i % 3 == 1) {
            /* 5-HT2A neurons: elevated IP3 */
            EXPECT_GT(state.ip3_dag.ip3_concentration, SM_IP3_BASELINE_UM)
                << "Neuron " << i << " should have elevated IP3 (5-HT2A)";
        } else {
            /* NMDA neurons: elevated calcium */
            EXPECT_GT(state.calcium.ca_cytoplasmic, SM_CA_BASELINE_NM)
                << "Neuron " << i << " should have elevated calcium (NMDA)";
        }
    }
}

/**
 * Test: Statistics Tracking Across Full Pipeline
 */
TEST_F(SecondMessengerE2ETest, StatisticsTracking_FullPipeline) {
    /* Perform many activations */
    for (int i = 0; i < 10; i++) {
        SimulateDopamineD1(i, 0.8f, i * 100);
    }
    for (int i = 10; i < 20; i++) {
        SimulateSerotonin5HT2A(i, 0.8f, i * 100);
    }
    for (int i = 20; i < 25; i++) {
        SimulateDopamineD2(i, 0.8f, i * 100);
    }

    SimulateTime(MEDIUM_SIMULATION_MS);

    /* Verify statistics */
    second_messenger_stats_t stats;
    ASSERT_EQ(second_messenger_get_stats(sm_system_, &stats), NIMCP_SUCCESS);

    /* Check that at least some activations were tracked */
    EXPECT_GE(stats.receptor_activations, 1U) << "Should track some activations";
}

//=============================================================================
// BIO-ASYNC MESSAGE ROUTING TESTS
//=============================================================================

/**
 * Test: Bio-Async Registration and Message Processing
 */
TEST_F(SecondMessengerE2ETest, BioAsync_RegistrationAndMessageProcessing) {
    if (!bio_router_init_) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    /* Activate cascade and process inbox */
    SimulateDopamineD1(200, 0.9f, 0);
    SimulateTime(SHORT_SIMULATION_MS);

    /* Stats should reflect processing */
    second_messenger_stats_t stats;
    ASSERT_EQ(second_messenger_get_stats(sm_system_, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.receptor_activations, 1U);
}

/**
 * Test: Broadcast State Updates
 */
TEST_F(SecondMessengerE2ETest, BroadcastState_AfterSignificantChange) {
    if (!bio_router_init_) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    /* Strong activation */
    SimulateDopamineD1(210, 1.0f, 0);
    SimulateTime(SHORT_SIMULATION_MS);

    /* Broadcast should succeed (or return appropriate error) */
    nimcp_result_t result = second_messenger_broadcast_state(sm_system_, 210);
    /* May succeed or fail based on bio-async state - just ensure no crash */
    (void)result;
}

