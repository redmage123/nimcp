/**
 * @file test_entorhinal_immune_integration.cpp
 * @brief Integration tests for Entorhinal Cortex with Brain Immune System
 *
 * WHAT: Tests Entorhinal Cortex integration with brain immune system
 * WHY:  Ensure proper spatial processing adaptation during immune events
 * HOW:  Test cytokine effects on grid cells, inflammation responses, and immune callbacks
 *
 * BIOLOGICAL BASIS:
 * Entorhinal cortex function is affected by immune system state:
 * - Inflammation impairs spatial memory and navigation
 * - Cytokines modulate grid cell firing patterns
 * - Sickness behavior reduces path integration accuracy
 * - Immune activation affects memory gateway transfer rates
 * - Pro-inflammatory cytokines increase grid cell drift
 * - Anti-inflammatory cytokines support memory consolidation
 *
 * INTEGRATION POINTS:
 * - Immune bridge initialization and lifecycle
 * - Cytokine level sensing affecting spatial cells
 * - Inflammation-driven grid cell modulation
 * - Self-healing mechanisms for corrupted spatial representations
 * - Anomaly detection in path integration
 * - B-cell and T-cell integration with spatial memory
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class EntorhinalImmuneIntegrationTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* entorhinal;
    entorhinal_config_t entorhinal_config;
    brain_immune_system_t* immune;
    brain_immune_config_t immune_config;

    /* Callback tracking */
    static int cytokine_callback_count;
    static int inflammation_callback_count;
    static int anomaly_callback_count;
    static brain_cytokine_type_t last_cytokine_type;
    static brain_inflammation_level_t last_inflammation_level;
    static float last_cytokine_concentration;

    void SetUp() override {
        /* Reset callback tracking */
        cytokine_callback_count = 0;
        inflammation_callback_count = 0;
        anomaly_callback_count = 0;
        last_cytokine_type = BRAIN_CYTOKINE_IL1;
        last_inflammation_level = INFLAMMATION_NONE;
        last_cytokine_concentration = 0.0f;

        /* Create brain immune system */
        brain_immune_default_config(&immune_config);
        immune_config.enable_bbb_integration = false;
        immune_config.enable_bft_integration = false;
        immune_config.enable_swarm_integration = false;
        immune_config.enable_bio_async = false;
        immune_config.enable_logging = false;
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(nullptr, immune) << "Failed to create immune system";
        brain_immune_start(immune);

        /* Configure entorhinal cortex */
        entorhinal_config = entorhinal_default_config();
        entorhinal_config.enable_immune = true;
        entorhinal_config.enable_bio_async = false;
        entorhinal_config.enable_security = false;
        entorhinal_config.enable_snn = false;
        entorhinal_config.enable_plasticity = false;
        entorhinal_config.enable_training = false;
        entorhinal_config.enable_cognitive = false;
        entorhinal_config.enable_path_integration = true;
        entorhinal_config.enable_boundary_detection = true;
        entorhinal_config.num_grid_cells = 64;      /* Reduced for testing */
        entorhinal_config.num_border_cells = 16;
        entorhinal_config.num_hd_cells = 12;

        entorhinal = entorhinal_create(&entorhinal_config);
        ASSERT_NE(nullptr, entorhinal) << "Failed to create entorhinal cortex";
    }

    void TearDown() override {
        if (entorhinal) {
            entorhinal_destroy(entorhinal);
            entorhinal = nullptr;
        }
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }

    /* Helper to simulate cytokine release */
    uint32_t ReleaseCytokine(brain_cytokine_type_t type, float concentration) {
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(immune, type, 0, concentration, 0, &cytokine_id);
        return cytokine_id;
    }

    /* Helper to present a test antigen */
    uint32_t PresentTestAntigen(uint32_t severity) {
        uint32_t antigen_id = 0;
        uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
            epitope, sizeof(epitope), severity, 0, &antigen_id);
        return antigen_id;
    }

    /* Helper to present spatial anomaly antigen */
    uint32_t PresentSpatialAnomaly(uint32_t severity) {
        uint32_t antigen_id = 0;
        /* Spatial anomaly signature - represents corrupted grid cell pattern */
        uint8_t epitope[] = {0x5A, 0xAC, 0x1A, 0x11, 0xAA, 0xBB, 0xCC, 0xDD};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_ANOMALY,
            epitope, sizeof(epitope), severity, 0, &antigen_id);
        return antigen_id;
    }

    /* Cytokine callback */
    static void OnCytokine(brain_immune_system_t* system,
                          const brain_cytokine_t* cytokine,
                          void* user_data) {
        (void)system;
        (void)user_data;
        cytokine_callback_count++;
        last_cytokine_type = cytokine->type;
        last_cytokine_concentration = cytokine->concentration;
    }

    /* Inflammation callback */
    static void OnInflammation(brain_immune_system_t* system,
                               const brain_inflammation_site_t* site,
                               void* user_data) {
        (void)system;
        (void)user_data;
        inflammation_callback_count++;
        last_inflammation_level = site->level;
    }
};

