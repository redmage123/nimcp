/**
 * @file centrality_demo.c
 * @brief Demonstration of hub detection and centrality measures
 *
 * WHAT: Interactive demo showing network centrality analysis
 * WHY: Illustrate brain hub detection and network importance metrics
 * HOW: Create test networks, compute centralities, identify hubs
 */

#include <stdio.h>
#include <stdlib.h>
#include "utils/algorithms/nimcp_centrality.h"
#include "utils/containers/nimcp_graph.h"
#include "utils/memory/nimcp_memory.h"

/**
 * WHAT: Print centrality scores for all nodes
 * WHY: Visualize network importance
 */
static void print_centrality_scores(const char* name, const NimcpCentralityScores* scores) {
    if (!scores) {
        printf("  %s: [FAILED]\n", name);
        return;
    }

    printf("  %s:\n", name);
    for (uint32_t i = 0; i < scores->num_scores; i++) {
        printf("    Node %u: %.4f\n", i, scores->scores[i]);
    }
}

/**
 * WHAT: Create star network topology
 * WHY: Test centrality on hub-and-spoke structure (typical brain topology)
 */
static NimcpGraph* create_star_network(uint32_t num_leaves) {
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) {
        return NULL;
    }

    // Create hub (node 0)
    nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);

    // Create leaves and connect to hub
    for (uint32_t i = 1; i <= num_leaves; i++) {
        nimcp_graph_add_vertex(graph, i, (float)i, 0.0f, 0.0f, 0);
        nimcp_graph_add_edge(graph, 0, i, 1.0f);
        nimcp_graph_add_edge(graph, i, 0, 1.0f);  // Undirected
    }

    return graph;
}

/**
 * WHAT: Create bridge network topology
 * WHY: Test betweenness centrality (bridge nodes have high BC)
 */
static NimcpGraph* create_bridge_network(void) {
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) {
        return NULL;
    }

    // Left cluster (nodes 0-2, complete graph)
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, (float)i, 0.0f, 0);
    }
    for (uint32_t i = 0; i < 3; i++) {
        for (uint32_t j = i + 1; j < 3; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    // Right cluster (nodes 3-5, complete graph)
    for (uint32_t i = 3; i < 6; i++) {
        nimcp_graph_add_vertex(graph, i, 3.0f, (float)(i-3), 0.0f, 0);
    }
    for (uint32_t i = 3; i < 6; i++) {
        for (uint32_t j = i + 1; j < 6; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    // Bridge edge (0 <-> 3)
    nimcp_graph_add_edge(graph, 0, 3, 1.0f);
    nimcp_graph_add_edge(graph, 3, 0, 1.0f);

    return graph;
}

/**
 * WHAT: Demo 1 - Star network (brain hub topology)
 * WHY: Show how hubs are detected in hub-and-spoke networks
 */
static void demo_star_network(void) {
    printf("\n========================================\n");
    printf("DEMO 1: Star Network (Brain Hub)\n");
    printf("========================================\n");
    printf("Topology: Hub (thalamus) connected to 5 regions\n");
    printf("Expected: Hub has high centrality, leaves have low\n\n");

    NimcpGraph* graph = create_star_network(5);
    if (!graph) {
        printf("ERROR: Failed to create graph\n");
        return;
    }

    // Compute all centralities
    NimcpCentralityScores* degree = nimcp_degree_centrality(graph);
    NimcpCentralityScores* between = nimcp_betweenness_centrality(graph);
    NimcpCentralityScores* close = nimcp_closeness_centrality(graph);
    NimcpCentralityScores* eigen = nimcp_eigenvector_centrality(graph, 100);

    // Print results
    print_centrality_scores("Degree Centrality", degree);
    printf("\n");
    print_centrality_scores("Betweenness Centrality", between);
    printf("\n");
    print_centrality_scores("Closeness Centrality", close);
    printf("\n");
    print_centrality_scores("Eigenvector Centrality", eigen);

    // Detect hubs
    printf("\nHub Detection (threshold = 1.0 stdev):\n");
    uint32_t hubs[10];
    uint32_t num_hubs = nimcp_detect_hubs(degree, 1.0, hubs, 10);
    printf("  Found %u hub(s):", num_hubs);
    for (uint32_t i = 0; i < num_hubs; i++) {
        printf(" %u", hubs[i]);
    }
    printf("\n");

    // Cleanup
    nimcp_centrality_scores_destroy(degree);
    nimcp_centrality_scores_destroy(between);
    nimcp_centrality_scores_destroy(close);
    nimcp_centrality_scores_destroy(eigen);
    nimcp_graph_destroy(graph);
}

/**
 * WHAT: Demo 2 - Bridge network (interhemispheric connection)
 * WHY: Show how betweenness detects critical bridge nodes
 */
static void demo_bridge_network(void) {
    printf("\n========================================\n");
    printf("DEMO 2: Bridge Network (Corpus Callosum)\n");
    printf("========================================\n");
    printf("Topology: Two hemispheres connected by bridge\n");
    printf("Expected: Bridge nodes have high betweenness\n\n");

    NimcpGraph* graph = create_bridge_network();
    if (!graph) {
        printf("ERROR: Failed to create graph\n");
        return;
    }

    // Compute betweenness (most relevant for bridges)
    NimcpCentralityScores* between = nimcp_betweenness_centrality(graph);

    // Print results
    print_centrality_scores("Betweenness Centrality", between);

    // Find bridge nodes (high betweenness)
    printf("\nBridge Node Analysis:\n");
    printf("  Nodes 0 and 3 are bridge nodes connecting clusters\n");
    printf("  Betweenness(0) = %.4f\n", nimcp_get_centrality_score(between, 0));
    printf("  Betweenness(3) = %.4f\n", nimcp_get_centrality_score(between, 3));
    printf("  Betweenness(1) = %.4f (non-bridge, for comparison)\n",
           nimcp_get_centrality_score(between, 1));

    // Cleanup
    nimcp_centrality_scores_destroy(between);
    nimcp_graph_destroy(graph);
}

/**
 * WHAT: Main demo entry point
 */
int main(void) {
    // Initialize memory tracking
    nimcp_memory_init();
    nimcp_memory_enable_tracking(true);

    printf("\n");
    printf("================================================================================\n");
    printf("NIMCP Brain Network Centrality Demo\n");
    printf("================================================================================\n");
    printf("\nBIOLOGY:\n");
    printf("  - Brain hubs: Thalamus (sensory relay), precuneus (consciousness)\n");
    printf("  - High centrality → critical for information integration\n");
    printf("  - Hub damage → catastrophic network failure (stroke)\n");
    printf("\nALGORITHMS:\n");
    printf("  - Degree: Number of connections (local importance)\n");
    printf("  - Betweenness: Fraction of shortest paths (bridge nodes)\n");
    printf("  - Closeness: Inverse average distance (global access)\n");
    printf("  - Eigenvector: Influence based on neighbors (power structure)\n");

    // Run demos
    demo_star_network();
    demo_bridge_network();

    printf("\n========================================\n");
    printf("Demo Complete\n");
    printf("========================================\n\n");

    // Check for memory leaks
    nimcp_memory_check_leaks();
    nimcp_memory_cleanup();

    return 0;
}
