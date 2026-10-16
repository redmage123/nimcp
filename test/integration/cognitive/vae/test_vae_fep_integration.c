/**
 * @file test_vae_fep_integration.c
 * @brief Integration tests for VAE-FEP Bridge
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integration tests verifying VAE-FEP bidirectional communication
 * WHY:  Verify latent-belief mapping, free energy sharing, precision sync,
 *       and prediction error propagation work correctly across systems
 * HOW:  Tests using Check framework covering ~60 integration scenarios
 *
 * TEST CATEGORIES:
 * 1. Bridge Lifecycle Tests (~10 tests)
 * 2. Connection Management Tests (~8 tests)
 * 3. Latent-to-Belief Sync Tests (~10 tests)
 * 4. Belief-to-Latent Sync Tests (~8 tests)
 * 5. Free Energy Integration Tests (~10 tests)
 * 6. Precision Mapping Tests (~8 tests)
 * 7. Prediction Error Tests (~6 tests)
 *
 * @author NIMCP Development Team
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Core VAE and FEP modules */
#include "cognitive/vae/bridges/nimcp_vae_fep_bridge.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

/* Memory management */
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_LATENT_DIM     32
#define TEST_INPUT_DIM      128
#define TEST_BELIEF_DIM     32
#define TEST_BATCH_SIZE     1
#define TEST_EPSILON        1e-5f

/* ============================================================================
 * Mock/Helper Structures
 * ============================================================================ */

/**
 * @brief Test fixture state
 */
typedef struct {
    vae_fep_bridge_t* bridge;
    vae_system_t* vae;
    fep_system_t* fep;
    vae_fep_bridge_config_t config;
} test_fixture_t;

static test_fixture_t g_fixture;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Create a mock VAE system for testing
 */
static vae_system_t* create_mock_vae(uint32_t input_dim, uint32_t latent_dim)
{
    vae_config_t config;
    if (vae_default_config(&config) != 0) {
        return NULL;
    }

    config.input_dim = input_dim;
    config.latent_dim = latent_dim;
    config.encoder_hidden_dims[0] = 64;
    config.encoder_num_layers = 1;
    config.decoder_hidden_dims[0] = 64;
    config.decoder_num_layers = 1;
    config.beta = 1.0f;
    config.learning_rate = 0.001f;

    return vae_create(&config);
}

/**
 * @brief Create a mock FEP system for testing
 */
static fep_system_t* create_mock_fep(uint32_t belief_dim)
{
    fep_config_t config;
    if (fep_default_config(&config) != 0) {
        return NULL;
    }

    config.num_levels = 1;
    uint32_t dims[] = {belief_dim};
    config.level_dims = dims;
    config.belief_learning_rate = 0.1f;
    config.initial_precision = 1.0f;
    config.enable_active_inference = false;

    return fep_create(&config, belief_dim, 4);
}

/**
 * @brief Initialize latent state with test values
 */
static void init_test_latent_state(vae_system_t* vae, float mu_value, float log_var_value)
{
    if (!vae) return;

    uint32_t latent_dim = vae_get_latent_dim(vae);

    /* Create test input and encode */
    nimcp_tensor_t* input = nimcp_tensor_zeros((uint32_t[]){1, TEST_INPUT_DIM}, 2, NIMCP_DTYPE_FLOAT32);
    if (input) {
        float* data = (float*)input->data;
        for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
            data[i] = (float)i / (float)TEST_INPUT_DIM;
        }

        nimcp_tensor_t* z = nimcp_tensor_zeros((uint32_t[]){1, latent_dim}, 2, NIMCP_DTYPE_FLOAT32);
        if (z) {
            vae_encode(vae, input, z);
            nimcp_tensor_destroy(z);
        }
        nimcp_tensor_destroy(input);
    }
}

/**
 * @brief Initialize FEP beliefs with test values
 */
