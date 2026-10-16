/**
 * @file nimcp_sort.c
 * @brief Consolidated Sorting and Graph Algorithms Implementation
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Central implementation of sorting and graph traversal algorithms
 * WHY:  Eliminate code duplication across modules (previously 4+ copies of Kahn's)
 * HOW:  Generic callback-based APIs for data structure independence
 *
 * @author NIMCP Development Team
 */

#include "utils/algorithms/nimcp_sort.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sort)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Swap two memory regions
 */
static inline void swap_bytes(void* a, void* b, size_t size) {
    unsigned char* pa = (unsigned char*)a;
    unsigned char* pb = (unsigned char*)b;
    unsigned char tmp;

    for (size_t i = 0; i < size; i++) {
        tmp = pa[i];
        pa[i] = pb[i];
        pb[i] = tmp;
    }
}

/**
 * @brief Allocate work buffer, preferring stack for small sizes
 *
 * @param count     Number of elements needed
 * @param elem_size Size of each element
 * @param stack_buf Pointer to stack buffer (if small enough)
 * @param stack_cap Capacity of stack buffer in elements
 * @return Pointer to buffer (either stack or heap allocated)
 */
static inline void* alloc_work_buffer(
    uint32_t count,
    size_t elem_size,
    void* stack_buf,
    uint32_t stack_cap
) {
    if (count <= stack_cap) {
        memset(stack_buf, 0, count * elem_size);
        return stack_buf;
    }
    return nimcp_calloc(count, elem_size);
}

/**
 * @brief Free work buffer if heap allocated
 */
static inline void free_work_buffer(void* buf, void* stack_buf) {
    if (buf != stack_buf && buf != NULL) {
        nimcp_free(buf);
    }
}

/* ============================================================================
 * Topological Sort Implementation (Kahn's Algorithm)
 * ============================================================================ */

nimcp_sort_result_t nimcp_topological_sort(
    const nimcp_topo_config_t* config,
    uint32_t* order_out,
    uint32_t max_order,
    uint32_t* sorted_count
) {
    /* Validate parameters */
    if (!config || !order_out || !sorted_count) {
        return NIMCP_SORT_ERROR_NULL;
    }
    if (!config->get_dep_count || !config->get_dep) {
        return NIMCP_SORT_ERROR_NULL;
    }
    if (config->node_count == 0) {
        *sorted_count = 0;
        return NIMCP_SORT_OK;
    }
    if (max_order < config->node_count) {
        return NIMCP_SORT_ERROR_OVERFLOW;
    }

    uint32_t n = config->node_count;
    nimcp_sort_result_t result = NIMCP_SORT_OK;

    /* Stack buffers for small graphs */
    uint32_t stack_in_degree[NIMCP_SORT_MAX_STACK_NODES];
    uint32_t stack_queue[NIMCP_SORT_MAX_STACK_NODES];

    /* Allocate work buffers */
    uint32_t* in_degree = (uint32_t*)alloc_work_buffer(
        n, sizeof(uint32_t), stack_in_degree, NIMCP_SORT_MAX_STACK_NODES);
    uint32_t* queue = (uint32_t*)alloc_work_buffer(
        n, sizeof(uint32_t), stack_queue, NIMCP_SORT_MAX_STACK_NODES);

    if (!in_degree || !queue) {
        free_work_buffer(in_degree, stack_in_degree);
        free_work_buffer(queue, stack_queue);
        return NIMCP_SORT_ERROR_MEMORY;
    }

    /* Compute in-degree for each node */
    for (uint32_t i = 0; i < n; i++) {
        uint32_t dep_count = config->get_dep_count(i, config->user_data);
        if (dep_count == UINT32_MAX) {
            result = NIMCP_SORT_ERROR_CALLBACK;
            goto cleanup;
        }
        in_degree[i] = dep_count;
    }

    /* Initialize queue with zero in-degree nodes */
    uint32_t queue_head = 0;
    uint32_t queue_tail = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (in_degree[i] == 0) {
            queue[queue_tail++] = i;
        }
    }

    /* Process queue (Kahn's algorithm) */
    uint32_t output_idx = 0;
    while (queue_head < queue_tail) {
        uint32_t node = queue[queue_head++];
        order_out[output_idx++] = node;

        /* Decrease in-degree of dependents */
        if (config->get_dependent_count && config->get_dependent) {
            /* Efficient path: iterate dependents directly */
            uint32_t dep_cnt = config->get_dependent_count(node, config->user_data);
            if (dep_cnt == UINT32_MAX) {
                result = NIMCP_SORT_ERROR_CALLBACK;
                goto cleanup;
            }
            for (uint32_t j = 0; j < dep_cnt; j++) {
                uint32_t dependent = config->get_dependent(node, j, config->user_data);
                if (dependent == UINT32_MAX || dependent >= n) {
                    continue;
                }
                in_degree[dependent]--;
                if (in_degree[dependent] == 0) {
                    queue[queue_tail++] = dependent;
                }
            }
        } else {
            /* Fallback: scan all nodes to find dependents */
            for (uint32_t i = 0; i < n; i++) {
                uint32_t dep_cnt = config->get_dep_count(i, config->user_data);
                if (dep_cnt == UINT32_MAX) continue;

                for (uint32_t j = 0; j < dep_cnt; j++) {
                    uint32_t dep = config->get_dep(i, j, config->user_data);
                    if (dep == node) {
                        in_degree[i]--;
                        if (in_degree[i] == 0) {
                            queue[queue_tail++] = i;
                        }
                        break;
                    }
                }
            }
        }
    }

    *sorted_count = output_idx;

    /* Check for cycles */
    if (output_idx < n) {
        result = NIMCP_SORT_ERROR_CYCLE;
    }

