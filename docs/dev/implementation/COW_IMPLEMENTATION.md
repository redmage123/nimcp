# Page-Level Copy-on-Write (COW) System

## Overview

The NIMCP page-level COW system provides fine-grained memory sharing for large data structures with 4KB page granularity. Unlike object-level COW which copies entire objects, page-level COW only copies the specific 4KB pages that are modified, enabling efficient memory usage for neural network weights, knowledge graphs, and brain state snapshots.

## Architecture

### Memory Layout

```
Shared Pages (read-only, multiple references):
+----------+----------+----------+----------+----------+
|  Page 0  |  Page 1  |  Page 2  |  Page 3  |  Page N  |
|  (4KB)   |  (4KB)   |  (4KB)   |  (4KB)   |  (4KB)   |
|  REF=3   |  REF=2   |  REF=3   |  REF=1   |  REF=3   |
+----------+----------+----------+----------+----------+

On Write to Page 1:
1. SIGSEGV triggered (page is read-only)
2. Handler allocates new private page
3. Copies 4KB from shared page
4. Remaps view to private page
5. Decrements shared page refcount
6. Write proceeds to private page
```

### Core Components

| Component | Purpose |
|-----------|---------|
| `page_cow_region_t` | Base memory region with page table tracking refcounts |
| `page_cow_view_t` | View into a region; tracks shared vs private pages per view |
| `page_cow_snapshot_t` | Instant snapshot of view state for checkpointing |
| SIGSEGV Handler | Catches writes to read-only pages, triggers COW |

### Data Flow

```
                    +-----------------+
                    |    Region       |
                    |  (base memory)  |
                    +--------+--------+
                             |
         +-------------------+-------------------+
         |                   |                   |
    +----v----+         +----v----+         +----v----+
    |  View 1 |         |  View 2 |         |  View 3 |
    | (worker)|         | (worker)|         | (worker)|
    +---------+         +---------+         +---------+
         |                   |                   |
    All share same      View 2 writes       View 3 reads
    physical pages      -> COW copies       unchanged data
                        only modified
                        pages
```

---

## API Reference

### Initialization

```c
// Initialize subsystem (installs SIGSEGV handler)
bool page_cow_init(void);

// Shutdown subsystem (restores original handler)
void page_cow_shutdown(void);
```

### Region API

```c
// Create a COW region
page_cow_region_t page_cow_region_create(
    const page_cow_config_t* config,
    const void* initial_data         // NULL = zero-initialize
);

// Destroy region (all views must be destroyed first)
void page_cow_region_destroy(page_cow_region_t region);

// Get statistics
bool page_cow_region_get_stats(page_cow_region_t region, page_cow_stats_t* stats);

// Get region size
size_t page_cow_region_get_size(page_cow_region_t region);
```

### View API

```c
// Create view from region (O(1) - increments refcounts)
page_cow_view_t page_cow_view_create(page_cow_region_t region);

// Clone an existing view
page_cow_view_t page_cow_view_clone(page_cow_view_t source);

// Destroy view
void page_cow_view_destroy(page_cow_view_t view);

// Read-only access (no COW trigger)
const void* page_cow_view_read(page_cow_view_t view);

// Writable access (enables COW on write)
void* page_cow_view_write(page_cow_view_t view);

// Pre-emptive COW for known write regions
bool page_cow_view_make_page_private(page_cow_view_t view, size_t page_index);
size_t page_cow_view_make_range_private(page_cow_view_t view, size_t start, size_t count);

// Query page state
page_state_t page_cow_view_get_page_state(page_cow_view_t view, size_t page_index);
size_t page_cow_view_get_private_page_count(page_cow_view_t view);
size_t page_cow_view_get_shared_page_count(page_cow_view_t view);
size_t page_cow_view_get_memory_saved(page_cow_view_t view);
```

### Snapshot API