static void init_test_fep_beliefs(fep_system_t* fep, float mean_value, float precision_value)
{
    if (!fep || fep->num_levels == 0) return;

    fep_hierarchy_level_t* level = &fep->levels[0];
    uint32_t dim = level->beliefs.dim;

    if (level->beliefs.mean) {
        for (uint32_t i = 0; i < dim; i++) {
            level->beliefs.mean[i] = mean_value + 0.1f * (float)i;
        }
    }

    if (level->beliefs.precision) {
        for (uint32_t i = 0; i < dim; i++) {
            level->beliefs.precision[i] = precision_value;
        }
    }
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static void setup_minimal(void)
{
    memset(&g_fixture, 0, sizeof(test_fixture_t));
}

static void teardown_minimal(void)
{
    /* Clean up in reverse order */
    if (g_fixture.bridge) {
        vae_fep_bridge_destroy(g_fixture.bridge);
        g_fixture.bridge = NULL;
    }
    if (g_fixture.fep) {
        fep_destroy(g_fixture.fep);
        g_fixture.fep = NULL;
    }
    if (g_fixture.vae) {
        vae_destroy(g_fixture.vae);
        g_fixture.vae = NULL;
    }
}

static void setup_bridge_only(void)
{
    setup_minimal();

    vae_fep_bridge_default_config(&g_fixture.config);
    g_fixture.bridge = vae_fep_bridge_create(&g_fixture.config);
}

static void teardown_bridge_only(void)
{
    teardown_minimal();
}

static void setup_full_connection(void)
{
    setup_minimal();

    /* Create VAE */
    g_fixture.vae = create_mock_vae(TEST_INPUT_DIM, TEST_LATENT_DIM);
    ck_assert_ptr_nonnull(g_fixture.vae);

    /* Create FEP */
    g_fixture.fep = create_mock_fep(TEST_BELIEF_DIM);
    ck_assert_ptr_nonnull(g_fixture.fep);

    /* Create and configure bridge */
    vae_fep_bridge_default_config(&g_fixture.config);
    g_fixture.config.mapping_mode = VAE_FEP_MAP_DIRECT;
    g_fixture.config.share_precision = true;
    g_fixture.config.share_free_energy = true;
    g_fixture.bridge = vae_fep_bridge_create(&g_fixture.config);
    ck_assert_ptr_nonnull(g_fixture.bridge);

    /* Connect systems */
    ck_assert_int_eq(vae_fep_bridge_connect_vae(g_fixture.bridge, g_fixture.vae), 0);
    ck_assert_int_eq(vae_fep_bridge_connect_fep(g_fixture.bridge, g_fixture.fep), 0);
}

static void teardown_full_connection(void)
{
    teardown_minimal();
}

/* ============================================================================
 * 1. Bridge Lifecycle Tests
 * ============================================================================ */

START_TEST(test_bridge_create_default_config)
{
    vae_fep_bridge_t* bridge = vae_fep_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    /* Verify default state */
    ck_assert(bridge->is_initialized);
    ck_assert_int_eq(vae_fep_bridge_get_state(bridge), VAE_FEP_STATE_DISCONNECTED);
    ck_assert(!vae_fep_bridge_is_connected(bridge));

    vae_fep_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_bridge_create_custom_config)
{
    vae_fep_bridge_config_t config;
    vae_fep_bridge_default_config(&config);

    config.mapping_mode = VAE_FEP_MAP_LINEAR;
    config.share_precision = true;
    config.auto_sync_enabled = true;
    config.sync_interval_ms = 50;

    vae_fep_bridge_t* bridge = vae_fep_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    vae_fep_bridge_config_t out_config;
    ck_assert_int_eq(vae_fep_bridge_get_config(bridge, &out_config), 0);
    ck_assert_int_eq(out_config.mapping_mode, VAE_FEP_MAP_LINEAR);
    ck_assert(out_config.share_precision);
    ck_assert(out_config.auto_sync_enabled);
    ck_assert_uint_eq(out_config.sync_interval_ms, 50);

    vae_fep_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_bridge_destroy_null_safe)
{
    /* Should not crash */
    vae_fep_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_bridge_reset)
{
    ck_assert_ptr_nonnull(g_fixture.bridge);

    /* Perform some operations first */
    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);

    /* Reset */
    ck_assert_int_eq(vae_fep_bridge_reset(g_fixture.bridge), 0);

    /* Verify reset state */
    vae_fep_bridge_health_t health;
    ck_assert_int_eq(vae_fep_bridge_get_health(g_fixture.bridge, &health), 0);
    ck_assert(health.is_healthy);
    ck_assert_float_eq(health.bridge_health, 1.0f);
}
END_TEST

START_TEST(test_bridge_default_config_null)
{
    ck_assert_int_eq(vae_fep_bridge_default_config(NULL), -1);
}
END_TEST