cleanup:
    free_work_buffer(in_degree, stack_in_degree);
    free_work_buffer(queue, stack_queue);
    return result;
}

bool nimcp_has_cycle(const nimcp_topo_config_t* config) {
    if (!config || config->node_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_has_cycle: invalid parameters");

            return false;
    }

    uint32_t* order = (uint32_t*)nimcp_calloc(config->node_count, sizeof(uint32_t));
    if (!order) {
        return true;  /* Conservative: assume cycle if can't allocate */
    }

    uint32_t sorted_count = 0;
    nimcp_sort_result_t result = nimcp_topological_sort(
        config, order, config->node_count, &sorted_count);

    nimcp_free(order);
    return (result == NIMCP_SORT_ERROR_CYCLE);
}

nimcp_sort_result_t nimcp_find_cycle_nodes(
    const nimcp_topo_config_t* config,
    uint32_t* cycle_nodes,
    uint32_t max_nodes,
    uint32_t* cycle_count
) {
    if (!config || !cycle_nodes || !cycle_count) {
        return NIMCP_SORT_ERROR_NULL;
    }

    *cycle_count = 0;

    if (config->node_count == 0) {
        return NIMCP_SORT_OK;
    }

    /* Get topological order */
    uint32_t* order = (uint32_t*)nimcp_calloc(config->node_count, sizeof(uint32_t));
    bool* in_order = (bool*)nimcp_calloc(config->node_count, sizeof(bool));

    if (!order || !in_order) {
        nimcp_free(order);
        nimcp_free(in_order);
        return NIMCP_SORT_ERROR_MEMORY;
    }

    uint32_t sorted_count = 0;
    nimcp_topological_sort(config, order, config->node_count, &sorted_count);

    /* Mark nodes that were sorted */
    for (uint32_t i = 0; i < sorted_count; i++) {
        in_order[order[i]] = true;
    }

    /* Nodes not in sorted order are in cycles */
    uint32_t found = 0;
    for (uint32_t i = 0; i < config->node_count && found < max_nodes; i++) {
        if (!in_order[i]) {
            cycle_nodes[found++] = i;
        }
    }

    *cycle_count = found;

    nimcp_free(order);
    nimcp_free(in_order);
    return NIMCP_SORT_OK;
}

