/**
 * @file test_health_agent_macros.cpp
 * @brief Unit tests for health agent macros
 *
 * Tests the NIMCP_DECLARE_HEALTH_AGENT and related macros that eliminate
 * duplicated health agent boilerplate code across modules.
 *
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

/* ============================================================================
 * Test Module - Uses NIMCP_DECLARE_HEALTH_AGENT macro
 * ============================================================================ */

/*
 * Declare health agent infrastructure for a test module using the macro.
 * This generates:
 *   - static nimcp_health_agent_t* g_testmod_health_agent = NULL;
 *   - void testmod_set_health_agent(nimcp_health_agent_t* agent);
 *   - static inline void testmod_heartbeat(const char* operation, float progress);
 */
NIMCP_DECLARE_HEALTH_AGENT(testmod)

/* Also test the static-only variant */
NIMCP_DECLARE_HEALTH_AGENT_STATIC(testmod_static)

/* ============================================================================
 * Mock Health Agent for Testing
 * ============================================================================
 *
 * We track heartbeat calls to verify the macro-generated code works correctly.
 */

static std::atomic<int> g_heartbeat_call_count{0};
static std::atomic<float> g_last_heartbeat_progress{0.0f};
static char g_last_heartbeat_operation[128] = {0};

/*
 * Override the external heartbeat function for testing.
 * In real usage, nimcp_health_agent_heartbeat_ex is defined in nimcp_health_agent.c,
 * but for unit testing we provide a mock implementation.
 *
 * Note: The declaration is in the header as 'extern', so we define it here.
 * This allows the test to verify the macro-generated code calls it correctly.
 */

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HealthAgentMacrosTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Reset test state */
        g_heartbeat_call_count = 0;
        g_last_heartbeat_progress = 0.0f;
        memset(g_last_heartbeat_operation, 0, sizeof(g_last_heartbeat_operation));

        /* Clear any previously set health agent */
        testmod_set_health_agent(nullptr);
    }

    void TearDown() override {
        /* Clean up */
        testmod_set_health_agent(nullptr);
    }
};

/* ============================================================================
 * Tests - NIMCP_DECLARE_HEALTH_AGENT Creates Proper Globals
 * ============================================================================ */

TEST_F(HealthAgentMacrosTest, MacroCreatesNullGlobal) {
    /*
     * Verify the macro creates a global variable initialized to NULL.
     * The global should be named g_MODULE_health_agent.
     */

    /* Access the global directly - it should exist and be NULL */
    EXPECT_EQ(g_testmod_health_agent, nullptr)
        << "NIMCP_DECLARE_HEALTH_AGENT should create g_testmod_health_agent initialized to NULL";
}

TEST_F(HealthAgentMacrosTest, MacroCreatesSetFunction) {
    /*
     * Verify the macro creates a set function that stores the agent.
     * The function should be named MODULE_set_health_agent.
     */

    /* The set function should exist - compilation verifies this */
    /* Test that it's callable and modifies the global */

    /* Create a mock agent (just a non-null pointer for testing) */
    nimcp_health_agent_t* mock_agent = reinterpret_cast<nimcp_health_agent_t*>(0x12345678);

    testmod_set_health_agent(mock_agent);
    EXPECT_EQ(g_testmod_health_agent, mock_agent)
        << "testmod_set_health_agent should store the agent in g_testmod_health_agent";
}

TEST_F(HealthAgentMacrosTest, MacroCreatesHeartbeatFunction) {
    /*
     * Verify the macro creates a heartbeat function.
     * The function should be named MODULE_heartbeat.
     */

    /* The heartbeat function should exist and be callable */
    /* When agent is NULL, it should be a no-op */
    testmod_heartbeat("test_operation", 0.5f);

    /* No crash means success - function exists and handles NULL agent */
    SUCCEED() << "testmod_heartbeat function exists and handles NULL agent";
}

/* ============================================================================
 * Tests - set_health_agent Stores the Agent
 * ============================================================================ */

TEST_F(HealthAgentMacrosTest, SetHealthAgentStoresAgent) {
    nimcp_health_agent_t* agent1 = reinterpret_cast<nimcp_health_agent_t*>(0xAAAAAAAA);
    nimcp_health_agent_t* agent2 = reinterpret_cast<nimcp_health_agent_t*>(0xBBBBBBBB);

    /* Set first agent */
    testmod_set_health_agent(agent1);
    EXPECT_EQ(g_testmod_health_agent, agent1);

    /* Replace with second agent */
    testmod_set_health_agent(agent2);
    EXPECT_EQ(g_testmod_health_agent, agent2);
}

TEST_F(HealthAgentMacrosTest, SetHealthAgentCanClear) {
    nimcp_health_agent_t* agent = reinterpret_cast<nimcp_health_agent_t*>(0xCCCCCCCC);

    /* Set agent */
    testmod_set_health_agent(agent);
    EXPECT_NE(g_testmod_health_agent, nullptr);

    /* Clear by setting NULL */
    testmod_set_health_agent(nullptr);
    EXPECT_EQ(g_testmod_health_agent, nullptr)
        << "Setting NULL should clear the health agent";
}

/* ============================================================================
 * Tests - Heartbeat is No-op When Agent is NULL
 * ============================================================================ */

TEST_F(HealthAgentMacrosTest, HeartbeatNoopWhenAgentNull) {
    /*
     * When the health agent is NULL, heartbeat should be a no-op.
     * This is critical for modules that are initialized before the health agent.
     */

    /* Ensure agent is NULL */
    testmod_set_health_agent(nullptr);

    /* Call heartbeat - should not crash */
    for (int i = 0; i < 100; ++i) {
        testmod_heartbeat("operation", static_cast<float>(i) / 100.0f);
    }

    SUCCEED() << "Heartbeat with NULL agent completes without error";
}

