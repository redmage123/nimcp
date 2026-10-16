/**
 * @file test_code_immune.cpp
 * @brief Unit tests for Code Immune System (Self-Healing)
 * @version 1.0.0
 * @date 2025-12-27
 *
 * Tests for the code immune system including:
 * - Lifecycle (create, destroy, start, stop)
 * - Crash antigen presentation
 * - B cell recognition and activation
 * - Antibody production and application
 * - Memory formation
 * - Pattern affinity computation
 * - Brain immune integration
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_code_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class CodeImmuneTest : public ::testing::Test {
protected:
    code_immune_system_t* system = nullptr;
    code_immune_config_t config;

    void SetUp() override {
        code_immune_default_config(&config);
        /* Disable logging for cleaner test output */
        config.enable_logging = false;
        system = code_immune_create_with_config(nullptr, &config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            code_immune_destroy(system);
            system = nullptr;
        }
    }
};

/* Test fixture with brain immune integration */
class CodeImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* brain_immune = nullptr;
    code_immune_system_t* code_immune = nullptr;
    code_immune_config_t config;

    void SetUp() override {
        /* Create brain immune first */
        brain_immune_config_t bi_config;
        brain_immune_default_config(&bi_config);
        bi_config.enable_logging = false;
        brain_immune = brain_immune_create(&bi_config);
        ASSERT_NE(brain_immune, nullptr);

        /* Create code immune linked to brain immune */
        code_immune_default_config(&config);
        config.enable_logging = false;
        config.sync_with_brain_immune = true;
        code_immune = code_immune_create_with_config(brain_immune, &config);
        ASSERT_NE(code_immune, nullptr);
    }

    void TearDown() override {
        if (code_immune) {
            code_immune_destroy(code_immune);
            code_immune = nullptr;
        }
        if (brain_immune) {
            brain_immune_destroy(brain_immune);
            brain_immune = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(CodeImmuneTest, DefaultConfigIsValid) {
    code_immune_config_t cfg;
    int result = code_immune_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(cfg.max_antigens, CODE_IMMUNE_MAX_ANTIGENS);
    EXPECT_EQ(cfg.max_b_cells, CODE_IMMUNE_MAX_B_CELLS);
    EXPECT_EQ(cfg.max_antibodies, CODE_IMMUNE_MAX_ANTIBODIES);
    EXPECT_GE(cfg.recognition_threshold, 0.0f);
    EXPECT_LE(cfg.recognition_threshold, 1.0f);
    EXPECT_GE(cfg.activation_threshold, 0.0f);
    EXPECT_LE(cfg.activation_threshold, 1.0f);
}

TEST_F(CodeImmuneTest, DefaultConfigNullFails) {
    int result = code_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(CodeImmuneTest, CreateWithNullConfigAndNoParent) {
    code_immune_system_t* sys = code_immune_create(nullptr);
    ASSERT_NE(sys, nullptr);
    EXPECT_EQ(sys->config.max_antigens, CODE_IMMUNE_MAX_ANTIGENS);
    EXPECT_EQ(sys->parent_immune, nullptr);
    code_immune_destroy(sys);
}

TEST_F(CodeImmuneTest, CreateWithCustomConfig) {
    code_immune_config_t custom_cfg;
    code_immune_default_config(&custom_cfg);
    custom_cfg.max_antigens = 64;
    custom_cfg.max_b_cells = 128;
    custom_cfg.max_antibodies = 256;

    code_immune_system_t* sys = code_immune_create_with_config(nullptr, &custom_cfg);
    ASSERT_NE(sys, nullptr);
    EXPECT_EQ(sys->config.max_antigens, 64u);
    EXPECT_EQ(sys->config.max_b_cells, 128u);
    EXPECT_EQ(sys->config.max_antibodies, 256u);
    code_immune_destroy(sys);
}

TEST_F(CodeImmuneTest, StartAndStop) {
    int result = code_immune_start(system);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(system->running);

    result = code_immune_stop(system);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(system->running);
}

TEST_F(CodeImmuneTest, StartNullFails) {
    int result = code_immune_start(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(CodeImmuneTest, StopNullFails) {
    int result = code_immune_stop(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(CodeImmuneTest, InitialStateIsNotRunning) {
    EXPECT_FALSE(system->running);
}

TEST_F(CodeImmuneTest, DestroyNullIsSafe) {
    code_immune_destroy(nullptr);
    /* No crash expected */
    SUCCEED();
}

/* ============================================================================
 * Crash Antigen Presentation Tests
 * ============================================================================ */

TEST_F(CodeImmuneTest, PresentCrashBasic) {
    code_immune_start(system);

    int result = code_immune_present_crash(
        system, SIGSEGV, nullptr, (void*)0xDEADBEEF
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(system->antigen_count, 1u);
    EXPECT_EQ(system->stats.crashes_detected, 1u);
    EXPECT_EQ(system->stats.sigsegv_count, 1u);
}

TEST_F(CodeImmuneTest, PresentCrashNullSystemFails) {
    int result = code_immune_present_crash(
        nullptr, SIGSEGV, nullptr, (void*)0x1234
    );
    EXPECT_EQ(result, -1);
}

TEST_F(CodeImmuneTest, PresentCrashDifferentSignals) {
    code_immune_start(system);

    code_immune_present_crash(system, SIGSEGV, nullptr, nullptr);
    code_immune_present_crash(system, SIGBUS, nullptr, nullptr);
    code_immune_present_crash(system, SIGFPE, nullptr, nullptr);
    code_immune_present_crash(system, SIGILL, nullptr, nullptr);
    code_immune_present_crash(system, SIGABRT, nullptr, nullptr);

    EXPECT_EQ(system->antigen_count, 5u);
    EXPECT_EQ(system->stats.sigsegv_count, 1u);
    EXPECT_EQ(system->stats.sigbus_count, 1u);
    EXPECT_EQ(system->stats.sigfpe_count, 1u);
    EXPECT_EQ(system->stats.sigill_count, 1u);
    EXPECT_EQ(system->stats.sigabrt_count, 1u);
}

TEST_F(CodeImmuneTest, PresentCrashDetailed) {
    code_immune_start(system);

    void* bt[4] = {(void*)0x1000, (void*)0x2000, (void*)0x3000, (void*)0x4000};
    uint64_t antigen_id = 0;

    int result = code_immune_present_crash_detailed(
        system, SIGSEGV,
        (void*)0xDEAD,  /* fault_addr */
        (void*)0xBEEF,  /* ip */
        "test_file.c",  /* source_file */
        42,             /* line */
        "test_func",    /* function */
        bt, 4,          /* backtrace */
        &antigen_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);
    EXPECT_EQ(system->antigen_count, 1u);

    const code_antigen_t* ag = code_immune_get_antigen(system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->signal, SIGSEGV);
    EXPECT_EQ(ag->fault_address, (void*)0xDEAD);
    EXPECT_EQ(ag->instruction_pointer, (void*)0xBEEF);
    EXPECT_EQ(ag->line_number, 42u);
    EXPECT_STREQ(ag->source_file, "test_file.c");
    EXPECT_STREQ(ag->function_name, "test_func");
    EXPECT_EQ(ag->backtrace_depth, 4);
}

TEST_F(CodeImmuneTest, CrashAntigenHasCorrectSeverity) {
    code_immune_start(system);

    code_immune_present_crash(system, SIGSEGV, nullptr, nullptr);
    const code_antigen_t* ag = code_immune_get_antigen(system, 1);
    ASSERT_NE(ag, nullptr);
    EXPECT_GE(ag->severity, 0.8f);  /* SIGSEGV is high severity */

    code_immune_present_crash(system, SIGFPE, nullptr, nullptr);
    ag = code_immune_get_antigen(system, 2);
    ASSERT_NE(ag, nullptr);
    EXPECT_LE(ag->severity, 0.7f);  /* SIGFPE is medium severity */
}

/* ============================================================================
 * B Cell Tests
 * ============================================================================ */

TEST_F(CodeImmuneTest, CreateBCellForAntigen) {
    code_immune_start(system);

    /* Present a crash */
    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, (void*)0x1000, (void*)0x2000,
        "test.c", 10, "test_fn", nullptr, 0, &antigen_id
    );

    /* Create B cell for it */
    uint64_t b_cell_id = 0;
    int result = code_immune_create_b_cell(system, antigen_id, &b_cell_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(b_cell_id, 0u);
    EXPECT_EQ(system->b_cell_count, 1u);

    const code_b_cell_t* bc = code_immune_get_b_cell(system, b_cell_id);
    ASSERT_NE(bc, nullptr);
    EXPECT_EQ(bc->state, CODE_B_CELL_NAIVE);
    EXPECT_EQ(bc->crash_types, CODE_CRASH_SIGSEGV);
}

TEST_F(CodeImmuneTest, CreateBCellNullIdFails) {
    code_immune_start(system);
    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    int result = code_immune_create_b_cell(system, antigen_id, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(CodeImmuneTest, CreateBCellInvalidAntigenFails) {
    uint64_t b_cell_id = 0;
    int result = code_immune_create_b_cell(system, 999, &b_cell_id);
    EXPECT_EQ(result, -1);
}

TEST_F(CodeImmuneTest, ActivateBCell) {
    code_immune_start(system);

    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);

    int result = code_immune_activate_b_cell(system, b_cell_id, antigen_id);
    EXPECT_EQ(result, 0);

    const code_b_cell_t* bc = code_immune_get_b_cell(system, b_cell_id);
    ASSERT_NE(bc, nullptr);
    EXPECT_EQ(bc->state, CODE_B_CELL_ACTIVATED);
    EXPECT_EQ(bc->bound_antigen_id, antigen_id);
}

TEST_F(CodeImmuneTest, FindMatchingBCell) {
    code_immune_start(system);

    /* Create antigen and B cell */
    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, (void*)0x1000, (void*)0x2000,
        "test.c", 10, "test_fn", nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);

    /* Try to find matching B cell for same antigen */
    uint64_t found_id = 0;
    int result = code_immune_find_matching_b_cell(system, antigen_id, &found_id);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(found_id, b_cell_id);
}

TEST_F(CodeImmuneTest, FormMemoryBCell) {
    code_immune_start(system);

    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);
    code_immune_activate_b_cell(system, b_cell_id, antigen_id);

    /* Set high affinity for memory formation */
    code_b_cell_t* bc = (code_b_cell_t*)code_immune_get_b_cell(system, b_cell_id);
    bc->affinity = 0.9f;
    bc->successful_fixes = 1;

    int result = code_immune_form_memory(system, b_cell_id);
    EXPECT_EQ(result, 0);

    bc = (code_b_cell_t*)code_immune_get_b_cell(system, b_cell_id);
    EXPECT_EQ(bc->state, CODE_B_CELL_MEMORY);
}

TEST_F(CodeImmuneTest, SetFixTemplate) {
    code_immune_start(system);

    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);

    const char* fix = "if (ptr == NULL) return -1;";
    int result = code_immune_set_fix_template(system, b_cell_id, fix);
    EXPECT_EQ(result, 0);

    const code_b_cell_t* bc = code_immune_get_b_cell(system, b_cell_id);
    EXPECT_STREQ(bc->fix_template, fix);
}

/* ============================================================================
 * Antibody Tests
 * ============================================================================ */

TEST_F(CodeImmuneTest, ProduceAntibody) {
    code_immune_start(system);

    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);
    code_immune_activate_b_cell(system, b_cell_id, antigen_id);
    code_immune_set_fix_template(system, b_cell_id, "/* null check */");

    uint64_t antibody_id = 0;
    int result = code_immune_produce_antibody(
        system, b_cell_id, CODE_ANTIBODY_IGM, &antibody_id
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(antibody_id, 0u);
    EXPECT_EQ(system->antibody_count, 1u);

    const code_antibody_t* ab = code_immune_get_antibody(system, antibody_id);
    ASSERT_NE(ab, nullptr);
    EXPECT_EQ(ab->ab_class, CODE_ANTIBODY_IGM);
    EXPECT_EQ(ab->target_antigen_id, antigen_id);
    EXPECT_EQ(ab->producer_b_cell_id, b_cell_id);
}

TEST_F(CodeImmuneTest, ProduceAntibodyNaiveBCellFails) {
    code_immune_start(system);

    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);
    /* Don't activate - B cell is still NAIVE */

    uint64_t antibody_id = 0;
    int result = code_immune_produce_antibody(
        system, b_cell_id, CODE_ANTIBODY_IGM, &antibody_id
    );

    EXPECT_EQ(result, -1);
}

TEST_F(CodeImmuneTest, ValidateAntibody) {
    code_immune_start(system);

    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);
    code_immune_activate_b_cell(system, b_cell_id, antigen_id);

    uint64_t antibody_id = 0;
    code_immune_produce_antibody(system, b_cell_id, CODE_ANTIBODY_IGG, &antibody_id);

    int result = code_immune_validate_antibody(system, antibody_id);
    EXPECT_EQ(result, 0);

    const code_antibody_t* ab = code_immune_get_antibody(system, antibody_id);
    EXPECT_TRUE(ab->validated);
}

TEST_F(CodeImmuneTest, UpgradeAntibody) {
    code_immune_start(system);

    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);
    code_immune_activate_b_cell(system, b_cell_id, antigen_id);

    uint64_t antibody_id = 0;
    code_immune_produce_antibody(system, b_cell_id, CODE_ANTIBODY_IGM, &antibody_id);

    int result = code_immune_upgrade_antibody(system, antibody_id, CODE_ANTIBODY_IGG);
    EXPECT_EQ(result, 0);

    const code_antibody_t* ab = code_immune_get_antibody(system, antibody_id);
    EXPECT_EQ(ab->ab_class, CODE_ANTIBODY_IGG);
}

TEST_F(CodeImmuneTest, AntibodyApoptosis) {
    code_immune_start(system);

    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);
    code_immune_activate_b_cell(system, b_cell_id, antigen_id);

    uint64_t antibody_id = 0;
    code_immune_produce_antibody(system, b_cell_id, CODE_ANTIBODY_IGM, &antibody_id);

    int result = code_immune_apoptosis(system, antibody_id);
    EXPECT_EQ(result, 0);

    const code_antibody_t* ab = code_immune_get_antibody(system, antibody_id);
    EXPECT_FALSE(ab->injected);
    EXPECT_EQ(ab->effectiveness, 0.0f);
}

/* ============================================================================
 * Pattern Matching Tests
 * ============================================================================ */

TEST_F(CodeImmuneTest, ComputeEpitope) {
    char epitope[CODE_IMMUNE_EPITOPE_SIZE];
    void* bt[2] = {(void*)0x1000, (void*)0x2000};

    int result = code_immune_compute_epitope(
        SIGSEGV, (void*)0xDEAD, (void*)0xBEEF,
        bt, 2, epitope
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(strlen(epitope), 0u);
}

TEST_F(CodeImmuneTest, ComputeEpitopeNullFails) {
    int result = code_immune_compute_epitope(
        SIGSEGV, nullptr, nullptr, nullptr, 0, nullptr
    );
    EXPECT_EQ(result, -1);
}

TEST_F(CodeImmuneTest, ComputeAffinityIdentical) {
    float affinity = code_immune_compute_affinity("pattern123", "pattern123");
    EXPECT_EQ(affinity, 1.0f);
}

TEST_F(CodeImmuneTest, ComputeAffinityDifferent) {
    float affinity = code_immune_compute_affinity("aaaa", "bbbb");
    EXPECT_LT(affinity, 1.0f);
}

TEST_F(CodeImmuneTest, ComputeAffinityNull) {
    float affinity = code_immune_compute_affinity(nullptr, "test");
    EXPECT_EQ(affinity, 0.0f);

    affinity = code_immune_compute_affinity("test", nullptr);
    EXPECT_EQ(affinity, 0.0f);
}

TEST_F(CodeImmuneTest, ComputeAffinityEmpty) {
    float affinity = code_immune_compute_affinity("", "test");
    EXPECT_EQ(affinity, 0.0f);
}

TEST_F(CodeImmuneTest, ComputeAffinitySameSignal) {
    /* Patterns with same signal suffix should have bonus */
    float affinity1 = code_immune_compute_affinity("abc_11", "def_11");
    float affinity2 = code_immune_compute_affinity("abc_11", "def_99");

    EXPECT_GT(affinity1, affinity2);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CodeImmuneTest, GetStats) {
    code_immune_start(system);

    code_immune_present_crash(system, SIGSEGV, nullptr, nullptr);
    code_immune_present_crash(system, SIGSEGV, nullptr, nullptr);
    code_immune_present_crash(system, SIGFPE, nullptr, nullptr);

    code_immune_stats_t stats;
    int result = code_immune_get_stats(system, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.crashes_detected, 3u);
    EXPECT_EQ(stats.sigsegv_count, 2u);
    EXPECT_EQ(stats.sigfpe_count, 1u);
}

TEST_F(CodeImmuneTest, GetStatsNullFails) {
    int result = code_immune_get_stats(system, nullptr);
    EXPECT_EQ(result, -1);

    code_immune_stats_t stats;
    result = code_immune_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(CodeImmuneTest, UpdateProcessesAntigens) {
    code_immune_start(system);

    /* Present crash with high danger signal */
    code_immune_present_crash(system, SIGSEGV, nullptr, (void*)0x1000);

    /* First antigen should have danger_signal >= threshold */
    const code_antigen_t* ag = code_immune_get_antigen(system, 1);
    ASSERT_NE(ag, nullptr);
    EXPECT_GE(ag->danger_signal, system->config.activation_threshold);

    /* Update should process it and create B cell */
    code_immune_update(system, 100);

    /* B cell should have been created */
    EXPECT_GT(system->b_cell_count, 0u);
}

TEST_F(CodeImmuneTest, UpdateNullIsSafe) {
    int result = code_immune_update(nullptr, 100);
    EXPECT_EQ(result, -1);
}

TEST_F(CodeImmuneTest, UpdateWhileNotRunning) {
    /* System not started */
    code_immune_present_crash(system, SIGSEGV, nullptr, nullptr);
    code_immune_update(system, 100);

    /* Should not process */
    EXPECT_EQ(system->b_cell_count, 0u);
}

/* ============================================================================
 * Memory Query Tests
 * ============================================================================ */

TEST_F(CodeImmuneTest, HasMemoryForCrashType) {
    code_immune_start(system);

    /* No memory initially */
    EXPECT_FALSE(code_immune_has_memory_for(system, CODE_CRASH_SIGSEGV));

    /* Create memory B cell for SIGSEGV */
    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        system, SIGSEGV, nullptr, nullptr,
        nullptr, 0, nullptr, nullptr, 0, &antigen_id
    );

    uint64_t b_cell_id = 0;
    code_immune_create_b_cell(system, antigen_id, &b_cell_id);
    code_immune_activate_b_cell(system, b_cell_id, antigen_id);

    code_b_cell_t* bc = (code_b_cell_t*)code_immune_get_b_cell(system, b_cell_id);
    bc->affinity = 0.9f;
    bc->successful_fixes = 1;
    code_immune_form_memory(system, b_cell_id);

    /* Now should have memory */
    EXPECT_TRUE(code_immune_has_memory_for(system, CODE_CRASH_SIGSEGV));
    EXPECT_FALSE(code_immune_has_memory_for(system, CODE_CRASH_SIGBUS));
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(CodeImmuneTest, CrashTypeToString) {
    EXPECT_STREQ(code_immune_crash_type_to_string(CODE_CRASH_NONE), "NONE");
    EXPECT_STREQ(code_immune_crash_type_to_string(CODE_CRASH_SIGSEGV), "SIGSEGV");
    EXPECT_STREQ(code_immune_crash_type_to_string(CODE_CRASH_SIGBUS), "SIGBUS");
    EXPECT_STREQ(code_immune_crash_type_to_string(CODE_CRASH_SIGILL), "SIGILL");
    EXPECT_STREQ(code_immune_crash_type_to_string(CODE_CRASH_SIGFPE), "SIGFPE");
    EXPECT_STREQ(code_immune_crash_type_to_string(CODE_CRASH_SIGABRT), "SIGABRT");
}

TEST_F(CodeImmuneTest, BCellStateToString) {
    EXPECT_STREQ(code_immune_b_cell_state_to_string(CODE_B_CELL_NAIVE), "NAIVE");
    EXPECT_STREQ(code_immune_b_cell_state_to_string(CODE_B_CELL_ACTIVATED), "ACTIVATED");
    EXPECT_STREQ(code_immune_b_cell_state_to_string(CODE_B_CELL_PLASMA), "PLASMA");
    EXPECT_STREQ(code_immune_b_cell_state_to_string(CODE_B_CELL_MEMORY), "MEMORY");
    EXPECT_STREQ(code_immune_b_cell_state_to_string(CODE_B_CELL_APOPTOTIC), "APOPTOTIC");
}

TEST_F(CodeImmuneTest, AntibodyClassToString) {
    EXPECT_STREQ(code_immune_antibody_class_to_string(CODE_ANTIBODY_IGM), "IgM");
    EXPECT_STREQ(code_immune_antibody_class_to_string(CODE_ANTIBODY_IGG), "IgG");
    EXPECT_STREQ(code_immune_antibody_class_to_string(CODE_ANTIBODY_IGE), "IgE");
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(CodeImmuneIntegrationTest, CreateWithBrainImmune) {
    EXPECT_NE(code_immune->parent_immune, nullptr);
    EXPECT_EQ(code_immune->parent_immune, brain_immune);
}

TEST_F(CodeImmuneIntegrationTest, SyncToBrainImmune) {
    code_immune_start(code_immune);
    brain_immune_start(brain_immune);

    uint64_t antigen_id = 0;
    code_immune_present_crash_detailed(
        code_immune, SIGSEGV, (void*)0x1000, (void*)0x2000,
        "test.c", 42, "crashy_fn", nullptr, 0, &antigen_id
    );

    /* Sync should happen automatically due to config.sync_with_brain_immune */
    /* Check that brain immune received an antigen */
    EXPECT_GT(brain_immune->antigen_count, 0u);
}

TEST_F(CodeImmuneIntegrationTest, RequestCytokine) {
    code_immune_start(code_immune);
    brain_immune_start(brain_immune);

    int result = code_immune_request_cytokine(
        code_immune, BRAIN_CYTOKINE_IL1, 0.5f
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(brain_immune->cytokine_count, 0u);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static int crash_callback_count = 0;
static void test_crash_callback(
    code_immune_system_t* /*system*/,
    const code_antigen_t* /*antigen*/,
    void* /*user_data*/
) {
    crash_callback_count++;
}

TEST_F(CodeImmuneTest, CrashCallback) {
    crash_callback_count = 0;

    code_immune_set_crash_callback(system, test_crash_callback, nullptr);
    code_immune_start(system);

    code_immune_present_crash(system, SIGSEGV, nullptr, nullptr);
    EXPECT_EQ(crash_callback_count, 1);

    code_immune_present_crash(system, SIGBUS, nullptr, nullptr);
    EXPECT_EQ(crash_callback_count, 2);
}
