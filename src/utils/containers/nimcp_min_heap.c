/**
 * @file nimcp_min_heap.c
 * @brief Binary min-heap implementation for Dijkstra's algorithm
 *
 * ALGORITHM: Binary heap with array-based storage
 * - Parent of node i: (i-1)/2
 * - Left child of node i: 2*i + 1
 * - Right child of node i: 2*i + 2
 *
 * OPTIMIZATION: Maintains position map for O(log n) decrease-key
 * - position[vertex_id] = index in heap array
 * - Updated during bubble-up and bubble-down
 *
 * @complexity
 * - Insert: O(log n)
 * - Extract-min: O(log n)
 * - Decrease-key: O(log n)
 * - Peek-min: O(1)
 */

#include "utils/containers/nimcp_min_heap.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(min_heap)

/**
 * @brief Internal heap structure (hidden from users)
 *
 * DESIGN PATTERN: Information Hiding (ADT)
 */
struct nimcp_min_heap_t {
    nimcp_heap_element_t* elements; /**< Array-based heap storage */
    uint32_t* position_map;         /**< Maps vertex_id -> heap_index for O(log n) decrease-key */
    uint32_t size;                  /**< Current number of elements */
    uint32_t capacity;              /**< Maximum capacity */
    uint32_t max_vertex_id;         /**< Maximum vertex ID for position map sizing */
};

/* Sentinel value for "not in heap" */
#define NOT_IN_HEAP UINT32_MAX

//==============================================================================
// Helper Functions (Private)
//==============================================================================

/**
 * @brief Get parent index
 */
static inline uint32_t parent(uint32_t i)
{
    return (i > 0) ? (i - 1) / 2 : 0;
}

/**
 * @brief Get left child index
 */
static inline uint32_t left_child(uint32_t i)
{
    return 2 * i + 1;
}

/**
 * @brief Get right child index
 */
static inline uint32_t right_child(uint32_t i)
{
    return 2 * i + 2;
}

/**
 * @brief Swap two elements in heap and update position map
 */
static void heap_swap(nimcp_min_heap_t* heap, uint32_t i, uint32_t j)
{
    nimcp_heap_element_t temp = heap->elements[i];
    heap->elements[i] = heap->elements[j];
    heap->elements[j] = temp;

    // Update position map
    heap->position_map[heap->elements[i].vertex_id] = i;
    heap->position_map[heap->elements[j].vertex_id] = j;
}

/**
 * @brief Bubble up element at index i to maintain heap property
 *
 * @complexity O(log n)
 */
static void bubble_up(nimcp_min_heap_t* heap, uint32_t i)
{
    while (i > 0) {
        uint32_t p = parent(i);
        if (heap->elements[i].priority < heap->elements[p].priority) {
            heap_swap(heap, i, p);
            i = p;
        } else {
            break;
        }
    }
}

/**
 * @brief Bubble down element at index i to maintain heap property
 *
 * @complexity O(log n)
 */
static void bubble_down(nimcp_min_heap_t* heap, uint32_t i)
{
    while (true) {
        uint32_t smallest = i;
        uint32_t left = left_child(i);
        uint32_t right = right_child(i);

        if (left < heap->size && heap->elements[left].priority < heap->elements[smallest].priority) {
            smallest = left;
        }

        if (right < heap->size &&
            heap->elements[right].priority < heap->elements[smallest].priority) {
            smallest = right;
        }

        if (smallest != i) {
            heap_swap(heap, i, smallest);
            i = smallest;
        } else {
            break;
        }
    }
}

//==============================================================================
// Public API Implementation
//==============================================================================

nimcp_min_heap_t* nimcp_min_heap_create(uint32_t capacity)
{
    LOG_TRACE("Entering nimcp_min_heap_create");
    if (capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_min_heap_create: capacity is 0");
        LOG_ERROR("nimcp_min_heap_create failed: returning error");
        return NULL;
    }

    nimcp_min_heap_t* heap = (nimcp_min_heap_t*)nimcp_malloc(sizeof(nimcp_min_heap_t));
    if (!heap) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_min_heap_create: failed to allocate heap");
        LOG_ERROR("nimcp_min_heap_create failed: returning error");
        return NULL;
    }

    heap->elements = (nimcp_heap_element_t*)nimcp_malloc(capacity * sizeof(nimcp_heap_element_t));
    if (!heap->elements) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_min_heap_create: failed to allocate elements array");
        nimcp_free(heap);
        LOG_ERROR("nimcp_min_heap_create failed: returning error");
        return NULL;
    }

    // Position map sized for maximum vertex ID (conservative: use capacity)
    heap->max_vertex_id = capacity;
    heap->position_map = (uint32_t*)nimcp_malloc(capacity * sizeof(uint32_t));
    if (!heap->position_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_min_heap_create: failed to allocate position_map");
        nimcp_free(heap->elements);
        nimcp_free(heap);
        LOG_ERROR("nimcp_min_heap_create failed: returning error");
        return NULL;
    }

    // Initialize position map (all vertices not in heap)
    for (uint32_t i = 0; i < capacity; i++) {
        heap->position_map[i] = NOT_IN_HEAP;
    }

    heap->size = 0;
    heap->capacity = capacity;

    return heap;
}

