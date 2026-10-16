/**
 * @file test_financial_memory_bridges.c
 * @brief Unit tests for financial memory system bridges
 *
 * WHAT: Test suite for financial memory bridges:
 *       - Working Memory Bridge (Miller's 7+/-2, salience eviction)
 *       - Mammillary Bridge (memory trace relay, consolidation)
 *       - Resonance Bridge (prime signature encoding, Kuramoto coherence)
 *       - Autobio Bridge (trading episode recording, emotional tagging)
 *
 * WHY:  Verify correct behavior of memory storage, retrieval, consolidation,
 *       and subsystem integration for financial trading applications.
 *
 * HOW:  Unit tests using Check framework covering lifecycle, core API,
 *       and subsystem setters for all four memory bridges.
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/parietal/nimcp_financial_working_memory_bridge.h"
#include "cognitive/parietal/nimcp_financial_mammillary_bridge.h"
#include "cognitive/parietal/nimcp_financial_resonance_bridge.h"
#include "cognitive/parietal/nimcp_financial_autobio_bridge.h"

/* ============================================================================
 * Test Fixtures - Working Memory Bridge
 * ============================================================================ */

static financial_wm_bridge_t* g_wm_bridge = NULL;

static void setup_working_memory(void)
{
    int result = financial_wm_bridge_create(&g_wm_bridge, NULL);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
    ck_assert_ptr_nonnull(g_wm_bridge);
}

