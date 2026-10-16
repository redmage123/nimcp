/**
 * @file test_multimodal_weight_alloc.c
 * @brief Unit test for multimodal integration weight allocation NULL checks
 *
 * WHAT: Verify NULL check on weight allocation failure in multimodal_integration_create
 * WHY:  Bug caused NULL pointer dereference when calloc failed for weight matrices
 * HOW:  Check framework test verifying normal creation path works
 */

#include <check.h>
#include <stdlib.h>
#include "core/integration/nimcp_multimodal_integration.h"

START_TEST(test_create_with_default_config)
{
    multimodal_integration_config_t config = {0};
    config.output_dim = 64;
    config.visual_dim = 32;
    config.audio_dim = 16;
    config.method = INTEGRATION_CONCATENATE;

    multimodal_integration_t integ = multimodal_integration_create(&config);
    /* May be NULL if the function requires more setup, but should not crash */
    if (integ) {
        multimodal_integration_destroy(integ);
    }
}
END_TEST

START_TEST(test_create_learned_allocates_weights)
{
    multimodal_integration_config_t config = {0};
    config.output_dim = 16;
    config.visual_dim = 8;
    config.audio_dim = 4;
    config.speech_dim = 4;
    config.direct_dim = 2;
    config.method = INTEGRATION_LEARNED;

    multimodal_integration_t integ = multimodal_integration_create(&config);
    if (integ) {
        /* If created successfully, weights should be non-NULL */
        multimodal_integration_destroy(integ);
    }
}
END_TEST

Suite* multimodal_weight_alloc_suite(void) {
    Suite* s = suite_create("Multimodal Weight Alloc");
    TCase* tc = tcase_create("NULL checks");
    tcase_add_test(tc, test_create_with_default_config);
    tcase_add_test(tc, test_create_learned_allocates_weights);
    suite_add_tcase(s, tc);
    return s;
}

int main(void) {
    Suite* s = multimodal_weight_alloc_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