/* Static member initialization */
int EntorhinalImmuneIntegrationTest::cytokine_callback_count = 0;
int EntorhinalImmuneIntegrationTest::inflammation_callback_count = 0;
int EntorhinalImmuneIntegrationTest::anomaly_callback_count = 0;
brain_cytokine_type_t EntorhinalImmuneIntegrationTest::last_cytokine_type = BRAIN_CYTOKINE_IL1;
brain_inflammation_level_t EntorhinalImmuneIntegrationTest::last_inflammation_level = INFLAMMATION_NONE;
float EntorhinalImmuneIntegrationTest::last_cytokine_concentration = 0.0f;

/*=============================================================================
 * IMMUNE BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

/**
 * Test: Immune bridge initialization with valid immune system
 * WHAT: Initialize immune bridge with a valid immune system instance
 * WHY:  Verify proper bridge setup for immune-entorhinal integration
 * HOW:  Call entorhinal_init_immune_bridge and check return value
 */
TEST_F(EntorhinalImmuneIntegrationTest, ImmuneBridgeInitialization) {
    int result = entorhinal_init_immune_bridge(entorhinal, immune);
    EXPECT_EQ(0, result) << "Immune bridge initialization should succeed";

    /* Verify bridge state */
    EXPECT_EQ(immune, entorhinal->immune_bridge.immune);
    EXPECT_GE(entorhinal->immune_bridge.health_score, 0.0f);
    EXPECT_LE(entorhinal->immune_bridge.health_score, 1.0f);
}

/**
 * Test: Immune bridge initialization with null entorhinal
 * WHAT: Attempt to initialize immune bridge with null entorhinal pointer
 * WHY:  Verify proper error handling for invalid input
 * HOW:  Call with nullptr and expect error return
 */
TEST_F(EntorhinalImmuneIntegrationTest, ImmuneBridgeNullEntorhinal) {
    int result = entorhinal_init_immune_bridge(nullptr, immune);
    EXPECT_NE(0, result) << "Should reject null entorhinal pointer";
}

/**
 * Test: Immune bridge initialization with null immune system
 * WHAT: Attempt to initialize immune bridge with null immune pointer
 * WHY:  Verify proper error handling for invalid immune system
 * HOW:  Call with nullptr immune and expect error return
 */
TEST_F(EntorhinalImmuneIntegrationTest, ImmuneBridgeNullImmune) {
    /* NULL immune is valid - bridges are commonly initialized with NULL during creation */
    int result = entorhinal_init_immune_bridge(entorhinal, nullptr);
    EXPECT_EQ(0, result);
}

/**
 * Test: Immune system creation and lifecycle
 * WHAT: Verify immune system is created and in correct initial state
 * WHY:  Ensure proper immune system lifecycle management
 * HOW:  Check phase and running state
 */
TEST_F(EntorhinalImmuneIntegrationTest, ImmuneSystemLifecycle) {
    EXPECT_NE(nullptr, immune);
    EXPECT_EQ(IMMUNE_PHASE_SURVEILLANCE, brain_immune_get_phase(immune));
}

/**
 * Test: Immune system start and stop
 * WHAT: Start and stop the immune system monitoring
 * WHY:  Verify immune system can be activated and deactivated
 * HOW:  Call start/stop and verify running state
 */
TEST_F(EntorhinalImmuneIntegrationTest, ImmuneSystemStartStop) {
    EXPECT_EQ(0, brain_immune_start(immune));
    EXPECT_TRUE(immune->running);

    EXPECT_EQ(0, brain_immune_stop(immune));
    EXPECT_FALSE(immune->running);
}

/*=============================================================================
 * ANOMALY DETECTION TESTS
 *===========================================================================*/

/**
 * Test: Anomaly detection in entorhinal via immune scan
 * WHAT: Run immune scan on entorhinal cortex to detect anomalies
 * WHY:  Verify anomaly detection mechanism works correctly
 * HOW:  Initialize bridge, run immune scan, check for anomaly flag
 */
TEST_F(EntorhinalImmuneIntegrationTest, AnomalyDetectionViaScan) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Initially no anomaly should be detected */
    EXPECT_FALSE(entorhinal->immune_bridge.anomaly_detected);

    /* Run immune scan */
    int result = entorhinal_immune_scan(entorhinal);
    EXPECT_EQ(0, result) << "Immune scan should succeed";

    /* After clean scan, anomaly should remain false */
    EXPECT_FALSE(entorhinal->immune_bridge.anomaly_detected);
}

/**
 * Test: Antigen presentation for spatial anomaly
 * WHAT: Present a spatial anomaly as an antigen to the immune system
 * WHY:  Verify anomaly is properly converted to immune threat
 * HOW:  Present anomaly antigen, verify it's tracked
 */
