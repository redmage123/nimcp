/**
 * @file e2e_test_portia_bio_async_pipeline.cpp
 * @brief End-to-end test for Portia bio-async message pipeline
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"

class PortiaBioAsyncPipelineE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_log_init(NULL);
        nimcp_bio_async_init(nullptr);
        bio_router_init(nullptr);
    }

    void TearDown() override {
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_log_shutdown();
    }
};

TEST_F(PortiaBioAsyncPipelineE2ETest, FullMessageFlow) {
    // Test complete message flow through Portia modules
    nimcp_error_t err = portia_init(nullptr);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Update multiple times to generate messages
    for (int i = 0; i < 10; i++) {
        err = portia_update();
        ASSERT_EQ(err, NIMCP_SUCCESS);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    portia_destroy();
    nimcp_log(LOG_LEVEL_INFO, "FullMessageFlow: PASS");
}

TEST_F(PortiaBioAsyncPipelineE2ETest, MessageHandlingUnderLoad) {
    // Test message handling under high load
    nimcp_error_t err = portia_init(nullptr);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    std::atomic<int> updates{0};

    for (int i = 0; i < 50; i++) {
        err = portia_update();
        if (err == NIMCP_SUCCESS) {
            updates++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_GT(updates.load(), 40) << "Should complete most updates under load";

    portia_destroy();
    nimcp_log(LOG_LEVEL_INFO, "MessageHandlingUnderLoad: PASS - Updates=%d", updates.load());
}
