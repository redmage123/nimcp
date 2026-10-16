//=============================================================================
// test_lang_attach_unit.cpp — broca/wernicke attach_snn_pop pure state cycle
//=============================================================================
/**
 * @file test_lang_attach_unit.cpp
 * @brief Unit: state-machine of broca_attach_snn_pop / wernicke_attach_snn_pop.
 *
 * WHAT: Verifies the (snn, pop_id) cache held inside each language adapter
 *       transitions correctly across attach / unbind / re-attach calls, and
 *       that the get-accessors are NULL-safe.
 * WHY:  The bug class we're guarding against is: calloc zeroes the adapter
 *       struct, which would make the pop_id field 0 — but 0 is a VALID pop
 *       id, so a freshly-created adapter would falsely report "attached to
 *       pop 0" if we didn't explicitly initialize to -1. This unit test pins
 *       that invariant in place. We also verify the unbind path clears BOTH
 *       (snn, pop_id) together so callers never observe (snn != NULL, id<0).
 * HOW:  No SNN network needed — we pass a sentinel non-NULL pointer for the
 *       snn arg since the adapter only stores the pointer (it never derefs
 *       it). This keeps the unit test fast and deterministic.
 */

#include <gtest/gtest.h>

extern "C" {
#include "nimcp.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
}

namespace {

/* Sentinel pointer used as the snn argument. attach() only stores it and
 * never dereferences it, so any non-NULL value is sufficient for unit-level
 * state-machine verification. */
static snn_network_t* const SENTINEL_SNN =
    reinterpret_cast<snn_network_t*>(static_cast<uintptr_t>(0xDEADBEEF));

static constexpr int VALID_POP_ID_ZERO = 0;   // 0 is a valid pop id (calloc trap)
static constexpr int VALID_POP_ID_NONZERO = 7;
static constexpr int UNBIND_POP_ID = -1;

class BrocaAttachTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS); }
    static void TearDownTestSuite() { nimcp_shutdown(); }

    void SetUp() override {
        broca_config_t cfg = broca_default_config();
        // Disable bio-async so no router is required for this unit test.
        cfg.enable_bio_async = false;
        cfg.enable_events = false;
        cfg.enable_lexicon = true;
        adapter = broca_create(&cfg);
        ASSERT_NE(adapter, nullptr);
    }
    void TearDown() override {
        if (adapter) broca_destroy(adapter);
        adapter = nullptr;
    }
    broca_adapter_t* adapter = nullptr;
};

class WernickeAttachTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS); }
    static void TearDownTestSuite() { nimcp_shutdown(); }

    void SetUp() override {
        wernicke_config_t cfg = wernicke_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_events = false;
        cfg.enable_lexicon = true;
        adapter = wernicke_create(&cfg);
        ASSERT_NE(adapter, nullptr);
    }
    void TearDown() override {
        if (adapter) wernicke_destroy(adapter);
        adapter = nullptr;
    }
    wernicke_adapter_t* adapter = nullptr;
};

//=============================================================================
// Broca: NULL safety + initial state
//=============================================================================

TEST_F(BrocaAttachTest, GetReturnsSentinelOnNullAdapter) {
    EXPECT_EQ(broca_get_snn_pop_id(nullptr), -1);
    EXPECT_EQ(broca_get_snn_network(nullptr), nullptr);
}

TEST_F(BrocaAttachTest, AttachReturnsFalseOnNullAdapter) {
    // Attach with NULL adapter must not crash; must return false.
    EXPECT_FALSE(broca_attach_snn_pop(nullptr, SENTINEL_SNN, VALID_POP_ID_NONZERO));
}

TEST_F(BrocaAttachTest, FreshAdapterReportsUnbound) {
    // CRITICAL invariant: a freshly-created adapter must report unbound,
    // NOT pop_id=0 (which calloc would have left). This catches the
    // "0 is a valid pop id" trap.
    EXPECT_EQ(broca_get_snn_pop_id(adapter), -1);
    EXPECT_EQ(broca_get_snn_network(adapter), nullptr);
}

//=============================================================================
// Broca: attach / unbind / re-attach state cycle
//=============================================================================

TEST_F(BrocaAttachTest, AttachStoresPopIdAndSnn) {
    EXPECT_TRUE(broca_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_EQ(broca_get_snn_pop_id(adapter), VALID_POP_ID_NONZERO);
    EXPECT_EQ(broca_get_snn_network(adapter), SENTINEL_SNN);
}

TEST_F(BrocaAttachTest, AttachAcceptsPopIdZero) {
    // 0 is a valid pop id — must be stored and reported back, NOT
    // misinterpreted as "unbound".
    EXPECT_TRUE(broca_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_ZERO));
    EXPECT_EQ(broca_get_snn_pop_id(adapter), VALID_POP_ID_ZERO);
    EXPECT_EQ(broca_get_snn_network(adapter), SENTINEL_SNN);
}

