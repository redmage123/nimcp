#ifndef NIMCP_BTREE_H
#define NIMCP_BTREE_H

#include <stdbool.h>
#include <stddef.h>
#ifndef __cplusplus
    #include <stdatomic.h>
#endif
#include <pthread.h>

// Forward declarations
#ifdef __cplusplus
extern "C" {
#endif

typedef struct btree_t btree_t;
typedef struct btree_node_t btree_node_t;
typedef struct btree_iterator_t btree_iterator_t;

// Function type definitions
typedef int (*btree_compare_func)(const char* key1, const char* key2);
typedef const char* (*btree_key_func)(const void* data);
typedef void (*btree_free_func)(void* data);
typedef void (*btree_traverse_func)(void* data, void* user_data);

// B-tree order (minimum degree)
#define BTREE_ORDER 3

// Error codes
#define BTREE_SUCCESS 0
#define BTREE_ERROR (-1)
#define BTREE_NOT_FOUND (-2)
#define BTREE_DUPLICATE (-3)
#define BTREE_NO_MEMORY (-4)
#define BTREE_LOCKED (-5)
#define BTREE_TIMEOUT (-6)

/**
 * Creates a new thread-safe B-tree
 * @param compare Function to compare keys
 * @param key_func Function to extract key from data
 * @param free_func Function to free data (can be NULL)
 * @return New B-tree instance or NULL on failure
 * @thread_safety Thread-safe
 */
btree_t* btree_create(btree_compare_func compare, btree_key_func key_func,
                      btree_free_func free_func);

/**
 * Destroys a B-tree and all its contents
 * @param tree The B-tree to destroy
 * @thread_safety Thread-safe, but should not be called while other threads are using the tree
 */
void btree_destroy(btree_t* tree);

/**
 * Inserts data into the B-tree
 * @param tree The B-tree
 * @param data The data to insert
 * @return BTREE_SUCCESS on success, error code on failure
 * @thread_safety Thread-safe
 */
int btree_insert(btree_t* tree, void* data);

/**
 * Finds data in the B-tree by key
 * @param tree The B-tree
 * @param key The key to search for
 * @return The found data or NULL if not found
 * @thread_safety Thread-safe
 */
void* btree_find(const btree_t* tree, const char* key);

/**
 * Removes data from the B-tree by key
 * @param tree The B-tree
 * @param key The key of the data to remove
 * @return BTREE_SUCCESS on success, error code on failure
 * @thread_safety Thread-safe
 */
int btree_remove(btree_t* tree, const char* key);

/**
 * Returns the number of items in the B-tree
 * @param tree The B-tree
 * @return Number of items
 * @thread_safety Thread-safe
 */
size_t btree_count(const btree_t* tree);

/**
 * Traverses the B-tree in-order, calling callback for each item
 * @param tree The B-tree
 * @param callback Function to call for each item
 * @param user_data User data passed to callback
 * @thread_safety Thread-safe
 */
void btree_foreach(const btree_t* tree, btree_traverse_func callback, void* user_data);

/**
 * Creates an iterator for the B-tree
 * @param tree The B-tree
 * @return New iterator or NULL on failure
 * @thread_safety Thread-safe
 */
btree_iterator_t* btree_iterator_create(btree_t* tree);

/**
 * Advances the iterator to the next item
 * @param iterator The iterator
 * @param data Pointer to store the next item
 * @return true if next item exists, false if end reached
 * @thread_safety Thread-safe
 */
bool btree_iterator_next(btree_iterator_t* iterator, void** data);

/**
 * Destroys an iterator
 * @param iterator The iterator to destroy
 */
void btree_iterator_destroy(btree_iterator_t* iterator);


#ifdef __cplusplus
}
#endif
#endif  // NIMCP_BTREE_H