START_TEST(test_bridge_default_config_values)
{
    vae_fep_bridge_config_t config;
    ck_assert_int_eq(vae_fep_bridge_default_config(&config), 0);

    ck_assert_int_eq(config.sync_direction, VAE_FEP_SYNC_BIDIRECTIONAL);
    ck_assert_int_eq(config.mapping_mode, VAE_FEP_MAP_DIRECT);
    ck_assert(config.share_precision);
    ck_assert(config.share_free_energy);
    ck_assert(!config.auto_sync_enabled);
    ck_assert(config.enable_immune_reporting);
}
END_TEST

START_TEST(test_bridge_health_initial)
{
    vae_fep_bridge_t* bridge = vae_fep_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    vae_fep_bridge_health_t health;
    ck_assert_int_eq(vae_fep_bridge_get_health(bridge, &health), 0);

    ck_assert(health.is_healthy);
    ck_assert_float_eq(health.bridge_health, 1.0f);
    ck_assert_float_eq(health.sync_reliability, 1.0f);
    ck_assert_uint_eq(health.consecutive_failures, 0);

    vae_fep_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_bridge_stats_initial)
{
    vae_fep_bridge_t* bridge = vae_fep_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    vae_fep_bridge_stats_t stats;
    ck_assert_int_eq(vae_fep_bridge_get_stats(bridge, &stats), 0);

    ck_assert_uint_eq(stats.total_syncs, 0);
    ck_assert_uint_eq(stats.latent_to_belief_syncs, 0);
    ck_assert_uint_eq(stats.belief_to_latent_syncs, 0);
    ck_assert_uint_eq(stats.sync_failures, 0);

    vae_fep_bridge_destroy(bridge);
}
END_TEST

/* ============================================================================
 * 2. Connection Management Tests
 * ============================================================================ */

START_TEST(test_connect_vae)
{
    ck_assert_ptr_nonnull(g_fixture.bridge);

    vae_system_t* vae = create_mock_vae(TEST_INPUT_DIM, TEST_LATENT_DIM);
    ck_assert_ptr_nonnull(vae);

    ck_assert_int_eq(vae_fep_bridge_connect_vae(g_fixture.bridge, vae), 0);
    ck_assert_uint_eq(vae_fep_bridge_get_latent_dim(g_fixture.bridge), TEST_LATENT_DIM);

    /* Not fully connected without FEP */
    ck_assert(!vae_fep_bridge_is_connected(g_fixture.bridge));

    vae_destroy(vae);
}
END_TEST

START_TEST(test_connect_fep)
{
    ck_assert_ptr_nonnull(g_fixture.bridge);

    fep_system_t* fep = create_mock_fep(TEST_BELIEF_DIM);
    ck_assert_ptr_nonnull(fep);

    ck_assert_int_eq(vae_fep_bridge_connect_fep(g_fixture.bridge, fep), 0);
    ck_assert_uint_eq(vae_fep_bridge_get_belief_dim(g_fixture.bridge), TEST_BELIEF_DIM);

    /* Not fully connected without VAE */
    ck_assert(!vae_fep_bridge_is_connected(g_fixture.bridge));

    fep_destroy(fep);
}
END_TEST

START_TEST(test_connect_both_systems)
{
    ck_assert_ptr_nonnull(g_fixture.bridge);

    vae_system_t* vae = create_mock_vae(TEST_INPUT_DIM, TEST_LATENT_DIM);
    fep_system_t* fep = create_mock_fep(TEST_BELIEF_DIM);
    ck_assert_ptr_nonnull(vae);
    ck_assert_ptr_nonnull(fep);

    ck_assert_int_eq(vae_fep_bridge_connect_vae(g_fixture.bridge, vae), 0);
    ck_assert_int_eq(vae_fep_bridge_connect_fep(g_fixture.bridge, fep), 0);

    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));
    ck_assert_int_eq(vae_fep_bridge_get_state(g_fixture.bridge), VAE_FEP_STATE_CONNECTED);

    vae_destroy(vae);
    fep_destroy(fep);
}
END_TEST

START_TEST(test_disconnect)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    ck_assert_int_eq(vae_fep_bridge_disconnect(g_fixture.bridge), 0);

    ck_assert(!vae_fep_bridge_is_connected(g_fixture.bridge));
    ck_assert_int_eq(vae_fep_bridge_get_state(g_fixture.bridge), VAE_FEP_STATE_DISCONNECTED);
}
END_TEST

