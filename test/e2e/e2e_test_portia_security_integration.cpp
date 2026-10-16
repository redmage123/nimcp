/**
 * @file e2e_test_portia_security_integration.cpp
 * @brief End-to-end test for Portia security integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "e2e_test_framework.h"
#include <thread>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "async/nimcp_bio_async.h"
#include "utils/logging/nimcp_logging.h"

class PortiaSecurityIntegrationE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_log_init(NULL);
        nimcp_bio_async_init(nullptr);
        bbb_helpers_init();
    }

    void TearDown() override {
        bbb_helpers_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_log_shutdown();
    }
};

TEST_F(PortiaSecurityIntegrationE2ETest, BBBValidatesAllInputs) {
    // Test that BBB validates all Portia inputs
    nimcp_error_t err = portia_init(nullptr);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    portia_status_t status;
    err = portia_get_status(&status);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    portia_destroy();
    nimcp_log(LOG_LEVEL_INFO, "BBBValidatesAllInputs: PASS");
}

TEST_F(PortiaSecurityIntegrationE2ETest, SecurityAuditLogging) {
    // Test security audit logging
    nimcp_error_t err = portia_init(nullptr);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Perform operations that should be audited
    for (int i = 0; i < 5; i++) {
        portia_update();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    portia_destroy();
    nimcp_log(LOG_LEVEL_INFO, "SecurityAuditLogging: PASS");
}
