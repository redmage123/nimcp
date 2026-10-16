/**
 * @file nimcp_list.h
 * @brief Simple dynamic list for pointer storage
 *
 * WHAT: Dynamic array of void pointers with list-like API
 * WHY:  Provide simple indexed collection for scenarios
 * HOW:  Thin wrapper around nimcp_darray storing void* elements
 *
 * @author NIMCP Development Team
 * @date 2026-01-02
 */

#ifndef NIMCP_LIST_H
#define NIMCP_LIST_H

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include "utils/containers/nimcp_darray.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief List structure (wraps darray)
 */
typedef struct nimcp_list {
    nimcp_darray_t* items;
} nimcp_list_t;

/**
 * @brief Create a new list
 * @return New list or NULL on failure
 */
static inline nimcp_list_t* nimcp_list_create(void) {
    nimcp_list_t* list = (nimcp_list_t*)nimcp_malloc(sizeof(nimcp_list_t));
    if (!list) return NULL;

    list->items = nimcp_darray_create(sizeof(void*), 16);
    if (!list->items) {
        nimcp_free(list);
        return NULL;
    }
    return list;
}

/**
 * @brief Destroy a list (does not free stored pointers)
 * @param list List to destroy
 */
static inline void nimcp_list_destroy(nimcp_list_t* list) {
    if (!list) return;
    if (list->items) {
        nimcp_darray_destroy(list->items);
    }
    nimcp_free(list);
}

/**
 * @brief Get number of items in list
 * @param list List
 * @return Number of items
 */
static inline size_t nimcp_list_size(const nimcp_list_t* list) {
    if (!list || !list->items) return 0;
    return nimcp_darray_size(list->items);
}

/**
 * @brief Get item at index
 * @param list List
 * @param index Index (0-based)
 * @return Item pointer or NULL if out of bounds
 */
static inline void* nimcp_list_get(nimcp_list_t* list, size_t index) {
    if (!list || !list->items) return NULL;
    if (index >= nimcp_darray_size(list->items)) return NULL;

    void** ptr = (void**)nimcp_darray_at(list->items, index);
    return ptr ? *ptr : NULL;
}

/**
 * @brief Append item to end of list
 * @param list List
 * @param item Item to append
 * @return true on success
 */
static inline bool nimcp_list_append(nimcp_list_t* list, void* item) {
    if (!list || !list->items) return false;
    return nimcp_darray_push_back(list->items, &item);
}

/**
 * @brief Remove item at index
 * @param list List
 * @param index Index to remove
 * @return true on success
 */
static inline bool nimcp_list_remove(nimcp_list_t* list, size_t index) {
    if (!list || !list->items) return false;
    return nimcp_darray_remove_at(list->items, index, NULL);
}

/**
 * @brief Clear all items from list
 * @param list List
 */
static inline void nimcp_list_clear(nimcp_list_t* list) {
    if (!list || !list->items) return;
    nimcp_darray_clear(list->items);
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LIST_H */
