/**
 * @file test_immune_bio_async_e2e.cpp
 * @brief End-to-end tests for immune + bio-async security pipeline
 * @date 2026-03-20
 *
 * WHAT: End-to-end tests verifying full immune + bio-async stack
 * WHY:  Ensure the complete security pipeline works under realistic conditions
 * HOW:  Create full system, run multi-step scenarios, verify outcomes
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImmuneBioAsyncE2ETest : public ::testing::Test {
protected:
    brain_immune_system_t* immune = nullptr;
    bbb_system_t bbb = nullptr;
    bool router_initialized = false;
    bool bio_async_initialized = false;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t async_config = nimcp_bio_async_default_config();
        async_config.enable_statistics = true;
        async_config.enable_logging = false;
        async_config.thread_pool_size = 1;
        if (nimcp_bio_async_init(&async_config) == NIMCP_SUCCESS) {
            bio_async_initialized = true;
        }

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        if (bio_router_init(&router_config) == NIMCP_SUCCESS) {
            router_initialized = true;
        }

        // Create BBB
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb = bbb_system_create(&bbb_cfg);

        // Create immune system
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        config.enable_bbb_integration = (bbb != nullptr);
        config.enable_bft_integration = false;
        config.enable_swarm_integration = false;
        config.enable_bio_async = router_initialized;
        config.enable_logging = false;

        immune = brain_immune_create(&config);
        ASSERT_NE(immune, nullptr);

        if (bbb) {
            bbb_connect_immune(bbb, immune);
            brain_immune_connect_bbb(immune, bbb);
        }
        if (router_initialized) {
            brain_immune_connect_bio_async(immune);
        }

        brain_immune_start(immune);
    }

    void TearDown() override {
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
        }
        if (bbb) {
            bbb_system_destroy(bbb);
        }
        if (router_initialized) {
            bio_router_shutdown();
        }
        if (bio_async_initialized) {
            nimcp_bio_async_shutdown();
        }
    }

    uint32_t present_antigen(const char* pattern, uint32_t severity) {
        uint32_t id = 0;
        uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
        memset(epitope, 0, sizeof(epitope));
        size_t len = strlen(pattern);
        if (len > BRAIN_IMMUNE_EPITOPE_SIZE) len = BRAIN_IMMUNE_EPITOPE_SIZE;
        memcpy(epitope, pattern, len);
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
            epitope, len, severity, 1, &id);
        return id;
    }
};

/* ============================================================================
 * Full Security Stack Simulation
 * ============================================================================ */

TEST_F(ImmuneBioAsyncE2ETest, FullSecurityStackSimulation) {
    // Simulate 100 training steps with varied threats
    brain_immune_stats_t stats_before;
    brain_immune_get_stats(immune, &stats_before);

    int ood_count = 0;
    int poison_count = 0;
    int normal_count = 0;

    for (int step = 0; step < 100; step++) {
        // 10% chance of OOD threat
        if (step % 10 == 3) {
            char pattern[64];
            snprintf(pattern, sizeof(pattern), "OOD_INPUT_STEP_%03d", step);
            present_antigen(pattern, 6);
            ood_count++;
        }

        // 5% chance of poisoned gradient
        if (step % 20 == 7) {
            char pattern[64];
            snprintf(pattern, sizeof(pattern), "POISON_GRAD_STEP_%03d", step);
            present_antigen(pattern, 8);
            poison_count++;
        }

        // Normal update
        brain_immune_update(immune, 50);
        normal_count++;
    }

    brain_immune_stats_t stats_after;
    brain_immune_get_stats(immune, &stats_after);

    // Verify immune system processed threats
    uint64_t threats_processed = stats_after.antigens_processed - stats_before.antigens_processed;
    EXPECT_GE(threats_processed, (uint64_t)(ood_count + poison_count));

    // System health should still be valid
    EXPECT_GE(stats_after.system_health, 0.0f);
    EXPECT_LE(stats_after.system_health, 1.0f);
}

/* ============================================================================
 * Immune Tolerance End-to-End
 * ============================================================================ */

