#include <stdio.h>
#include "src/include/nimcp.h"

int main() {
    // Initialize
    nimcp_init();

    // Create original brain
    nimcp_brain_t original = nimcp_brain_create(
        "test",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        10, 3
    );

    printf("Original brain created\n");

    // Probe original
    nimcp_brain_probe_t probe_orig;
    nimcp_brain_probe(original, &probe_orig);
    printf("Original brain:\n");
    printf("  is_cow_clone: %d\n", probe_orig.is_cow_clone);
    printf("  cow_ref_count: %u\n", probe_orig.cow_ref_count);
    printf("  cow_shared_bytes: %zu\n", probe_orig.cow_shared_bytes);
    printf("  cow_private_bytes: %zu\n", probe_orig.cow_private_bytes);
    printf("  num_neurons: %u\n", probe_orig.num_neurons);
    printf("  memory_bytes: %zu\n", probe_orig.memory_bytes);

    // Clone with COW
    nimcp_brain_t clone = nimcp_brain_clone_cow(original);
    printf("\nClone created\n");

    // Probe clone
    nimcp_brain_probe_t probe_clone;
    nimcp_brain_probe(clone, &probe_clone);
    printf("Clone brain:\n");
    printf("  is_cow_clone: %d\n", probe_clone.is_cow_clone);
    printf("  cow_ref_count: %u\n", probe_clone.cow_ref_count);
    printf("  cow_shared_bytes: %zu\n", probe_clone.cow_shared_bytes);
    printf("  cow_private_bytes: %zu\n", probe_clone.cow_private_bytes);
    printf("  num_neurons: %u\n", probe_clone.num_neurons);
    printf("  memory_bytes: %zu\n", probe_clone.memory_bytes);

    // Cleanup
    nimcp_brain_destroy(clone);
    nimcp_brain_destroy(original);
    nimcp_shutdown();

    return 0;
}