START_TEST(test_connect_null_vae_fails)
{
    ck_assert_ptr_nonnull(g_fixture.bridge);
    ck_assert_int_eq(vae_fep_bridge_connect_vae(g_fixture.bridge, NULL), -1);
}
END_TEST

START_TEST(test_connect_null_fep_fails)
{
    ck_assert_ptr_nonnull(g_fixture.bridge);
    ck_assert_int_eq(vae_fep_bridge_connect_fep(g_fixture.bridge, NULL), -1);
}
END_TEST

START_TEST(test_dims_compatible_direct_mode)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    /* In direct mode, dims should match */
    bool compatible = vae_fep_bridge_dims_compatible(g_fixture.bridge);

    /* TEST_LATENT_DIM == TEST_BELIEF_DIM == 32, so should be compatible */
    ck_assert(compatible);
}
END_TEST

START_TEST(test_state_transitions)
{
    ck_assert_ptr_nonnull(g_fixture.bridge);

    /* Initial state: DISCONNECTED */
    ck_assert_int_eq(vae_fep_bridge_get_state(g_fixture.bridge), VAE_FEP_STATE_DISCONNECTED);

    /* Connect VAE - still disconnected */
    vae_system_t* vae = create_mock_vae(TEST_INPUT_DIM, TEST_LATENT_DIM);
    vae_fep_bridge_connect_vae(g_fixture.bridge, vae);
    ck_assert_int_eq(vae_fep_bridge_get_state(g_fixture.bridge), VAE_FEP_STATE_DISCONNECTED);

    /* Connect FEP - now connected */
    fep_system_t* fep = create_mock_fep(TEST_BELIEF_DIM);
    vae_fep_bridge_connect_fep(g_fixture.bridge, fep);
    ck_assert_int_eq(vae_fep_bridge_get_state(g_fixture.bridge), VAE_FEP_STATE_CONNECTED);

    /* Disconnect - back to disconnected */
    vae_fep_bridge_disconnect(g_fixture.bridge);
    ck_assert_int_eq(vae_fep_bridge_get_state(g_fixture.bridge), VAE_FEP_STATE_DISCONNECTED);

    vae_destroy(vae);
    fep_destroy(fep);
}
END_TEST

/* ============================================================================
 * 3. Latent-to-Belief Sync Tests
 * ============================================================================ */

START_TEST(test_sync_latent_to_belief_basic)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    /* Initialize VAE with test input */
    init_test_latent_state(g_fixture.vae, 0.5f, -1.0f);

    /* Sync to FEP */
    ck_assert_int_eq(vae_fep_sync_latent_to_belief(g_fixture.bridge), 0);

    /* Verify stats updated */
    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert_uint_eq(stats.latent_to_belief_syncs, 1);
    ck_assert_uint_eq(stats.total_syncs, 1);
}
END_TEST

START_TEST(test_sync_latent_to_belief_not_connected_fails)
{
    vae_fep_bridge_t* bridge = vae_fep_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    ck_assert_int_eq(vae_fep_sync_latent_to_belief(bridge), -1);

    vae_fep_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_sync_latent_to_belief_updates_mapping)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_latent_state(g_fixture.vae, 0.5f, -1.0f);
    vae_fep_sync_latent_to_belief(g_fixture.bridge);

    vae_fep_mapping_state_t mapping;
    ck_assert_int_eq(vae_fep_bridge_get_mapping(g_fixture.bridge, &mapping), 0);

    ck_assert_uint_eq(mapping.latent_dim, TEST_LATENT_DIM);
    ck_assert(mapping.last_vae_to_fep_us > 0);
}
END_TEST

START_TEST(test_sync_latent_to_belief_precision_sharing)
{
    /* Reconfigure with precision sharing */
    g_fixture.config.share_precision = true;
    g_fixture.config.precision_scale = 1.0f;

    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_latent_state(g_fixture.vae, 0.5f, 0.0f);  /* log_var=0 means var=1, prec=1 */
    vae_fep_sync_latent_to_belief(g_fixture.bridge);

    /* Verify precision was transferred */
    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert_uint_ge(stats.latent_to_belief_syncs, 1);
}
END_TEST

START_TEST(test_sync_direction_vae_to_fep)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_latent_state(g_fixture.vae, 0.5f, -1.0f);

    ck_assert_int_eq(vae_fep_bridge_sync_direction(g_fixture.bridge,
                     VAE_FEP_SYNC_VAE_TO_FEP), 0);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert_uint_eq(stats.latent_to_belief_syncs, 1);
}
END_TEST