```c
// Create instant snapshot (O(1) - no data copy)
page_cow_snapshot_t page_cow_snapshot_create(page_cow_view_t view);

// Restore view to snapshot state
bool page_cow_snapshot_restore(page_cow_view_t view, page_cow_snapshot_t snapshot);

// Destroy snapshot
void page_cow_snapshot_destroy(page_cow_snapshot_t snapshot);

// Get number of pages that changed since snapshot
size_t page_cow_snapshot_get_delta_pages(page_cow_view_t view, page_cow_snapshot_t snapshot);
```

### Helper Functions

```c
// Page calculations
size_t page_cow_offset_to_page(size_t offset);    // Byte offset -> page index
size_t page_cow_page_to_offset(size_t page_idx);  // Page index -> byte offset
size_t page_cow_align_size(size_t size);          // Round up to page boundary
size_t page_cow_num_pages(size_t size);           // Calculate pages needed

// Default configuration
page_cow_config_t page_cow_default_config(size_t size);
```

### Configuration Structure

```c
typedef struct {
    size_t size;                // Total size in bytes (page-aligned)
    bool enable_tracking;       // Enable statistics tracking
    bool zero_on_allocate;      // Zero-initialize pages
    size_t max_private_pages;   // Limit on private pages (0 = unlimited)
} page_cow_config_t;
```

### Statistics Structure

```c
typedef struct {
    size_t total_pages;         // Total pages in region
    size_t shared_pages;        // Pages still shared
    size_t private_pages;       // Pages made private
    size_t total_views;         // Total views created
    size_t active_views;        // Currently active views
    uint64_t cow_faults;        // Total COW faults handled
    uint64_t total_bytes_copied;// Bytes copied by COW
    uint64_t total_copy_time_ns;// Time spent in COW copies
    size_t memory_used_bytes;   // Total memory used
    size_t memory_saved_bytes;  // Memory saved by sharing
} page_cow_stats_t;
```

---

## Usage Examples

### Basic Region and View Creation

```c
#include "utils/memory/nimcp_page_cow.h"

// Initialize (once at startup)
if (!page_cow_init()) {
    fprintf(stderr, "Failed to initialize page COW\n");
    return -1;
}

// Create 50MB region for neural network weights
float* weights = load_pretrained_weights();  // Your weight data
page_cow_config_t config = {
    .size = 50 * 1024 * 1024,
    .enable_tracking = true,
    .zero_on_allocate = false,
};

page_cow_region_t region = page_cow_region_create(&config, weights);
if (!region) {
    fprintf(stderr, "Failed to create region\n");
    return -1;
}

// Create view for inference
page_cow_view_t view = page_cow_view_create(region);
const float* read_weights = (const float*)page_cow_view_read(view);

// Use weights for inference (no copy, all pages shared)
float output = inference(read_weights);

// Cleanup
page_cow_view_destroy(view);
page_cow_region_destroy(region);
```

### COW Cloning for Parallel Inference

```c
// Create base region with pretrained weights
page_cow_config_t config = page_cow_default_config(50 * 1024 * 1024);
page_cow_region_t region = page_cow_region_create(&config, pretrained_weights);

// Create one view per worker thread - O(1) each, all share same pages
#define NUM_WORKERS 8
page_cow_view_t views[NUM_WORKERS];
for (int i = 0; i < NUM_WORKERS; i++) {
    views[i] = page_cow_view_create(region);
    // Memory used: ~64 bytes metadata, NOT 50MB!
}

// Each worker can read independently
#pragma omp parallel for
for (int i = 0; i < NUM_WORKERS; i++) {
    const float* weights = (const float*)page_cow_view_read(views[i]);
    process_batch(weights, batch_data[i]);
}

// If a worker needs to fine-tune (write), only modified pages are copied
float* writable = (float*)page_cow_view_write(views[0]);
writable[1000] = 0.5f;  // Triggers COW for ONE 4KB page only

// Check memory savings
page_cow_stats_t stats;
page_cow_region_get_stats(region, &stats);
printf("Memory saved: %zu MB\n", stats.memory_saved_bytes / (1024*1024));

// Cleanup
for (int i = 0; i < NUM_WORKERS; i++) {
    page_cow_view_destroy(views[i]);
}
page_cow_region_destroy(region);
```