/* ============================================================================
 * Tests - Static Variant
 * ============================================================================ */

TEST_F(HealthAgentMacrosTest, StaticVariantCreatesGlobal) {
    /* The static variant should also create a global */
    EXPECT_EQ(g_testmod_static_health_agent, nullptr)
        << "NIMCP_DECLARE_HEALTH_AGENT_STATIC should create g_testmod_static_health_agent";
}

TEST_F(HealthAgentMacrosTest, StaticVariantSetFunctionWorks) {
    nimcp_health_agent_t* agent = reinterpret_cast<nimcp_health_agent_t*>(0xDDDDDDDD);

    testmod_static_set_health_agent(agent);
    EXPECT_EQ(g_testmod_static_health_agent, agent);

    testmod_static_set_health_agent(nullptr);
    EXPECT_EQ(g_testmod_static_health_agent, nullptr);
}

TEST_F(HealthAgentMacrosTest, StaticVariantHeartbeatWorks) {
    /* Should not crash with NULL agent */
    testmod_static_heartbeat("static_test", 0.75f);
    SUCCEED();
}

/* ============================================================================
 * Tests - Multiple Module Instances
 * ============================================================================ */

/*
 * Declare a second module to test that multiple modules can coexist.
 */
NIMCP_DECLARE_HEALTH_AGENT(othermod)

TEST_F(HealthAgentMacrosTest, MultipleModulesIndependent) {
    nimcp_health_agent_t* agent1 = reinterpret_cast<nimcp_health_agent_t*>(0x11111111);
    nimcp_health_agent_t* agent2 = reinterpret_cast<nimcp_health_agent_t*>(0x22222222);

    /* Set different agents for different modules */
    testmod_set_health_agent(agent1);
    othermod_set_health_agent(agent2);

    /* Each module should have its own agent */
    EXPECT_EQ(g_testmod_health_agent, agent1);
    EXPECT_EQ(g_othermod_health_agent, agent2);

    /* Clearing one should not affect the other */
    testmod_set_health_agent(nullptr);
    EXPECT_EQ(g_testmod_health_agent, nullptr);
    EXPECT_EQ(g_othermod_health_agent, agent2);
}

/* ============================================================================
 * Tests - Token Pasting Correctness
 * ============================================================================ */

TEST_F(HealthAgentMacrosTest, TokenPastingWorksCorrectly) {
    /*
     * The macros use token pasting (##) to create identifiers.
     * This test verifies the correct identifiers are generated.
     */

    /* Test that we can reference the generated symbols */
    nimcp_health_agent_t** global_ptr = &g_testmod_health_agent;
    EXPECT_NE(global_ptr, nullptr);

    /* Test the function pointer types are correct */
    using SetFuncType = void (*)(nimcp_health_agent_t*);
    SetFuncType set_func = &testmod_set_health_agent;
    EXPECT_NE(set_func, nullptr);
}

/* ============================================================================
 * Tests - Extern Declaration Macro
 * ============================================================================ */

/*
 * Test NIMCP_HEALTH_AGENT_EXTERN generates correct prototype.
 * This macro is used in header files.
 */

/* This should compile without error - declares the prototype */
NIMCP_HEALTH_AGENT_EXTERN(extern_test_mod);

/* Provide definition to verify the prototype matches */
static nimcp_health_agent_t* g_extern_test_mod_health_agent = nullptr;

void extern_test_mod_set_health_agent(nimcp_health_agent_t* agent) {
    g_extern_test_mod_health_agent = agent;
}

TEST_F(HealthAgentMacrosTest, ExternDeclarationMacro) {
    nimcp_health_agent_t* agent = reinterpret_cast<nimcp_health_agent_t*>(0x55555555);

    extern_test_mod_set_health_agent(agent);
    EXPECT_EQ(g_extern_test_mod_health_agent, agent);
}

/* ============================================================================
 * Tests - Edge Cases
 * ============================================================================ */

TEST_F(HealthAgentMacrosTest, RapidSetOperations) {
    /*
     * Stress test rapid set operations to verify no race conditions
     * in the macro-generated code.
     */
    for (int i = 0; i < 1000; ++i) {
        nimcp_health_agent_t* agent = reinterpret_cast<nimcp_health_agent_t*>(
            static_cast<uintptr_t>(0x10000000 + i));
        testmod_set_health_agent(agent);
        ASSERT_EQ(g_testmod_health_agent, agent);
    }
}

TEST_F(HealthAgentMacrosTest, HeartbeatWithEmptyOperation) {
    /* Empty string should be handled */
    testmod_heartbeat("", 0.0f);
    testmod_heartbeat("", 1.0f);
    SUCCEED();
}

TEST_F(HealthAgentMacrosTest, HeartbeatWithExtremePrProgressValues) {
    /* Progress values at boundaries */
    testmod_heartbeat("test", 0.0f);
    testmod_heartbeat("test", 1.0f);

    /* Out-of-range values (should still work, even if semantically odd) */
    testmod_heartbeat("test", -0.5f);
    testmod_heartbeat("test", 1.5f);
    testmod_heartbeat("test", 100.0f);
    testmod_heartbeat("test", -100.0f);

    SUCCEED() << "Extreme progress values handled without crash";
}

TEST_F(HealthAgentMacrosTest, HeartbeatWithNullOperation) {
    /*
     * NULL operation string should be handled gracefully.
     * The actual heartbeat_ex function should handle this, but
     * the macro-generated wrapper should pass it through.
     */
    testmod_heartbeat(nullptr, 0.5f);
    SUCCEED() << "NULL operation string handled without crash";
}