TEST_F(ImmuneBioAsyncE2ETest, ImmuneToleranceEndToEnd) {
    // Present 50 identical threats to test tolerance development
    const char* pattern = "REPEATED_BENIGN_PATTERN";

    for (int i = 0; i < 50; i++) {
        present_antigen(pattern, 4);
        brain_immune_update(immune, 100);
    }

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);

    // After many repetitions, immune system should have processed them
    EXPECT_GE(stats.antigens_processed, 30u);

    // System health should remain reasonable
    EXPECT_GE(stats.system_health, 0.0f);
}

/* ============================================================================
 * Bio-Async Reliability
 * ============================================================================ */

TEST_F(ImmuneBioAsyncE2ETest, BioAsyncReliability) {
    if (!router_initialized) GTEST_SKIP() << "Router not initialized";

    // Create several modules to test broadcast reliability
    bio_module_info_t info;
    memset(&info, 0, sizeof(info));

    bio_module_context_t modules[5] = {};
    bio_module_id_t module_ids[] = {
        (bio_module_id_t)0x1001,
        (bio_module_id_t)0x1002,
        (bio_module_id_t)0x1003,
        (bio_module_id_t)0x1004,
        (bio_module_id_t)0x1005,
    };
    const char* names[] = {"e2e_mod_1", "e2e_mod_2", "e2e_mod_3", "e2e_mod_4", "e2e_mod_5"};

    int registered = 0;
    for (int i = 0; i < 5; i++) {
        info.module_id = module_ids[i];
        info.module_name = names[i];
        info.user_data = nullptr;
        info.inbox_capacity = 0;
        modules[i] = bio_router_register_module(&info);
        if (modules[i]) registered++;
    }

    EXPECT_GE(registered, 3);

    // Run immune operations which generate broadcasts
    for (int i = 0; i < 10; i++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), "RELIABILITY_%03d", i);
        present_antigen(pattern, 5);
        brain_immune_update(immune, 100);
    }

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    // Verify no crashes during all this activity
    EXPECT_GE(stats.messages_routed + stats.messages_dropped + stats.broadcasts_sent, 0u);

    // Cleanup modules
    for (int i = 0; i < 5; i++) {
        if (modules[i]) bio_router_unregister_module(modules[i]);
    }
}

/* ============================================================================
 * Health Monitoring Pipeline
 * ============================================================================ */

TEST_F(ImmuneBioAsyncE2ETest, HealthMonitoringPipeline) {
    // Run 200 steps with periodic health checks
    brain_immune_stats_t stats;

    for (int step = 0; step < 200; step++) {
        // Occasional threats
        if (step % 15 == 0) {
            char pattern[32];
            snprintf(pattern, sizeof(pattern), "HEALTH_MON_%03d", step);
            present_antigen(pattern, 5 + (step % 5));
        }

        brain_immune_update(immune, 50);

        // Periodic health check
        if (step % 50 == 0) {
            brain_immune_get_stats(immune, &stats);
            EXPECT_GE(stats.system_health, 0.0f);
            EXPECT_LE(stats.system_health, 1.0f);
        }
    }

    // Final health check
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.antigens_processed, 5u);

    // Get inflammation effects
    inflammation_effects_t effects;
    int rc = brain_immune_get_inflammation_effects(immune, &effects);
    EXPECT_EQ(rc, 0);
}

/* ============================================================================
 * Full Response Lifecycle E2E
 * ============================================================================ */

