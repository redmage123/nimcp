/**
 * @file test_neurogenesis.cpp
 * @brief Unit tests for Neurogenesis module
 *
 * WHAT: Test suite for nimcp_neurogenesis
 * WHY:  Verify stem cells, niches, neuron creation, and pruning
 * HOW:  Unit tests for create, modify, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "biology/neurogenesis/nimcp_neurogenesis.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeurogenesisTest : public ::testing::Test {
protected:
    nimcp_neurogenesis_t ng = nullptr;

    void SetUp() override {
        nimcp_neurogenesis_config_t config = nimcp_neurogenesis_default_config();
        ng = nimcp_neurogenesis_create(&config);
        ASSERT_NE(ng, nullptr);
    }

    void TearDown() override {
        if (ng) {
            nimcp_neurogenesis_destroy(ng);
            ng = nullptr;
        }
    }

    /* Helper to create a niche for tests */
    void CreateTestNiche(uint32_t niche_id = 1, uint32_t stem_cells = 10) {
        nimcp_niche_config_t niche_config = {
            .niche_id = niche_id,
            .type = NICHE_TYPE_HIPPOCAMPUS,
            .region_start = 0,
            .region_end = 999,
            .initial_stem_cells = stem_cells,
            .local_proliferation_rate = 1.0f,
            .default_type = NEW_NEURON_TYPE_GRANULE
        };
        nimcp_neurogenesis_create_niche(ng, &niche_config);
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST(NeurogenesisCreateTest, CreateWithDefaultConfig) {
    nimcp_neurogenesis_t n = nimcp_neurogenesis_create(nullptr);
    ASSERT_NE(n, nullptr);
    nimcp_neurogenesis_destroy(n);
}

TEST(NeurogenesisCreateTest, CreateWithCustomConfig) {
    nimcp_neurogenesis_config_t config = nimcp_neurogenesis_default_config();
    config.max_niches = 32;
    config.base_proliferation_rate = 0.05f;
    config.enable_activity_modulation = true;

    nimcp_neurogenesis_t n = nimcp_neurogenesis_create(&config);
    ASSERT_NE(n, nullptr);
    nimcp_neurogenesis_destroy(n);
}

TEST(NeurogenesisCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_neurogenesis_destroy(nullptr);
}

TEST(NeurogenesisCreateTest, DefaultConfigValues) {
    nimcp_neurogenesis_config_t config = nimcp_neurogenesis_default_config();

    EXPECT_GT(config.max_niches, 0u);
    EXPECT_GT(config.base_proliferation_rate, 0.0f);
    EXPECT_GT(config.survival_threshold, 0.0f);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(NeurogenesisTest, InitSuccess) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_init(ng, nullptr);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, InitNull) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_init(nullptr, nullptr);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NULL_PTR);
}

TEST_F(NeurogenesisTest, DoubleInit) {
    nimcp_neurogenesis_init(ng, nullptr);
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_init(ng, nullptr);
    EXPECT_EQ(err, NEUROGENESIS_ERR_ALREADY_INITIALIZED);
}

TEST_F(NeurogenesisTest, ShutdownSuccess) {
    nimcp_neurogenesis_init(ng, nullptr);
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_shutdown(ng);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, ShutdownNotInitialized) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_shutdown(ng);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(NeurogenesisTest, UpdateSuccess) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_update(ng, 0.1f);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, UpdateNull) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_update(nullptr, 0.1f);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NULL_PTR);
}

TEST_F(NeurogenesisTest, MultipleUpdates) {
    CreateTestNiche();
    for (int i = 0; i < 100; i++) {
        nimcp_neurogenesis_error_t err = nimcp_neurogenesis_update(ng, 0.1f);
        EXPECT_EQ(err, NEUROGENESIS_OK);
    }
}

//=============================================================================
// State and Stats Tests
//=============================================================================

TEST_F(NeurogenesisTest, GetStateSuccess) {
    nimcp_neurogenesis_state_t state;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_state(ng, &state);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, GetStateNull) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_state(ng, nullptr);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NULL_PTR);
}

TEST_F(NeurogenesisTest, GetStatsSuccess) {
    nimcp_neurogenesis_stats_t stats;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_stats(ng, &stats);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, ResetStatsSuccess) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_reset_stats(ng);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, ResetStatsNull) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_reset_stats(nullptr);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NULL_PTR);
}

//=============================================================================
// Niche Tests
//=============================================================================

TEST_F(NeurogenesisTest, CreateNicheSuccess) {
    nimcp_niche_config_t config = {
        .niche_id = 1,
        .type = NICHE_TYPE_HIPPOCAMPUS,
        .region_start = 0,
        .region_end = 999,
        .initial_stem_cells = 10,
        .local_proliferation_rate = 1.0f,
        .default_type = NEW_NEURON_TYPE_GRANULE
    };

    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_create_niche(ng, &config);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, CreateNicheNull) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_create_niche(ng, nullptr);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NULL_PTR);
}