### Snapshot/Restore for Checkpointing

```c
page_cow_region_t region = page_cow_region_create(&config, initial_data);
page_cow_view_t view = page_cow_view_create(region);

// Create checkpoint before risky operation
page_cow_snapshot_t checkpoint = page_cow_snapshot_create(view);

// Attempt operation that might fail
float* data = (float*)page_cow_view_write(view);
bool success = risky_training_step(data);

if (!success) {
    // Rollback to checkpoint - only modified pages restored
    page_cow_snapshot_restore(view, checkpoint);
    printf("Rolled back %zu modified pages\n",
           page_cow_snapshot_get_delta_pages(view, checkpoint));
} else {
    // Keep changes, optionally create new checkpoint
    page_cow_snapshot_destroy(checkpoint);
    checkpoint = page_cow_snapshot_create(view);
}

page_cow_snapshot_destroy(checkpoint);
page_cow_view_destroy(view);
page_cow_region_destroy(region);
```

### Knowledge Sharing Across Brains

```c
// Create shared knowledge base
size_t knowledge_size = 100 * 1024 * 1024;  // 100MB knowledge graph
page_cow_config_t config = page_cow_default_config(knowledge_size);
page_cow_region_t shared_knowledge = page_cow_region_create(&config, knowledge_data);

// Create view for each brain instance
typedef struct {
    nimcp_brain_t* brain;
    page_cow_view_t knowledge_view;
} brain_instance_t;

brain_instance_t brains[4];
for (int i = 0; i < 4; i++) {
    brains[i].brain = nimcp_brain_create();
    brains[i].knowledge_view = page_cow_view_create(shared_knowledge);
    // Each brain shares 100MB knowledge, uses ~0 extra memory
}

// Brain 0 learns something new (COW triggers)
float* knowledge = (float*)page_cow_view_write(brains[0].knowledge_view);
knowledge[42] = new_learned_value;
// Only 4KB copied, other brains unchanged

// Pre-emptively privatize a range for known writes
page_cow_view_make_range_private(brains[1].knowledge_view,
                                  1000,  // Start page
                                  10);   // Number of pages

// Query how much memory brain 1 is actually using
size_t private_pages = page_cow_view_get_private_page_count(brains[1].knowledge_view);
size_t memory_saved = page_cow_view_get_memory_saved(brains[1].knowledge_view);
printf("Brain 1: %zu private pages, saving %zu bytes\n", private_pages, memory_saved);
```

### Pre-emptive COW for Batch Writes

```c
// When you know which pages will be written, avoid SIGSEGV overhead
page_cow_view_t view = page_cow_view_create(region);

// Calculate which pages will be modified
size_t start_offset = batch_start_index * sizeof(float);
size_t end_offset = batch_end_index * sizeof(float);
size_t start_page = page_cow_offset_to_page(start_offset);
size_t end_page = page_cow_offset_to_page(end_offset);
size_t num_pages = end_page - start_page + 1;

// Pre-emptively make those pages private (batch operation)
size_t made_private = page_cow_view_make_range_private(view, start_page, num_pages);
printf("Pre-copied %zu pages\n", made_private);

// Now writes don't trigger SIGSEGV - faster!
float* data = (float*)page_cow_view_write(view);
for (size_t i = batch_start_index; i < batch_end_index; i++) {
    data[i] = compute_gradient(i);  // Direct write, no signal overhead
}
```

---

## Performance Characteristics

| Operation | Complexity | Typical Time |
|-----------|------------|--------------|
| `page_cow_view_create` | O(1) | < 1us |
| `page_cow_view_clone` | O(private_pages) | ~1us + copies |
| `page_cow_view_read` | O(1) | < 10ns |
| `page_cow_view_write` | O(1) | < 10ns |
| First write to shared page | O(page_size) | ~1us (4KB copy) |
| Subsequent writes to private page | O(1) | < 10ns |
| `page_cow_snapshot_create` | O(num_pages) | ~10us for 12K pages |
| `page_cow_snapshot_restore` | O(private_pages) | ~1us per restored page |