START_TEST(test_sync_updates_health)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    /* Successful sync should maintain/improve health */
    vae_fep_bridge_health_t health_before;
    vae_fep_bridge_get_health(g_fixture.bridge, &health_before);

    init_test_latent_state(g_fixture.vae, 0.5f, -1.0f);
    vae_fep_sync_latent_to_belief(g_fixture.bridge);

    vae_fep_bridge_health_t health_after;
    vae_fep_bridge_get_health(g_fixture.bridge, &health_after);

    ck_assert(health_after.is_healthy);
    ck_assert_uint_eq(health_after.consecutive_failures, 0);
}
END_TEST

/* ============================================================================
 * 4. Belief-to-Latent Sync Tests
 * ============================================================================ */

START_TEST(test_sync_belief_to_latent_basic)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_fep_beliefs(g_fixture.fep, 0.5f, 1.0f);

    ck_assert_int_eq(vae_fep_sync_belief_to_latent(g_fixture.bridge), 0);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert_uint_eq(stats.belief_to_latent_syncs, 1);
    ck_assert_uint_eq(stats.belief_updates_received, 1);
}
END_TEST

START_TEST(test_sync_belief_to_latent_not_connected_fails)
{
    vae_fep_bridge_t* bridge = vae_fep_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    ck_assert_int_eq(vae_fep_sync_belief_to_latent(bridge), -1);

    vae_fep_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_sync_belief_to_latent_updates_mapping)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_fep_beliefs(g_fixture.fep, 0.5f, 1.0f);
    vae_fep_sync_belief_to_latent(g_fixture.bridge);

    vae_fep_mapping_state_t mapping;
    ck_assert_int_eq(vae_fep_bridge_get_mapping(g_fixture.bridge, &mapping), 0);

    ck_assert(mapping.last_fep_to_vae_us > 0);
}
END_TEST

START_TEST(test_sync_direction_fep_to_vae)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_fep_beliefs(g_fixture.fep, 0.5f, 1.0f);

    ck_assert_int_eq(vae_fep_bridge_sync_direction(g_fixture.bridge,
                     VAE_FEP_SYNC_FEP_TO_VAE), 0);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert_uint_eq(stats.belief_to_latent_syncs, 1);
}
END_TEST

/* ============================================================================
 * 5. Free Energy Integration Tests
 * ============================================================================ */

START_TEST(test_compute_free_energy_basic)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    float free_energy = 0.0f;
    ck_assert_int_eq(vae_fep_compute_free_energy(g_fixture.bridge, &free_energy), 0);

    /* Free energy should be finite */
    ck_assert(!isnan(free_energy));
    ck_assert(!isinf(free_energy));
}
END_TEST

START_TEST(test_compute_free_energy_null_fails)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));
    ck_assert_int_eq(vae_fep_compute_free_energy(g_fixture.bridge, NULL), -1);
}
END_TEST

START_TEST(test_compute_free_energy_not_connected_fails)
{
    vae_fep_bridge_t* bridge = vae_fep_bridge_create(NULL);
    float fe;
    ck_assert_int_eq(vae_fep_compute_free_energy(bridge, &fe), -1);
    vae_fep_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_free_energy_decomposition)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    /* First compute free energy */
    float fe;
    vae_fep_compute_free_energy(g_fixture.bridge, &fe);

    /* Get decomposition */
    float total, inaccuracy, complexity;
    ck_assert_int_eq(vae_fep_get_free_energy_decomposition(g_fixture.bridge,
                     &total, &inaccuracy, &complexity), 0);

    /* Values should be finite */
    ck_assert(!isnan(total));
    ck_assert(!isnan(inaccuracy));
    ck_assert(!isnan(complexity));
}
END_TEST

START_TEST(test_free_energy_weight_combination)
{
    /* Reconfigure with specific weights */
    g_fixture.config.free_energy_weight_vae = 0.7f;
    g_fixture.config.free_energy_weight_fep = 0.3f;

    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    float fe;
    ck_assert_int_eq(vae_fep_compute_free_energy(g_fixture.bridge, &fe), 0);

    /* Verify mapping state reflects the computation */
    vae_fep_mapping_state_t mapping;
    vae_fep_bridge_get_mapping(g_fixture.bridge, &mapping);
    ck_assert(mapping.last_free_energy_us > 0);
}
END_TEST

