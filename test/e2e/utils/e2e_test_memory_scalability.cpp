/**
 * @file e2e_test_memory_scalability.cpp
 * @brief E2E tests for memory scalability with brain creation
 *
 * WHAT: Validate that brain creation/destruction with all modules doesn't crash
 * WHY: Brain creation with ethics/wellbeing/etc. pushes >100K memory allocations.
 *      The old linked list tracking caused munmap_chunk crashes at scale.
 * HOW: Create brains with various configurations, train, destroy — no crash.
 */

#include "e2e_test_framework.h"
#include <vector>
#include <cstring>

using namespace nimcp::e2e;

class MemoryScalabilityTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        nimcp_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_enable_debug_output(false);
    }

    void TearDown() override
    {
        nimcp_memory_check_leaks();
        nimcp_shutdown();
    }
};

E2E_TEST(MemoryScalabilityTest, BrainCreateDestroyClean)
{
    E2E_PIPELINE_START("Brain Create-Train-Destroy");

    nimcp_brain_t brain = nullptr;

    E2E_STAGE_BEGIN("Create brain", 30000);
    {
        brain = nimcp_brain_create(
            "scalability_test",
            NIMCP_BRAIN_SMALL,
            NIMCP_TASK_CLASSIFICATION,
            10,
            3
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Configure and train", 30000);
    {
        nimcp_training_config_t config = nimcp_training_config_default();
        config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
        config.optimizer_type = NIMCP_API_OPT_SGD;
        config.learning_rate = 0.01f;

        nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
        E2E_ASSERT_SUCCESS(status, "Training configuration failed");

        std::vector<float> features;
        std::vector<float> labels;
        TestDataGenerator::generate_training_batch(10, 10, 3, features, labels);

        for (int step = 0; step < 10; step++) {
            nimcp_training_result_t result;
            nimcp_brain_train_step(brain, features.data(), 10, labels.data(), 3, &result);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Destroy brain", 30000);
    {
        nimcp_brain_destroy(brain);
        brain = nullptr;
    }
    E2E_STAGE_END();

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    // No crash is the primary assertion — stats are secondary
}

E2E_TEST(MemoryScalabilityTest, MultipleBrainsSequential)
{
    E2E_PIPELINE_START("Multiple Brains Sequential");

    for (int b = 0; b < 3; b++) {
        char name[64];
        snprintf(name, sizeof(name), "seq_brain_%d", b);

        E2E_STAGE_BEGIN(name, 30000);
        {
            nimcp_brain_t brain = nimcp_brain_create(
                name,
                NIMCP_BRAIN_SMALL,
                NIMCP_TASK_CLASSIFICATION,
                10,
                3
            );
            E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
            nimcp_brain_destroy(brain);
        }
        E2E_STAGE_END();
    }
}

E2E_TEST(MemoryScalabilityTest, BrainWithAllModulesNoMunmapCrash)
{
    E2E_PIPELINE_START("Brain With All Modules - No Crash");

    nimcp_brain_t brain = nullptr;

    // Create a brain that exercises many subsystems, generating many allocations.
    // This is the original crash scenario where >100K tracked allocations caused
    // get_guard_size() to exceed MAX_ITERATIONS.
    E2E_STAGE_BEGIN("Create brain with many subsystems", 30000);
    {
        brain = nimcp_brain_create(
            "full_module_test",
            NIMCP_BRAIN_MEDIUM,
            NIMCP_TASK_CLASSIFICATION,
            20,
            5
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Train and exercise", 30000);
    {
        nimcp_training_config_t config = nimcp_training_config_default();
        config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
        config.optimizer_type = NIMCP_API_OPT_ADAM;
        config.learning_rate = 0.001f;

        nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
        E2E_ASSERT_SUCCESS(status, "Training config failed");

        std::vector<float> features;
        std::vector<float> labels;
        TestDataGenerator::generate_training_batch(10, 20, 5, features, labels);

        for (int step = 0; step < 20; step++) {
            nimcp_training_result_t result;
            nimcp_brain_train_step(brain, features.data(), 20, labels.data(), 5, &result);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Destroy brain - no munmap crash", 30000);
    {
        nimcp_brain_destroy(brain);
        brain = nullptr;
    }
    E2E_STAGE_END();

    // If we reach here, the munmap_chunk crash is fixed
    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
}