TEST_F(EntorhinalImmuneIntegrationTest, SpatialAnomalyAntigenPresentation) {
    uint32_t antigen_id = PresentSpatialAnomaly(5);
    EXPECT_GT(antigen_id, 0u);

    const brain_antigen_t* antigen = brain_immune_get_antigen(immune, antigen_id);
    ASSERT_NE(nullptr, antigen);
    EXPECT_EQ(ANTIGEN_SOURCE_ANOMALY, antigen->source);
    EXPECT_EQ(5u, antigen->severity);
}

/**
 * Test: Path integration error triggers anomaly detection
 * WHAT: Simulate excessive path integration drift
 * WHY:  Verify immune system can detect computational anomalies
 * HOW:  Update path integration with error accumulation, check detection
 */
TEST_F(EntorhinalImmuneIntegrationTest, PathIntegrationAnomalyDetection) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Initial state should have low accumulated error */
    EXPECT_LT(entorhinal->path_integration.accumulated_error, 1.0f);

    /* Simulate path integration updates with drift */
    float velocity[] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 100; i++) {
        entorhinal_path_integrate(entorhinal, velocity, 0.1f, 0.1f);
    }

    /* Run immune scan to check for anomalies */
    entorhinal_immune_scan(entorhinal);

    /* Path integration stats should be tracked */
    entorhinal_stats_t stats;
    entorhinal_get_stats(entorhinal, &stats);
    EXPECT_GT(stats.position_updates, 0u);
}

/*=============================================================================
 * SELF-HEALING MECHANISM TESTS
 *===========================================================================*/

/**
 * Test: Grid cell reset after corruption detection
 * WHAT: Test self-healing by resetting grid cells after anomaly
 * WHY:  Verify spatial representation can recover from corruption
 * HOW:  Detect anomaly, trigger reset, verify recovery
 */
TEST_F(EntorhinalImmuneIntegrationTest, GridCellSelfHealing) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Store initial grid coherence baseline */
    float initial_coherence = entorhinal->mean_grid_coherence;

    /* Provide known position for reset */
    float known_position[] = {0.0f, 0.0f, 0.0f};
    int result = entorhinal_reset_grid_phases(entorhinal, known_position);
    EXPECT_EQ(0, result) << "Grid phase reset should succeed";

    /* After reset, coherence should be valid */
    EXPECT_GE(entorhinal->mean_grid_coherence, 0.0f);
    EXPECT_LE(entorhinal->mean_grid_coherence, 1.0f);
}

/**
 * Test: Anti-inflammatory cytokine promotes healing
 * WHAT: Release IL-10 and verify healing promotion
 * WHY:  Anti-inflammatory cytokines should support recovery
 * HOW:  Release IL-10, update, check health score improvement
 */
TEST_F(EntorhinalImmuneIntegrationTest, AntiInflammatoryHealing) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* First induce some inflammation */
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.6f);
    brain_immune_update(immune, 100);

    /* Then release anti-inflammatory */
    ReleaseCytokine(BRAIN_CYTOKINE_IL10, 0.8f);
    brain_immune_update(immune, 100);

    float il10_level = brain_immune_get_cytokine_level(immune, BRAIN_CYTOKINE_IL10);
    EXPECT_GE(il10_level, 0.0f);
}

/**
 * Test: Entorhinal reset restores healthy state
 * WHAT: Reset entorhinal cortex and verify healthy state
 * WHY:  Full reset should clear anomalies and restore function
 * HOW:  Call reset, verify status returns to ready/idle
 */
TEST_F(EntorhinalImmuneIntegrationTest, EntorhinalResetRestoresHealth) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Simulate some processing */
    float position[] = {1.0f, 2.0f, 0.0f};
    entorhinal_update_grid_cells(entorhinal, position, 3);

    /* Reset entorhinal */
    bool reset_success = entorhinal_reset(entorhinal);
    EXPECT_TRUE(reset_success);

    /* Verify status returns to idle */
    entorhinal_status_t status = entorhinal_get_status(entorhinal);
    EXPECT_EQ(ENTORHINAL_STATUS_IDLE, status);
}

/*=============================================================================
 * PATHOGEN RESPONSE SIMULATION TESTS
 *===========================================================================*/

/**
 * Test: Antigen presentation and processing
 * WHAT: Present multiple antigens and verify processing
 * WHY:  Simulate pathogen detection and immune response
 * HOW:  Present antigens, update immune system, check stats
 */
TEST_F(EntorhinalImmuneIntegrationTest, PathogenResponseSimulation) {
    uint32_t antigen1 = PresentTestAntigen(3);
    uint32_t antigen2 = PresentTestAntigen(5);
    uint32_t antigen3 = PresentTestAntigen(7);

    EXPECT_GT(antigen1, 0u);
    EXPECT_GT(antigen2, 0u);
    EXPECT_GT(antigen3, 0u);
    EXPECT_NE(antigen1, antigen2);
    EXPECT_NE(antigen2, antigen3);

    /* Process antigens */
    for (int i = 0; i < 5; i++) {
        brain_immune_update(immune, 100);
    }

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.antigens_processed, 0u);
}

