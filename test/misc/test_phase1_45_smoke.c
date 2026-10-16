//=============================================================================
// Phase 1.4 & 1.5 Smoke Test
//=============================================================================

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdatomic.h>

// Phase 1.4: Pattern CoW
#include "middleware/patterns/nimcp_pattern_cow.h"
#include "middleware/patterns/nimcp_pattern_library.h"

// Phase 1.5: Event Queue Pool
#include "middleware/events/nimcp_event_queue.h"
#include "middleware/events/nimcp_event_types.h"

void test_phase1_4_pattern_cow() {
    printf("=== Phase 1.4: Pattern CoW Test ===\n");

    // Create pattern data
    float pattern_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t dimension = 4;

    // Create CoW wrapper
    pattern_cow_t* cow1 = pattern_cow_create(pattern_data, dimension);
    assert(cow1 != NULL);
    assert(pattern_cow_refcount(cow1) == 1);
    printf("✓ Created CoW pattern, refcount=1\n");

    // Clone (should increment refcount)
    pattern_cow_t* cow2 = pattern_cow_clone(cow1);
    assert(cow2 == cow1);  // Should be same pointer
    assert(pattern_cow_refcount(cow1) == 2);
    printf("✓ Cloned CoW pattern, refcount=2\n");

    // Verify data access
    const float* data = pattern_cow_data(cow1);
    assert(data != NULL);
    assert(data[0] == 1.0f && data[3] == 4.0f);
    printf("✓ Data access works correctly\n");

    // Release first reference
    pattern_cow_release(cow1);
    // Note: Can't safely access cow1 anymore, but cow2 should still work
    assert(pattern_cow_refcount(cow2) == 1);
    printf("✓ Released first ref, refcount=1\n");

    // Release second reference (should free)
    pattern_cow_release(cow2);
    printf("✓ Released second ref, memory freed\n");

    printf("Phase 1.4: PASS ✓\n\n");
}

void test_phase1_5_event_queue_pool() {
    printf("=== Phase 1.5: Event Queue Pool Test ===\n");

    // Create event queue with pool
    event_queue_config_t config = event_queue_default_config();
    config.capacity = 10;
    config.max_payload_size = 256;  // Phase 1.5: pool for payloads

    event_queue_t queue = event_queue_create(&config);
    assert(queue != NULL);
    printf("✓ Created event queue with payload pool\n");

    // Create event with payload (CUSTOM event with small payload)
    uint32_t payload_data[] = {100, 200, 300};
    event_t event = event_create_custom(
        payload_data,
        sizeof(payload_data),
        "test_payload",
        MW_EVENT_PRIORITY_NORMAL,
        EVENT_SOURCE_PATTERN_DETECTOR
    );
    printf("✓ Created event with payload\n");

    // Enqueue (should use pool for payload allocation)
    bool enqueued = event_queue_enqueue(queue, &event);
    assert(enqueued);
    printf("✓ Enqueued event (payload from pool)\n");

    // Verify queue size
    assert(event_queue_size(queue) == 1);
    printf("✓ Queue size = 1\n");

    // Dequeue
    event_t dequeued_event;
    bool dequeued = event_queue_dequeue(queue, &dequeued_event);
    assert(dequeued);
    assert(dequeued_event.type == EVENT_TYPE_CUSTOM);
    assert(dequeued_event.data.custom.data_size == sizeof(payload_data));
    printf("✓ Dequeued event successfully\n");

    // Verify payload data
    uint32_t* dequeued_data = (uint32_t*)dequeued_event.data.custom.data;
    assert(dequeued_data[0] == 100 && dequeued_data[2] == 300);
    printf("✓ Payload data intact\n");

    // Clean up (should release to pool)
    event_free(&dequeued_event);
    printf("✓ Freed event (returned to pool)\n");

    // Destroy queue (should destroy pool)
    event_queue_destroy(queue);
    printf("✓ Destroyed queue and pool\n");

    printf("Phase 1.5: PASS ✓\n\n");
}

void test_pattern_library_with_cow() {
    printf("=== Phase 1.4: Pattern Library KNN Pool Test ===\n");

    // Create pattern library
    pattern_library_config_t config = pattern_library_default_config();
    config.max_capacity = 10;
    config.max_dimension = 4;

    pattern_library_t library = pattern_library_create(&config);
    assert(library != NULL);
    printf("✓ Created pattern library\n");

    // Add patterns
    float pattern1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float pattern2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float pattern3[] = {0.0f, 0.0f, 1.0f, 0.0f};

    pattern_library_add(library, pattern1, 4, "p1", "pattern1");
    pattern_library_add(library, pattern2, 4, "p2", "pattern2");
    pattern_library_add(library, pattern3, 4, "p3", "pattern3");
    printf("✓ Added 3 patterns\n");

    // Test KNN (uses memory pool internally)
    float query[] = {0.9f, 0.1f, 0.0f, 0.0f};
    uint32_t ids[3];
    float similarities[3];

    bool success = pattern_library_knn(library, query, 4, 2, ids, similarities);
    assert(success);
    printf("✓ KNN search successful (used memory pool for temp arrays)\n");

    // Verify results
    assert(ids[0] == 0);  // Should match pattern1 best
    printf("✓ KNN results correct\n");

    // Clean up
    pattern_library_destroy(library);
    printf("✓ Destroyed pattern library\n");

    printf("Phase 1.4 Integration: PASS ✓\n\n");
}

int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║        Phase 1.4 & 1.5 Smoke Test Suite                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    test_phase1_4_pattern_cow();
    test_phase1_5_event_queue_pool();
    test_pattern_library_with_cow();

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                 ALL TESTS PASSED ✓                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    return 0;
}
