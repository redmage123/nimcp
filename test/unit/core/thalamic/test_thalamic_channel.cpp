/**
 * @file test_thalamic_channel.cpp
 * @brief Unit tests for the shared thalamic-channel adapter.
 */

#include <gtest/gtest.h>

#include "core/thalamic/nimcp_thalamic_channel.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "utils/error/nimcp_error_codes.h"

namespace {

/* RAII router fixture used by most tests. */
struct RouterFixture {
    thalamic_router_t* router = nullptr;

    RouterFixture() {
        thalamic_router_config_t cfg = thalamic_router_default_config();
        /* Second-messenger and quantum routing are irrelevant for channel
         * unit tests and pull in heavier deps — disable to keep init cheap. */
        cfg.enable_second_messengers = false;
        cfg.enable_quantum_routing   = false;
        cfg.enable_statistics        = true;
        router = thalamic_router_create(&cfg);
    }
    ~RouterFixture() {
        if (router) thalamic_router_destroy(router);
    }
};

constexpr float kTol = 1e-4f;

}  // namespace

/* ============================================================================
 * 1. Create with valid router
 * ============================================================================ */
TEST(ThalamicChannel, CreateWithValidRouter) {
    RouterFixture f;
    ASSERT_NE(f.router, nullptr);

    thalamic_channel_t* ch = thalamic_channel_create(f.router, /*source_id=*/42);
    ASSERT_NE(ch, nullptr);
    EXPECT_EQ(ch->source_id, 42u);
    EXPECT_EQ(ch->n_destinations, 0u);
    EXPECT_EQ(ch->router, f.router);
    thalamic_channel_destroy(ch);
}

/* ============================================================================
 * 2. Create with NULL router
 * ============================================================================ */
TEST(ThalamicChannel, CreateWithNullRouterReturnsNull) {
    thalamic_channel_t* ch = thalamic_channel_create(nullptr, 0);
    EXPECT_EQ(ch, nullptr);
}

/* ============================================================================
 * 3. Destroy NULL is safe
 * ============================================================================ */
TEST(ThalamicChannel, DestroyNullIsNoOp) {
    thalamic_channel_destroy(nullptr);  /* must not crash */
    SUCCEED();
}

/* ============================================================================
 * 4. Add destinations
 * ============================================================================ */
TEST(ThalamicChannel, AddDestinationIncrementsCount) {
    RouterFixture f;
    thalamic_channel_t* ch = thalamic_channel_create(f.router, 1);
    ASSERT_NE(ch, nullptr);

    EXPECT_EQ(thalamic_channel_add_destination(ch, 100), NIMCP_SUCCESS);
    EXPECT_EQ(thalamic_channel_add_destination(ch, 200), NIMCP_SUCCESS);
    ASSERT_EQ(ch->n_destinations, 2u);
    EXPECT_EQ(ch->destinations[0], 100u);
    EXPECT_EQ(ch->destinations[1], 200u);

    thalamic_channel_destroy(ch);
}

/* ============================================================================
 * 5. Capacity exceeded
 * ============================================================================ */
TEST(ThalamicChannel, AddDestinationBeyondCapacityReturnsError) {
    RouterFixture f;
    thalamic_channel_t* ch = thalamic_channel_create(f.router, 1);
    ASSERT_NE(ch, nullptr);

    for (uint32_t i = 0; i < THALAMIC_CHANNEL_MAX_DESTS; i++) {
        ASSERT_EQ(thalamic_channel_add_destination(ch, 1000 + i), NIMCP_SUCCESS);
    }
    EXPECT_EQ(ch->n_destinations, (uint32_t)THALAMIC_CHANNEL_MAX_DESTS);

    /* One beyond capacity → capacity-exceeded. */
    EXPECT_EQ(thalamic_channel_add_destination(ch, 9999),
              NIMCP_ERROR_CAPACITY_EXCEEDED);
    EXPECT_EQ(ch->n_destinations, (uint32_t)THALAMIC_CHANNEL_MAX_DESTS);

    thalamic_channel_destroy(ch);
}