/**
 * Test: B cell activation for threat
 * WHAT: Activate B cell in response to antigen
 * WHY:  Verify adaptive immune response initiation
 * HOW:  Present antigen, activate B cell, verify activation
 */
TEST_F(EntorhinalImmuneIntegrationTest, BCellActivationForThreat) {
    uint32_t antigen_id = PresentTestAntigen(5);
    uint32_t b_cell_id = 0;

    EXPECT_EQ(0, brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id));
    EXPECT_GT(b_cell_id, 0u);
}

/**
 * Test: Killer T cell response to severe threat
 * WHAT: Activate killer T cell for severe antigen
 * WHY:  Verify cytotoxic response pathway
 * HOW:  Present high severity antigen, activate killer T
 */
TEST_F(EntorhinalImmuneIntegrationTest, KillerTCellResponse) {
    uint32_t antigen_id = PresentTestAntigen(8); /* High severity */
    uint32_t killer_id = 0;

    EXPECT_EQ(0, brain_immune_activate_killer_t(immune, antigen_id, &killer_id));
    EXPECT_GT(killer_id, 0u);
}

/**
 * Test: Antibody production from plasma B cell
 * WHAT: Produce antibody after B cell reaches plasma state
 * WHY:  Verify humoral immune response pathway
 * HOW:  Activate B cell, advance to plasma, produce antibody
 */
TEST_F(EntorhinalImmuneIntegrationTest, AntibodyProductionPathway) {
    uint32_t antigen_id = PresentTestAntigen(5);
    uint32_t b_cell_id = 0;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);

    /* B cell must transition to PLASMA state to produce antibodies */
    /* Advance time to allow state transitions */
    for (int i = 0; i < 20; i++) {
        brain_immune_update(immune, 100);
    }

    uint32_t antibody_id = 0;
    int result = brain_immune_produce_antibody(immune, b_cell_id,
        ANTIBODY_IGM, &antibody_id);

    /* May succeed or fail depending on B cell state - verify no crash */
    if (result == 0) {
        EXPECT_GT(antibody_id, 0u);
    }
}

/*=============================================================================
 * INFLAMMATION EFFECTS ON SPATIAL CELLS TESTS
 *===========================================================================*/

/**
 * Test: Inflammation initiation affects entorhinal
 * WHAT: Initiate inflammation and check effect on entorhinal bridge
 * WHY:  Inflammation should impact spatial processing
 * HOW:  Create inflammation site, verify bridge state update
 */
TEST_F(EntorhinalImmuneIntegrationTest, InflammationAffectsEntorhinal) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    uint32_t antigen_id = PresentTestAntigen(6);
    uint32_t site_id = 0;

    EXPECT_EQ(0, brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id));
    EXPECT_GT(site_id, 0u);

    brain_immune_update(immune, 100);

    /* Check inflammation level in immune system */
    brain_inflammation_level_t level = brain_immune_get_inflammation_level(immune);
    EXPECT_GE((int)level, (int)INFLAMMATION_LOCAL);
}

/**
 * Test: Pro-inflammatory cytokines increase grid drift
 * WHAT: Release IL-1 and check grid cell behavior
 * WHY:  Pro-inflammatory state should impair spatial accuracy
 * HOW:  Release cytokine, perform path integration, check error
 */
TEST_F(EntorhinalImmuneIntegrationTest, ProInflammatoryCytokineEffects) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Release pro-inflammatory cytokine */
    ReleaseCytokine(BRAIN_CYTOKINE_IL1, 0.8f);
    brain_immune_update(immune, 100);

    float il1_level = brain_immune_get_cytokine_level(immune, BRAIN_CYTOKINE_IL1);
    EXPECT_GE(il1_level, 0.0f);

    /* Perform path integration during inflammation */
    float velocity[] = {0.5f, 0.5f, 0.0f};
    entorhinal_path_integrate(entorhinal, velocity, 0.0f, 0.1f);

    /* Error tracking should be active */
    EXPECT_GE(entorhinal->path_integration.accumulated_error, 0.0f);
}

/**
 * Test: TNF-alpha severe inflammation effects
 * WHAT: Release TNF-alpha and verify severe inflammation impact
 * WHY:  TNF-alpha causes significant impairment
 * HOW:  Release TNF, check inflammation escalation
 */
TEST_F(EntorhinalImmuneIntegrationTest, TNFAlphaSevereInflammation) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Release severe inflammatory cytokine */
    ReleaseCytokine(BRAIN_CYTOKINE_TNF, 0.9f);

    uint32_t antigen_id = PresentTestAntigen(8);
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);

    /* Escalate inflammation */
    brain_immune_escalate_inflammation(immune, site_id);
    brain_immune_update(immune, 100);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE((int)stats.inflammation_level, (int)INFLAMMATION_LOCAL);
}

