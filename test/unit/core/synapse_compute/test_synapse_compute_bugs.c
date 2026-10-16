/**
 * @file test_synapse_compute_bugs.c
 * @brief Unit tests for synapse compute and multimodal integration bug fixes
 *
 * WHAT: Tests for C7 (get_dopamine_phasic_tonic), H15 (expf overflow),
 *       H16 (multimodal thread safety), and error code corrections
 * WHY:  Verify that bug fixes produce correct behavior
 * HOW:  Check framework unit tests with minimal scaffolding
 *
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "core/synapse_compute/nimcp_synapse_compute.h"
#include "core/integration/nimcp_multimodal_integration.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"

/* ============================================================================
 * Bug C7: get_dopamine_phasic_tonic no longer unconditionally fails
 * ============================================================================ */

START_TEST(test_dopamine_phasic_tonic_null_returns_null)
{
    /* neuromodulator_get_dopamine_phasic_tonic(NULL) must return NULL */
    phasic_tonic_state_t* result = neuromodulator_get_dopamine_phasic_tonic(NULL);
    ck_assert_ptr_null(result);
}
END_TEST

START_TEST(test_dopamine_phasic_tonic_valid_system)
{
    /* Create a real neuromodulator system and verify accessor returns non-NULL */
    neuromodulator_config_t config = {
        .baseline_dopamine = 0.3f,
        .baseline_serotonin = 0.5f,
        .baseline_acetylcholine = 0.4f,
        .baseline_norepinephrine = 0.3f,
        .dopamine_decay = 2.0f,
        .serotonin_decay = 10.0f,
        .acetylcholine_decay = 0.5f,
        .norepinephrine_decay = 3.0f,
        .reward_dopamine_gain = 0.5f,
        .threat_norepinephrine_gain = 0.5f,
        .salience_acetylcholine_gain = 0.3f,
        .punishment_serotonin_gain = 0.3f,
        .enable_volume_transmission = false,
        .diffusion_rate = 0.0f
    };

    neuromodulator_system_t sys = neuromodulator_system_create(&config);
    ck_assert_ptr_nonnull(sys);

    phasic_tonic_state_t* pt = neuromodulator_get_dopamine_phasic_tonic(sys);
    ck_assert_ptr_nonnull(pt);

    neuromodulator_system_destroy(sys);
}
END_TEST

START_TEST(test_three_factor_learning_with_neuromodulator)
{
    /* Verify that synapse_learn_three_factor does not crash when
     * neuromodulator system is present in the context */
    neuromodulator_config_t config = {
        .baseline_dopamine = 0.3f,
        .baseline_serotonin = 0.5f,
        .baseline_acetylcholine = 0.4f,
        .baseline_norepinephrine = 0.3f,
        .dopamine_decay = 2.0f,
        .serotonin_decay = 10.0f,
        .acetylcholine_decay = 0.5f,
        .norepinephrine_decay = 3.0f,
        .reward_dopamine_gain = 0.5f,
        .threat_norepinephrine_gain = 0.5f,
        .salience_acetylcholine_gain = 0.3f,
        .punishment_serotonin_gain = 0.3f,
        .enable_volume_transmission = false,
        .diffusion_rate = 0.0f
    };

    neuromodulator_system_t sys = neuromodulator_system_create(&config);
    ck_assert_ptr_nonnull(sys);

    /* Set up a minimal synapse */
    struct synapse_t syn;
    memset(&syn, 0, sizeof(syn));
    syn.weight = 0.5f;
    syn.trace = 0.0f;
    syn.meta_plasticity = 1.0f;
    syn.eligibility = NULL;
    syn.enable_eligibility = false;

    /* Set up context with neuromodulator system */
    synapse_compute_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.neuromodulator_system = (void*)sys;
    ctx.neuromodulation = 0.5f;

    /* Call three-factor learning — must not crash */
    synapse_learn_three_factor(&syn, NULL, NULL, 10.0f, 12.0f, 1.0f, &ctx);

    /* Weight should have changed from the STDP update */
    ck_assert(syn.weight != 0.5f || syn.last_change != 0.0f);

    neuromodulator_system_destroy(sys);
}
END_TEST