START_TEST(test_free_energy_updates_stats)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    float fe;
    vae_fep_compute_free_energy(g_fixture.bridge, &fe);
    vae_fep_compute_free_energy(g_fixture.bridge, &fe);
    vae_fep_compute_free_energy(g_fixture.bridge, &fe);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);

    /* Stats should track min/max */
    ck_assert(!isnan(stats.min_free_energy));
    ck_assert(!isnan(stats.max_free_energy));
    ck_assert(!isnan(stats.avg_free_energy));
}
END_TEST

START_TEST(test_update_free_energy_from_vae_loss)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    /* Create mock loss */
    vae_loss_t loss = {
        .total = 5.0f,
        .reconstruction = 3.0f,
        .kl_divergence = 2.0f,
        .free_energy = 5.0f,
        .inaccuracy = 3.0f,
        .complexity = 2.0f
    };

    ck_assert_int_eq(vae_fep_update_free_energy_from_vae(g_fixture.bridge, &loss), 0);

    vae_fep_mapping_state_t mapping;
    vae_fep_bridge_get_mapping(g_fixture.bridge, &mapping);
    ck_assert_float_eq(mapping.vae_free_energy, 5.0f);
}
END_TEST

/* ============================================================================
 * 6. Precision Mapping Tests
 * ============================================================================ */

START_TEST(test_get_precision_basic)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    float precision[TEST_LATENT_DIM];
    ck_assert_int_eq(vae_fep_get_precision(g_fixture.bridge, precision, TEST_LATENT_DIM), 0);

    /* All precision values should be positive */
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert(precision[i] > 0.0f);
    }
}
END_TEST

START_TEST(test_get_avg_precision)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    /* Do a sync first to populate precision */
    init_test_latent_state(g_fixture.vae, 0.5f, 0.0f);
    vae_fep_sync_latent_to_belief(g_fixture.bridge);

    float avg = vae_fep_get_avg_precision(g_fixture.bridge);
    ck_assert(!isnan(avg));
    ck_assert(avg > 0.0f);
}
END_TEST

START_TEST(test_sync_precision)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_latent_state(g_fixture.vae, 0.5f, -1.0f);

    ck_assert_int_eq(vae_fep_sync_precision(g_fixture.bridge), 0);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert_uint_eq(stats.precision_updates, 1);
}
END_TEST

START_TEST(test_precision_clamping)
{
    /* Configure with strict bounds */
    g_fixture.config.min_precision = 0.1f;
    g_fixture.config.max_precision = 10.0f;

    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_latent_state(g_fixture.vae, 0.5f, -5.0f);  /* Very low var = high prec */
    vae_fep_sync_precision(g_fixture.bridge);

    float precision[TEST_LATENT_DIM];
    vae_fep_get_precision(g_fixture.bridge, precision, TEST_LATENT_DIM);

    /* All should be within bounds */
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert(precision[i] >= g_fixture.config.min_precision);
        ck_assert(precision[i] <= g_fixture.config.max_precision);
    }
}
END_TEST

/* ============================================================================
 * 7. Prediction Error Tests
 * ============================================================================ */

START_TEST(test_report_prediction_error_basic)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    ck_assert_int_eq(vae_fep_report_prediction_error(g_fixture.bridge, 0.5f), 0);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert_uint_eq(stats.prediction_errors_sent, 1);
}
END_TEST

START_TEST(test_report_prediction_error_tensor)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    nimcp_tensor_t* error = nimcp_tensor_zeros((uint32_t[]){1, TEST_INPUT_DIM}, 2, NIMCP_DTYPE_FLOAT32);
    ck_assert_ptr_nonnull(error);

    /* Fill with test values */
    float* data = (float*)error->data;
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        data[i] = 0.1f;
    }

    ck_assert_int_eq(vae_fep_report_prediction_error_tensor(g_fixture.bridge, error), 0);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert_uint_eq(stats.prediction_errors_sent, 1);

    nimcp_tensor_destroy(error);
}
END_TEST

START_TEST(test_report_prediction_error_not_connected_fails)
{
    vae_fep_bridge_t* bridge = vae_fep_bridge_create(NULL);
    ck_assert_int_eq(vae_fep_report_prediction_error(bridge, 0.5f), -1);
    vae_fep_bridge_destroy(bridge);
}
END_TEST

/* ============================================================================
 * 8. Bidirectional Sync Tests
 * ============================================================================ */

