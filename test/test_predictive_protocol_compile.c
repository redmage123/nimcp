/**
 * @file test_predictive_protocol_compile.c
 * @brief Simple compilation test for predictive protocol
 */

#include "networking/nlp/nimcp_predictive_protocol.h"
#include <stdio.h>

int main(void) {
    /* Test basic lifecycle */
    predictive_protocol_config_t config = {
        .prediction_window_ms = 1000,
        .history_buffer_size = 512,
        .confidence_threshold = 0.5f,
        .enable_prefetch = true,
        .enable_bio_async = false
    };

    predictive_protocol_t* protocol = predictive_protocol_create(&config);
    if (!protocol) {
        fprintf(stderr, "Failed to create protocol\n");
        return 1;
    }

    /* Test observation */
    int ret = predictive_protocol_observe_message(
        protocol, 0x100, 0x200, 0x300, 1000000);

    if (ret != 0) {
        fprintf(stderr, "Failed to observe message\n");
        predictive_protocol_destroy(protocol);
        return 1;
    }

    /* Get stats */
    predictive_stats_t stats = predictive_protocol_get_stats(protocol);
    printf("Stats: predictions=%llu accuracy=%.2f\n",
        stats.predictions_made, stats.prediction_accuracy);

    predictive_protocol_destroy(protocol);
    printf("Predictive protocol compilation test passed\n");
    return 0;
}
