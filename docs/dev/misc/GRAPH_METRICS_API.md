# Graph Metrics API Reference

## Quick Start

```c
#include "utils/algorithms/nimcp_graph_metrics.h"

// Create and populate graph
NimcpGraph* graph = nimcp_graph_create();
// ... add vertices and edges ...

// Compute all metrics
graph_metrics_t* metrics = compute_graph_metrics(graph);

// Access results
printf("Modularity: %.3f\n", metrics->modularity);
printf("Clustering: %.3f\n", metrics->clustering_coefficient);
printf("Path Length: %.3f\n", metrics->characteristic_path_length);
printf("Small-World: %.3f\n", metrics->small_world_coefficient);
printf("Diameter: %u\n", metrics->diameter);
printf("Assortativity: %.3f\n", metrics->assortativity);

// Cleanup
graph_metrics_destroy(metrics);
nimcp_graph_destroy(graph);
```

## Data Structures

### graph_metrics_t

Complete network characterization:

```c
typedef struct {
    float modularity;                  // Newman's Q: -0.5 to 1.0
    float clustering_coefficient;      // Average C: 0 to 1.0
    float characteristic_path_length;  // Average L: >1
    float small_world_coefficient;     // σ = (C/C_rand) / (L/L_rand)
    uint32_t diameter;                 // Longest shortest path
    float assortativity;               // Degree correlation: -1 to 1
} graph_metrics_t;
```

## Core Functions

### compute_graph_metrics()

Compute all network metrics in single call:

```c
graph_metrics_t* compute_graph_metrics(NimcpGraph* graph);
```

**Parameters:**
- `graph` - Network to analyze (non-NULL)

**Returns:**
- Allocated metrics structure, or NULL on error
- Caller must free with `graph_metrics_destroy()`

**Complexity:** O(V³) for path length computation

**Example:**
```c
graph_metrics_t* metrics = compute_graph_metrics(brain_graph);
if (metrics) {
    validate_brain_topology(metrics);
    graph_metrics_destroy(metrics);
}
```

### graph_metrics_destroy()

Free metrics structure:

```c
void graph_metrics_destroy(graph_metrics_t* metrics);
```

**Parameters:**
- `metrics` - Structure to free (NULL-safe)

## Individual Metric Functions

### compute_modularity_q()

Measure community structure strength:

```c
float compute_modularity_q(NimcpGraph* graph, uint32_t* communities);
```

**Parameters:**
- `graph` - Network to analyze
- `communities` - Array of community labels (one per vertex)

**Returns:**
- Q in range [-0.5, 1.0]
- -1.0 on error

**Interpretation:**
- Q > 0.3: Strong modularity (brain-like)
- Q ≈ 0.0: Random structure
- Q < 0.0: Anti-modular

**Algorithm:** Newman's modularity
```
Q = (1/2m) * Σ[A_ij - (k_i*k_j)/(2m)] * δ(c_i, c_j)
```

**Complexity:** O(V²)

**Example:**
```c
uint32_t communities[NUM_NEURONS];
detect_communities(graph, communities);  // Your algorithm
float Q = compute_modularity_q(graph, communities);
printf("Modularity: %.3f\n", Q);
```

### compute_clustering_coefficient()

Measure local network density:

```c
float compute_clustering_coefficient(NimcpGraph* graph);
```

**Parameters:**
- `graph` - Network to analyze

**Returns:**
- C in range [0, 1]
- -1.0 on error

**Interpretation:**
- C = 1.0: Complete graph (all neighbors connected)
- C ≈ 0.5: Brain-like (high local clustering)
- C = 0.0: Tree/star (no triangles)

**Algorithm:** Watts-Strogatz clustering
```
C = (1/n) * Σ[2*T_i / (k_i*(k_i-1))]
```

**Complexity:** O(V·k²) where k = average degree

**Example:**
```c
float C = compute_clustering_coefficient(brain_graph);
if (C > 0.4f) {
    printf("High local clustering (brain-like)\n");
}
```

### compute_characteristic_path_length()

Measure global communication efficiency:

```c
float compute_characteristic_path_length(NimcpGraph* graph);
```

**Parameters:**
- `graph` - Network to analyze

**Returns:**
- Average path length
- -1.0 on error
- 0.0 for disconnected graph

**Interpretation:**
- L ≈ 2: Very efficient (brain-like)
- L ≈ log(N): Random network
- L ≈ N: Lattice (poor efficiency)

**Algorithm:** Floyd-Warshall all-pairs shortest paths
```
L = (1/(n*(n-1))) * Σ_i Σ_j d(i,j)
```

**Complexity:** O(V³)

**Example:**
```c
float L = compute_characteristic_path_length(brain_graph);
if (L < 4.0f) {
    printf("Short paths (efficient communication)\n");
}
```

### compute_assortativity()

Measure hub connectivity patterns:

```c
float compute_assortativity(NimcpGraph* graph);
```

**Parameters:**
- `graph` - Network to analyze

**Returns:**
- r in range [-1, 1]
- -2.0 on error

**Interpretation:**
- r > 0: Assortative (hubs connect to hubs)
- r ≈ 0: Neutral (brain-like)
- r < 0: Disassortative (hubs avoid hubs)

**Algorithm:** Pearson correlation of endpoint degrees
```
r = correlation(degrees at edge endpoints)
```

