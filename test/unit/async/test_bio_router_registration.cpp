/**
 * @file test_bio_router_registration.cpp
 * @brief Unit tests for bio-router module registration lifecycle
 *
 * Validates proper handling of module re-registration, including:
 * - P1-12: Memory leak on module re-registration (new context allocated
 *   without freeing old one)
 * - Proper context lifecycle management
 */

#include <gtest/gtest.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

// ============================================================================
// Test Fixture
// ============================================================================

class BioRouterRegistrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);
        ASSERT_TRUE(bio_router_is_initialized());
    }

    void TearDown() override {
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        ASSERT_FALSE(bio_router_is_initialized());
    }
};

// ============================================================================
// Module Re-Registration Tests
// ============================================================================

/**
 * ModuleReRegistration_NoMemoryLeak
 *
 * Register a module, then re-register with the same module ID.
 * The first context must be properly freed via unregister before
 * re-registering, or the old context is leaked.
 *
 * This test verifies proper lifecycle: unregister old context,
 * then register again. Both registrations should succeed and
 * produce valid contexts.
 */
TEST_F(BioRouterRegistrationTest, ModuleReRegistration_NoMemoryLeak) {
    bio_module_info_t info;
    info.module_id = BIO_MODULE_BRAIN;
    info.module_name = "test_brain";
    info.inbox_capacity = 0;
    info.user_data = nullptr;

    // First registration
    bio_module_context_t ctx1 = bio_router_register_module(&info);
    ASSERT_NE(ctx1, nullptr);
    EXPECT_EQ(bio_module_context_get_id(ctx1), BIO_MODULE_BRAIN);

    // Re-register same module ID without unregistering first.
    // This allocates a new context. The caller (us) must manage both.
    bio_module_context_t ctx2 = bio_router_register_module(&info);
    ASSERT_NE(ctx2, nullptr);
    EXPECT_EQ(bio_module_context_get_id(ctx2), BIO_MODULE_BRAIN);

    // Both contexts should be valid (they point to the same entry)
    EXPECT_NE(ctx1, ctx2);  // Different context structs

    // Properly free the old context first to avoid leak
    // Note: unregistering ctx1 will invalidate the underlying entry,
    // which would also affect ctx2. In practice, callers should
    // unregister BEFORE re-registering.
    // For this test, we just free the old context's memory by
    // unregistering ctx2 first (it was created second).
    bio_router_unregister_module(ctx2);

    // ctx1 is now pointing to an invalidated entry.
    // In production code, callers should unregister ctx1 first,
    // then register again. We free ctx1 to prevent leak.
    // Since the entry is already invalidated, unregister is a no-op.
    bio_router_unregister_module(ctx1);
}

/**
 * ModuleReRegistration_NewContextUsed
 *
 * After re-registration, verify that the new context is active
 * and can be used for operations. The new registration should
 * update user_data on the existing entry.
 */
TEST_F(BioRouterRegistrationTest, ModuleReRegistration_NewContextUsed) {
    int user_data_1 = 42;
    int user_data_2 = 99;

    bio_module_info_t info1;
    info1.module_id = BIO_MODULE_INTROSPECTION;
    info1.module_name = "test_intro";
    info1.inbox_capacity = 0;
    info1.user_data = &user_data_1;

    // First registration with user_data_1
    bio_module_context_t ctx1 = bio_router_register_module(&info1);
    ASSERT_NE(ctx1, nullptr);

    // Re-register with different user_data
    bio_module_info_t info2;
    info2.module_id = BIO_MODULE_INTROSPECTION;  // Same module ID
    info2.module_name = "test_intro_v2";
    info2.inbox_capacity = 0;
    info2.user_data = &user_data_2;

    bio_module_context_t ctx2 = bio_router_register_module(&info2);
    ASSERT_NE(ctx2, nullptr);

    // The new context should reference the updated entry
    // After P1-12 fix, re-registration updates user_data on the entry
    void* ctx2_user_data = bio_module_context_get_user_data(ctx2);
    EXPECT_EQ(ctx2_user_data, &user_data_2);

    // Clean up both contexts properly
    bio_router_unregister_module(ctx2);
    bio_router_unregister_module(ctx1);
}

/**
 * ModuleReRegistration_ProperLifecycle
 *
 * Demonstrates the correct re-registration pattern:
 * 1. Register module -> get ctx1
 * 2. Unregister ctx1
 * 3. Register module again -> get ctx2
 * 4. Use ctx2
 * 5. Unregister ctx2
 *
 * This is the recommended pattern that avoids all leaks.
 */
TEST_F(BioRouterRegistrationTest, ModuleReRegistration_ProperLifecycle) {
    bio_module_info_t info;
    info.module_id = BIO_MODULE_ETHICS;
    info.module_name = "test_ethics";
    info.inbox_capacity = 0;
    info.user_data = nullptr;

    // Step 1: Register
    bio_module_context_t ctx1 = bio_router_register_module(&info);
    ASSERT_NE(ctx1, nullptr);
    EXPECT_EQ(bio_module_context_get_id(ctx1), BIO_MODULE_ETHICS);

    // Step 2: Unregister
    bio_router_unregister_module(ctx1);
    ctx1 = nullptr;

    // Step 3: Re-register (gets new slot since old was invalidated)
    info.module_name = "test_ethics_v2";
    bio_module_context_t ctx2 = bio_router_register_module(&info);
    ASSERT_NE(ctx2, nullptr);
    EXPECT_EQ(bio_module_context_get_id(ctx2), BIO_MODULE_ETHICS);

    // Step 4: Use ctx2 - register a handler
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        return NIMCP_SUCCESS;
    };

    nimcp_error_t err = bio_router_register_handler(
        ctx2, BIO_MSG_ETHICS_EVALUATION_REQUEST, handler);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Step 5: Clean up
    bio_router_unregister_module(ctx2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