TEST_F(NeurogenesisTest, RemoveNiche) {
    CreateTestNiche(42);
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_remove_niche(ng, 42);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, RemoveNicheNotFound) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_remove_niche(ng, 999);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NICHE_NOT_FOUND);
}

TEST_F(NeurogenesisTest, GetNicheInfo) {
    CreateTestNiche(1, 20);

    uint32_t stem_cells, pending;
    float rate;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_niche_info(
        ng, 1, &stem_cells, &pending, &rate);
    EXPECT_EQ(err, NEUROGENESIS_OK);
    EXPECT_EQ(stem_cells, 20u);
    EXPECT_EQ(pending, 0u);
    EXPECT_GT(rate, 0.0f);
}

TEST_F(NeurogenesisTest, SetNicheRate) {
    CreateTestNiche();
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_set_niche_rate(ng, 1, 0.5f);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, SetNicheRateNotFound) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_set_niche_rate(ng, 999, 0.5f);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NICHE_NOT_FOUND);
}

//=============================================================================
// Stem Cell Tests
//=============================================================================

TEST_F(NeurogenesisTest, AddStemCells) {
    CreateTestNiche(1, 5);
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_add_stem_cells(ng, 1, 10);
    EXPECT_EQ(err, NEUROGENESIS_OK);

    uint32_t count;
    nimcp_neurogenesis_get_stem_count(ng, 1, &count);
    EXPECT_EQ(count, 15u);
}

TEST_F(NeurogenesisTest, GetStemCount) {
    CreateTestNiche(1, 25);

    uint32_t count;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_stem_count(ng, 1, &count);
    EXPECT_EQ(err, NEUROGENESIS_OK);
    EXPECT_EQ(count, 25u);
}

TEST_F(NeurogenesisTest, GetStemCountNotFound) {
    uint32_t count;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_stem_count(ng, 999, &count);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NICHE_NOT_FOUND);
}

TEST_F(NeurogenesisTest, ActivateStemCells) {
    CreateTestNiche(1, 10);
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_activate_stem_cells(ng, 1, 5);
    EXPECT_EQ(err, NEUROGENESIS_OK);

    uint32_t quiescent, activated, prolif, diff;
    nimcp_neurogenesis_get_stem_distribution(ng, 1, &quiescent, &activated, &prolif, &diff);
    EXPECT_EQ(activated, 5u);
    EXPECT_EQ(quiescent, 5u);
}

TEST_F(NeurogenesisTest, GetStemDistribution) {
    CreateTestNiche(1, 10);

    uint32_t q, a, p, d;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_stem_distribution(
        ng, 1, &q, &a, &p, &d);
    EXPECT_EQ(err, NEUROGENESIS_OK);
    EXPECT_EQ(q, 10u);  /* All start quiescent */
    EXPECT_EQ(a, 0u);
    EXPECT_EQ(p, 0u);
    EXPECT_EQ(d, 0u);
}

//=============================================================================
// Neuron Creation Tests
//=============================================================================

TEST_F(NeurogenesisTest, CreateNeuronSuccess) {
    CreateTestNiche();

    uint32_t neuron_id;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_create_neuron(ng, 1, &neuron_id);
    EXPECT_EQ(err, NEUROGENESIS_OK);
    EXPECT_GT(neuron_id, 0u);
}

TEST_F(NeurogenesisTest, CreateNeuronNicheNotFound) {
    uint32_t neuron_id;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_create_neuron(ng, 999, &neuron_id);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NICHE_NOT_FOUND);
}

TEST_F(NeurogenesisTest, GetNeuronState) {
    CreateTestNiche();

    uint32_t neuron_id;
    nimcp_neurogenesis_create_neuron(ng, 1, &neuron_id);

    nimcp_new_neuron_state_t state;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_neuron_state(ng, neuron_id, &state);
    EXPECT_EQ(err, NEUROGENESIS_OK);
    EXPECT_EQ(state.neuron_id, neuron_id);
    EXPECT_EQ(state.stage, NEURON_STAGE_PROGENITOR);
    EXPECT_TRUE(state.is_active);
}

TEST_F(NeurogenesisTest, GetNeuronStateNotFound) {
    nimcp_new_neuron_state_t state;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_neuron_state(ng, 999, &state);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NEURON_NOT_FOUND);
}