/* ============================================================================
 * Comparison Sort Implementation
 * ============================================================================ */

void nimcp_insertion_sort(
    void* base,
    size_t nmemb,
    size_t size,
    int (*compare)(const void*, const void*)
) {
    if (!base || nmemb < 2 || size == 0 || !compare) {
        return;
    }

    unsigned char* arr = (unsigned char*)base;

    for (size_t i = 1; i < nmemb; i++) {
        size_t j = i;
        while (j > 0 && compare(arr + j * size, arr + (j - 1) * size) < 0) {
            swap_bytes(arr + j * size, arr + (j - 1) * size, size);
            j--;
        }
    }
}

void nimcp_sort(
    void* base,
    size_t nmemb,
    size_t size,
    int (*compare)(const void*, const void*)
) {
    if (!base || nmemb < 2 || size == 0 || !compare) {
        return;
    }

    if (nmemb <= NIMCP_SORT_INSERTION_THRESHOLD) {
        nimcp_insertion_sort(base, nmemb, size, compare);
    } else {
        qsort(base, nmemb, size, compare);
    }
}

/* Context wrapper for qsort_r emulation on platforms without it */
typedef struct {
    int (*compare)(const void*, const void*, void*);
    void* context;
} sort_r_context_t;

static sort_r_context_t g_sort_r_ctx;  /* Thread-local would be better */

static int sort_r_wrapper(const void* a, const void* b) {
    return g_sort_r_ctx.compare(a, b, g_sort_r_ctx.context);
}

void nimcp_sort_r(
    void* base,
    size_t nmemb,
    size_t size,
    int (*compare)(const void*, const void*, void*),
    void* context
) {
    if (!base || nmemb < 2 || size == 0 || !compare) {
        return;
    }

    /* Use global context wrapper (not thread-safe, but simple) */
    /* TODO: Use thread-local storage or platform-specific qsort_r */
    g_sort_r_ctx.compare = compare;
    g_sort_r_ctx.context = context;

    qsort(base, nmemb, size, sort_r_wrapper);
}

/* ============================================================================
 * Graph Traversal Implementation
 * ============================================================================ */

nimcp_sort_result_t nimcp_bfs(const nimcp_traversal_config_t* config) {
    if (!config) {
        return NIMCP_SORT_ERROR_NULL;
    }
    if (!config->get_neighbor_count || !config->get_neighbor || !config->visit) {
        return NIMCP_SORT_ERROR_NULL;
    }
    if (config->node_count == 0) {
        return NIMCP_SORT_OK;
    }
    if (config->start_node >= config->node_count) {
        return NIMCP_SORT_ERROR_INVALID;
    }

    uint32_t n = config->node_count;
    nimcp_sort_result_t result = NIMCP_SORT_OK;

    /* Stack buffers for small graphs */
    bool stack_visited[NIMCP_SORT_MAX_STACK_NODES];
    uint32_t stack_queue[NIMCP_SORT_MAX_STACK_NODES];
    uint32_t stack_depth[NIMCP_SORT_MAX_STACK_NODES];

    bool* visited = (bool*)alloc_work_buffer(
        n, sizeof(bool), stack_visited, NIMCP_SORT_MAX_STACK_NODES);
    uint32_t* queue = (uint32_t*)alloc_work_buffer(
        n, sizeof(uint32_t), stack_queue, NIMCP_SORT_MAX_STACK_NODES);
    uint32_t* depth = (uint32_t*)alloc_work_buffer(
        n, sizeof(uint32_t), stack_depth, NIMCP_SORT_MAX_STACK_NODES);

    if (!visited || !queue || !depth) {
        result = NIMCP_SORT_ERROR_MEMORY;
        goto cleanup;
    }

    /* Initialize BFS */
    uint32_t queue_head = 0;
    uint32_t queue_tail = 0;
    queue[queue_tail] = config->start_node;
    depth[queue_tail] = 0;
    queue_tail++;
    visited[config->start_node] = true;

    /* Process queue */
    while (queue_head < queue_tail) {
        uint32_t node = queue[queue_head];
        uint32_t node_depth = depth[queue_head];
        queue_head++;

        /* Visit node */
        void* visit_data = config->visit_user_data ? config->visit_user_data : config->user_data;
        if (!config->visit(node, node_depth, visit_data)) {
            break;  /* Visitor requested stop */
        }

        /* Enqueue neighbors */
        uint32_t neighbor_count = config->get_neighbor_count(node, config->user_data);
        if (neighbor_count == UINT32_MAX) {
            result = NIMCP_SORT_ERROR_CALLBACK;
            goto cleanup;
        }

        for (uint32_t i = 0; i < neighbor_count; i++) {
            uint32_t neighbor = config->get_neighbor(node, i, config->user_data);
            if (neighbor == UINT32_MAX || neighbor >= n) {
                continue;
            }
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                queue[queue_tail] = neighbor;
                depth[queue_tail] = node_depth + 1;
                queue_tail++;
            }
        }
    }

