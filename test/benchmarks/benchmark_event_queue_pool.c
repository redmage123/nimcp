//=============================================================================
// benchmark_event_queue_pool.c - Phase 1.5 Event Queue Pool Benchmark
//=============================================================================

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#include "middleware/events/nimcp_event_queue.h"
#include "middleware/events/nimcp_event_types.h"
#include "utils/memory/nimcp_memory.h"

#define PAYLOAD_SIZE 128  // bytes
#define NUM_EVENTS 10000
#define NUM_TRIALS 10

//=============================================================================
// Benchmark: Baseline (No Pool - Direct malloc)
//=============================================================================

static double benchmark_baseline_no_pool() {
    // Create queue with pool disabled (max_payload_size = 0)
    // When pool is disabled, internal copies use nimcp_malloc
    event_queue_config_t config = event_queue_default_config();
    config.capacity = NUM_EVENTS;
    config.max_payload_size = 0;  // Disable pool, force malloc for internal copies

    event_queue_t queue = event_queue_create(&config);

    // Stack-allocated payload data - queue will copy it during enqueue
    uint32_t payload_data[PAYLOAD_SIZE / sizeof(uint32_t)];
    for (size_t i = 0; i < sizeof(payload_data) / sizeof(uint32_t); i++) {
        payload_data[i] = i;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Enqueue events
    // Note: event_queue_enqueue copies the payload data internally using malloc
    // (since pool is disabled), so we can pass a pointer to stack data
    for (uint32_t i = 0; i < NUM_EVENTS; i++) {
        event_t event = event_create_custom(
            payload_data,
            PAYLOAD_SIZE,
            "benchmark",
            MW_EVENT_PRIORITY_NORMAL,
            EVENT_SOURCE_PATTERN_DETECTOR
        );
        event_queue_enqueue(queue, &event);
    }

    // Dequeue events and free them
    // Since pool is disabled, the queue used nimcp_malloc for payloads,
    // so event_free (which uses nimcp_free) will work correctly
    for (uint32_t i = 0; i < NUM_EVENTS; i++) {
        event_t event;
        event_queue_dequeue(queue, &event);
        event_free(&event);  // Safe: payload was nimcp_malloc'd
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    event_queue_destroy(queue);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    return elapsed;
}

//=============================================================================
// Benchmark: Phase 1.5 (With Pool)
//=============================================================================

static double benchmark_with_pool() {
    // Create queue with pool enabled
    // When pool is enabled, internal copies use pool for small payloads
    // On dequeue, pool memory is copied to malloc'd memory for safe event_free()
    event_queue_config_t config = event_queue_default_config();
    config.capacity = NUM_EVENTS;
    config.max_payload_size = 256;  // Phase 1.5: Enable pool for payloads ≤ 256 bytes

    event_queue_t queue = event_queue_create(&config);

    // Stack-allocated payload data - queue will copy it during enqueue
    uint32_t payload_data[PAYLOAD_SIZE / sizeof(uint32_t)];
    for (size_t i = 0; i < sizeof(payload_data) / sizeof(uint32_t); i++) {
        payload_data[i] = i;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Enqueue events (uses pool for internal copies)
    // Note: event_queue_enqueue copies the payload data internally using pool
    // (since payload size fits), so we can pass a pointer to stack data
    for (uint32_t i = 0; i < NUM_EVENTS; i++) {
        event_t event = event_create_custom(
            payload_data,
            PAYLOAD_SIZE,
            "benchmark",
            MW_EVENT_PRIORITY_NORMAL,
            EVENT_SOURCE_PATTERN_DETECTOR
        );
        event_queue_enqueue(queue, &event);
    }

    // Dequeue events and free them
    // The queue now properly converts pool-allocated payloads to malloc'd memory
    // on dequeue, so event_free() works correctly
    for (uint32_t i = 0; i < NUM_EVENTS; i++) {
        event_t event;
        event_queue_dequeue(queue, &event);
        event_free(&event);  // Safe: dequeue copies pool memory to malloc'd memory
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    event_queue_destroy(queue);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    return elapsed;
}

//=============================================================================
// Main Benchmark
//=============================================================================

int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║       Phase 1.5: Event Queue Pool Benchmark                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("Configuration:\n");
    printf("  Payload size:       %u bytes\n", PAYLOAD_SIZE);
    printf("  Number of events:   %u\n", NUM_EVENTS);
    printf("  Trials:             %u\n\n", NUM_TRIALS);

    // Warmup
    benchmark_baseline_no_pool();
    benchmark_with_pool();

    // Baseline: No Pool (malloc/free)
    printf("Running baseline (no pool, direct malloc)...\n");
    double baseline_total = 0.0;
    for (int i = 0; i < NUM_TRIALS; i++) {
        double t = benchmark_baseline_no_pool();
        baseline_total += t;
        printf("  Trial %d: %.6f seconds\n", i+1, t);
    }
    double baseline_avg = baseline_total / NUM_TRIALS;

    printf("\nRunning Phase 1.5 (with pool)...\n");
    double pool_total = 0.0;
    for (int i = 0; i < NUM_TRIALS; i++) {
        double t = benchmark_with_pool();
        pool_total += t;
        printf("  Trial %d: %.6f seconds\n", i+1, t);
    }
    double pool_avg = pool_total / NUM_TRIALS;

    // Results
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                        RESULTS                               ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Baseline (malloc):           %.6f seconds                ║\n", baseline_avg);
    printf("║ Phase 1.5 (pool):            %.6f seconds                ║\n", pool_avg);
    printf("║                                                              ║\n");
    printf("║ Speedup:                     %.2fx                         ║\n", baseline_avg / pool_avg);
    printf("║ Time saved per event:        %.2f µs                       ║\n",
           ((baseline_avg - pool_avg) / NUM_EVENTS) * 1e6);
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    double events_per_sec_baseline = NUM_EVENTS / baseline_avg;
    double events_per_sec_pool = NUM_EVENTS / pool_avg;

    printf("\nThroughput:\n");
    printf("  Baseline: %.0f events/second\n", events_per_sec_baseline);
    printf("  Phase 1.5: %.0f events/second\n", events_per_sec_pool);

    printf("\n✓ Phase 1.5 event queue pool benchmark complete\n\n");

    return 0;
}