TEST_F(NeurogenesisTest, ReportActivity) {
    CreateTestNiche();

    uint32_t neuron_id;
    nimcp_neurogenesis_create_neuron(ng, 1, &neuron_id);

    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_report_activity(ng, neuron_id, 0.5f);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, ForceMature) {
    CreateTestNiche();

    uint32_t neuron_id;
    nimcp_neurogenesis_create_neuron(ng, 1, &neuron_id);

    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_force_mature(ng, neuron_id);
    EXPECT_EQ(err, NEUROGENESIS_OK);

    nimcp_new_neuron_state_t state;
    nimcp_neurogenesis_get_neuron_state(ng, neuron_id, &state);
    EXPECT_EQ(state.stage, NEURON_STAGE_MATURE);
    EXPECT_NEAR(state.maturity, 1.0f, 0.01f);
}

TEST_F(NeurogenesisTest, PruneNeuron) {
    CreateTestNiche();

    uint32_t neuron_id;
    nimcp_neurogenesis_create_neuron(ng, 1, &neuron_id);

    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_prune_neuron(ng, neuron_id);
    EXPECT_EQ(err, NEUROGENESIS_OK);

    /* Neuron should no longer be found as active */
    nimcp_new_neuron_state_t state;
    nimcp_neurogenesis_error_t get_err = nimcp_neurogenesis_get_neuron_state(ng, neuron_id, &state);
    EXPECT_EQ(get_err, NEUROGENESIS_ERR_NEURON_NOT_FOUND);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int g_created_count = 0;
static int g_matured_count = 0;
static int g_pruned_count = 0;

static void on_created(uint32_t neuron_id, nimcp_new_neuron_type_t type,
                       uint32_t niche_id, void* user_data) {
    (void)neuron_id; (void)type; (void)niche_id; (void)user_data;
    g_created_count++;
}

static void on_matured(uint32_t neuron_id, uint32_t connections, void* user_data) {
    (void)neuron_id; (void)connections; (void)user_data;
    g_matured_count++;
}

static void on_pruned(uint32_t neuron_id, float final_activity, void* user_data) {
    (void)neuron_id; (void)final_activity; (void)user_data;
    g_pruned_count++;
}

TEST_F(NeurogenesisTest, SetCallbacks) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_set_callbacks(
        ng, on_created, on_matured, on_pruned, nullptr);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, CallbacksTriggered) {
    g_created_count = 0;

    nimcp_neurogenesis_set_callbacks(ng, on_created, on_matured, on_pruned, nullptr);
    CreateTestNiche();

    uint32_t neuron_id;
    nimcp_neurogenesis_create_neuron(ng, 1, &neuron_id);

    EXPECT_EQ(g_created_count, 1);
}

//=============================================================================
// Environmental Factor Tests
//=============================================================================

TEST_F(NeurogenesisTest, SetEnvironment) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_set_environment(
        ng, 0.8f, 0.2f, 0.6f, 0.5f);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

TEST_F(NeurogenesisTest, SetEnvironmentNull) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_set_environment(
        nullptr, 0.5f, 0.5f, 0.5f, 0.5f);
    EXPECT_EQ(err, NEUROGENESIS_ERR_NULL_PTR);
}

TEST_F(NeurogenesisTest, GetEnvironment) {
    nimcp_neurogenesis_set_environment(ng, 0.9f, 0.0f, 0.8f, 0.7f);
    nimcp_neurogenesis_update(ng, 0.1f);

    float modifier;
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_get_environment(ng, &modifier);
    EXPECT_EQ(err, NEUROGENESIS_OK);
    EXPECT_GT(modifier, 1.0f);  /* Positive factors should increase rate */
}

TEST_F(NeurogenesisTest, StressReducesRate) {
    /* High stress should reduce proliferation rate */
    nimcp_neurogenesis_set_environment(ng, 0.5f, 0.9f, 0.0f, 0.0f);
    nimcp_neurogenesis_update(ng, 0.1f);

    float modifier;
    nimcp_neurogenesis_get_environment(ng, &modifier);
    EXPECT_LT(modifier, 1.0f);
}

TEST_F(NeurogenesisTest, SetGlobalRate) {
    nimcp_neurogenesis_error_t err = nimcp_neurogenesis_set_global_rate(ng, 0.05f);
    EXPECT_EQ(err, NEUROGENESIS_OK);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST(ErrorStringTest, AllErrorsHaveStrings) {
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_OK), nullptr);
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_ERR_NULL_PTR), nullptr);
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_ERR_INVALID_PARAM), nullptr);
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_ERR_ALREADY_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_ERR_NO_MEMORY), nullptr);
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_ERR_NICHE_NOT_FOUND), nullptr);
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_ERR_NICHE_FULL), nullptr);
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_ERR_NEURON_NOT_FOUND), nullptr);
    EXPECT_NE(nimcp_neurogenesis_error_string(NEUROGENESIS_ERR_NO_STEM_CELLS), nullptr);
}

TEST(ErrorStringTest, UnknownErrorCode) {
    const char* str = nimcp_neurogenesis_error_string((nimcp_neurogenesis_error_t)-999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