**Complexity:** O(E)

**Example:**
```c
float r = compute_assortativity(brain_graph);
if (fabs(r) < 0.1f) {
    printf("Neutral hub mixing (brain-like)\n");
}
```

## Brain Topology Validation

### Small-World Detection

Brain networks are "small-world": high clustering + short paths

```c
graph_metrics_t* metrics = compute_graph_metrics(brain_graph);

bool is_small_world =
    metrics->clustering_coefficient > 0.4f &&
    metrics->characteristic_path_length < 4.0f &&
    metrics->small_world_coefficient > 1.0f;

if (is_small_world) {
    printf("✓ Small-world topology (brain-like)\n");
}
```

### Modularity Validation

Brain networks are highly modular (Q ≈ 0.3-0.5):

```c
uint32_t* communities = detect_functional_modules(brain_graph);
float Q = compute_modularity_q(brain_graph, communities);

if (Q > 0.3f) {
    printf("✓ Strong functional modularity\n");
}
```

### Hub Organization

Brain hubs don't form "rich clubs" (r ≈ 0):

```c
float r = compute_assortativity(brain_graph);

if (fabs(r) < 0.2f) {
    printf("✓ Neutral hub connectivity\n");
}
```

### Complete Validation Example

```c
bool validate_brain_topology(NimcpGraph* brain_graph) {
    graph_metrics_t* m = compute_graph_metrics(brain_graph);
    if (!m) return false;

    bool valid = true;

    // Check modularity
    if (m->modularity < 0.3f) {
        printf("⚠ Low modularity (%.3f < 0.3)\n", m->modularity);
        valid = false;
    }

    // Check small-world
    if (m->small_world_coefficient < 1.0f) {
        printf("⚠ Not small-world (σ = %.3f)\n",
               m->small_world_coefficient);
        valid = false;
    }

    // Check clustering
    if (m->clustering_coefficient < 0.4f) {
        printf("⚠ Low clustering (%.3f < 0.4)\n",
               m->clustering_coefficient);
        valid = false;
    }

    // Check path length
    if (m->characteristic_path_length > 6.0f) {
        printf("⚠ Long paths (%.3f > 6.0)\n",
               m->characteristic_path_length);
        valid = false;
    }

    graph_metrics_destroy(m);
    return valid;
}
```

## Performance Considerations

### Time Complexity

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| `compute_modularity_q()` | O(V²) | Dense graphs; O(V+E) optimizable |
| `compute_clustering_coefficient()` | O(V·k²) | k = average degree |
| `compute_characteristic_path_length()` | O(V³) | Floyd-Warshall |
| `compute_assortativity()` | O(E) | Single edge pass |
| `compute_graph_metrics()` | O(V³) | Dominated by path length |

### Memory Usage

- Floyd-Warshall requires O(V²) distance matrix
- Results can be cached for static graphs
- All allocations use `nimcp_malloc()` for leak tracking

### Optimization Tips

1. **Cache Results:** If graph is static, compute once and cache
2. **Parallel Floyd-Warshall:** For V > 1000, consider parallelization
3. **Approximate Clustering:** For sparse graphs, sample vertices
4. **Incremental Updates:** For dynamic graphs, update only affected metrics

## Error Handling

All functions return error codes for NULL inputs:

```c
// Modularity
float Q = compute_modularity_q(NULL, communities);
// Returns: -1.0

// Clustering
float C = compute_clustering_coefficient(NULL);
// Returns: -1.0

// Path length
float L = compute_characteristic_path_length(NULL);
// Returns: -1.0

// Assortativity
float r = compute_assortativity(NULL);
// Returns: -2.0

// All metrics
graph_metrics_t* m = compute_graph_metrics(NULL);
// Returns: NULL
```

Check return values:

```c
graph_metrics_t* metrics = compute_graph_metrics(graph);
if (!metrics) {
    fprintf(stderr, "Failed to compute metrics\n");
    return ERROR;
}
```

## Thread Safety

- All functions are **read-only** on input graph
- Graph must not be modified during metric computation
- Use graph's internal mutex if concurrent access needed
- Results structure is **not thread-safe** (single-threaded use only)

## Memory Management

```c
// Always pair create/destroy
graph_metrics_t* metrics = compute_graph_metrics(graph);
// ... use metrics ...
graph_metrics_destroy(metrics);

// NULL-safe destruction
graph_metrics_destroy(NULL);  // Safe, no-op
```

## References

### Scientific Papers

1. **Modularity:** Newman, M. E. J., & Girvan, M. (2004). Finding and evaluating community structure in networks. *Physical Review E*, 69(2), 026113.

2. **Small-World:** Watts, D. J., & Strogatz, S. H. (1998). Collective dynamics of 'small-world' networks. *Nature*, 393(6684), 440-442.

3. **Brain Networks:** Bullmore, E., & Sporns, O. (2009). Complex brain networks: graph theoretical analysis of structural and functional systems. *Nature Reviews Neuroscience*, 10(3), 186-198.

### Implementation Files

- **Header:** `src/utils/algorithms/nimcp_graph_metrics.h`
- **Implementation:** `src/utils/algorithms/nimcp_graph_metrics.c`
- **Tests:** `test/unit/utils/algorithms/test_graph_metrics.cpp`
