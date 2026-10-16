//=============================================================================
// Phase 1.4 & 1.5 Simple Smoke Test
//=============================================================================

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdatomic.h>

// Phase 1.4: Pattern CoW
#include "middleware/patterns/nimcp_pattern_cow.h"

// Phase 1.5: Event Queue Pool
#include "middleware/events/nimcp_event_queue.h"
#include "middleware/events/nimcp_event_types.h"

void test_phase1_4_cow() {
    printf("=== Phase 1.4: Pattern CoW Test ===\n");

    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};

    // Create
    pattern_cow_t* cow1 = pattern_cow_create(data, 4);
    assert(cow1 != NULL);
    assert(pattern_cow_refcount(cow1) == 1);
    printf("✓ Created CoW, refcount=1\n");

    // Clone
    pattern_cow_t* cow2 = pattern_cow_clone(cow1);
    assert(cow2 == cow1);
    assert(pattern_cow_refcount(cow1) == 2);
    printf("✓ Cloned CoW, refcount=2\n");

    // Release
    pattern_cow_release(cow1);
    assert(pattern_cow_refcount(cow2) == 1);
    printf("✓ Released first ref, refcount=1\n");

    pattern_cow_release(cow2);
    printf("✓ Released second ref, freed\n\n");
}

void test_phase1_5_pool() {
    printf("=== Phase 1.5: Event Queue Pool Test ===\n");

    event_queue_config_t config = event_queue_default_config();
    config.capacity = 10;
    config.max_payload_size = 256;

    event_queue_t queue = event_queue_create(&config);
    assert(queue != NULL);
    printf("✓ Created event queue with pool\n");

    // Create event with small payload (will use pool)
    uint32_t payload[] = {100, 200, 300};
    event_t event = event_create_custom(payload, sizeof(payload), "test",
                                         MW_EVENT_PRIORITY_NORMAL,
                                         EVENT_SOURCE_PATTERN_DETECTOR);

    // Enqueue
    assert(event_queue_enqueue(queue, &event));
    printf("✓ Enqueued event (payload from pool)\n");

    // Dequeue
    event_t out;
    assert(event_queue_dequeue(queue, &out));
    printf("✓ Dequeued event\n");

    // Verify
    assert(out.type == EVENT_TYPE_CUSTOM);
    uint32_t* out_data = (uint32_t*)out.data.custom.data;
    assert(out_data[0] == 100);
    printf("✓ Payload correct\n");

    event_free(&out);
    printf("✓ Freed event (returned to pool)\n");

    event_queue_destroy(queue);
    printf("✓ Destroyed queue\n\n");
}

int main() {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║   Phase 1.4 & 1.5 Smoke Test        ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    test_phase1_4_cow();
    test_phase1_5_pool();

    printf("╔══════════════════════════════════════╗\n");
    printf("║       ALL TESTS PASSED ✓             ║\n");
    printf("╚══════════════════════════════════════╝\n");

    return 0;
}
