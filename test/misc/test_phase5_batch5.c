/**
 * @file test_phase5_batch5.c
 * @brief Phase 5 Batch 5: PR-SNN Retrieval, Mirror-Prefrontal Messages
 *
 * Tests for: pr_snn_retrieve_via_snn, mirror_prefrontal_process_messages,
 * pr_snn_bridge pre-existing warning fixes.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "cognitive/memory/core/nimcp_pr_snn_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_prefrontal_bridge.h"
#include "utils/memory/nimcp_memory.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* =========================================================================
 * PR-SNN Bridge Tests
 * ========================================================================= */

static void test_pr_snn_create_destroy(void) {
    TEST("PR-SNN: create and destroy bridge");
    pr_snn_bridge_t bridge = pr_snn_bridge_create(NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");
    pr_snn_bridge_destroy(bridge);
    PASS();
}

static void test_pr_snn_retrieve_null_safety(void) {
    TEST("PR-SNN: retrieve_via_snn NULL safety");
    int rc = pr_snn_retrieve_via_snn(NULL, NULL, NULL, 5, NULL, NULL);
    ASSERT_TRUE(rc == -1, "NULL should return -1");
    PASS();
}

static void test_pr_snn_retrieve_not_initialized(void) {
    TEST("PR-SNN: retrieve fails when not initialized");
    /* Create bridge with valid config but don't fully initialize */
    pr_snn_bridge_t bridge = pr_snn_bridge_create(NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    prime_signature_t sig;
    memset(&sig, 0, sizeof(sig));
    sig.num_factors = 3;
    sig.exponents[0] = 5;
    sig.exponents[1] = 3;
    sig.exponents[2] = 7;

    uint64_t result_ids[5];
    float result_scores[5];

    /* Without an SNN, should still not crash */
    int rc = pr_snn_retrieve_via_snn(bridge, &sig, NULL, 5, result_ids, result_scores);
    ASSERT_TRUE(rc == -1, "NULL SNN should return -1");

    pr_snn_bridge_destroy(bridge);
    PASS();
}

static void test_pr_snn_retrieve_top_k_zero(void) {
    TEST("PR-SNN: retrieve with top_k=0 returns 0");
    pr_snn_bridge_t bridge = pr_snn_bridge_create(NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    prime_signature_t sig;
    memset(&sig, 0, sizeof(sig));
    uint64_t ids[1];
    float scores[1];

    /* Dummy SNN pointer (won't be used with top_k=0) */
    int dummy_snn = 1;
    int rc = pr_snn_retrieve_via_snn(bridge, &sig, (snn_network_t*)&dummy_snn, 0, ids, scores);
    ASSERT_TRUE(rc == 0, "top_k=0 should return 0");

    pr_snn_bridge_destroy(bridge);
    PASS();
}

static void test_pr_snn_bridge_stats(void) {
    TEST("PR-SNN: bridge stats accessible");
    pr_snn_bridge_t bridge = pr_snn_bridge_create(NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    pr_snn_bridge_stats_t stats;
    pr_snn_error_t err = pr_snn_bridge_get_stats(bridge, &stats);
    ASSERT_TRUE(err == PR_SNN_SUCCESS, "get_stats failed");
    ASSERT_TRUE(stats.total_encodings == 0, "initial encodings should be 0");

    pr_snn_bridge_destroy(bridge);
    PASS();
}

static void test_pr_snn_encode_rate(void) {
    TEST("PR-SNN: rate encoding basic");
    pr_snn_bridge_t bridge = pr_snn_bridge_create(NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    pr_snn_bridge_config_t cfg = pr_snn_bridge_config_default();
    pr_spike_pattern_t* pattern = pr_spike_pattern_create(
        cfg.population_size, cfg.encoding_window_ms);
    ASSERT_TRUE(pattern != NULL, "pattern NULL");

    pr_snn_error_t err = pr_snn_encode_rate(bridge, 0.5f, pattern);
    ASSERT_TRUE(err == PR_SNN_SUCCESS, "encode_rate failed");
    ASSERT_TRUE(pattern->num_spikes > 0, "should generate spikes");

    pr_spike_pattern_destroy(pattern);
    pr_snn_bridge_destroy(bridge);
    PASS();
}

/* =========================================================================
 * Mirror-Prefrontal Bridge Tests
 * ========================================================================= */

static void test_mirror_pfc_create_destroy(void) {
    TEST("Mirror-PFC: create and destroy");
    mirror_prefrontal_bridge_t bridge = mirror_prefrontal_bridge_create(
        NULL, NULL, NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");
    mirror_prefrontal_bridge_destroy(bridge);
    PASS();
}

static void test_mirror_pfc_process_null(void) {
    TEST("Mirror-PFC: process_messages NULL safety");
    uint32_t n = mirror_prefrontal_process_messages(NULL, 10);
    ASSERT_TRUE(n == 0, "NULL bridge should return 0");
    PASS();
}

static void test_mirror_pfc_process_no_bio(void) {
    TEST("Mirror-PFC: process_messages without bio-async");
    mirror_prefrontal_bridge_t bridge = mirror_prefrontal_bridge_create(
        NULL, NULL, NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    /* Bio-async not enabled — should return 0 */
    uint32_t n = mirror_prefrontal_process_messages(bridge, 10);
    ASSERT_TRUE(n == 0, "no bio-async should return 0");

    mirror_prefrontal_bridge_destroy(bridge);
    PASS();
}

static void test_mirror_pfc_bio_async_status(void) {
    TEST("Mirror-PFC: bio_async initially disconnected");
    mirror_prefrontal_bridge_t bridge = mirror_prefrontal_bridge_create(
        NULL, NULL, NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    bool connected = mirror_prefrontal_is_bio_async_connected(bridge);
    ASSERT_TRUE(!connected, "should not be connected initially");

    mirror_prefrontal_bridge_destroy(bridge);
    PASS();
}

static void test_mirror_pfc_stats(void) {
    TEST("Mirror-PFC: stats accessible");
    mirror_prefrontal_bridge_t bridge = mirror_prefrontal_bridge_create(
        NULL, NULL, NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    mirror_prefrontal_stats_t stats;
    int rc = mirror_prefrontal_get_stats(bridge, &stats);
    ASSERT_TRUE(rc == 0, "get_stats failed");
    ASSERT_TRUE(stats.messages_received == 0, "initial messages should be 0");

    mirror_prefrontal_bridge_destroy(bridge);
    PASS();
}

static void test_mirror_pfc_reset_stats(void) {
    TEST("Mirror-PFC: reset_stats works");
    mirror_prefrontal_bridge_t bridge = mirror_prefrontal_bridge_create(
        NULL, NULL, NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    int rc = mirror_prefrontal_reset_stats(bridge);
    ASSERT_TRUE(rc == 0, "reset_stats failed");

    mirror_prefrontal_stats_t stats;
    rc = mirror_prefrontal_get_stats(bridge, &stats);
    ASSERT_TRUE(rc == 0, "get_stats after reset failed");

    mirror_prefrontal_bridge_destroy(bridge);
    PASS();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5 Batch 5: PR-SNN Retrieval, Mirror-PFC Messages ===\n\n");

    printf("--- PR-SNN Bridge ---\n");
    test_pr_snn_create_destroy();
    test_pr_snn_retrieve_null_safety();
    test_pr_snn_retrieve_not_initialized();
    test_pr_snn_retrieve_top_k_zero();
    test_pr_snn_bridge_stats();
    test_pr_snn_encode_rate();

    printf("\n--- Mirror-Prefrontal Bridge ---\n");
    test_mirror_pfc_create_destroy();
    test_mirror_pfc_process_null();
    test_mirror_pfc_process_no_bio();
    test_mirror_pfc_bio_async_status();
    test_mirror_pfc_stats();
    test_mirror_pfc_reset_stats();

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