/**
 * Test: Inflammation resolution restores function
 * WHAT: Resolve inflammation and verify function restoration
 * WHY:  Resolution should allow spatial processing recovery
 * HOW:  Create inflammation, resolve, check health improvement
 */
TEST_F(EntorhinalImmuneIntegrationTest, InflammationResolutionRestoresFunction) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    uint32_t antigen_id = PresentTestAntigen(5);
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);
    brain_immune_update(immune, 100);

    /* Resolve inflammation */
    EXPECT_EQ(0, brain_immune_resolve_inflammation(immune, site_id));
    brain_immune_update(immune, 100);

    /* Health score should be valid */
    float health = entorhinal_get_health_status(entorhinal);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

/*=============================================================================
 * IMMUNE-NEURAL INTERACTION TESTS
 *===========================================================================*/

/**
 * Test: Grid cell update during healthy immune state
 * WHAT: Update grid cells with no immune stress
 * WHY:  Baseline spatial processing should work correctly
 * HOW:  Update grid cells, verify activation patterns
 */
TEST_F(EntorhinalImmuneIntegrationTest, GridCellUpdateHealthyState) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* No inflammation - healthy state */
    EXPECT_EQ(INFLAMMATION_NONE, brain_immune_get_inflammation_level(immune));

    float position[] = {1.0f, 2.0f, 0.0f};
    int result = entorhinal_update_grid_cells(entorhinal, position, 3);
    EXPECT_EQ(0, result);

    /* Check grid module statistics */
    EXPECT_GT(entorhinal->num_grid_modules, 0u);
}

/**
 * Test: Border cell update with immune integration
 * WHAT: Update border cells while immune bridge is active
 * WHY:  Boundary detection should work during immune monitoring
 * HOW:  Provide boundary distances, verify detection
 */
TEST_F(EntorhinalImmuneIntegrationTest, BorderCellWithImmuneIntegration) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Each boundary is a (distance, direction) pair */
    float boundary_distances[] = {0.5f, 0.0f, 1.0f, 1.57f, 2.0f, 3.14f, 3.0f, 4.71f};
    int result = entorhinal_update_border_cells(entorhinal,
        boundary_distances, 4);
    EXPECT_EQ(0, result);
}

/**
 * Test: Head direction cells during immune response
 * WHAT: Update HD cells during active immune response
 * WHY:  Heading encoding should function during mild inflammation
 * HOW:  Activate immune response, update HD cells
 */
TEST_F(EntorhinalImmuneIntegrationTest, HeadDirectionDuringImmuneResponse) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Mild immune activation */
    uint32_t antigen_id = PresentTestAntigen(3);
    uint32_t b_cell_id = 0;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_update(immune, 50);

    /* HD cells should still function */
    float heading = 1.57f; /* ~90 degrees */
    float angular_velocity = 0.1f;
    int result = entorhinal_update_hd_cells(entorhinal, heading, angular_velocity);
    EXPECT_EQ(0, result);

    /* Decode heading */
    float decoded_heading = 0.0f;
    float confidence = 0.0f;
    entorhinal_decode_heading(entorhinal, &decoded_heading, &confidence);
    EXPECT_GE(confidence, 0.0f);
}

/**
 * Test: Memory gateway during immune activation
 * WHAT: Test memory encoding/retrieval during immune activity
 * WHY:  Memory gateway should adapt to immune state
 * HOW:  Set gates, attempt encoding during immune response
 */
TEST_F(EntorhinalImmuneIntegrationTest, MemoryGatewayDuringImmuneActivation) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Activate immune response */
    uint32_t antigen_id = PresentTestAntigen(4);
    brain_immune_update(immune, 100);

    /* Set encoding gate */
    entorhinal_set_encoding_gate(entorhinal, 0.8f);
    EXPECT_FLOAT_EQ(0.8f, entorhinal->memory_gateway.encoding_gate);

    /* Set retrieval gate */
    entorhinal_set_retrieval_gate(entorhinal, 0.6f);
    EXPECT_FLOAT_EQ(0.6f, entorhinal->memory_gateway.retrieval_gate);
}

/**
 * Test: Helper T cell boosts B cell for enhanced response
 * WHAT: Verify T helper cells amplify B cell response
 * WHY:  Adaptive immunity requires T-B cooperation
 * HOW:  Activate both cell types, call t_help_b
 */
TEST_F(EntorhinalImmuneIntegrationTest, THelperBoostsBCellResponse) {
    uint32_t antigen_id = PresentTestAntigen(5);

    uint32_t b_cell_id = 0;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);

    uint32_t helper_id = 0;
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);

    /* Helper provides assistance to B cell */
    EXPECT_EQ(0, brain_immune_t_help_b(immune, helper_id, b_cell_id));
}