/* ============================================================================
 * Bug H15: expf() overflow protection in attention weight
 * ============================================================================ */

START_TEST(test_attention_no_inf_for_large_score)
{
    /* Create a synapse and neurons with activity history that would
     * produce a large dot product, triggering expf overflow without clamping */
    struct synapse_t syn;
    memset(&syn, 0, sizeof(syn));
    syn.weight = 1.0f;

    /* Create minimal neurons with activity history */
    neuron_t pre, post;
    memset(&pre, 0, sizeof(pre));
    memset(&post, 0, sizeof(post));

    /* Allocate activity history with large values to produce big dot product */
    float pre_hist[16];
    float post_hist[16];
    for (int i = 0; i < 16; i++) {
        pre_hist[i] = 100.0f;   /* Large values to produce big dot product */
        post_hist[i] = 100.0f;
    }

    pre.activity_history = pre_hist;
    pre.activity_history_capacity = 16;
    post.activity_history = post_hist;
    post.activity_history_capacity = 16;

    /* Compute attention — must return finite value, not INF */
    float result = synapse_compute_attention(&syn, &pre, &post, 1.0f, NULL);

    ck_assert(isfinite(result));
}
END_TEST

START_TEST(test_attention_normal_values)
{
    /* Verify attention computation works correctly for normal input values */
    struct synapse_t syn;
    memset(&syn, 0, sizeof(syn));
    syn.weight = 0.5f;

    neuron_t pre, post;
    memset(&pre, 0, sizeof(pre));
    memset(&post, 0, sizeof(post));

    float pre_hist[16];
    float post_hist[16];
    for (int i = 0; i < 16; i++) {
        pre_hist[i] = 0.1f * (float)(i + 1);
        post_hist[i] = 0.1f * (float)(16 - i);
    }

    pre.activity_history = pre_hist;
    pre.activity_history_capacity = 16;
    post.activity_history = post_hist;
    post.activity_history_capacity = 16;

    float result = synapse_compute_attention(&syn, &pre, &post, 1.0f, NULL);

    ck_assert(isfinite(result));
    ck_assert_float_gt(result, 0.0f);
}
END_TEST

START_TEST(test_default_compute_null_synapse)
{
    /* synapse_compute_default with NULL synapse returns 0 */
    float result = synapse_compute_default(NULL, NULL, NULL, 1.0f, NULL);
    ck_assert_float_eq(result, 0.0f);
}
END_TEST

/* ============================================================================
 * Bug H16: Multimodal integration thread safety + error codes
 * ============================================================================ */

START_TEST(test_multimodal_create_destroy)
{
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    multimodal_integration_t mi = multimodal_integration_create(&config);
    ck_assert_ptr_nonnull(mi);

    /* Destroy must not crash (mutex is freed properly) */
    multimodal_integration_destroy(mi);
}
END_TEST

START_TEST(test_multimodal_create_null_config)
{
    /* NULL config should return NULL, not crash */
    multimodal_integration_t mi = multimodal_integration_create(NULL);
    ck_assert_ptr_null(mi);
}
END_TEST

START_TEST(test_multimodal_create_zero_output_dim)
{
    /* Zero output_dim should return NULL */
    multimodal_config_t config = {
        .visual_dim = 0,
        .audio_dim = 0,
        .speech_dim = 0,
        .direct_dim = 0,
        .output_dim = 0,
        .method = INTEGRATION_ATTENTION,
        .visual_weight = 0.25f,
        .audio_weight = 0.25f,
        .speech_weight = 0.25f,
        .direct_weight = 0.25f
    };
    multimodal_integration_t mi = multimodal_integration_create(&config);
    ck_assert_ptr_null(mi);
}
END_TEST