cleanup:
    free_work_buffer(visited, stack_visited);
    free_work_buffer(queue, stack_queue);
    free_work_buffer(depth, stack_depth);
    return result;
}

nimcp_sort_result_t nimcp_dfs(const nimcp_traversal_config_t* config) {
    if (!config) {
        return NIMCP_SORT_ERROR_NULL;
    }
    if (!config->get_neighbor_count || !config->get_neighbor || !config->visit) {
        return NIMCP_SORT_ERROR_NULL;
    }
    if (config->node_count == 0) {
        return NIMCP_SORT_OK;
    }
    if (config->start_node >= config->node_count) {
        return NIMCP_SORT_ERROR_INVALID;
    }

    uint32_t n = config->node_count;
    nimcp_sort_result_t result = NIMCP_SORT_OK;

    /* Stack buffers */
    bool stack_visited[NIMCP_SORT_MAX_STACK_NODES];
    uint32_t stack_stack[NIMCP_SORT_MAX_STACK_NODES];
    uint32_t stack_depth[NIMCP_SORT_MAX_STACK_NODES];

    bool* visited = (bool*)alloc_work_buffer(
        n, sizeof(bool), stack_visited, NIMCP_SORT_MAX_STACK_NODES);
    uint32_t* stack = (uint32_t*)alloc_work_buffer(
        n, sizeof(uint32_t), stack_stack, NIMCP_SORT_MAX_STACK_NODES);
    uint32_t* depth_stack = (uint32_t*)alloc_work_buffer(
        n, sizeof(uint32_t), stack_depth, NIMCP_SORT_MAX_STACK_NODES);

    if (!visited || !stack || !depth_stack) {
        result = NIMCP_SORT_ERROR_MEMORY;
        goto cleanup;
    }

    /* Initialize DFS */
    uint32_t stack_top = 0;
    stack[stack_top] = config->start_node;
    depth_stack[stack_top] = 0;
    stack_top++;

    /* Process stack */
    while (stack_top > 0) {
        stack_top--;
        uint32_t node = stack[stack_top];
        uint32_t node_depth = depth_stack[stack_top];

        if (visited[node]) {
            continue;
        }
        visited[node] = true;

        /* Visit node */
        void* visit_data = config->visit_user_data ? config->visit_user_data : config->user_data;
        if (!config->visit(node, node_depth, visit_data)) {
            break;  /* Visitor requested stop */
        }

        /* Push neighbors (in reverse order for correct DFS ordering) */
        uint32_t neighbor_count = config->get_neighbor_count(node, config->user_data);
        if (neighbor_count == UINT32_MAX) {
            result = NIMCP_SORT_ERROR_CALLBACK;
            goto cleanup;
        }

        for (uint32_t i = neighbor_count; i > 0; i--) {
            uint32_t neighbor = config->get_neighbor(node, i - 1, config->user_data);
            if (neighbor == UINT32_MAX || neighbor >= n) {
                continue;
            }
            if (!visited[neighbor]) {
                stack[stack_top] = neighbor;
                depth_stack[stack_top] = node_depth + 1;
                stack_top++;
            }
        }
    }

cleanup:
    free_work_buffer(visited, stack_visited);
    free_work_buffer(stack, stack_stack);
    free_work_buffer(depth_stack, stack_depth);
    return result;
}