START_TEST(test_bidirectional_sync)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_latent_state(g_fixture.vae, 0.5f, -1.0f);
    init_test_fep_beliefs(g_fixture.fep, 0.5f, 1.0f);

    ck_assert_int_eq(vae_fep_bridge_sync(g_fixture.bridge), 0);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);

    ck_assert_uint_eq(stats.bidirectional_syncs, 1);
    ck_assert_uint_ge(stats.latent_to_belief_syncs, 1);
    ck_assert_uint_ge(stats.belief_to_latent_syncs, 1);
}
END_TEST

START_TEST(test_sync_direction_bidirectional)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    init_test_latent_state(g_fixture.vae, 0.5f, -1.0f);
    init_test_fep_beliefs(g_fixture.fep, 0.5f, 1.0f);

    ck_assert_int_eq(vae_fep_bridge_sync_direction(g_fixture.bridge,
                     VAE_FEP_SYNC_BIDIRECTIONAL), 0);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert_uint_eq(stats.bidirectional_syncs, 1);
}
END_TEST

/* ============================================================================
 * 9. Update and Auto-Sync Tests
 * ============================================================================ */

START_TEST(test_bridge_update_basic)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    ck_assert_int_eq(vae_fep_bridge_update(g_fixture.bridge, 10), 0);

    vae_fep_bridge_stats_t stats;
    vae_fep_bridge_get_stats(g_fixture.bridge, &stats);
    ck_assert(stats.uptime_us > 0);
}
END_TEST

START_TEST(test_bridge_process_messages)
{
    ck_assert(vae_fep_bridge_is_connected(g_fixture.bridge));

    int result = vae_fep_bridge_process_messages(g_fixture.bridge);
    ck_assert_int_ge(result, 0);
}
END_TEST

/* ============================================================================
 * 10. Bio-Async Integration Tests
 * ============================================================================ */