static void teardown_working_memory(void)
{
    if (g_wm_bridge) {
        financial_wm_bridge_destroy(g_wm_bridge);
        g_wm_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Mammillary Bridge
 * ============================================================================ */

static financial_mammillary_bridge_t* g_mammillary_bridge = NULL;

static void setup_mammillary(void)
{
    g_mammillary_bridge = financial_mammillary_bridge_create(NULL);
    ck_assert_ptr_nonnull(g_mammillary_bridge);
}

static void teardown_mammillary(void)
{
    if (g_mammillary_bridge) {
        financial_mammillary_bridge_destroy(g_mammillary_bridge);
        g_mammillary_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Resonance Bridge
 * ============================================================================ */

static financial_resonance_bridge_t* g_resonance_bridge = NULL;

static void setup_resonance(void)
{
    g_resonance_bridge = financial_resonance_bridge_create(NULL);
    ck_assert_ptr_nonnull(g_resonance_bridge);
}

static void teardown_resonance(void)
{
    if (g_resonance_bridge) {
        financial_resonance_bridge_destroy(g_resonance_bridge);
        g_resonance_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Autobio Bridge
 * ============================================================================ */

static financial_autobio_bridge_t* g_autobio_bridge = NULL;

static void setup_autobio(void)
{
    g_autobio_bridge = financial_autobio_bridge_create(NULL);
    ck_assert_ptr_nonnull(g_autobio_bridge);
}

static void teardown_autobio(void)
{
    if (g_autobio_bridge) {
        financial_autobio_bridge_destroy(g_autobio_bridge);
        g_autobio_bridge = NULL;
    }
}

/* ============================================================================
 * Working Memory Bridge Tests - Lifecycle
 * ============================================================================ */

START_TEST(test_wm_create_default)
{
    financial_wm_bridge_t* bridge = NULL;
    int result = financial_wm_bridge_create(&bridge, NULL);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
    ck_assert_ptr_nonnull(bridge);
    financial_wm_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_wm_create_with_config)
{
    fin_wm_bridge_config_t config;
    financial_wm_bridge_default_config(&config);
    config.capacity = 9;
    config.decay_rate = 0.05f;

    financial_wm_bridge_t* bridge = NULL;
    int result = financial_wm_bridge_create(&bridge, &config);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
    ck_assert_ptr_nonnull(bridge);
    financial_wm_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_wm_create_null_output)
{
    int result = financial_wm_bridge_create(NULL, NULL);
    ck_assert_int_eq(result, FIN_WM_ERR_NULL);
}
END_TEST

START_TEST(test_wm_destroy_null)
{
    /* Should not crash */
    financial_wm_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_wm_default_config)
{
    fin_wm_bridge_config_t config;
    financial_wm_bridge_default_config(&config);
    ck_assert_uint_eq(config.capacity, FIN_WM_CAPACITY);
    ck_assert(config.decay_rate > 0.0f);
}
END_TEST

/* ============================================================================
 * Working Memory Bridge Tests - Subsystem Setters
 * ============================================================================ */

START_TEST(test_wm_set_immune)
{
    int dummy_immune = 42;
    int result = financial_wm_bridge_set_immune(g_wm_bridge, (void*)&dummy_immune);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);

    /* NULL should also succeed (disconnect) */
    result = financial_wm_bridge_set_immune(g_wm_bridge, NULL);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
}
END_TEST

START_TEST(test_wm_set_immune_null_bridge)
{
    int dummy = 0;
    int result = financial_wm_bridge_set_immune(NULL, &dummy);
    ck_assert_int_eq(result, FIN_WM_ERR_NULL);
}
END_TEST

START_TEST(test_wm_set_bbb)
{
    int dummy_bbb = 123;
    int result = financial_wm_bridge_set_bbb(g_wm_bridge, (bbb_system_t)&dummy_bbb);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
}
END_TEST

START_TEST(test_wm_set_kg_wiring)
{
    int dummy_kg = 456;
    int result = financial_wm_bridge_set_kg_wiring(g_wm_bridge, (kg_wiring_t*)&dummy_kg);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
}
END_TEST

START_TEST(test_wm_set_health_agent)
{
    int dummy_agent = 789;
    int result = financial_wm_bridge_set_health_agent(g_wm_bridge, (nimcp_health_agent_t*)&dummy_agent);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
}
END_TEST

START_TEST(test_wm_set_logger)
{
    int dummy_logger = 111;
    int result = financial_wm_bridge_set_logger(g_wm_bridge, &dummy_logger);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
}
END_TEST

START_TEST(test_wm_set_bio_async)
{
    int dummy_ctx = 222;
    int result = financial_wm_bridge_set_bio_async(g_wm_bridge, (bio_async_context_t*)&dummy_ctx);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
}
END_TEST

/* ============================================================================
 * Working Memory Bridge Tests - Core API
 * ============================================================================ */

START_TEST(test_wm_add_item)
{
    fin_wm_item_t item = {
        .type = FIN_WM_ITEM_PRICE,
        .salience = 0.8f,
        .timestamp_ms = 1000,
        .data = {100.5f, 101.0f},
        .label = "SPY price"
    };
    strcpy(item.label, "SPY price");

    int result = financial_wm_bridge_add(g_wm_bridge, &item);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
}
END_TEST

START_TEST(test_wm_add_null_item)
{
    int result = financial_wm_bridge_add(g_wm_bridge, NULL);
    ck_assert_int_eq(result, FIN_WM_ERR_NULL);
}
END_TEST

START_TEST(test_wm_get_active)
{
    /* Add some items first */
    fin_wm_item_t item1 = {.type = FIN_WM_ITEM_PRICE, .salience = 0.9f, .timestamp_ms = 1000};
    fin_wm_item_t item2 = {.type = FIN_WM_ITEM_SIGNAL, .salience = 0.7f, .timestamp_ms = 2000};

    financial_wm_bridge_add(g_wm_bridge, &item1);
    financial_wm_bridge_add(g_wm_bridge, &item2);

    fin_wm_item_t items[FIN_WM_CAPACITY];
    uint32_t count = 0;
    int result = financial_wm_bridge_get_active(g_wm_bridge, items, &count);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
    ck_assert_uint_eq(count, 2);
}
END_TEST

START_TEST(test_wm_get_active_null)
{
    uint32_t count = 0;
    int result = financial_wm_bridge_get_active(g_wm_bridge, NULL, &count);
    ck_assert_int_eq(result, FIN_WM_ERR_NULL);

    fin_wm_item_t items[FIN_WM_CAPACITY];
    result = financial_wm_bridge_get_active(g_wm_bridge, items, NULL);
    ck_assert_int_eq(result, FIN_WM_ERR_NULL);
}
END_TEST

START_TEST(test_wm_decay_step)
{
    fin_wm_item_t item = {.type = FIN_WM_ITEM_NEWS, .salience = 0.5f, .timestamp_ms = 1000};
    financial_wm_bridge_add(g_wm_bridge, &item);

    int result = financial_wm_bridge_decay_step(g_wm_bridge, 1.0f);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
}
END_TEST

START_TEST(test_wm_refresh)
{
    fin_wm_item_t item = {.type = FIN_WM_ITEM_SIGNAL, .salience = 0.5f, .timestamp_ms = 1000};
    financial_wm_bridge_add(g_wm_bridge, &item);

    int result = financial_wm_bridge_refresh(g_wm_bridge, 0);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
}
END_TEST

START_TEST(test_wm_refresh_invalid_index)
{
    int result = financial_wm_bridge_refresh(g_wm_bridge, 999);
    ck_assert_int_eq(result, FIN_WM_ERR_NOT_FOUND);
}
END_TEST

START_TEST(test_wm_get_by_type)
{
    fin_wm_item_t item1 = {.type = FIN_WM_ITEM_PRICE, .salience = 0.8f};
    fin_wm_item_t item2 = {.type = FIN_WM_ITEM_PRICE, .salience = 0.6f};
    fin_wm_item_t item3 = {.type = FIN_WM_ITEM_SIGNAL, .salience = 0.7f};

    financial_wm_bridge_add(g_wm_bridge, &item1);
    financial_wm_bridge_add(g_wm_bridge, &item2);
    financial_wm_bridge_add(g_wm_bridge, &item3);

    fin_wm_item_t results[FIN_WM_CAPACITY];
    uint32_t count = FIN_WM_CAPACITY;
    int result = financial_wm_bridge_get_by_type(g_wm_bridge, FIN_WM_ITEM_PRICE, results, &count);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
    ck_assert_uint_eq(count, 2);
}
END_TEST

START_TEST(test_wm_clear)
{
    fin_wm_item_t item = {.type = FIN_WM_ITEM_RISK_ALERT, .salience = 1.0f};
    financial_wm_bridge_add(g_wm_bridge, &item);

    int result = financial_wm_bridge_clear(g_wm_bridge);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);

    fin_wm_item_t items[FIN_WM_CAPACITY];
    uint32_t count = 0;
    financial_wm_bridge_get_active(g_wm_bridge, items, &count);
    ck_assert_uint_eq(count, 0);
}
END_TEST

START_TEST(test_wm_capacity_limit)
{
    /* Fill to capacity */
    for (int i = 0; i < FIN_WM_CAPACITY + 2; i++) {
        fin_wm_item_t item = {
            .type = FIN_WM_ITEM_PRICE,
            .salience = 0.5f + (float)i * 0.01f,
            .timestamp_ms = (uint64_t)i * 100
        };
        financial_wm_bridge_add(g_wm_bridge, &item);
    }

    fin_wm_item_t items[FIN_WM_CAPACITY];
    uint32_t count = 0;
    financial_wm_bridge_get_active(g_wm_bridge, items, &count);
    ck_assert_uint_le(count, FIN_WM_CAPACITY);
}
END_TEST

START_TEST(test_wm_stats)
{
    fin_wm_item_t item = {.type = FIN_WM_ITEM_PATTERN, .salience = 0.9f};
    financial_wm_bridge_add(g_wm_bridge, &item);

    fin_wm_bridge_stats_t stats;
    int result = financial_wm_bridge_get_stats(g_wm_bridge, &stats);
    ck_assert_int_eq(result, FIN_WM_ERR_OK);
    ck_assert_uint_ge(stats.items_added, 1);
}
END_TEST

/* ============================================================================
 * Mammillary Bridge Tests - Lifecycle
 * ============================================================================ */

START_TEST(test_mammillary_create_default)
{
    financial_mammillary_bridge_t* bridge = financial_mammillary_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_mammillary_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_mammillary_create_with_config)
{
    fin_mammillary_config_t config = financial_mammillary_bridge_default_config();
    config.max_traces = 2048;
    config.decay_rate = 0.02f;

    financial_mammillary_bridge_t* bridge = financial_mammillary_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_mammillary_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_mammillary_destroy_null)
{
    /* Should not crash */
    financial_mammillary_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_mammillary_default_config)
{
    fin_mammillary_config_t config = financial_mammillary_bridge_default_config();
    ck_assert_uint_gt(config.max_traces, 0);
    ck_assert(config.decay_rate >= 0.0f);
}
END_TEST

START_TEST(test_mammillary_get_state)
{
    fin_mammillary_state_t state = financial_mammillary_bridge_get_state(g_mammillary_bridge);
    ck_assert_int_ne(state, FIN_MAMMILLARY_STATE_UNINITIALIZED);
}
END_TEST

/* ============================================================================
 * Mammillary Bridge Tests - Subsystem Setters
 * ============================================================================ */

START_TEST(test_mammillary_set_immune)
{
    int dummy = 1;
    int result = financial_mammillary_bridge_set_immune(g_mammillary_bridge, &dummy);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

START_TEST(test_mammillary_set_immune_null_bridge)
{
    int dummy = 1;
    int result = financial_mammillary_bridge_set_immune(NULL, &dummy);
    ck_assert_int_ne(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

START_TEST(test_mammillary_set_bbb)
{
    int dummy = 2;
    int result = financial_mammillary_bridge_set_bbb(g_mammillary_bridge, &dummy);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

START_TEST(test_mammillary_set_kg_wiring)
{
    int dummy = 3;
    int result = financial_mammillary_bridge_set_kg_wiring(g_mammillary_bridge, (kg_wiring_t*)&dummy);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

START_TEST(test_mammillary_set_health_agent)
{
    int dummy = 4;
    int result = financial_mammillary_bridge_set_health_agent(g_mammillary_bridge, &dummy);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

START_TEST(test_mammillary_set_logger)
{
    int dummy = 5;
    int result = financial_mammillary_bridge_set_logger(g_mammillary_bridge, &dummy);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

/* ============================================================================
 * Mammillary Bridge Tests - Core API
 * ============================================================================ */

START_TEST(test_mammillary_relay_trade)
{
    fin_memory_trace_t trace = {
        .trade_price = 150.25f,
        .trade_quantity = 100.0f,
        .trade_direction = 1,  /* Buy */
        .outcome = 250.0f,
        .market_volatility = 0.15f,
        .market_trend = 0.3f,
        .emotional_intensity = 0.4f,
        .timestamp_ms = 1700000000000ULL
    };

    int result = financial_mammillary_bridge_relay_trade(g_mammillary_bridge, &trace);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

START_TEST(test_mammillary_relay_trade_null)
{
    int result = financial_mammillary_bridge_relay_trade(g_mammillary_bridge, NULL);
    ck_assert_int_ne(result, FIN_MAMMILLARY_ERR_OK);

    fin_memory_trace_t trace = {.trade_price = 100.0f};
    result = financial_mammillary_bridge_relay_trade(NULL, &trace);
    ck_assert_int_ne(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

START_TEST(test_mammillary_consolidate)
{
    /* Add some traces first */
    for (int i = 0; i < 5; i++) {
        fin_memory_trace_t trace = {
            .trade_price = 100.0f + (float)i,
            .trade_quantity = 50.0f,
            .trade_direction = (i % 2) ? 1 : -1,
            .outcome = (float)(i - 2) * 100.0f,
            .market_volatility = 0.1f + (float)i * 0.02f,
            .timestamp_ms = 1700000000000ULL + (uint64_t)i * 1000
        };
        financial_mammillary_bridge_relay_trade(g_mammillary_bridge, &trace);
    }

    int result = financial_mammillary_bridge_consolidate(g_mammillary_bridge);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

START_TEST(test_mammillary_query_similar)
{
    /* Add traces with different market conditions */
    fin_memory_trace_t trace1 = {
        .trade_price = 100.0f, .market_volatility = 0.2f, .market_trend = 0.5f,
        .outcome = 500.0f, .timestamp_ms = 1700000000000ULL
    };
    fin_memory_trace_t trace2 = {
        .trade_price = 105.0f, .market_volatility = 0.22f, .market_trend = 0.45f,
        .outcome = 300.0f, .timestamp_ms = 1700000100000ULL
    };
    financial_mammillary_bridge_relay_trade(g_mammillary_bridge, &trace1);
    financial_mammillary_bridge_relay_trade(g_mammillary_bridge, &trace2);

    fin_query_params_t params = financial_mammillary_bridge_default_query_params();
    params.target_volatility = 0.21f;
    params.target_trend = 0.48f;
    params.min_similarity = 0.5f;
    params.max_results = 10;

    fin_query_result_t results[FIN_MAMMILLARY_MAX_QUERY_RESULTS];
    uint32_t count = 0;
    int result = financial_mammillary_bridge_query_similar(
        g_mammillary_bridge, &params, results, 10, &count);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);
}
END_TEST

START_TEST(test_mammillary_trace_count)
{
    uint32_t initial_count = financial_mammillary_bridge_get_trace_count(g_mammillary_bridge);

    fin_memory_trace_t trace = {.trade_price = 200.0f, .timestamp_ms = 1700000000000ULL};
    financial_mammillary_bridge_relay_trade(g_mammillary_bridge, &trace);

    uint32_t new_count = financial_mammillary_bridge_get_trace_count(g_mammillary_bridge);
    ck_assert_uint_eq(new_count, initial_count + 1);
}
END_TEST

START_TEST(test_mammillary_reset)
{
    fin_memory_trace_t trace = {.trade_price = 150.0f, .timestamp_ms = 1700000000000ULL};
    financial_mammillary_bridge_relay_trade(g_mammillary_bridge, &trace);

    int result = financial_mammillary_bridge_reset(g_mammillary_bridge);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);

    uint32_t count = financial_mammillary_bridge_get_trace_count(g_mammillary_bridge);
    ck_assert_uint_eq(count, 0);
}
END_TEST

START_TEST(test_mammillary_stats)
{
    fin_memory_trace_t trace = {.trade_price = 100.0f, .timestamp_ms = 1700000000000ULL};
    financial_mammillary_bridge_relay_trade(g_mammillary_bridge, &trace);

    fin_mammillary_bridge_stats_t stats;
    int result = financial_mammillary_bridge_get_stats(g_mammillary_bridge, &stats);
    ck_assert_int_eq(result, FIN_MAMMILLARY_ERR_OK);
    ck_assert_uint_ge(stats.traces_stored, 1);
}
END_TEST

/* ============================================================================
 * Resonance Bridge Tests - Lifecycle
 * ============================================================================ */

START_TEST(test_resonance_create_default)
{
    financial_resonance_bridge_t* bridge = financial_resonance_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_resonance_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_resonance_create_with_config)
{
    fin_resonance_config_t config = financial_resonance_bridge_default_config();
    config.num_oscillators = 6;
    config.jaccard_weight = 0.3f;
    config.kuramoto_weight = 0.4f;

    financial_resonance_bridge_t* bridge = financial_resonance_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_resonance_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_resonance_destroy_null)
{
    financial_resonance_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_resonance_default_config)
{
    fin_resonance_config_t config = financial_resonance_bridge_default_config();
    ck_assert_uint_gt(config.num_oscillators, 0);
    ck_assert(config.kuramoto_coupling_strength > 0.0f);
}
END_TEST

START_TEST(test_resonance_get_state)
{
    fin_resonance_state_t state = financial_resonance_bridge_get_state(g_resonance_bridge);
    ck_assert_int_ne(state, FIN_RESONANCE_STATE_UNINITIALIZED);
}
END_TEST

/* ============================================================================
 * Resonance Bridge Tests - Subsystem Setters
 * ============================================================================ */

START_TEST(test_resonance_set_immune)
{
    int dummy = 1;
    int result = financial_resonance_bridge_set_immune(g_resonance_bridge, &dummy);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

START_TEST(test_resonance_set_immune_null_bridge)
{
    int dummy = 1;
    int result = financial_resonance_bridge_set_immune(NULL, &dummy);
    ck_assert_int_ne(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

START_TEST(test_resonance_set_bbb)
{
    int dummy = 2;
    int result = financial_resonance_bridge_set_bbb(g_resonance_bridge, &dummy);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

START_TEST(test_resonance_set_kg_wiring)
{
    int dummy = 3;
    int result = financial_resonance_bridge_set_kg_wiring(g_resonance_bridge, &dummy);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

START_TEST(test_resonance_set_health_agent)
{
    int dummy = 4;
    int result = financial_resonance_bridge_set_health_agent(g_resonance_bridge, &dummy);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

START_TEST(test_resonance_set_logger)
{
    int dummy = 5;
    int result = financial_resonance_bridge_set_logger(g_resonance_bridge, &dummy);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

/* ============================================================================
 * Resonance Bridge Tests - Core API (Encoding)
 * ============================================================================ */

START_TEST(test_resonance_encode_market)
{
    fin_market_state_t state = {
        .price = 450.75f,
        .volume = 1000000.0f,
        .volatility = 0.18f,
        .momentum = 0.25f,
        .rsi = 65.0f,
        .macd = 2.5f,
        .timestamp_us = 1700000000000000ULL,
        .regime = FIN_REGIME_BULL
    };
    strcpy(state.symbol, "SPY");

    fin_resonance_query_t query;
    int result = financial_resonance_bridge_encode_market(g_resonance_bridge, &state, &query);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
    ck_assert(query.signature_hash != 0);
}
END_TEST

START_TEST(test_resonance_encode_null)
{
    fin_market_state_t state = {.price = 100.0f};
    fin_resonance_query_t query;

    int result = financial_resonance_bridge_encode_market(g_resonance_bridge, NULL, &query);
    ck_assert_int_ne(result, FIN_RESONANCE_ERR_OK);

    result = financial_resonance_bridge_encode_market(g_resonance_bridge, &state, NULL);
    ck_assert_int_ne(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

/* ============================================================================
 * Resonance Bridge Tests - Core API (Similarity)
 * ============================================================================ */

START_TEST(test_resonance_compute_similarity)
{
    fin_resonance_query_t query1 = {
        .signature_hash = 0xABCD1234,
        .phase = 1.5f,
        .num_oscillators = 4
    };
    query1.oscillator_phases[0] = 0.5f;
    query1.oscillator_phases[1] = 1.0f;
    query1.oscillator_phases[2] = 1.5f;
    query1.oscillator_phases[3] = 2.0f;

    fin_resonance_query_t query2 = {
        .signature_hash = 0xABCD1235,
        .phase = 1.6f,
        .num_oscillators = 4
    };
    query2.oscillator_phases[0] = 0.55f;
    query2.oscillator_phases[1] = 1.05f;
    query2.oscillator_phases[2] = 1.55f;
    query2.oscillator_phases[3] = 2.05f;

    fin_resonance_result_t result_data;
    int result = financial_resonance_bridge_compute_similarity(
        g_resonance_bridge, &query1, &query2, &result_data);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
    ck_assert(result_data.combined_score >= 0.0f && result_data.combined_score <= 1.0f);
}
END_TEST

START_TEST(test_resonance_compute_similarity_null)
{
    fin_resonance_query_t query = {.signature_hash = 0x1234};
    fin_resonance_result_t result_data;

    int result = financial_resonance_bridge_compute_similarity(
        g_resonance_bridge, NULL, &query, &result_data);
    ck_assert_int_ne(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

/* ============================================================================
 * Resonance Bridge Tests - Core API (Kuramoto Coherence)
 * ============================================================================ */

START_TEST(test_resonance_kuramoto_coherence)
{
    fin_kuramoto_input_t input = {.num_assets = 4};
    strcpy(input.symbols[0], "SPY");
    strcpy(input.symbols[1], "QQQ");
    strcpy(input.symbols[2], "IWM");
    strcpy(input.symbols[3], "DIA");
    input.phases[0] = 0.1f;
    input.phases[1] = 0.15f;
    input.phases[2] = 0.12f;
    input.phases[3] = 0.11f;
    input.natural_freqs[0] = 1.0f;
    input.natural_freqs[1] = 1.1f;
    input.natural_freqs[2] = 0.95f;
    input.natural_freqs[3] = 1.05f;

    fin_kuramoto_output_t output;
    output.asset_contributions = NULL;
    int result = financial_resonance_bridge_kuramoto_coherence(
        g_resonance_bridge, &input, &output);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
    ck_assert(output.order_parameter >= 0.0f && output.order_parameter <= 1.0f);
}
END_TEST

START_TEST(test_resonance_quick_coherence)
{
    float phases[] = {0.1f, 0.12f, 0.11f, 0.13f, 0.09f};
    float order_param = 0.0f;

    int result = financial_resonance_bridge_quick_coherence(
        g_resonance_bridge, phases, 5, &order_param);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
    ck_assert(order_param >= 0.0f && order_param <= 1.0f);
}
END_TEST

START_TEST(test_resonance_quick_coherence_null)
{
    float order_param;
    int result = financial_resonance_bridge_quick_coherence(
        g_resonance_bridge, NULL, 5, &order_param);
    ck_assert_int_ne(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

/* ============================================================================
 * Resonance Bridge Tests - Pattern Storage
 * ============================================================================ */

START_TEST(test_resonance_store_pattern)
{
    fin_resonance_query_t query = {
        .signature_hash = 0xDEADBEEF,
        .phase = 2.0f,
        .num_oscillators = 2
    };

    int result = financial_resonance_bridge_store_pattern(
        g_resonance_bridge, &query, 150.0f, 0.8f, "Bullish breakout");
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

START_TEST(test_resonance_pattern_count)
{
    uint32_t initial_count = financial_resonance_bridge_get_pattern_count(g_resonance_bridge);

    fin_resonance_query_t query = {.signature_hash = 0x12345678, .phase = 1.0f};
    financial_resonance_bridge_store_pattern(g_resonance_bridge, &query, 100.0f, 0.5f, NULL);

    uint32_t new_count = financial_resonance_bridge_get_pattern_count(g_resonance_bridge);
    ck_assert_uint_eq(new_count, initial_count + 1);
}
END_TEST

START_TEST(test_resonance_stats)
{
    fin_resonance_bridge_stats_t stats;
    int result = financial_resonance_bridge_get_stats(g_resonance_bridge, &stats);
    ck_assert_int_eq(result, FIN_RESONANCE_ERR_OK);
}
END_TEST

/* ============================================================================
 * Autobio Bridge Tests - Lifecycle
 * ============================================================================ */

START_TEST(test_autobio_create_default)
{
    financial_autobio_bridge_t* bridge = financial_autobio_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_autobio_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_autobio_create_with_config)
{
    fin_autobio_config_t config;
    financial_autobio_bridge_default_config(&config);
    config.max_episodes = 2048;
    config.enable_auto_lesson_extraction = true;

    financial_autobio_bridge_t* bridge = financial_autobio_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_autobio_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_autobio_destroy_null)
{
    financial_autobio_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_autobio_default_config)
{
    fin_autobio_config_t config;
    int result = financial_autobio_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_gt(config.max_episodes, 0);
}
END_TEST

START_TEST(test_autobio_get_state)
{
    fin_autobio_state_t state = financial_autobio_bridge_get_state(g_autobio_bridge);
    ck_assert_int_ne(state, FIN_AUTOBIO_STATE_UNINITIALIZED);
}
END_TEST

/* ============================================================================
 * Autobio Bridge Tests - Subsystem Setters
 * ============================================================================ */

START_TEST(test_autobio_set_immune)
{
    int dummy = 1;
    int result = financial_autobio_bridge_set_immune(g_autobio_bridge, &dummy);
    ck_assert_int_eq(result, FIN_AUTOBIO_ERR_OK);
}
END_TEST

START_TEST(test_autobio_set_immune_null_bridge)
{
    int dummy = 1;
    int result = financial_autobio_bridge_set_immune(NULL, &dummy);
    ck_assert_int_ne(result, FIN_AUTOBIO_ERR_OK);
}
END_TEST

START_TEST(test_autobio_set_bbb)
{
    int dummy = 2;
    int result = financial_autobio_bridge_set_bbb(g_autobio_bridge, (bbb_system_t)&dummy);
    ck_assert_int_eq(result, FIN_AUTOBIO_ERR_OK);
}
END_TEST

START_TEST(test_autobio_set_kg_wiring)
{
    int dummy = 3;
    int result = financial_autobio_bridge_set_kg_wiring(g_autobio_bridge, &dummy);
    ck_assert_int_eq(result, FIN_AUTOBIO_ERR_OK);
}
END_TEST

START_TEST(test_autobio_set_health_agent)
{
    int dummy = 4;
    int result = financial_autobio_bridge_set_health_agent(g_autobio_bridge, &dummy);
    ck_assert_int_eq(result, FIN_AUTOBIO_ERR_OK);
}
END_TEST

START_TEST(test_autobio_set_logger)
{
    int dummy = 5;
    int result = financial_autobio_bridge_set_logger(g_autobio_bridge, &dummy);
    ck_assert_int_eq(result, FIN_AUTOBIO_ERR_OK);
}
END_TEST

/* ============================================================================
 * Autobio Bridge Tests - Episode Recording
 * ============================================================================ */

START_TEST(test_autobio_record_episode)
{
    fin_trading_episode_t episode = {
        .episode_id = 0,  /* Auto-assigned */
        .trade_price = 155.50f,
        .trade_quantity = 200.0f,
        .trade_direction = FIN_TRADE_BUY,
        .market_volatility = 0.22f,
        .emotional_state = FIN_EMOTION_FEAR,
        .outcome = -350.0f,
        .timestamp_ms = 1700000000000ULL
    };
    strcpy(episode.description, "Panic buy during market dip");
    strcpy(episode.lesson_learned, "Wait for confirmation before acting on fear");

    int result = financial_autobio_bridge_record_episode(g_autobio_bridge, &episode);
    ck_assert_int_eq(result, FIN_AUTOBIO_ERR_OK);
}
END_TEST

START_TEST(test_autobio_record_episode_null)
{
    int result = financial_autobio_bridge_record_episode(g_autobio_bridge, NULL);
    ck_assert_int_ne(result, FIN_AUTOBIO_ERR_OK);

    fin_trading_episode_t episode = {.trade_price = 100.0f};
    result = financial_autobio_bridge_record_episode(NULL, &episode);
    ck_assert_int_ne(result, FIN_AUTOBIO_ERR_OK);
}
END_TEST

START_TEST(test_autobio_record_simple)
{
    uint64_t episode_id = financial_autobio_bridge_record(
        g_autobio_bridge,
        "Successful momentum trade",
        145.25f,
        100.0f,
        FIN_TRADE_BUY,
        0.15f,
        FIN_EMOTION_JOY,
        500.0f,
        "Follow the trend"
    );
    ck_assert_uint_gt(episode_id, 0);
}
END_TEST

/* ============================================================================
 * Autobio Bridge Tests - Recall by Emotion
 * ============================================================================ */

START_TEST(test_autobio_recall_by_emotion)
{
    /* Record episodes with different emotions */
    financial_autobio_bridge_record(g_autobio_bridge,
        "Fear trade 1", 100.0f, 50.0f, FIN_TRADE_SELL, 0.3f,
        FIN_EMOTION_FEAR, -200.0f, NULL);
    financial_autobio_bridge_record(g_autobio_bridge,
        "Fear trade 2", 105.0f, 50.0f, FIN_TRADE_SELL, 0.35f,
        FIN_EMOTION_FEAR, -150.0f, NULL);
    financial_autobio_bridge_record(g_autobio_bridge,
        "Joy trade 1", 110.0f, 100.0f, FIN_TRADE_BUY, 0.1f,
        FIN_EMOTION_JOY, 300.0f, NULL);

    fin_recall_result_t result;
    int ret = financial_autobio_bridge_recall_by_emotion(
        g_autobio_bridge, FIN_EMOTION_FEAR, 10, &result);
    ck_assert_int_eq(ret, FIN_AUTOBIO_ERR_OK);
    ck_assert_uint_ge(result.count, 2);

    financial_autobio_bridge_free_recall_result(&result);
}
END_TEST

START_TEST(test_autobio_recall_by_emotion_null)
{
    fin_recall_result_t result;
    int ret = financial_autobio_bridge_recall_by_emotion(
        NULL, FIN_EMOTION_FEAR, 10, &result);
    ck_assert_int_ne(ret, FIN_AUTOBIO_ERR_OK);
}
END_TEST

/* ============================================================================
 * Autobio Bridge Tests - Recall by Outcome
 * ============================================================================ */

START_TEST(test_autobio_recall_by_outcome)
{
    /* Record episodes with different outcomes */
    financial_autobio_bridge_record(g_autobio_bridge,
        "Losing trade", 100.0f, 50.0f, FIN_TRADE_BUY, 0.2f,
        FIN_EMOTION_SADNESS, -500.0f, NULL);
    financial_autobio_bridge_record(g_autobio_bridge,
        "Winning trade", 100.0f, 50.0f, FIN_TRADE_BUY, 0.2f,
        FIN_EMOTION_JOY, 800.0f, NULL);
    financial_autobio_bridge_record(g_autobio_bridge,
        "Small win", 100.0f, 50.0f, FIN_TRADE_BUY, 0.2f,
        FIN_EMOTION_NEUTRAL, 100.0f, NULL);

    fin_recall_result_t result;
    int ret = financial_autobio_bridge_recall_by_outcome(
        g_autobio_bridge, 0.0f, 1000.0f, 10, &result);
    ck_assert_int_eq(ret, FIN_AUTOBIO_ERR_OK);
    ck_assert_uint_ge(result.count, 2);

    financial_autobio_bridge_free_recall_result(&result);
}
END_TEST

START_TEST(test_autobio_recall_by_outcome_null)
{
    fin_recall_result_t result;
    int ret = financial_autobio_bridge_recall_by_outcome(
        NULL, -1000.0f, 1000.0f, 10, &result);
    ck_assert_int_ne(ret, FIN_AUTOBIO_ERR_OK);
}
END_TEST

/* ============================================================================
 * Autobio Bridge Tests - Lesson Extraction
 * ============================================================================ */

START_TEST(test_autobio_get_lessons)
{
    /* Record multiple episodes with patterns */
    for (int i = 0; i < 5; i++) {
        financial_autobio_bridge_record(g_autobio_bridge,
            "Pattern trade", 100.0f + (float)i, 50.0f, FIN_TRADE_BUY, 0.25f,
            FIN_EMOTION_GREED, (float)(i % 2 ? 200 : -300), "Don't chase");
    }

    fin_lesson_result_t result;
    int ret = financial_autobio_bridge_get_lessons(
        g_autobio_bridge, FIN_EMOTION_GREED, 5, &result);
    ck_assert_int_eq(ret, FIN_AUTOBIO_ERR_OK);

    financial_autobio_bridge_free_lesson_result(&result);
}
END_TEST

START_TEST(test_autobio_get_lessons_all_emotions)
{
    fin_lesson_result_t result;
    int ret = financial_autobio_bridge_get_lessons(
        g_autobio_bridge, FIN_EMOTION_COUNT, 10, &result);
    ck_assert_int_eq(ret, FIN_AUTOBIO_ERR_OK);

    financial_autobio_bridge_free_lesson_result(&result);
}
END_TEST

/* ============================================================================
 * Autobio Bridge Tests - Stats and Utilities
 * ============================================================================ */

START_TEST(test_autobio_episode_count)
{
    uint32_t initial_count = financial_autobio_bridge_get_episode_count(g_autobio_bridge);

    financial_autobio_bridge_record(g_autobio_bridge,
        "New episode", 100.0f, 50.0f, FIN_TRADE_BUY, 0.1f,
        FIN_EMOTION_NEUTRAL, 0.0f, NULL);

    uint32_t new_count = financial_autobio_bridge_get_episode_count(g_autobio_bridge);
    ck_assert_uint_eq(new_count, initial_count + 1);
}
END_TEST

START_TEST(test_autobio_stats)
{
    fin_autobio_bridge_stats_t stats;
    int result = financial_autobio_bridge_get_stats(g_autobio_bridge, &stats);
    ck_assert_int_eq(result, FIN_AUTOBIO_ERR_OK);
}
END_TEST

START_TEST(test_autobio_reset)
{
    financial_autobio_bridge_record(g_autobio_bridge,
        "Episode to reset", 100.0f, 50.0f, FIN_TRADE_BUY, 0.1f,
        FIN_EMOTION_NEUTRAL, 0.0f, NULL);

    int result = financial_autobio_bridge_reset(g_autobio_bridge);
    ck_assert_int_eq(result, FIN_AUTOBIO_ERR_OK);

    uint32_t count = financial_autobio_bridge_get_episode_count(g_autobio_bridge);
    ck_assert_uint_eq(count, 0);
}
END_TEST

START_TEST(test_autobio_emotion_name)
{
    const char* name = fin_autobio_emotion_name(FIN_EMOTION_FEAR);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_ne(name, "");
}
END_TEST

START_TEST(test_autobio_direction_name)
{
    const char* name = fin_autobio_direction_name(FIN_TRADE_BUY);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_ne(name, "");
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* financial_memory_bridges_suite(void)
{
    Suite* s = suite_create("Financial Memory Bridges");

    /* Working Memory Bridge - Lifecycle */
    TCase* tc_wm_lifecycle = tcase_create("WM Lifecycle");
    tcase_add_test(tc_wm_lifecycle, test_wm_create_default);
    tcase_add_test(tc_wm_lifecycle, test_wm_create_with_config);
    tcase_add_test(tc_wm_lifecycle, test_wm_create_null_output);
    tcase_add_test(tc_wm_lifecycle, test_wm_destroy_null);
    tcase_add_test(tc_wm_lifecycle, test_wm_default_config);
    suite_add_tcase(s, tc_wm_lifecycle);

    /* Working Memory Bridge - Subsystem Setters */
    TCase* tc_wm_setters = tcase_create("WM Subsystem Setters");
    tcase_add_checked_fixture(tc_wm_setters, setup_working_memory, teardown_working_memory);
    tcase_add_test(tc_wm_setters, test_wm_set_immune);
    tcase_add_test(tc_wm_setters, test_wm_set_immune_null_bridge);
    tcase_add_test(tc_wm_setters, test_wm_set_bbb);
    tcase_add_test(tc_wm_setters, test_wm_set_kg_wiring);
    tcase_add_test(tc_wm_setters, test_wm_set_health_agent);
    tcase_add_test(tc_wm_setters, test_wm_set_logger);
    tcase_add_test(tc_wm_setters, test_wm_set_bio_async);
    suite_add_tcase(s, tc_wm_setters);

    /* Working Memory Bridge - Core API */
    TCase* tc_wm_core = tcase_create("WM Core API");
    tcase_add_checked_fixture(tc_wm_core, setup_working_memory, teardown_working_memory);
    tcase_add_test(tc_wm_core, test_wm_add_item);
    tcase_add_test(tc_wm_core, test_wm_add_null_item);
    tcase_add_test(tc_wm_core, test_wm_get_active);
    tcase_add_test(tc_wm_core, test_wm_get_active_null);
    tcase_add_test(tc_wm_core, test_wm_decay_step);
    tcase_add_test(tc_wm_core, test_wm_refresh);
    tcase_add_test(tc_wm_core, test_wm_refresh_invalid_index);
    tcase_add_test(tc_wm_core, test_wm_get_by_type);
    tcase_add_test(tc_wm_core, test_wm_clear);
    tcase_add_test(tc_wm_core, test_wm_capacity_limit);
    tcase_add_test(tc_wm_core, test_wm_stats);
    suite_add_tcase(s, tc_wm_core);

    /* Mammillary Bridge - Lifecycle */
    TCase* tc_mamm_lifecycle = tcase_create("Mammillary Lifecycle");
    tcase_add_test(tc_mamm_lifecycle, test_mammillary_create_default);
    tcase_add_test(tc_mamm_lifecycle, test_mammillary_create_with_config);
    tcase_add_test(tc_mamm_lifecycle, test_mammillary_destroy_null);
    tcase_add_test(tc_mamm_lifecycle, test_mammillary_default_config);
    tcase_add_checked_fixture(tc_mamm_lifecycle, setup_mammillary, teardown_mammillary);
    tcase_add_test(tc_mamm_lifecycle, test_mammillary_get_state);
    suite_add_tcase(s, tc_mamm_lifecycle);

    /* Mammillary Bridge - Subsystem Setters */
    TCase* tc_mamm_setters = tcase_create("Mammillary Subsystem Setters");
    tcase_add_checked_fixture(tc_mamm_setters, setup_mammillary, teardown_mammillary);
    tcase_add_test(tc_mamm_setters, test_mammillary_set_immune);
    tcase_add_test(tc_mamm_setters, test_mammillary_set_immune_null_bridge);
    tcase_add_test(tc_mamm_setters, test_mammillary_set_bbb);
    tcase_add_test(tc_mamm_setters, test_mammillary_set_kg_wiring);
    tcase_add_test(tc_mamm_setters, test_mammillary_set_health_agent);
    tcase_add_test(tc_mamm_setters, test_mammillary_set_logger);
    suite_add_tcase(s, tc_mamm_setters);

    /* Mammillary Bridge - Core API */
    TCase* tc_mamm_core = tcase_create("Mammillary Core API");
    tcase_add_checked_fixture(tc_mamm_core, setup_mammillary, teardown_mammillary);
    tcase_add_test(tc_mamm_core, test_mammillary_relay_trade);
    tcase_add_test(tc_mamm_core, test_mammillary_relay_trade_null);
    tcase_add_test(tc_mamm_core, test_mammillary_consolidate);
    tcase_add_test(tc_mamm_core, test_mammillary_query_similar);
    tcase_add_test(tc_mamm_core, test_mammillary_trace_count);
    tcase_add_test(tc_mamm_core, test_mammillary_reset);
    tcase_add_test(tc_mamm_core, test_mammillary_stats);
    suite_add_tcase(s, tc_mamm_core);

    /* Resonance Bridge - Lifecycle */
    TCase* tc_res_lifecycle = tcase_create("Resonance Lifecycle");
    tcase_add_test(tc_res_lifecycle, test_resonance_create_default);
    tcase_add_test(tc_res_lifecycle, test_resonance_create_with_config);
    tcase_add_test(tc_res_lifecycle, test_resonance_destroy_null);
    tcase_add_test(tc_res_lifecycle, test_resonance_default_config);
    tcase_add_checked_fixture(tc_res_lifecycle, setup_resonance, teardown_resonance);
    tcase_add_test(tc_res_lifecycle, test_resonance_get_state);
    suite_add_tcase(s, tc_res_lifecycle);

    /* Resonance Bridge - Subsystem Setters */
    TCase* tc_res_setters = tcase_create("Resonance Subsystem Setters");
    tcase_add_checked_fixture(tc_res_setters, setup_resonance, teardown_resonance);
    tcase_add_test(tc_res_setters, test_resonance_set_immune);
    tcase_add_test(tc_res_setters, test_resonance_set_immune_null_bridge);
    tcase_add_test(tc_res_setters, test_resonance_set_bbb);
    tcase_add_test(tc_res_setters, test_resonance_set_kg_wiring);
    tcase_add_test(tc_res_setters, test_resonance_set_health_agent);
    tcase_add_test(tc_res_setters, test_resonance_set_logger);
    suite_add_tcase(s, tc_res_setters);

    /* Resonance Bridge - Encoding */
    TCase* tc_res_encoding = tcase_create("Resonance Encoding");
    tcase_add_checked_fixture(tc_res_encoding, setup_resonance, teardown_resonance);
    tcase_add_test(tc_res_encoding, test_resonance_encode_market);
    tcase_add_test(tc_res_encoding, test_resonance_encode_null);
    suite_add_tcase(s, tc_res_encoding);

    /* Resonance Bridge - Similarity */
    TCase* tc_res_similarity = tcase_create("Resonance Similarity");
    tcase_add_checked_fixture(tc_res_similarity, setup_resonance, teardown_resonance);
    tcase_add_test(tc_res_similarity, test_resonance_compute_similarity);
    tcase_add_test(tc_res_similarity, test_resonance_compute_similarity_null);
    suite_add_tcase(s, tc_res_similarity);

    /* Resonance Bridge - Kuramoto Coherence */
    TCase* tc_res_kuramoto = tcase_create("Resonance Kuramoto");
    tcase_add_checked_fixture(tc_res_kuramoto, setup_resonance, teardown_resonance);
    tcase_add_test(tc_res_kuramoto, test_resonance_kuramoto_coherence);
    tcase_add_test(tc_res_kuramoto, test_resonance_quick_coherence);
    tcase_add_test(tc_res_kuramoto, test_resonance_quick_coherence_null);
    suite_add_tcase(s, tc_res_kuramoto);

    /* Resonance Bridge - Pattern Storage */
    TCase* tc_res_patterns = tcase_create("Resonance Patterns");
    tcase_add_checked_fixture(tc_res_patterns, setup_resonance, teardown_resonance);
    tcase_add_test(tc_res_patterns, test_resonance_store_pattern);
    tcase_add_test(tc_res_patterns, test_resonance_pattern_count);
    tcase_add_test(tc_res_patterns, test_resonance_stats);
    suite_add_tcase(s, tc_res_patterns);

    /* Autobio Bridge - Lifecycle */
    TCase* tc_autobio_lifecycle = tcase_create("Autobio Lifecycle");
    tcase_add_test(tc_autobio_lifecycle, test_autobio_create_default);
    tcase_add_test(tc_autobio_lifecycle, test_autobio_create_with_config);
    tcase_add_test(tc_autobio_lifecycle, test_autobio_destroy_null);
    tcase_add_test(tc_autobio_lifecycle, test_autobio_default_config);
    tcase_add_checked_fixture(tc_autobio_lifecycle, setup_autobio, teardown_autobio);
    tcase_add_test(tc_autobio_lifecycle, test_autobio_get_state);
    suite_add_tcase(s, tc_autobio_lifecycle);

    /* Autobio Bridge - Subsystem Setters */
    TCase* tc_autobio_setters = tcase_create("Autobio Subsystem Setters");
    tcase_add_checked_fixture(tc_autobio_setters, setup_autobio, teardown_autobio);
    tcase_add_test(tc_autobio_setters, test_autobio_set_immune);
    tcase_add_test(tc_autobio_setters, test_autobio_set_immune_null_bridge);
    tcase_add_test(tc_autobio_setters, test_autobio_set_bbb);
    tcase_add_test(tc_autobio_setters, test_autobio_set_kg_wiring);
    tcase_add_test(tc_autobio_setters, test_autobio_set_health_agent);
    tcase_add_test(tc_autobio_setters, test_autobio_set_logger);
    suite_add_tcase(s, tc_autobio_setters);

    /* Autobio Bridge - Episode Recording */
    TCase* tc_autobio_record = tcase_create("Autobio Recording");
    tcase_add_checked_fixture(tc_autobio_record, setup_autobio, teardown_autobio);
    tcase_add_test(tc_autobio_record, test_autobio_record_episode);
    tcase_add_test(tc_autobio_record, test_autobio_record_episode_null);
    tcase_add_test(tc_autobio_record, test_autobio_record_simple);
    suite_add_tcase(s, tc_autobio_record);

    /* Autobio Bridge - Recall */
    TCase* tc_autobio_recall = tcase_create("Autobio Recall");
    tcase_add_checked_fixture(tc_autobio_recall, setup_autobio, teardown_autobio);
    tcase_add_test(tc_autobio_recall, test_autobio_recall_by_emotion);
    tcase_add_test(tc_autobio_recall, test_autobio_recall_by_emotion_null);
    tcase_add_test(tc_autobio_recall, test_autobio_recall_by_outcome);
    tcase_add_test(tc_autobio_recall, test_autobio_recall_by_outcome_null);
    suite_add_tcase(s, tc_autobio_recall);

    /* Autobio Bridge - Lesson Extraction */
    TCase* tc_autobio_lessons = tcase_create("Autobio Lessons");
    tcase_add_checked_fixture(tc_autobio_lessons, setup_autobio, teardown_autobio);
    tcase_add_test(tc_autobio_lessons, test_autobio_get_lessons);
    tcase_add_test(tc_autobio_lessons, test_autobio_get_lessons_all_emotions);
    suite_add_tcase(s, tc_autobio_lessons);

    /* Autobio Bridge - Stats and Utilities */
    TCase* tc_autobio_utils = tcase_create("Autobio Utils");
    tcase_add_checked_fixture(tc_autobio_utils, setup_autobio, teardown_autobio);
    tcase_add_test(tc_autobio_utils, test_autobio_episode_count);
    tcase_add_test(tc_autobio_utils, test_autobio_stats);
    tcase_add_test(tc_autobio_utils, test_autobio_reset);
    tcase_add_test(tc_autobio_utils, test_autobio_emotion_name);
    tcase_add_test(tc_autobio_utils, test_autobio_direction_name);
    suite_add_tcase(s, tc_autobio_utils);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = financial_memory_bridges_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