void nimcp_min_heap_destroy(nimcp_min_heap_t* heap)
{
    LOG_TRACE("Entering nimcp_min_heap_destroy");
    if (!heap) {
        return;
    }

    if (heap->elements) {
        nimcp_free(heap->elements);
    }

    if (heap->position_map) {
        nimcp_free(heap->position_map);
    }

    nimcp_free(heap);
}

bool nimcp_min_heap_insert(nimcp_min_heap_t* heap, const nimcp_heap_element_t* element)
{
    LOG_TRACE("Entering nimcp_min_heap_insert");
    if (!heap || !element) {
        LOG_ERROR("nimcp_min_heap_insert failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_min_heap_insert: required parameter is NULL (heap, element)");
        return false;
    }

    if (heap->size >= heap->capacity) {
        LOG_ERROR("nimcp_min_heap_insert failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_min_heap_insert: capacity exceeded");
        return false;  // Heap full
    }

    if (element->vertex_id >= heap->max_vertex_id) {
        LOG_ERROR("nimcp_min_heap_insert failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_min_heap_insert: capacity exceeded");
        return false;  // Vertex ID out of range for position map
    }

    // Add element at end
    uint32_t index = heap->size;
    heap->elements[index] = *element;
    heap->position_map[element->vertex_id] = index;
    heap->size++;

    // Restore heap property
    bubble_up(heap, index);

    return true;
}

bool nimcp_min_heap_extract_min(nimcp_min_heap_t* heap, nimcp_heap_element_t* element)
{
    LOG_TRACE("Entering nimcp_min_heap_extract_min");
    if (!heap || !element || heap->size == 0) {
        LOG_ERROR("nimcp_min_heap_extract_min failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_min_heap_extract_min: required parameter is NULL (heap, element)");
        return false;
    }

    // Return minimum (root)
    *element = heap->elements[0];
    heap->position_map[element->vertex_id] = NOT_IN_HEAP;

    // Move last element to root
    heap->size--;
    if (heap->size > 0) {
        heap->elements[0] = heap->elements[heap->size];
        heap->position_map[heap->elements[0].vertex_id] = 0;

        // Restore heap property
        bubble_down(heap, 0);
    }

    return true;
}

bool nimcp_min_heap_peek_min(const nimcp_min_heap_t* heap, nimcp_heap_element_t* element)
{
    LOG_TRACE("Entering nimcp_min_heap_peek_min");
    if (!heap || !element || heap->size == 0) {
        LOG_ERROR("nimcp_min_heap_peek_min failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_min_heap_peek_min: required parameter is NULL (heap, element)");
        return false;
    }

    *element = heap->elements[0];
    return true;
}

bool nimcp_min_heap_decrease_key(nimcp_min_heap_t* heap, uint32_t vertex_id, float new_priority)
{
    LOG_TRACE("Entering nimcp_min_heap_decrease_key");
    if (!heap) {
        LOG_ERROR("nimcp_min_heap_decrease_key failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_min_heap_decrease_key: heap is NULL");
        return false;
    }

    if (vertex_id >= heap->max_vertex_id) {
        LOG_ERROR("nimcp_min_heap_decrease_key failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_min_heap_decrease_key: capacity exceeded");
        return false;
    }

    uint32_t index = heap->position_map[vertex_id];
    if (index == NOT_IN_HEAP || index >= heap->size) {
        LOG_ERROR("nimcp_min_heap_decrease_key failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_min_heap_decrease_key: capacity exceeded");
        return false;  // Vertex not in heap
    }

    // Verify new priority is actually lower (decrease-key only)
    if (new_priority >= heap->elements[index].priority) {
        LOG_ERROR("nimcp_min_heap_decrease_key failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_min_heap_decrease_key: capacity exceeded");
        return false;
    }

    // Update priority
    heap->elements[index].priority = new_priority;

    // Restore heap property (only need to bubble up since we decreased the key)
    bubble_up(heap, index);

    return true;
}

bool nimcp_min_heap_is_empty(const nimcp_min_heap_t* heap)
{
    LOG_TRACE("Entering nimcp_min_heap_is_empty");
    return !heap || heap->size == 0;
}

bool nimcp_min_heap_is_full(const nimcp_min_heap_t* heap)
{
    LOG_TRACE("Entering nimcp_min_heap_is_full");
    return heap && heap->size >= heap->capacity;
}

uint32_t nimcp_min_heap_size(const nimcp_min_heap_t* heap)
{
    LOG_TRACE("Entering nimcp_min_heap_size");
    return heap ? heap->size : 0;
}

uint32_t nimcp_min_heap_capacity(const nimcp_min_heap_t* heap)
{
    LOG_TRACE("Entering nimcp_min_heap_capacity");
    return heap ? heap->capacity : 0;
}

void nimcp_min_heap_clear(nimcp_min_heap_t* heap)
{
    LOG_TRACE("Entering nimcp_min_heap_clear");
    if (!heap) {
        return;
    }

    // Mark all vertices as not in heap
    for (uint32_t i = 0; i < heap->size; i++) {
        uint32_t vertex_id = heap->elements[i].vertex_id;
        if (vertex_id < heap->max_vertex_id) {
            heap->position_map[vertex_id] = NOT_IN_HEAP;
        }
    }

    heap->size = 0;
}