START_TEST(test_multimodal_integrate_attention)
{
    multimodal_config_t config = multimodal_default_config(4, 4, 0, 0);
    multimodal_integration_t mi = multimodal_integration_create(&config);
    ck_assert_ptr_nonnull(mi);

    float visual[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float audio[4] = {0.5f, 1.5f, 2.5f, 3.5f};
    float output[8];
    memset(output, 0, sizeof(output));

    multimodal_input_t input = {
        .visual_features = visual,
        .visual_dim = 4,
        .audio_features = audio,
        .audio_dim = 4,
        .speech_features = NULL,
        .speech_dim = 0,
        .direct_features = NULL,
        .direct_dim = 0,
        .timestamp = 0
    };

    bool ok = multimodal_integrate(mi, &input, output);
    ck_assert(ok);

    /* At least some output should be non-zero */
    float sum = 0.0f;
    for (int i = 0; i < 8; i++) sum += fabsf(output[i]);
    ck_assert_float_gt(sum, 0.0f);

    multimodal_integration_destroy(mi);
}
END_TEST

START_TEST(test_multimodal_get_attention_values)
{
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    multimodal_integration_t mi = multimodal_integration_create(&config);
    ck_assert_ptr_nonnull(mi);

    float v = 0, a = 0, s = 0, d = 0;
    bool ok = multimodal_get_attention(mi, &v, &a, &s, &d);
    ck_assert(ok);

    /* Attention weights should be normalized (sum ~1.0) */
    float total = v + a + s + d;
    ck_assert_float_gt(total, 0.9f);
    ck_assert_float_lt(total, 1.1f);

    multimodal_integration_destroy(mi);
}
END_TEST

START_TEST(test_multimodal_update_weights_no_crash)
{
    multimodal_config_t config = multimodal_default_config(64, 32, 16, 8);
    multimodal_integration_t mi = multimodal_integration_create(&config);
    ck_assert_ptr_nonnull(mi);

    bool ok = multimodal_update_weights(mi, 0.5f, 0.01f);
    ck_assert(ok);

    multimodal_integration_destroy(mi);
}
END_TEST

START_TEST(test_multimodal_destroy_null)
{
    /* Destroying NULL should not crash */
    multimodal_integration_destroy(NULL);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* synapse_compute_bugs_suite(void)
{
    Suite* s = suite_create("SynapseComputeBugs");

    /* C7: dopamine phasic-tonic accessor */
    TCase* tc_c7 = tcase_create("C7_PhasicTonic");
    tcase_add_test(tc_c7, test_dopamine_phasic_tonic_null_returns_null);
    tcase_add_test(tc_c7, test_dopamine_phasic_tonic_valid_system);
    tcase_add_test(tc_c7, test_three_factor_learning_with_neuromodulator);
    tcase_set_timeout(tc_c7, 30);
    suite_add_tcase(s, tc_c7);

    /* H15: expf overflow protection */
    TCase* tc_h15 = tcase_create("H15_ExpfOverflow");
    tcase_add_test(tc_h15, test_attention_no_inf_for_large_score);
    tcase_add_test(tc_h15, test_attention_normal_values);
    tcase_add_test(tc_h15, test_default_compute_null_synapse);
    tcase_set_timeout(tc_h15, 30);
    suite_add_tcase(s, tc_h15);

    /* H16: Multimodal integration thread safety + error codes */
    TCase* tc_h16 = tcase_create("H16_MultimodalIntegration");
    tcase_add_test(tc_h16, test_multimodal_create_destroy);
    tcase_add_test(tc_h16, test_multimodal_create_null_config);
    tcase_add_test(tc_h16, test_multimodal_create_zero_output_dim);
    tcase_add_test(tc_h16, test_multimodal_integrate_attention);
    tcase_add_test(tc_h16, test_multimodal_get_attention_values);
    tcase_add_test(tc_h16, test_multimodal_update_weights_no_crash);
    tcase_add_test(tc_h16, test_multimodal_destroy_null);
    tcase_set_timeout(tc_h16, 30);
    suite_add_tcase(s, tc_h16);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = synapse_compute_bugs_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
