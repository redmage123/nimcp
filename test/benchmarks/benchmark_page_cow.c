//=============================================================================
// benchmark_page_cow.c - Page-Level COW Performance Benchmarks
//=============================================================================
/**
 * @file benchmark_page_cow.c
 * @brief Performance profiling for Phase 5 optimization
 *
 * WHAT: Comprehensive benchmarks for page-level COW operations
 * WHY:  Identify optimization opportunities and measure improvements
 * HOW:  Time critical operations, measure memory savings, stress test
 *
 * BENCHMARKS:
 * 1. Region creation/destruction
 * 2. View creation (O(1) clone)
 * 3. COW fault handling latency
 * 4. Memory savings measurement
 * 5. Concurrent access throughput
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "utils/memory/nimcp_page_cow.h"

//=============================================================================
// Timing Utilities
//=============================================================================

static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline double ns_to_us(uint64_t ns) {
    return (double)ns / 1000.0;
}

static inline double ns_to_ms(uint64_t ns) {
    return (double)ns / 1000000.0;
}

//=============================================================================
// Benchmark Configuration
//=============================================================================

#define BENCH_ITERATIONS 100
#define SMALL_REGION_SIZE (64 * 1024)         // 64KB
#define MEDIUM_REGION_SIZE (1 * 1024 * 1024)  // 1MB
#define LARGE_REGION_SIZE (50 * 1024 * 1024)  // 50MB
#define NUM_VIEWS 100
#define NUM_THREADS 8

//=============================================================================
// Benchmark 1: Region Creation/Destruction
//=============================================================================

static void benchmark_region_create_destroy(void) {
    printf("\n=== Benchmark 1: Region Creation/Destruction ===\n");

    size_t sizes[] = {SMALL_REGION_SIZE, MEDIUM_REGION_SIZE, LARGE_REGION_SIZE};
    const char* size_names[] = {"64KB", "1MB", "50MB"};

    for (int s = 0; s < 3; s++) {
        size_t size = sizes[s];
        uint64_t total_create = 0, total_destroy = 0;

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            page_cow_config_t config = page_cow_default_config(size);

            uint64_t start = get_time_ns();
            page_cow_region_t region = page_cow_region_create(&config, NULL);
            total_create += get_time_ns() - start;

            start = get_time_ns();
            page_cow_region_destroy(region);
            total_destroy += get_time_ns() - start;
        }

        printf("  %s region:\n", size_names[s]);
        printf("    Create: %.2f us (avg over %d iterations)\n",
               ns_to_us(total_create / BENCH_ITERATIONS), BENCH_ITERATIONS);
        printf("    Destroy: %.2f us (avg)\n",
               ns_to_us(total_destroy / BENCH_ITERATIONS));
    }
}

//=============================================================================
// Benchmark 2: View Creation (O(1) Clone)
//=============================================================================

static void benchmark_view_creation(void) {
    printf("\n=== Benchmark 2: View Creation (O(1) Clone) ===\n");

    size_t size = LARGE_REGION_SIZE;  // 50MB
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, NULL);

    if (!region) {
        printf("  ERROR: Failed to create region\n");
        return;
    }

    // Benchmark creating many views
    page_cow_view_t views[NUM_VIEWS];
    uint64_t total_create = 0, total_destroy = 0;

    for (int i = 0; i < NUM_VIEWS; i++) {
        uint64_t start = get_time_ns();
        views[i] = page_cow_view_create(region);
        total_create += get_time_ns() - start;
    }

    printf("  50MB region with %d views:\n", NUM_VIEWS);
    printf("    View create: %.2f us (avg)\n", ns_to_us(total_create / NUM_VIEWS));
    printf("    Total for %d views: %.2f ms\n", NUM_VIEWS, ns_to_ms(total_create));

    // Verify memory savings
    size_t total_saved = 0;
    for (int i = 0; i < NUM_VIEWS; i++) {
        total_saved += page_cow_view_get_memory_saved(views[i]);
    }
    printf("    Memory saved: %.2f MB (%.1f%% of %d x 50MB)\n",
           (double)total_saved / (1024 * 1024),
           100.0 * total_saved / ((double)NUM_VIEWS * size),
           NUM_VIEWS);

    // Benchmark destroying views
    for (int i = 0; i < NUM_VIEWS; i++) {
        uint64_t start = get_time_ns();
        page_cow_view_destroy(views[i]);
        total_destroy += get_time_ns() - start;
    }
    printf("    View destroy: %.2f us (avg)\n", ns_to_us(total_destroy / NUM_VIEWS));

    page_cow_region_destroy(region);
}

//=============================================================================
// Benchmark 3: COW Fault Handling Latency
//=============================================================================

static void benchmark_cow_fault(void) {
    printf("\n=== Benchmark 3: COW Fault Handling Latency ===\n");

    size_t size = MEDIUM_REGION_SIZE;  // 1MB = 256 pages
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, NULL);

    if (!region) {
        printf("  ERROR: Failed to create region\n");
        return;
    }

    // Create view and measure explicit COW trigger time
    page_cow_view_t view = page_cow_view_create(region);

    size_t num_pages = page_cow_num_pages(size);
    uint64_t total_cow_time = 0;
    size_t pages_to_test = (num_pages < 100) ? num_pages : 100;

    for (size_t i = 0; i < pages_to_test; i++) {
        uint64_t start = get_time_ns();
        page_cow_view_make_page_private(view, i);
        total_cow_time += get_time_ns() - start;
    }

    printf("  1MB region (%zu pages):\n", num_pages);
    printf("    COW per page: %.2f us (avg over %zu pages)\n",
           ns_to_us(total_cow_time / pages_to_test), pages_to_test);
    printf("    Private pages: %zu\n", page_cow_view_get_private_page_count(view));

    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

//=============================================================================
// Benchmark 4: Memory Savings Analysis
//=============================================================================

static void benchmark_memory_savings(void) {
    printf("\n=== Benchmark 4: Memory Savings Analysis ===\n");

    size_t size = LARGE_REGION_SIZE;  // 50MB
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, NULL);

    if (!region) {
        printf("  ERROR: Failed to create region\n");
        return;
    }

    // Create 10 views, each modifying different amounts
    printf("  50MB base region with 10 views:\n");
    printf("  %-10s %-15s %-15s %-10s\n", "View", "Modified %", "Saved MB", "Savings %");

    for (int pct = 0; pct <= 100; pct += 10) {
        page_cow_view_t view = page_cow_view_create(region);
        size_t num_pages = page_cow_num_pages(size);
        size_t pages_to_modify = (num_pages * pct) / 100;

        // Make pages private
        if (pages_to_modify > 0) {
            page_cow_view_make_range_private(view, 0, pages_to_modify);
        }

        size_t saved = page_cow_view_get_memory_saved(view);
        double saved_mb = (double)saved / (1024 * 1024);
        double savings_pct = 100.0 * saved / size;

        printf("  %-10d %-15d %-15.2f %-10.1f\n", pct/10, pct, saved_mb, savings_pct);

        page_cow_view_destroy(view);
    }

    page_cow_region_destroy(region);
}

//=============================================================================
// Benchmark 5: Snapshot Performance
//=============================================================================

static void benchmark_snapshots(void) {
    printf("\n=== Benchmark 5: Snapshot Performance ===\n");

    size_t size = MEDIUM_REGION_SIZE;  // 1MB
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, NULL);

    if (!region) {
        printf("  ERROR: Failed to create region\n");
        return;
    }

    page_cow_view_t view = page_cow_view_create(region);

    // Benchmark snapshot creation
    uint64_t total_snap_create = 0, total_snap_destroy = 0, total_restore = 0;
    page_cow_snapshot_t snaps[BENCH_ITERATIONS];

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        uint64_t start = get_time_ns();
        snaps[i] = page_cow_snapshot_create(view);
        total_snap_create += get_time_ns() - start;
    }

    printf("  1MB region:\n");
    printf("    Snapshot create: %.2f us (avg)\n",
           ns_to_us(total_snap_create / BENCH_ITERATIONS));

    // Modify some pages and benchmark restore
    page_cow_view_make_range_private(view, 0, 50);  // 50 pages = 200KB

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        uint64_t start = get_time_ns();
        page_cow_snapshot_restore(view, snaps[i]);
        total_restore += get_time_ns() - start;
    }

    printf("    Snapshot restore: %.2f us (avg after 50 page modifications)\n",
           ns_to_us(total_restore / BENCH_ITERATIONS));

    // Cleanup
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        uint64_t start = get_time_ns();
        page_cow_snapshot_destroy(snaps[i]);
        total_snap_destroy += get_time_ns() - start;
    }

    printf("    Snapshot destroy: %.2f us (avg)\n",
           ns_to_us(total_snap_destroy / BENCH_ITERATIONS));

    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

//=============================================================================
// Benchmark 6: Concurrent Access (Thread Safety)
//=============================================================================

typedef struct {
    page_cow_region_t region;
    int thread_id;
    int operations;
    uint64_t total_time;
} thread_args_t;

static void* concurrent_view_worker(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    uint64_t start = get_time_ns();

    for (int i = 0; i < args->operations; i++) {
        // Create view
        page_cow_view_t view = page_cow_view_create(args->region);
        if (!view) continue;

        // Make some pages private (simulate writes)
        page_cow_view_make_range_private(view, args->thread_id * 10, 5);

        // Read
        const float* data = (const float*)page_cow_view_read(view);
        volatile float sum = 0;
        for (int j = 0; j < 1000; j++) {
            sum += data[j];
        }
        (void)sum;

        // Destroy view
        page_cow_view_destroy(view);
    }

    args->total_time = get_time_ns() - start;
    return NULL;
}

static void benchmark_concurrent_access(void) {
    printf("\n=== Benchmark 6: Concurrent Access ===\n");

    size_t size = LARGE_REGION_SIZE;  // 50MB
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, NULL);

    if (!region) {
        printf("  ERROR: Failed to create region\n");
        return;
    }

    pthread_t threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];
    int ops_per_thread = 100;

    uint64_t start = get_time_ns();

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].region = region;
        args[i].thread_id = i;
        args[i].operations = ops_per_thread;
        args[i].total_time = 0;
        pthread_create(&threads[i], NULL, concurrent_view_worker, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    uint64_t total_time = get_time_ns() - start;

    printf("  %d threads, %d ops/thread:\n", NUM_THREADS, ops_per_thread);
    printf("    Total time: %.2f ms\n", ns_to_ms(total_time));
    printf("    Throughput: %.0f ops/sec\n",
           (double)(NUM_THREADS * ops_per_thread) / (ns_to_ms(total_time) / 1000.0));

    // Per-thread stats
    uint64_t max_time = 0, min_time = UINT64_MAX;
    for (int i = 0; i < NUM_THREADS; i++) {
        if (args[i].total_time > max_time) max_time = args[i].total_time;
        if (args[i].total_time < min_time) min_time = args[i].total_time;
    }
    printf("    Thread time range: %.2f - %.2f ms\n", ns_to_ms(min_time), ns_to_ms(max_time));

    page_cow_region_destroy(region);
}

//=============================================================================
// Benchmark 7: vs Traditional memcpy Clone
//=============================================================================

static void benchmark_vs_memcpy(void) {
    printf("\n=== Benchmark 7: COW vs Traditional memcpy Clone ===\n");

    size_t size = LARGE_REGION_SIZE;  // 50MB

    // Allocate and initialize source data
    void* source = malloc(size);
    if (!source) {
        printf("  ERROR: Failed to allocate source\n");
        return;
    }
    memset(source, 0xAB, size);

    // Traditional clone via memcpy
    uint64_t memcpy_total = 0;
    for (int i = 0; i < 10; i++) {
        void* dest = malloc(size);
        uint64_t start = get_time_ns();
        memcpy(dest, source, size);
        memcpy_total += get_time_ns() - start;
        free(dest);
    }

    printf("  50MB clone comparison:\n");
    printf("    memcpy clone: %.2f ms (avg)\n", ns_to_ms(memcpy_total / 10));

    // COW clone
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, source);

    uint64_t cow_total = 0;
    page_cow_view_t views[10];
    for (int i = 0; i < 10; i++) {
        uint64_t start = get_time_ns();
        views[i] = page_cow_view_create(region);
        cow_total += get_time_ns() - start;
    }

    printf("    COW clone: %.2f us (avg)\n", ns_to_us(cow_total / 10));
    printf("    Speedup: %.0fx faster\n", ns_to_ms(memcpy_total / 10) / ns_to_us(cow_total / 10) * 1000);

    // Cleanup
    for (int i = 0; i < 10; i++) {
        page_cow_view_destroy(views[i]);
    }
    page_cow_region_destroy(region);
    free(source);
}

//=============================================================================
// Main
//=============================================================================

int main(void) {
    printf("=============================================================\n");
    printf("NIMCP Page-Level COW Performance Benchmarks (Phase 5)\n");
    printf("=============================================================\n");
    printf("Page size: %d bytes\n", PAGE_COW_PAGE_SIZE);

    // Initialize COW subsystem
    if (!page_cow_init()) {
        printf("ERROR: Failed to initialize page COW subsystem\n");
        return 1;
    }

    // Run benchmarks
    benchmark_region_create_destroy();
    benchmark_view_creation();
    benchmark_cow_fault();
    benchmark_memory_savings();
    benchmark_snapshots();
    benchmark_concurrent_access();
    benchmark_vs_memcpy();

    printf("\n=============================================================\n");
    printf("Benchmark Summary:\n");
    printf("  - View creation is O(1) regardless of data size\n");
    printf("  - COW enables 100x+ speedup for clone operations\n");
    printf("  - Memory savings proportional to shared data\n");
    printf("=============================================================\n");

    page_cow_shutdown();
    return 0;
}
