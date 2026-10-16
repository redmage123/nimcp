/**
 * @file test_community_standalone.c
 * @brief Standalone test for community detection (no build system required)
 *
 * WHAT: Simple test to verify Louvain algorithm works
 * WHY: Quick validation without full CMake/GTest setup
 * HOW: Create simple graph, run algorithm, check results
 */

#include "src/utils/algorithms/nimcp_community_detection.h"
#include "src/utils/containers/nimcp_graph.h"
#include "src/utils/memory/nimcp_memory.h"
#include <stdio.h>
#include <stdlib.h>

/* Test helper */
static void test_simple_modular_graph() {
    printf("TEST: Simple modular graph (2 communities)...\n");

    NimcpGraph* g = nimcp_graph_create();
    if (!g) {
        printf("  FAIL: Could not create graph\n");
        return;
    }

    /* Add 6 nodes */
    for (uint32_t i = 0; i < 6; i++) {
        nimcp_graph_add_vertex(g, i, 0.0f, 0.0f, 0.0f, 0);
    }

    /* Community 1: nodes 0,1,2 (fully connected) */
    nimcp_graph_add_edge(g, 0, 1, 1.0f);
    nimcp_graph_add_edge(g, 1, 0, 1.0f);
    nimcp_graph_add_edge(g, 0, 2, 1.0f);
    nimcp_graph_add_edge(g, 2, 0, 1.0f);
    nimcp_graph_add_edge(g, 1, 2, 1.0f);
    nimcp_graph_add_edge(g, 2, 1, 1.0f);

    /* Community 2: nodes 3,4,5 (fully connected) */
    nimcp_graph_add_edge(g, 3, 4, 1.0f);
    nimcp_graph_add_edge(g, 4, 3, 1.0f);
    nimcp_graph_add_edge(g, 3, 5, 1.0f);
    nimcp_graph_add_edge(g, 5, 3, 1.0f);
    nimcp_graph_add_edge(g, 4, 5, 1.0f);
    nimcp_graph_add_edge(g, 5, 4, 1.0f);

    /* Bridge between communities (weak link) */
    nimcp_graph_add_edge(g, 2, 3, 0.1f);
    nimcp_graph_add_edge(g, 3, 2, 0.1f);

    /* Run Louvain */
    community_structure_t* comm = louvain_detect_communities(g);
    if (!comm) {
        printf("  FAIL: Algorithm returned NULL\n");
        nimcp_graph_destroy(g);
        return;
    }

    /* Verify results */
    printf("  Communities found: %u\n", comm->num_communities);
    printf("  Modularity: %.4f\n", comm->modularity);
    printf("  Iterations: %u\n", comm->iterations);

    bool pass = true;

    /* Should find 2 communities */
    if (comm->num_communities != 2) {
        printf("  FAIL: Expected 2 communities, got %u\n", comm->num_communities);
        pass = false;
    }

    /* Modularity should be positive */
    if (comm->modularity <= 0.0f) {
        printf("  FAIL: Modularity should be positive, got %.4f\n", comm->modularity);
        pass = false;
    }

    /* Nodes 0,1,2 should be in same community */
    uint32_t comm0 = get_node_community(comm, 0);
    uint32_t comm1 = get_node_community(comm, 1);
    uint32_t comm2 = get_node_community(comm, 2);

    if (comm0 != comm1 || comm0 != comm2) {
        printf("  FAIL: Nodes 0,1,2 should be in same community\n");
        printf("        Got: comm[0]=%u, comm[1]=%u, comm[2]=%u\n", comm0, comm1, comm2);
        pass = false;
    }

    /* Nodes 3,4,5 should be in same community (different from 0,1,2) */
    uint32_t comm3 = get_node_community(comm, 3);
    uint32_t comm4 = get_node_community(comm, 4);
    uint32_t comm5 = get_node_community(comm, 5);

    if (comm3 != comm4 || comm3 != comm5) {
        printf("  FAIL: Nodes 3,4,5 should be in same community\n");
        printf("        Got: comm[3]=%u, comm[4]=%u, comm[5]=%u\n", comm3, comm4, comm5);
        pass = false;
    }

    if (comm0 == comm3) {
        printf("  FAIL: Communities should be different\n");
        pass = false;
    }

    /* Print node assignments */
    printf("  Node assignments:\n");
    for (uint32_t i = 0; i < 6; i++) {
        printf("    Node %u -> Community %u\n", i, get_node_community(comm, i));
    }

    if (pass) {
        printf("  PASS\n");
    }

    community_structure_destroy(comm);
    nimcp_graph_destroy(g);
}

static void test_clique() {
    printf("\nTEST: Fully connected graph (1 community)...\n");

    NimcpGraph* g = nimcp_graph_create();
    if (!g) {
        printf("  FAIL: Could not create graph\n");
        return;
    }

    /* Create 5-node clique */
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_graph_add_vertex(g, i, 0.0f, 0.0f, 0.0f, 0);
    }

    for (uint32_t i = 0; i < 5; i++) {
        for (uint32_t j = i + 1; j < 5; j++) {
            nimcp_graph_add_edge(g, i, j, 1.0f);
            nimcp_graph_add_edge(g, j, i, 1.0f);
        }
    }

    community_structure_t* comm = louvain_detect_communities(g);
    if (!comm) {
        printf("  FAIL: Algorithm returned NULL\n");
        nimcp_graph_destroy(g);
        return;
    }

    printf("  Communities found: %u\n", comm->num_communities);
    printf("  Modularity: %.4f\n", comm->modularity);

    bool pass = true;

    /* Should find 1 community */
    if (comm->num_communities != 1) {
        printf("  FAIL: Expected 1 community, got %u\n", comm->num_communities);
        pass = false;
    }

    /* All nodes in same community */
    uint32_t first = get_node_community(comm, 0);
    for (uint32_t i = 1; i < 5; i++) {
        if (get_node_community(comm, i) != first) {
            printf("  FAIL: All nodes should be in same community\n");
            pass = false;
            break;
        }
    }

    if (pass) {
        printf("  PASS\n");
    }

    community_structure_destroy(comm);
    nimcp_graph_destroy(g);
}

int main() {
    printf("========================================\n");
    printf("Louvain Community Detection Tests\n");
    printf("========================================\n\n");

    /* Initialize memory system */
    nimcp_memory_init();
    nimcp_memory_enable_tracking(true);

    /* Run tests */
    test_simple_modular_graph();
    test_clique();

    /* Check for leaks */
    printf("\n========================================\n");
    printf("Memory Leak Check\n");
    printf("========================================\n");
    nimcp_memory_check_leaks();

    nimcp_memory_stats_t stats;
    if (nimcp_memory_get_stats(&stats)) {
        printf("\nMemory Statistics:\n");
        printf("  Total allocated: %zu bytes\n", stats.total_allocated);
        printf("  Current allocated: %zu bytes\n", stats.current_allocated);
        printf("  Peak allocated: %zu bytes\n", stats.peak_allocated);
        printf("  Allocations: %zu\n", stats.allocation_count);
        printf("  Frees: %zu\n", stats.free_count);

        if (stats.current_allocated == 0) {
            printf("\n✓ No memory leaks detected\n");
        } else {
            printf("\n✗ Memory leak: %zu bytes still allocated\n", stats.current_allocated);
        }
    }

    nimcp_memory_cleanup();

    printf("\n========================================\n");
    printf("Tests Complete\n");
    printf("========================================\n");

    return 0;
}