### Memory Overhead

- Per region: ~8 bytes per page (0.2% overhead)
- Per view: ~64 bytes base + 24 bytes per page
- Per snapshot: ~32 bytes base + 24 bytes per page

### Example: 50MB Weights with 5 Replicas

Without COW:
- 5 x 50MB = 250MB total

With COW (1% modification per replica):
- 50MB base + 5 x (0.5MB private) = 52.5MB total
- Savings: 197.5MB (79%)

---

## Thread Safety Guarantees

1. **Signal Handler**: Async-signal-safe using spinlocks only
2. **Page Table Access**: Protected by per-region and per-view spinlocks
3. **Reference Counting**: Uses `atomic_size_t` for lock-free updates
4. **Read Operations**: Lock-free after view creation

### Safe Usage Patterns

```c
// SAFE: Multiple readers, no synchronization needed
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    const void* data = page_cow_view_read(views[i]);
    read_only_operation(data);
}

// SAFE: Each view has independent write state
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    void* data = page_cow_view_write(views[i]);
    modify_data(data);  // COW is per-view, thread-safe
}
```

### Unsafe Patterns (Avoid)

```c
// UNSAFE: Concurrent writes to SAME view from multiple threads
// Use one view per thread instead
void* shared = page_cow_view_write(single_view);
#pragma omp parallel for  // WRONG!
for (int i = 0; i < N; i++) {
    shared[i] = compute(i);
}
```

---

## Limitations

1. **POSIX Only**: Requires `mmap`, `mprotect`, and `SIGSEGV` handling
2. **Page Granularity**: Minimum copy unit is 4KB (even for 1-byte writes)
3. **Signal Handler Overhead**: First write to each shared page incurs signal overhead (~1-5us)
4. **Global Handler**: Modifies process-wide `SIGSEGV` handler; chains to previous handler
5. **View Limit**: Maximum 256 regions x 16 views per region = 4096 total views
6. **No Cross-Process**: COW is within single process; use shared memory for IPC

---

## Best Practices

1. **Initialize Early**: Call `page_cow_init()` once at application startup
2. **One View Per Thread**: Create separate views for each worker to avoid contention
3. **Pre-emptive COW**: Use `make_range_private()` when you know write regions ahead of time
4. **Monitor Statistics**: Track `cow_faults` and `memory_saved_bytes` for optimization
5. **Snapshot Before Risk**: Create snapshots before operations that might fail
6. **Destroy in Order**: Destroy all views before destroying the region
7. **Avoid Partial Page Writes**: Group writes to minimize COW overhead

### Configuration Tips

```c
// For inference-heavy workloads (mostly reads)
page_cow_config_t inference_config = {
    .size = weights_size,
    .enable_tracking = false,  // Disable for performance
    .zero_on_allocate = false,
    .max_private_pages = 0,
};

// For training with checkpointing
page_cow_config_t training_config = {
    .size = model_size,
    .enable_tracking = true,   // Monitor learning
    .zero_on_allocate = true,
    .max_private_pages = 0,
};

// For memory-constrained environments
page_cow_config_t constrained_config = {
    .size = data_size,
    .enable_tracking = true,
    .zero_on_allocate = false,
    .max_private_pages = 1000,  // Limit divergence
};
```

---

## Files

| File | Description |
|------|-------------|
| `include/utils/memory/nimcp_page_cow.h` | Public API header |
| `src/utils/memory/nimcp_page_cow.c` | Implementation |
| `test/unit/utils/memory/test_page_cow.cpp` | Unit tests |
| `test/unit/utils/memory/test_page_cow_stress.cpp` | Stress tests |

---

## See Also

- [COW_CACHE_DESIGN.md](COW_CACHE_DESIGN.md) - Object-level COW cache design
- [COW_PHASE1_SUMMARY.md](COW_PHASE1_SUMMARY.md) - COW implementation history
- [MEMORY.md](MEMORY.md) - NIMCP memory system overview