/* ============================================================================
 * 6. get_gate of registered destination returns 1.0
 * ============================================================================ */
TEST(ThalamicChannel, GetGateRegisteredReturnsInitialOne) {
    RouterFixture f;
    thalamic_channel_t* ch = thalamic_channel_create(f.router, 1);
    ASSERT_NE(ch, nullptr);

    ASSERT_EQ(thalamic_channel_add_destination(ch, 7), NIMCP_SUCCESS);
    float gate = thalamic_channel_get_gate(ch, 7);
    EXPECT_NEAR(gate, 1.0f, kTol);

    thalamic_channel_destroy(ch);
}

/* ============================================================================
 * 7. get_gate of unknown destination returns 1.0 (conservative)
 * ============================================================================ */
TEST(ThalamicChannel, GetGateUnknownDestReturnsOne) {
    RouterFixture f;
    thalamic_channel_t* ch = thalamic_channel_create(f.router, 1);
    ASSERT_NE(ch, nullptr);

    /* No destinations added. */
    float gate = thalamic_channel_get_gate(ch, 12345);
    EXPECT_NEAR(gate, 1.0f, kTol);

    /* Add one; query a different id. */
    ASSERT_EQ(thalamic_channel_add_destination(ch, 7), NIMCP_SUCCESS);
    gate = thalamic_channel_get_gate(ch, 8);
    EXPECT_NEAR(gate, 1.0f, kTol);

    /* NULL channel also returns 1.0 conservatively. */
    EXPECT_NEAR(thalamic_channel_get_gate(nullptr, 7), 1.0f, kTol);

    thalamic_channel_destroy(ch);
}

/* No-op callback used so the router's synchronous HIGH-priority path can
 * mark a delivery as successful. Without a registered callback the router's
 * deliver_signal() reports false, which is a router implementation detail
 * unrelated to the channel's bookkeeping. */
static int g_callback_hits = 0;
static void noop_callback(uint32_t /*dest*/, const float* /*sig*/,
                          uint32_t /*size*/, float /*attention*/,
                          void* /*user*/) {
    g_callback_hits++;
}

/* ============================================================================
 * 8. submit increments submits_total; get_gate increments gates_fetched
 * ============================================================================ */
TEST(ThalamicChannel, SubmitAndGateCountersIncrement) {
    RouterFixture f;
    thalamic_channel_t* ch = thalamic_channel_create(f.router, 1);
    ASSERT_NE(ch, nullptr);

    /* Register callback for destination 7 so HIGH-priority delivers succeed. */
    g_callback_hits = 0;
    ASSERT_TRUE(thalamic_router_set_callback(f.router, /*dest_id=*/7,
                                             noop_callback, nullptr));

    ASSERT_EQ(thalamic_channel_add_destination(ch, 7), NIMCP_SUCCESS);

    /* get_gate bumps gates_fetched. */
    EXPECT_EQ(ch->gates_fetched, 0u);
    (void)thalamic_channel_get_gate(ch, 7);
    EXPECT_EQ(ch->gates_fetched, 1u);
    (void)thalamic_channel_get_gate(ch, 7);
    EXPECT_EQ(ch->gates_fetched, 2u);

    /* submit bumps submits_total on success. */
    EXPECT_EQ(ch->submits_total, 0u);
    float payload[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    nimcp_error_t rc = thalamic_channel_submit(
        ch, /*dest_id=*/7, payload, /*n=*/4, THALAMIC_PRIORITY_NORMAL);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    EXPECT_EQ(ch->submits_total, 1u);

    /* HIGH priority path delivers synchronously — also increments. */
    rc = thalamic_channel_submit(
        ch, /*dest_id=*/7, payload, /*n=*/4, THALAMIC_PRIORITY_HIGH);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    EXPECT_EQ(ch->submits_total, 2u);

    thalamic_channel_destroy(ch);
}