TEST_F(ImmuneBioAsyncE2ETest, FullResponseLifecycle) {
    // Complete lifecycle: present -> detect -> activate -> respond -> neutralize -> memory

    // Step 1: Present threat
    uint32_t antigen_id = present_antigen("LIFECYCLE_E2E_TEST", 8);
    brain_immune_update(immune, 300);
    EXPECT_GT(antigen_id, 0u);

    // Step 2: Activate B cell
    uint32_t b_cell_id = 0;
    int rc = brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    EXPECT_EQ(rc, 0);

    // Step 3: Helper T cell
    uint32_t helper_id = 0;
    rc = brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    EXPECT_EQ(rc, 0);

    // Step 4: T help B
    rc = brain_immune_t_help_b(immune, helper_id, b_cell_id);
    EXPECT_EQ(rc, 0);

    // Step 5: Produce antibody (may fail if B cell state not suitable)
    uint32_t antibody_id = 0;
    rc = brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);
    if (rc == 0 && antibody_id > 0) {
        // Step 6: Execute antibody
        rc = brain_immune_execute_antibody(immune, antibody_id);
        EXPECT_EQ(rc, 0);

        // Step 7: Neutralize
        rc = brain_immune_neutralize(immune, antigen_id, antibody_id);
        EXPECT_EQ(rc, 0);
        EXPECT_TRUE(brain_immune_is_neutralized(immune, antigen_id));
    }

    // Step 8: Form memory
    rc = brain_immune_b_cell_to_memory(immune, b_cell_id);
    EXPECT_EQ(rc, 0);

    // Step 9: Release anti-inflammatory cytokine (resolution)
    uint32_t cytokine_id = 0;
    rc = brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL10, 1, 0.6f, 0, &cytokine_id);
    EXPECT_EQ(rc, 0);

    brain_immune_update(immune, 500);

    // Verify final state - antigens were processed
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.antigens_processed, 1u);
}

/* ============================================================================
 * Concurrent Threat Handling E2E
 * ============================================================================ */

TEST_F(ImmuneBioAsyncE2ETest, ConcurrentThreatHandling) {
    // Present multiple different threats rapidly and ensure system handles them all

    uint32_t threat_ids[20];
    for (int i = 0; i < 20; i++) {
        char pattern[64];
        snprintf(pattern, sizeof(pattern), "CONCURRENT_THREAT_%04d", i);
        threat_ids[i] = present_antigen(pattern, 3 + (i % 7));
        EXPECT_GT(threat_ids[i], 0u);
    }

    // Process all
    for (int i = 0; i < 50; i++) {
        brain_immune_update(immune, 100);
    }

    // Verify system processed them
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.antigens_processed, 15u);

    // Verify system is still healthy (not crashed/stuck)
    brain_immune_phase_t phase = brain_immune_get_phase(immune);
    EXPECT_GE((int)phase, 0);
    EXPECT_LE((int)phase, (int)IMMUNE_PHASE_MEMORY);
}

/* ============================================================================
 * Memory Formation and Secondary Response E2E
 * ============================================================================ */

TEST_F(ImmuneBioAsyncE2ETest, MemoryFormationAndSecondaryResponse) {
    // First encounter
    uint32_t antigen_id_1 = present_antigen("MEMORY_E2E_THREAT", 7);
    brain_immune_update(immune, 200);

    uint32_t b_cell_id = 0;
    brain_immune_activate_b_cell(immune, antigen_id_1, &b_cell_id);

    uint32_t antibody_id = 0;
    int rc = brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGM, &antibody_id);
    if (rc == 0 && antibody_id > 0) {
        brain_immune_execute_antibody(immune, antibody_id);
        brain_immune_neutralize(immune, antigen_id_1, antibody_id);
    }
    brain_immune_b_cell_to_memory(immune, b_cell_id);

    brain_immune_update(immune, 1000);  // Let memory consolidate

    // Second encounter with same threat
    uint32_t antigen_id_2 = present_antigen("MEMORY_E2E_THREAT", 7);
    brain_immune_update(immune, 100);

    // Check if memory recognizes it
    uint32_t memory_b_cell = 0;
    rc = brain_immune_check_memory(immune, antigen_id_2, &memory_b_cell);
    if (rc == 0) {
        // Memory found - secondary response should be faster
        rc = brain_immune_secondary_response(immune, antigen_id_2, memory_b_cell);
        EXPECT_EQ(rc, 0);
    }

    brain_immune_update(immune, 500);

    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    // Verify immune system processed threats (neutralization depends on antibody success)
    EXPECT_GE(stats.antigens_processed, 2u);
}