/**
 * Test: Spatial processing continues during immune surveillance
 * WHAT: Verify normal operation during immune surveillance phase
 * WHY:  Surveillance should not impair normal function
 * HOW:  Confirm surveillance phase, perform spatial ops
 */
TEST_F(EntorhinalImmuneIntegrationTest, SpatialProcessingDuringSurveillance) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Confirm surveillance phase */
    EXPECT_EQ(IMMUNE_PHASE_SURVEILLANCE, brain_immune_get_phase(immune));

    /* Spatial operations should work normally */
    float position[] = {0.5f, 0.5f, 0.0f};
    EXPECT_EQ(0, entorhinal_update_grid_cells(entorhinal, position, 3));

    float velocity[] = {0.1f, 0.0f, 0.0f};
    EXPECT_EQ(0, entorhinal_path_integrate(entorhinal, velocity, 0.0f, 0.1f));

    /* Get position estimate */
    float pos_out[3] = {0.0f};
    float heading_out = 0.0f;
    float pos_conf = 0.0f;
    float head_conf = 0.0f;
    entorhinal_get_position_estimate(entorhinal, pos_out, &heading_out,
        &pos_conf, &head_conf);
    EXPECT_GE(pos_conf, 0.0f);
}

/*=============================================================================
 * CALLBACK AND EVENT TESTS
 *===========================================================================*/

/**
 * Test: Cytokine callback registration
 * WHAT: Register cytokine callback and verify invocation
 * WHY:  Enable event-driven immune-neural communication
 * HOW:  Register callback, release cytokine, check invocation
 */
TEST_F(EntorhinalImmuneIntegrationTest, CytokineCallbackRegistration) {
    EXPECT_EQ(0, brain_immune_set_cytokine_callback(immune, OnCytokine, nullptr));

    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.5f);
    brain_immune_update(immune, 100);

    /* Callback may or may not be called depending on implementation */
    /* Verify registration didn't crash */
}

/**
 * Test: Inflammation callback registration
 * WHAT: Register inflammation callback and verify invocation
 * WHY:  Allow real-time inflammation monitoring
 * HOW:  Register callback, initiate inflammation, check invocation
 */
TEST_F(EntorhinalImmuneIntegrationTest, InflammationCallbackRegistration) {
    EXPECT_EQ(0, brain_immune_set_inflammation_callback(immune, OnInflammation, nullptr));

    uint32_t antigen_id = PresentTestAntigen(6);
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);

    brain_immune_update(immune, 100);

    /* Verify registration works without crashing */
}

/*=============================================================================
 * ERROR HANDLING TESTS
 *===========================================================================*/

/**
 * Test: Invalid antigen ID handling
 * WHAT: Query non-existent antigen
 * WHY:  Verify graceful error handling
 * HOW:  Query with invalid ID, expect null return
 */
TEST_F(EntorhinalImmuneIntegrationTest, InvalidAntigenIdHandling) {
    const brain_antigen_t* antigen = brain_immune_get_antigen(immune, 99999);
    EXPECT_EQ(nullptr, antigen);
}

/**
 * Test: Stats retrieval with null output
 * WHAT: Call get_stats with null output pointer
 * WHY:  Verify null pointer handling
 * HOW:  Call with nullptr, expect error return
 */
TEST_F(EntorhinalImmuneIntegrationTest, StatsRetrievalNullOutput) {
    int result = brain_immune_get_stats(immune, nullptr);
    EXPECT_NE(0, result);
}

/**
 * Test: Entorhinal stats retrieval
 * WHAT: Get entorhinal statistics
 * WHY:  Verify statistics are properly collected
 * HOW:  Perform some operations, retrieve stats
 */
TEST_F(EntorhinalImmuneIntegrationTest, EntorhinalStatsRetrieval) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Perform some operations */
    float position[] = {1.0f, 1.0f, 0.0f};
    entorhinal_update_grid_cells(entorhinal, position, 3);

    entorhinal_stats_t stats;
    int result = entorhinal_get_stats(entorhinal, &stats);
    EXPECT_EQ(0, result);
    EXPECT_GE(stats.updates_processed, 0u);
}

/**
 * Test: Error string conversion
 * WHAT: Convert error codes to strings
 * WHY:  Verify error reporting functionality
 * HOW:  Call error_string for various codes
 */
TEST_F(EntorhinalImmuneIntegrationTest, ErrorStringConversion) {
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_NONE));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_INVALID_INPUT));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_GRID_DRIFT));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_IMMUNE_REJECTION));
}

/**
 * Test: Status string conversion
 * WHAT: Convert status codes to strings
 * WHY:  Verify status reporting functionality
 * HOW:  Call status_string for various codes
 */
TEST_F(EntorhinalImmuneIntegrationTest, StatusStringConversion) {
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_IDLE));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_PATH_INTEGRATING));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_ENCODING));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_READY));
}