TEST_F(BrocaAttachTest, NegativePopIdUnbinds) {
    // First bind, then unbind via negative pop id.
    ASSERT_TRUE(broca_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_TRUE(broca_attach_snn_pop(adapter, SENTINEL_SNN, UNBIND_POP_ID));
    // Both legs MUST clear together — never (snn != NULL, id < 0).
    EXPECT_EQ(broca_get_snn_pop_id(adapter), -1);
    EXPECT_EQ(broca_get_snn_network(adapter), nullptr);
}

TEST_F(BrocaAttachTest, NullSnnUnbinds) {
    // First bind, then unbind via NULL snn.
    ASSERT_TRUE(broca_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_TRUE(broca_attach_snn_pop(adapter, nullptr, VALID_POP_ID_NONZERO));
    EXPECT_EQ(broca_get_snn_pop_id(adapter), -1);
    EXPECT_EQ(broca_get_snn_network(adapter), nullptr);
}

TEST_F(BrocaAttachTest, ReAttachReplacesPriorBinding) {
    ASSERT_TRUE(broca_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    snn_network_t* new_sentinel =
        reinterpret_cast<snn_network_t*>(static_cast<uintptr_t>(0xCAFE1234));
    EXPECT_TRUE(broca_attach_snn_pop(adapter, new_sentinel, VALID_POP_ID_ZERO));
    EXPECT_EQ(broca_get_snn_pop_id(adapter), VALID_POP_ID_ZERO);
    EXPECT_EQ(broca_get_snn_network(adapter), new_sentinel);
}

TEST_F(BrocaAttachTest, IdempotentAttach) {
    // Calling attach twice with the same args is a no-op pass.
    EXPECT_TRUE(broca_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_TRUE(broca_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_EQ(broca_get_snn_pop_id(adapter), VALID_POP_ID_NONZERO);
    EXPECT_EQ(broca_get_snn_network(adapter), SENTINEL_SNN);
}

//=============================================================================
// Wernicke: mirror the broca tests — same invariants, different adapter
//=============================================================================

TEST_F(WernickeAttachTest, GetReturnsSentinelOnNullAdapter) {
    EXPECT_EQ(wernicke_get_snn_pop_id(nullptr), -1);
    EXPECT_EQ(wernicke_get_snn_network(nullptr), nullptr);
}

TEST_F(WernickeAttachTest, AttachReturnsFalseOnNullAdapter) {
    EXPECT_FALSE(wernicke_attach_snn_pop(nullptr, SENTINEL_SNN, VALID_POP_ID_NONZERO));
}

TEST_F(WernickeAttachTest, FreshAdapterReportsUnbound) {
    EXPECT_EQ(wernicke_get_snn_pop_id(adapter), -1);
    EXPECT_EQ(wernicke_get_snn_network(adapter), nullptr);
}

TEST_F(WernickeAttachTest, AttachStoresPopIdAndSnn) {
    EXPECT_TRUE(wernicke_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_EQ(wernicke_get_snn_pop_id(adapter), VALID_POP_ID_NONZERO);
    EXPECT_EQ(wernicke_get_snn_network(adapter), SENTINEL_SNN);
}

TEST_F(WernickeAttachTest, AttachAcceptsPopIdZero) {
    EXPECT_TRUE(wernicke_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_ZERO));
    EXPECT_EQ(wernicke_get_snn_pop_id(adapter), VALID_POP_ID_ZERO);
    EXPECT_EQ(wernicke_get_snn_network(adapter), SENTINEL_SNN);
}

TEST_F(WernickeAttachTest, NegativePopIdUnbinds) {
    ASSERT_TRUE(wernicke_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_TRUE(wernicke_attach_snn_pop(adapter, SENTINEL_SNN, UNBIND_POP_ID));
    EXPECT_EQ(wernicke_get_snn_pop_id(adapter), -1);
    EXPECT_EQ(wernicke_get_snn_network(adapter), nullptr);
}

TEST_F(WernickeAttachTest, NullSnnUnbinds) {
    ASSERT_TRUE(wernicke_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_TRUE(wernicke_attach_snn_pop(adapter, nullptr, VALID_POP_ID_NONZERO));
    EXPECT_EQ(wernicke_get_snn_pop_id(adapter), -1);
    EXPECT_EQ(wernicke_get_snn_network(adapter), nullptr);
}

TEST_F(WernickeAttachTest, ReAttachReplacesPriorBinding) {
    ASSERT_TRUE(wernicke_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    snn_network_t* new_sentinel =
        reinterpret_cast<snn_network_t*>(static_cast<uintptr_t>(0x12345678));
    EXPECT_TRUE(wernicke_attach_snn_pop(adapter, new_sentinel, VALID_POP_ID_ZERO));
    EXPECT_EQ(wernicke_get_snn_pop_id(adapter), VALID_POP_ID_ZERO);
    EXPECT_EQ(wernicke_get_snn_network(adapter), new_sentinel);
}

TEST_F(WernickeAttachTest, IdempotentAttach) {
    EXPECT_TRUE(wernicke_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_TRUE(wernicke_attach_snn_pop(adapter, SENTINEL_SNN, VALID_POP_ID_NONZERO));
    EXPECT_EQ(wernicke_get_snn_pop_id(adapter), VALID_POP_ID_NONZERO);
    EXPECT_EQ(wernicke_get_snn_network(adapter), SENTINEL_SNN);
}

}  // namespace