nimcp_sort_result_t nimcp_find_components(
    const nimcp_traversal_config_t* config,
    uint32_t* component_ids,
    uint32_t* num_components
) {
    if (!config || !component_ids || !num_components) {
        return NIMCP_SORT_ERROR_NULL;
    }
    if (!config->get_neighbor_count || !config->get_neighbor) {
        return NIMCP_SORT_ERROR_NULL;
    }

    *num_components = 0;

    if (config->node_count == 0) {
        return NIMCP_SORT_OK;
    }

    uint32_t n = config->node_count;

    /* Initialize all nodes as unvisited (UINT32_MAX) */
    for (uint32_t i = 0; i < n; i++) {
        component_ids[i] = UINT32_MAX;
    }

    uint32_t* queue = (uint32_t*)nimcp_calloc(n, sizeof(uint32_t));
    if (!queue) {
        return NIMCP_SORT_ERROR_MEMORY;
    }

    uint32_t current_component = 0;

    for (uint32_t start = 0; start < n; start++) {
        if (component_ids[start] != UINT32_MAX) {
            continue;  /* Already visited */
        }

        /* BFS from this node */
        uint32_t queue_head = 0;
        uint32_t queue_tail = 0;
        queue[queue_tail++] = start;
        component_ids[start] = current_component;

        while (queue_head < queue_tail) {
            uint32_t node = queue[queue_head++];

            uint32_t neighbor_count = config->get_neighbor_count(node, config->user_data);
            if (neighbor_count == UINT32_MAX) {
                continue;
            }

            for (uint32_t i = 0; i < neighbor_count; i++) {
                uint32_t neighbor = config->get_neighbor(node, i, config->user_data);
                if (neighbor == UINT32_MAX || neighbor >= n) {
                    continue;
                }
                if (component_ids[neighbor] == UINT32_MAX) {
                    component_ids[neighbor] = current_component;
                    queue[queue_tail++] = neighbor;
                }
            }
        }

        current_component++;
    }

    nimcp_free(queue);
    *num_components = current_component;
    return NIMCP_SORT_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void nimcp_reverse_u32(uint32_t* array, uint32_t count) {
    if (!array || count < 2) {
        return;
    }

    uint32_t left = 0;
    uint32_t right = count - 1;

    while (left < right) {
        uint32_t tmp = array[left];
        array[left] = array[right];
        array[right] = tmp;
        left++;
        right--;
    }
}

uint32_t nimcp_binary_search_u32(
    const uint32_t* array,
    uint32_t count,
    uint32_t value
) {
    if (!array || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_binary_search_u32: invalid parameters");

            return UINT32_MAX;
    }

    uint32_t left = 0;
    uint32_t right = count;

    while (left < right) {
        uint32_t mid = left + (right - left) / 2;
        if (array[mid] == value) {
            return mid;
        } else if (array[mid] < value) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    return UINT32_MAX;
}

bool nimcp_is_sorted_u32(const uint32_t* array, uint32_t count) {
    if (!array || count < 2) {
        return true;
    }

    for (uint32_t i = 1; i < count; i++) {
        if (array[i] < array[i - 1]) {
            return false;
        }
    }

    return true;
}