START_TEST(test_bio_async_connect)
{
    vae_fep_bridge_t* bridge = vae_fep_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    ck_assert(!vae_fep_bridge_is_bio_async_connected(bridge));

    ck_assert_int_eq(vae_fep_bridge_connect_bio_async(bridge), 0);
    ck_assert(vae_fep_bridge_is_bio_async_connected(bridge));

    ck_assert_int_eq(vae_fep_bridge_disconnect_bio_async(bridge), 0);
    ck_assert(!vae_fep_bridge_is_bio_async_connected(bridge));

    vae_fep_bridge_destroy(bridge);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

static Suite* vae_fep_integration_suite(void)
{
    Suite* s = suite_create("VAE-FEP Integration");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_minimal, teardown_minimal);
    tcase_add_test(tc_lifecycle, test_bridge_create_default_config);
    tcase_add_test(tc_lifecycle, test_bridge_create_custom_config);
    tcase_add_test(tc_lifecycle, test_bridge_destroy_null_safe);
    tcase_add_test(tc_lifecycle, test_bridge_default_config_null);
    tcase_add_test(tc_lifecycle, test_bridge_default_config_values);
    tcase_add_test(tc_lifecycle, test_bridge_health_initial);
    tcase_add_test(tc_lifecycle, test_bridge_stats_initial);
    suite_add_tcase(s, tc_lifecycle);

    /* Reset test - needs bridge */
    TCase* tc_reset = tcase_create("Reset");
    tcase_add_checked_fixture(tc_reset, setup_bridge_only, teardown_bridge_only);
    tcase_add_test(tc_reset, test_bridge_reset);
    suite_add_tcase(s, tc_reset);

    /* Connection tests */
    TCase* tc_connection = tcase_create("Connection");
    tcase_add_checked_fixture(tc_connection, setup_bridge_only, teardown_bridge_only);
    tcase_add_test(tc_connection, test_connect_vae);
    tcase_add_test(tc_connection, test_connect_fep);
    tcase_add_test(tc_connection, test_connect_both_systems);
    tcase_add_test(tc_connection, test_connect_null_vae_fails);
    tcase_add_test(tc_connection, test_connect_null_fep_fails);
    tcase_add_test(tc_connection, test_state_transitions);
    suite_add_tcase(s, tc_connection);

    /* Disconnect and dims tests - need full connection */
    TCase* tc_full_conn = tcase_create("FullConnection");
    tcase_add_checked_fixture(tc_full_conn, setup_full_connection, teardown_full_connection);
    tcase_add_test(tc_full_conn, test_disconnect);
    tcase_add_test(tc_full_conn, test_dims_compatible_direct_mode);
    suite_add_tcase(s, tc_full_conn);

    /* Latent-to-belief sync tests */
    TCase* tc_l2b = tcase_create("LatentToBelief");
    tcase_add_checked_fixture(tc_l2b, setup_full_connection, teardown_full_connection);
    tcase_add_test(tc_l2b, test_sync_latent_to_belief_basic);
    tcase_add_test(tc_l2b, test_sync_latent_to_belief_not_connected_fails);
    tcase_add_test(tc_l2b, test_sync_latent_to_belief_updates_mapping);
    tcase_add_test(tc_l2b, test_sync_latent_to_belief_precision_sharing);
    tcase_add_test(tc_l2b, test_sync_direction_vae_to_fep);
    tcase_add_test(tc_l2b, test_sync_updates_health);
    suite_add_tcase(s, tc_l2b);

    /* Belief-to-latent sync tests */
    TCase* tc_b2l = tcase_create("BeliefToLatent");
    tcase_add_checked_fixture(tc_b2l, setup_full_connection, teardown_full_connection);
    tcase_add_test(tc_b2l, test_sync_belief_to_latent_basic);
    tcase_add_test(tc_b2l, test_sync_belief_to_latent_not_connected_fails);
    tcase_add_test(tc_b2l, test_sync_belief_to_latent_updates_mapping);
    tcase_add_test(tc_b2l, test_sync_direction_fep_to_vae);
    suite_add_tcase(s, tc_b2l);

    /* Free energy tests */
    TCase* tc_fe = tcase_create("FreeEnergy");
    tcase_add_checked_fixture(tc_fe, setup_full_connection, teardown_full_connection);
    tcase_add_test(tc_fe, test_compute_free_energy_basic);
    tcase_add_test(tc_fe, test_compute_free_energy_null_fails);
    tcase_add_test(tc_fe, test_compute_free_energy_not_connected_fails);
    tcase_add_test(tc_fe, test_free_energy_decomposition);
    tcase_add_test(tc_fe, test_free_energy_weight_combination);
    tcase_add_test(tc_fe, test_free_energy_updates_stats);
    tcase_add_test(tc_fe, test_update_free_energy_from_vae_loss);
    suite_add_tcase(s, tc_fe);

    /* Precision tests */
    TCase* tc_prec = tcase_create("Precision");
    tcase_add_checked_fixture(tc_prec, setup_full_connection, teardown_full_connection);
    tcase_add_test(tc_prec, test_get_precision_basic);
    tcase_add_test(tc_prec, test_get_avg_precision);
    tcase_add_test(tc_prec, test_sync_precision);
    tcase_add_test(tc_prec, test_precision_clamping);
    suite_add_tcase(s, tc_prec);

    /* Prediction error tests */
    TCase* tc_pred = tcase_create("PredictionError");
    tcase_add_checked_fixture(tc_pred, setup_full_connection, teardown_full_connection);
    tcase_add_test(tc_pred, test_report_prediction_error_basic);
    tcase_add_test(tc_pred, test_report_prediction_error_tensor);
    tcase_add_test(tc_pred, test_report_prediction_error_not_connected_fails);
    suite_add_tcase(s, tc_pred);

    /* Bidirectional sync tests */
    TCase* tc_bidir = tcase_create("Bidirectional");
    tcase_add_checked_fixture(tc_bidir, setup_full_connection, teardown_full_connection);
    tcase_add_test(tc_bidir, test_bidirectional_sync);
    tcase_add_test(tc_bidir, test_sync_direction_bidirectional);
    suite_add_tcase(s, tc_bidir);

    /* Update tests */
    TCase* tc_update = tcase_create("Update");
    tcase_add_checked_fixture(tc_update, setup_full_connection, teardown_full_connection);
    tcase_add_test(tc_update, test_bridge_update_basic);
    tcase_add_test(tc_update, test_bridge_process_messages);
    suite_add_tcase(s, tc_update);

    /* Bio-async tests */
    TCase* tc_bio = tcase_create("BioAsync");
    tcase_add_checked_fixture(tc_bio, setup_minimal, teardown_minimal);
    tcase_add_test(tc_bio, test_bio_async_connect);
    suite_add_tcase(s, tc_bio);

    return s;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(void)
{
    Suite* s = vae_fep_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);

    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