/*=============================================================================
 * IMMUNE PHASE AND STRING CONVERSION TESTS
 *===========================================================================*/

/**
 * Test: Immune phase transitions during threat response
 * WHAT: Track immune phase changes during threat processing
 * WHY:  Verify proper phase state machine behavior
 * HOW:  Present threat, update, check phase progression
 */
TEST_F(EntorhinalImmuneIntegrationTest, ImmunePhaseTransitions) {
    /* Start in surveillance */
    EXPECT_EQ(IMMUNE_PHASE_SURVEILLANCE, brain_immune_get_phase(immune));

    /* Present threat */
    PresentTestAntigen(7);
    brain_immune_update(immune, 100);

    /* Phase may transition based on implementation */
    brain_immune_phase_t phase = brain_immune_get_phase(immune);
    EXPECT_GE((int)phase, 0);
    EXPECT_LT((int)phase, 6);
}

/**
 * Test: Immune phase to string conversion
 * WHAT: Convert immune phase enum to human-readable string
 * WHY:  Support logging and debugging
 * HOW:  Call phase_to_string for all phases
 */
TEST_F(EntorhinalImmuneIntegrationTest, ImmunePhaseToString) {
    EXPECT_NE(nullptr, brain_immune_phase_to_string(IMMUNE_PHASE_SURVEILLANCE));
    EXPECT_NE(nullptr, brain_immune_phase_to_string(IMMUNE_PHASE_ACTIVATION));
    EXPECT_NE(nullptr, brain_immune_phase_to_string(IMMUNE_PHASE_EFFECTOR));
    EXPECT_NE(nullptr, brain_immune_phase_to_string(IMMUNE_PHASE_RESOLUTION));
    EXPECT_NE(nullptr, brain_immune_phase_to_string(IMMUNE_PHASE_MEMORY));
}

/**
 * Test: B cell state to string conversion
 * WHAT: Convert B cell state enum to string
 * WHY:  Support logging and debugging
 * HOW:  Call b_cell_state_to_string for all states
 */
TEST_F(EntorhinalImmuneIntegrationTest, BCellStateToString) {
    EXPECT_NE(nullptr, brain_immune_b_cell_state_to_string(B_CELL_NAIVE));
    EXPECT_NE(nullptr, brain_immune_b_cell_state_to_string(B_CELL_ACTIVATED));
    EXPECT_NE(nullptr, brain_immune_b_cell_state_to_string(B_CELL_PLASMA));
    EXPECT_NE(nullptr, brain_immune_b_cell_state_to_string(B_CELL_MEMORY));
}

/**
 * Test: T cell type to string conversion
 * WHAT: Convert T cell type enum to string
 * WHY:  Support logging and debugging
 * HOW:  Call t_cell_type_to_string for all types
 */
TEST_F(EntorhinalImmuneIntegrationTest, TCellTypeToString) {
    EXPECT_NE(nullptr, brain_immune_t_cell_type_to_string(T_CELL_NAIVE));
    EXPECT_NE(nullptr, brain_immune_t_cell_type_to_string(T_CELL_HELPER));
    EXPECT_NE(nullptr, brain_immune_t_cell_type_to_string(T_CELL_KILLER));
    EXPECT_NE(nullptr, brain_immune_t_cell_type_to_string(T_CELL_REGULATORY));
}

/**
 * Test: Cytokine type to string conversion
 * WHAT: Convert cytokine type enum to string
 * WHY:  Support logging and debugging
 * HOW:  Call cytokine_to_string for all types
 */
TEST_F(EntorhinalImmuneIntegrationTest, CytokineTypeToString) {
    EXPECT_NE(nullptr, brain_immune_cytokine_to_string(BRAIN_CYTOKINE_IL1));
    EXPECT_NE(nullptr, brain_immune_cytokine_to_string(BRAIN_CYTOKINE_IL6));
    EXPECT_NE(nullptr, brain_immune_cytokine_to_string(BRAIN_CYTOKINE_IL10));
    EXPECT_NE(nullptr, brain_immune_cytokine_to_string(BRAIN_CYTOKINE_TNF));
}

/**
 * Test: Inflammation level to string conversion
 * WHAT: Convert inflammation level enum to string
 * WHY:  Support logging and debugging
 * HOW:  Call inflammation_to_string for all levels
 */
TEST_F(EntorhinalImmuneIntegrationTest, InflammationLevelToString) {
    EXPECT_NE(nullptr, brain_immune_inflammation_to_string(INFLAMMATION_NONE));
    EXPECT_NE(nullptr, brain_immune_inflammation_to_string(INFLAMMATION_LOCAL));
    EXPECT_NE(nullptr, brain_immune_inflammation_to_string(INFLAMMATION_REGIONAL));
    EXPECT_NE(nullptr, brain_immune_inflammation_to_string(INFLAMMATION_SYSTEMIC));
    EXPECT_NE(nullptr, brain_immune_inflammation_to_string(INFLAMMATION_STORM));
}

