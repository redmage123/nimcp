/**
 * @file graph_metrics_demo.c
 * @brief Demonstration of graph metrics API for brain topology validation
 *
 * WHAT: Example showing all graph metrics on canonical topologies
 * WHY: Validate implementations and demonstrate brain-like properties
 * HOW: Create test graphs (complete, ring, star, modular) and measure metrics
 */

#include <stdio.h>
#include <stdlib.h>
#include "utils/algorithms/nimcp_graph_metrics.h"
#include "utils/containers/nimcp_graph.h"
#include "utils/memory/nimcp_memory.h"

/**
 * @brief Create complete graph K_n (all vertices connected)
 */
static NimcpGraph* create_complete_graph(uint32_t n) {
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return NULL;

    for (uint32_t i = 0; i < n; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    return graph;
}

/**
 * @brief Create ring graph C_n (cycle)
 */
static NimcpGraph* create_ring_graph(uint32_t n) {
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return NULL;

    for (uint32_t i = 0; i < n; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }

    for (uint32_t i = 0; i < n; i++) {
        uint32_t next = (i + 1) % n;
        nimcp_graph_add_edge(graph, i, next, 1.0f);
        nimcp_graph_add_edge(graph, next, i, 1.0f);
    }

    return graph;
}

/**
 * @brief Create star graph (hub + leaves)
 */
static NimcpGraph* create_star_graph(uint32_t n) {
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return NULL;

    for (uint32_t i = 0; i < n; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }

    for (uint32_t i = 1; i < n; i++) {
        nimcp_graph_add_edge(graph, 0, i, 1.0f);
        nimcp_graph_add_edge(graph, i, 0, 1.0f);
    }

    return graph;
}

/**
 * @brief Create brain-like modular network
 */
static NimcpGraph* create_brain_like_graph(void) {
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return NULL;

    // Create 20 vertices in 4 modules of 5 each
    for (uint32_t i = 0; i < 20; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }

    // High clustering within modules
    for (uint32_t mod = 0; mod < 4; mod++) {
        for (uint32_t i = mod * 5; i < (mod + 1) * 5; i++) {
            for (uint32_t j = i + 1; j < (mod + 1) * 5; j++) {
                nimcp_graph_add_edge(graph, i, j, 1.0f);
                nimcp_graph_add_edge(graph, j, i, 1.0f);
            }
        }
    }

    // Add shortcuts for short paths
    nimcp_graph_add_edge(graph, 0, 10, 1.0f);
    nimcp_graph_add_edge(graph, 10, 0, 1.0f);
    nimcp_graph_add_edge(graph, 5, 15, 1.0f);
    nimcp_graph_add_edge(graph, 15, 5, 1.0f);

    return graph;
}

/**
 * @brief Print metrics in formatted table
 */
static void print_metrics(const char* name, graph_metrics_t* m) {
    printf("%-20s | ", name);
    printf("Q=%5.3f | ", m->modularity);
    printf("C=%5.3f | ", m->clustering_coefficient);
    printf("L=%5.2f | ", m->characteristic_path_length);
    printf("σ=%5.2f | ", m->small_world_coefficient);
    printf("D=%2u | ", m->diameter);
    printf("r=%+5.3f\n", m->assortativity);
}

/**
 * @brief Main demonstration
 */
int main(void) {
    nimcp_memory_init();
    nimcp_memory_enable_tracking(true);

    printf("\n");
    printf("=================================================================\n");
    printf("NIMCP Graph Metrics - Topology Validation Demo\n");
    printf("=================================================================\n\n");

    printf("Network Metrics:\n");
    printf("  Q = Modularity (Newman's Q)\n");
    printf("  C = Clustering coefficient\n");
    printf("  L = Characteristic path length\n");
    printf("  σ = Small-world coefficient\n");
    printf("  D = Diameter\n");
    printf("  r = Assortativity\n\n");

    printf("Graph Type           |    Q    |    C    |   L   |   σ   | D  |   r\n");
    printf("---------------------|---------|---------|-------|-------|----|---------\n");

    // Test 1: Complete graph
    NimcpGraph* complete = create_complete_graph(10);
    graph_metrics_t* m1 = compute_graph_metrics(complete);
    print_metrics("Complete K_10", m1);
    graph_metrics_destroy(m1);
    nimcp_graph_destroy(complete);

    // Test 2: Ring graph
    NimcpGraph* ring = create_ring_graph(20);
    graph_metrics_t* m2 = compute_graph_metrics(ring);
    print_metrics("Ring C_20", m2);
    graph_metrics_destroy(m2);
    nimcp_graph_destroy(ring);

    // Test 3: Star graph
    NimcpGraph* star = create_star_graph(15);
    graph_metrics_t* m3 = compute_graph_metrics(star);
    print_metrics("Star (15 nodes)", m3);
    graph_metrics_destroy(m3);
    nimcp_graph_destroy(star);

    // Test 4: Brain-like modular network
    NimcpGraph* brain = create_brain_like_graph();
    graph_metrics_t* m4 = compute_graph_metrics(brain);
    print_metrics("Brain-like (4 mod)", m4);
    graph_metrics_destroy(m4);
    nimcp_graph_destroy(brain);

    printf("\n");
    printf("=================================================================\n");
    printf("Brain Topology Validation\n");
    printf("=================================================================\n\n");

    // Validate brain-like properties
    brain = create_brain_like_graph();
    graph_metrics_t* brain_metrics = compute_graph_metrics(brain);

    printf("Brain-Like Properties Check:\n\n");

    // Check modularity
    printf("✓ Modularity (Q > 0.3):          ");
    if (brain_metrics->modularity > 0.3f) {
        printf("PASS (Q = %.3f)\n", brain_metrics->modularity);
    } else {
        printf("FAIL (Q = %.3f)\n", brain_metrics->modularity);
    }

    // Check clustering
    printf("✓ High Clustering (C > 0.4):     ");
    if (brain_metrics->clustering_coefficient > 0.4f) {
        printf("PASS (C = %.3f)\n", brain_metrics->clustering_coefficient);
    } else {
        printf("FAIL (C = %.3f)\n", brain_metrics->clustering_coefficient);
    }

    // Check path length
    printf("✓ Short Paths (L < 6.0):         ");
    if (brain_metrics->characteristic_path_length < 6.0f) {
        printf("PASS (L = %.2f)\n", brain_metrics->characteristic_path_length);
    } else {
        printf("FAIL (L = %.2f)\n", brain_metrics->characteristic_path_length);
    }

    // Check small-world
    printf("✓ Small-World (σ > 1.0):         ");
    if (brain_metrics->small_world_coefficient > 1.0f) {
        printf("PASS (σ = %.2f)\n", brain_metrics->small_world_coefficient);
    } else {
        printf("FAIL (σ = %.2f)\n", brain_metrics->small_world_coefficient);
    }

    // Check assortativity
    printf("✓ Neutral Hubs (|r| < 0.2):      ");
    float abs_r = brain_metrics->assortativity > 0 ?
                  brain_metrics->assortativity : -brain_metrics->assortativity;
    if (abs_r < 0.2f) {
        printf("PASS (r = %+.3f)\n", brain_metrics->assortativity);
    } else {
        printf("FAIL (r = %+.3f)\n", brain_metrics->assortativity);
    }

    printf("\n");
    printf("=================================================================\n");
    printf("Individual Metric Functions Demo\n");
    printf("=================================================================\n\n");

    // Demonstrate individual functions
    float Q = compute_modularity_q(brain, NULL);  // Needs community labels
    float C = compute_clustering_coefficient(brain);
    float L = compute_characteristic_path_length(brain);
    float r = compute_assortativity(brain);

    printf("Individual function calls:\n");
    printf("  compute_clustering_coefficient()        = %.3f\n", C);
    printf("  compute_characteristic_path_length()    = %.2f\n", L);
    printf("  compute_assortativity()                 = %+.3f\n", r);
    printf("\n");

    graph_metrics_destroy(brain_metrics);
    nimcp_graph_destroy(brain);

    printf("=================================================================\n");
    printf("Memory Check\n");
    printf("=================================================================\n\n");

    nimcp_memory_check_leaks();

    printf("\nDemo complete!\n\n");

    nimcp_memory_cleanup();
    return 0;
}