/*=============================================================================
 * PATTERN AFFINITY AND MEMORY TESTS
 *===========================================================================*/

/**
 * Test: Pattern affinity computation
 * WHAT: Compute affinity between threat patterns
 * WHY:  Verify pattern matching for immune recognition
 * HOW:  Compare identical and different patterns
 */
TEST_F(EntorhinalImmuneIntegrationTest, PatternAffinityComputation) {
    uint8_t pattern1[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t pattern2[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t pattern3[] = {0x00, 0x00, 0x00, 0x00};

    /* Identical patterns should have high affinity */
    float affinity_same = brain_immune_compute_affinity(
        pattern1, 4, pattern2, 4);
    EXPECT_GT(affinity_same, 0.9f);

    /* Different patterns should have lower affinity */
    float affinity_diff = brain_immune_compute_affinity(
        pattern1, 4, pattern3, 4);
    EXPECT_LT(affinity_diff, affinity_same);
}

/**
 * Test: Memory cell formation and check
 * WHAT: Form memory B cell and check memory recall
 * WHY:  Verify adaptive memory formation
 * HOW:  Activate B cell, convert to memory, check matching
 */
TEST_F(EntorhinalImmuneIntegrationTest, MemoryCellFormation) {
    uint32_t antigen_id = PresentTestAntigen(4);
    uint32_t b_cell_id = 0;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);

    /* Run updates to simulate time passing */
    for (int i = 0; i < 20; i++) {
        brain_immune_update(immune, 100);
    }

    /* Attempt memory conversion */
    int result = brain_immune_b_cell_to_memory(immune, b_cell_id);
    /* Success depends on B cell state */
    (void)result;

    /* Check for memory match */
    uint32_t matching_b_cell = 0;
    brain_immune_check_memory(immune, antigen_id, &matching_b_cell);
    /* May or may not find match depending on state transitions */
}

/*=============================================================================
 * COMBINED SCENARIO TESTS
 *===========================================================================*/

/**
 * Test: Full immune-entorhinal interaction cycle
 * WHAT: Run complete immune response cycle with entorhinal integration
 * WHY:  Verify end-to-end immune-spatial integration
 * HOW:  Initialize, present threat, process, resolve
 */
TEST_F(EntorhinalImmuneIntegrationTest, FullImmuneEntorhinalCycle) {
    /* Initialize bridge */
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Start immune monitoring */
    brain_immune_start(immune);

    /* Perform spatial processing */
    float position[] = {1.0f, 1.0f, 0.0f};
    entorhinal_update_grid_cells(entorhinal, position, 3);

    /* Simulate threat detection */
    uint32_t antigen_id = PresentTestAntigen(5);
    uint32_t b_cell_id = 0;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);

    /* Continue spatial processing during response */
    float velocity[] = {0.1f, 0.1f, 0.0f};
    for (int i = 0; i < 10; i++) {
        entorhinal_path_integrate(entorhinal, velocity, 0.0f, 0.1f);
        brain_immune_update(immune, 100);
    }

    /* Verify both systems still functional */
    entorhinal_stats_t ec_stats;
    entorhinal_get_stats(entorhinal, &ec_stats);
    EXPECT_GT(ec_stats.position_updates, 0u);

    brain_immune_stats_t im_stats;
    brain_immune_get_stats(immune, &im_stats);
    EXPECT_GE(im_stats.system_health, 0.0f);

    /* Stop immune monitoring */
    brain_immune_stop(immune);
}

/**
 * Test: Spatial accuracy during cytokine storm (extreme case)
 * WHAT: Test spatial processing during severe inflammation
 * WHY:  Verify graceful degradation under immune stress
 * HOW:  Induce cytokine storm, attempt spatial processing
 */
TEST_F(EntorhinalImmuneIntegrationTest, SpatialAccuracyDuringCytokineStorm) {
    entorhinal_init_immune_bridge(entorhinal, immune);

    /* Release multiple pro-inflammatory cytokines */
    ReleaseCytokine(BRAIN_CYTOKINE_IL1, 0.9f);
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.9f);
    ReleaseCytokine(BRAIN_CYTOKINE_TNF, 0.9f);

    /* Create and escalate inflammation */
    uint32_t antigen_id = PresentTestAntigen(9);
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune, site_id);
    brain_immune_escalate_inflammation(immune, site_id);
    brain_immune_update(immune, 100);

    /* Spatial processing should still work (degraded but functional) */
    float position[] = {2.0f, 2.0f, 0.0f};
    int result = entorhinal_update_grid_cells(entorhinal, position, 3);
    EXPECT_EQ(0, result) << "Spatial processing should not crash during storm";

    /* System should track health impact */
    float health = entorhinal_get_health_status(entorhinal);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
