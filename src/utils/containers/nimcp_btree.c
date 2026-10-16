#include "utils/containers/nimcp_btree.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include <string.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(btree)

#define MAX_LOCK_RETRIES 3
#define LOCK_TIMEOUT_MS NIMCP_MEDIUM_TIMEOUT_MS

// Node structure
struct btree_node_t {
    void* keys[2 * BTREE_ORDER - 1];
    btree_node_t* children[2 * BTREE_ORDER];
    int n;
    bool leaf;
    nimcp_platform_rwlock_t lock;
};

// B-tree structure
struct btree_t {
    btree_node_t* root;
    btree_compare_func compare;
    btree_key_func key_func;
    btree_free_func free_func;
    atomic_size_t count;
    nimcp_platform_mutex_t structural_lock;
};

// Iterator structure
struct btree_iterator_t {
    btree_t* tree;
    btree_node_t** stack;
    int* index_stack;
    int stack_size;
    int stack_capacity;
    nimcp_platform_mutex_t lock;
};

// Lock acquisition with timeout
static int acquire_write_lock(nimcp_platform_rwlock_t* lock, unsigned long timeout_ms)
{
    (void)timeout_ms;  // Platform abstraction doesn't support timed locks yet
    return nimcp_platform_rwlock_wrlock(lock);
}

static int acquire_read_lock(nimcp_platform_rwlock_t* lock, unsigned long timeout_ms)
{
    (void)timeout_ms;  // Platform abstraction doesn't support timed locks yet
    return nimcp_platform_rwlock_rdlock(lock);
}

// Node creation and destruction
static btree_node_t* create_node(bool leaf)
{
    btree_node_t* node = nimcp_calloc(1, sizeof(btree_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_node: node is NULL");
        return NULL;
    }

    node->leaf = leaf;
    node->n = 0;

    if (nimcp_platform_rwlock_init(&node->lock) != 0) {
        nimcp_free(node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "create_node: validation failed");
        return NULL;
    }

    return node;
}

static void destroy_node(btree_node_t* node, btree_free_func free_func)
{
    if (!node)
        return;

    if (free_func) {
        for (int i = 0; i < node->n; i++) {
            free_func(node->keys[i]);
        }
    }

    nimcp_platform_rwlock_destroy(&node->lock);
    nimcp_free(node);
}

// B-tree creation and destruction
btree_t* btree_create(btree_compare_func compare, btree_key_func key_func,
                      btree_free_func free_func)
{
    if (!compare || !key_func) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "btree_create: compare and key_func cannot be NULL");
        return NULL;
    }

    btree_t* tree = nimcp_calloc(1, sizeof(btree_t));
    if (!tree) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "btree_create: failed to allocate btree structure");
        return NULL;
    }

    tree->root = create_node(true);
    if (!tree->root) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "btree_create: failed to allocate root node");
        nimcp_free(tree);
        return NULL;
    }

    if (nimcp_platform_mutex_init(&tree->structural_lock, false) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "btree_create: failed to initialize structural_lock mutex");
        destroy_node(tree->root, NULL);
        nimcp_free(tree);
        return NULL;
    }

    tree->compare = compare;
    tree->key_func = key_func;
    tree->free_func = free_func;
    atomic_init(&tree->count, 0);

    return tree;
}

static void destroy_subtree(btree_node_t* node, btree_free_func free_func)
{
    if (!node)
        return;

    // No locking needed during destruction - structural_lock is held
    // and no concurrent operations should be happening

    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            destroy_subtree(node->children[i], free_func);
        }
    }

    destroy_node(node, free_func);
}

void btree_destroy(btree_t* tree)
{
    if (!tree)
        return;

    nimcp_platform_mutex_lock(&tree->structural_lock);
    destroy_subtree(tree->root, tree->free_func);
    nimcp_platform_mutex_unlock(&tree->structural_lock);
    nimcp_platform_mutex_destroy(&tree->structural_lock);
    nimcp_free(tree);
}

// Node splitting
static int split_child(btree_t* tree, btree_node_t* parent, int index)
{
    btree_node_t* child = parent->children[index];
    btree_node_t* new_node = create_node(child->leaf);
    if (!new_node) {
        // CRITICAL FIX: Return error instead of silently failing
        return BTREE_NO_MEMORY;
    }

    // Initialize new node
    new_node->n = BTREE_ORDER - 1;

    // Copy keys and children
    for (int j = 0; j < BTREE_ORDER - 1; j++) {
        new_node->keys[j] = child->keys[j + BTREE_ORDER];
    }

    if (!child->leaf) {
        for (int j = 0; j < BTREE_ORDER; j++) {
            new_node->children[j] = child->children[j + BTREE_ORDER];
        }
    }

    child->n = BTREE_ORDER - 1;

    // Move parent's children
    for (int j = parent->n; j >= index + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }

    parent->children[index + 1] = new_node;

    // Move parent's keys
    for (int j = parent->n - 1; j >= index; j--) {
        parent->keys[j + 1] = parent->keys[j];
    }

    parent->keys[index] = child->keys[BTREE_ORDER - 1];
    parent->n++;

    return BTREE_SUCCESS;
}

// Insertion
static int insert_nonfull(btree_t* tree, btree_node_t* node, void* data)
{
    int i = node->n - 1;
    const char* key = tree->key_func(data);

    if (node->leaf) {
        // Find position and insert
        while (i >= 0 && tree->compare(tree->key_func(node->keys[i]), key) > 0) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }

        node->keys[i + 1] = data;
        node->n++;
        atomic_fetch_add(&tree->count, 1);
        return BTREE_SUCCESS;
    } else {
        // Find child to recurse
        while (i >= 0 && tree->compare(tree->key_func(node->keys[i]), key) > 0) {
            i--;
        }
        i++;

        // Save original child index before potential split
        int child_index = i;

        // Lock child
        if (acquire_write_lock(&node->children[child_index]->lock, LOCK_TIMEOUT_MS) != 0) {
            return BTREE_LOCKED;
        }

        if (node->children[child_index]->n == 2 * BTREE_ORDER - 1) {
            int result = split_child(tree, node, child_index);
            if (result != BTREE_SUCCESS) {
                nimcp_platform_rwlock_wrunlock(&node->children[child_index]->lock);
                return result;
            }
            // Unlock the original child we locked
            nimcp_platform_rwlock_wrunlock(&node->children[child_index]->lock);

            // Determine which child to use after split
            if (tree->compare(key, tree->key_func(node->keys[child_index])) > 0) {
                i = child_index + 1;  // Use right child
            } else {
                i = child_index;  // Use left child
            }

            // Lock the chosen child for recursion
            if (acquire_write_lock(&node->children[i]->lock, LOCK_TIMEOUT_MS) != 0) {
                return BTREE_LOCKED;
            }
        }

        int result = insert_nonfull(tree, node->children[i], data);
        nimcp_platform_rwlock_wrunlock(&node->children[i]->lock);
        return result;
    }
}

int btree_insert(btree_t* tree, void* data)
{
    if (!tree || !data)
        return BTREE_ERROR;

    nimcp_platform_mutex_lock(&tree->structural_lock);

    if (acquire_write_lock(&tree->root->lock, LOCK_TIMEOUT_MS) != 0) {
        nimcp_platform_mutex_unlock(&tree->structural_lock);
        return BTREE_LOCKED;
    }

    int result = BTREE_SUCCESS;

    if (tree->root->n == 2 * BTREE_ORDER - 1) {
        btree_node_t* new_root = create_node(false);
        if (!new_root) {
            nimcp_platform_rwlock_wrunlock(&tree->root->lock);
            nimcp_platform_mutex_unlock(&tree->structural_lock);
            return BTREE_NO_MEMORY;
        }

        // CRITICAL FIX: Lock new_root immediately after creation
        // insert_nonfull() assumes the node is locked by caller
        if (acquire_write_lock(&new_root->lock, LOCK_TIMEOUT_MS) != 0) {
            nimcp_platform_rwlock_destroy(&new_root->lock);
            nimcp_free(new_root);
            nimcp_platform_rwlock_wrunlock(&tree->root->lock);
            nimcp_platform_mutex_unlock(&tree->structural_lock);
            return BTREE_LOCKED;
        }

        btree_node_t* old_root = tree->root;
        new_root->children[0] = old_root;

        result = split_child(tree, new_root, 0);
        if (result == BTREE_SUCCESS) {
            // CRITICAL: Unlock old root BEFORE updating tree->root
            // Otherwise old_root's lock remains held forever!
            nimcp_platform_rwlock_wrunlock(&old_root->lock);

            // Only update root if split succeeded
            tree->root = new_root;
            // new_root is now locked - safe to call insert_nonfull
            result = insert_nonfull(tree, new_root, data);
        } else {
            // Split failed - cleanup new_root and rollback
            nimcp_platform_rwlock_wrunlock(&old_root->lock);
            nimcp_platform_rwlock_wrunlock(&new_root->lock);
            nimcp_platform_rwlock_destroy(&new_root->lock);
            nimcp_free(new_root);
            result = BTREE_NO_MEMORY;
        }
    } else {
        result = insert_nonfull(tree, tree->root, data);
    }

    nimcp_platform_rwlock_wrunlock(&tree->root->lock);
    nimcp_platform_mutex_unlock(&tree->structural_lock);
    return result;
}

// Search
static void* search_node(const btree_t* tree, btree_node_t* node, const char* key)
{
    int i = 0;
    while (i < node->n && tree->compare(tree->key_func(node->keys[i]), key) < 0) {
        i++;
    }

    if (i < node->n && tree->compare(tree->key_func(node->keys[i]), key) == 0) {
        return node->keys[i];
    }

    if (node->leaf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "search_node: validation failed");
        return NULL;
    }

    if (acquire_read_lock(&node->children[i]->lock, LOCK_TIMEOUT_MS) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "search_node: validation failed");
        return NULL;
    }

    void* result = search_node(tree, node->children[i], key);
    nimcp_platform_rwlock_rdunlock(&node->children[i]->lock);
    return result;
}

void* btree_find(const btree_t* tree, const char* key)
{
    if (!tree || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "btree_find: required parameter is NULL (tree, key)");
        return NULL;
    }

    if (acquire_read_lock(&tree->root->lock, LOCK_TIMEOUT_MS) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "btree_find: validation failed");
        return NULL;
    }

    void* result = search_node(tree, tree->root, key);
    nimcp_platform_rwlock_rdunlock(&tree->root->lock);
    return result;
}

// Removal helper functions
static void merge_nodes(btree_t* tree, btree_node_t* parent, int index, btree_node_t* left,
                        btree_node_t* right)
{
    // Copy key from parent
    left->keys[left->n] = parent->keys[index];
    left->n++;

    // Copy keys and children from right
    for (int i = 0; i < right->n; i++) {
        left->keys[left->n + i] = right->keys[i];
    }

    if (!left->leaf) {
        for (int i = 0; i <= right->n; i++) {
            left->children[left->n + i] = right->children[i];
        }
    }

    left->n += right->n;

    // Update parent
    for (int i = index + 1; i < parent->n; i++) {
        parent->keys[i - 1] = parent->keys[i];
        parent->children[i] = parent->children[i + 1];
    }
    parent->n--;

    destroy_node(right, NULL);
}

static bool remove_from_node_internal(btree_t* tree, btree_node_t* node, const char* key, bool should_free_key);

static bool remove_from_node(btree_t* tree, btree_node_t* node, const char* key)
{
    return remove_from_node_internal(tree, node, key, true);
}

/**
 * @brief Get predecessor key (rightmost key in left subtree)
 *
 * WHAT: Finds the rightmost key by following rightmost children to a leaf
 * WHY:  Predecessor is needed for internal node removal
 * HOW:  Traverse rightmost path until leaf, return last key
 *
 * @param node Root of subtree to search
 * @return Pointer to predecessor key data
 */
static void* get_predecessor_key(btree_node_t* node)
{
    // Follow rightmost child until we reach a leaf
    while (!node->leaf) {
        node = node->children[node->n];
    }
    // Return rightmost key in leaf
    return node->keys[node->n - 1];
}

/**
 * @brief Get successor key (leftmost key in right subtree)
 *
 * WHAT: Finds the leftmost key by following leftmost children to a leaf
 * WHY:  Successor is needed for internal node removal
 * HOW:  Traverse leftmost path until leaf, return first key
 *
 * @param node Root of subtree to search
 * @return Pointer to successor key data
 */
static void* get_successor_key(btree_node_t* node)
{
    // Follow leftmost child until we reach a leaf
    while (!node->leaf) {
        node = node->children[0];
    }
    // Return leftmost key in leaf
    return node->keys[0];
}

static bool remove_from_node_internal(btree_t* tree, btree_node_t* node, const char* key, bool should_free_key)
{
    int i = 0;
    while (i < node->n && tree->compare(tree->key_func(node->keys[i]), key) < 0) {
        i++;
    }

    if (i < node->n && tree->compare(tree->key_func(node->keys[i]), key) == 0) {
        if (node->leaf) {
            // Remove from leaf
            void* key_to_free = node->keys[i];
            for (int j = i + 1; j < node->n; j++) {
                node->keys[j - 1] = node->keys[j];
            }
            node->n--;
            // Free only if requested (not if key is being moved to parent)
            if (should_free_key && tree->free_func) {
                tree->free_func(key_to_free);
            }
            return true;  // Key was found and removed
        } else {
            // Remove from internal node
            btree_node_t* pred = node->children[i];
            btree_node_t* succ = node->children[i + 1];

            if (pred->n >= BTREE_ORDER) {
                // Use predecessor - get rightmost key from left subtree
                void* key_to_free = node->keys[i];
                void* pred_key = get_predecessor_key(pred);
                node->keys[i] = pred_key;
                // Remove pred_key from pred subtree WITHOUT freeing (it moved to parent)
                remove_from_node_internal(tree, pred, tree->key_func(pred_key), false);
                // Free the original key that was replaced (if should_free_key)
                if (should_free_key && tree->free_func) {
                    tree->free_func(key_to_free);
                }
                return true;  // Key was found and removed
            } else if (succ->n >= BTREE_ORDER) {
                // Use successor - get leftmost key from right subtree
                void* key_to_free = node->keys[i];
                void* succ_key = get_successor_key(succ);
                node->keys[i] = succ_key;
                // Remove succ_key from succ subtree WITHOUT freeing (it moved to parent)
                remove_from_node_internal(tree, succ, tree->key_func(succ_key), false);
                // Free the original key that was replaced (if should_free_key)
                if (should_free_key && tree->free_func) {
                    tree->free_func(key_to_free);
                }
                return true;  // Key was found and removed
            } else {
                // Merge nodes and recursively remove
                merge_nodes(tree, node, i, pred, succ);
                // Pass through should_free_key to recursive call
                return remove_from_node_internal(tree, pred, key, should_free_key);
            }
        }
    } else if (!node->leaf) {
        // Recurse to appropriate child
        btree_node_t* child = node->children[i];

        if (child->n < BTREE_ORDER) {
            // Child needs more keys - borrow or merge
            btree_node_t* left_sibling = i > 0 ? node->children[i - 1] : NULL;
            btree_node_t* right_sibling = i < node->n ? node->children[i + 1] : NULL;

            if (left_sibling && left_sibling->n >= BTREE_ORDER) {
                // Borrow from left sibling
                for (int j = child->n; j > 0; j--) {
                    child->keys[j] = child->keys[j - 1];
                }
                child->keys[0] = node->keys[i - 1];
                node->keys[i - 1] = left_sibling->keys[left_sibling->n - 1];

                if (!child->leaf) {
                    for (int j = child->n + 1; j > 0; j--) {
                        child->children[j] = child->children[j - 1];
                    }
                    child->children[0] = left_sibling->children[left_sibling->n];
                }

                child->n++;
                left_sibling->n--;
            } else if (right_sibling && right_sibling->n >= BTREE_ORDER) {
                // Borrow from right sibling
                child->keys[child->n] = node->keys[i];
                node->keys[i] = right_sibling->keys[0];

                for (int j = 0; j < right_sibling->n - 1; j++) {
                    right_sibling->keys[j] = right_sibling->keys[j + 1];
                }

                if (!child->leaf) {
                    child->children[child->n + 1] = right_sibling->children[0];
                    for (int j = 0; j < right_sibling->n; j++) {
                        right_sibling->children[j] = right_sibling->children[j + 1];
                    }
                }

                child->n++;
                right_sibling->n--;
            } else {
                // Merge with a sibling
                if (left_sibling) {
                    merge_nodes(tree, node, i - 1, left_sibling, child);
                    child = left_sibling;
                } else if (right_sibling) {
                    merge_nodes(tree, node, i, child, right_sibling);
                }
            }
        }

        // Pass through should_free_key parameter to maintain consistency
        return remove_from_node_internal(tree, child, key, should_free_key);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "remove_from_node_internal: operation failed");
    return false;  // Key not found
}

int btree_remove(btree_t* tree, const char* key)
{
    if (!tree || !key)
        return BTREE_ERROR;

    nimcp_platform_mutex_lock(&tree->structural_lock);

    if (acquire_write_lock(&tree->root->lock, LOCK_TIMEOUT_MS) != 0) {
        nimcp_platform_mutex_unlock(&tree->structural_lock);
        return BTREE_LOCKED;
    }

    // Try to remove the key
    bool found = remove_from_node(tree, tree->root, key);

    if (found) {
        // Decrement count only if key was actually found and removed
        atomic_fetch_sub(&tree->count, 1);
    }

    // Handle empty root
    if (tree->root->n == 0 && !tree->root->leaf) {
        btree_node_t* old_root = tree->root;
        btree_node_t* new_root = tree->root->children[0];

        // CRITICAL: Unlock old root BEFORE changing tree->root
        nimcp_platform_rwlock_wrunlock(&old_root->lock);

        tree->root = new_root;
        destroy_node(old_root, NULL);
    } else {
        // Normal case: unlock the current root
        nimcp_platform_rwlock_wrunlock(&tree->root->lock);
    }

    nimcp_platform_mutex_unlock(&tree->structural_lock);
    return found ? BTREE_SUCCESS : BTREE_NOT_FOUND;
}

size_t btree_count(const btree_t* tree)
{
    return tree ? atomic_load(&tree->count) : 0;
}

// Iterator implementation
btree_iterator_t* btree_iterator_create(btree_t* tree)
{
    if (!tree) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "btree_iterator_create: tree is NULL");
        return NULL;
    }

    btree_iterator_t* iterator = nimcp_calloc(1, sizeof(btree_iterator_t));
    if (!iterator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "btree_iterator_create: failed to allocate iterator");
        return NULL;
    }

    iterator->tree = tree;
    iterator->stack_capacity = 32;  // Initial capacity
    iterator->stack = nimcp_calloc(iterator->stack_capacity, sizeof(btree_node_t*));
    iterator->index_stack = nimcp_calloc(iterator->stack_capacity, sizeof(int));

    if (!iterator->stack || !iterator->index_stack) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "btree_iterator_create: failed to allocate stack arrays");
        nimcp_free(iterator->stack);
        nimcp_free(iterator->index_stack);
        nimcp_free(iterator);
        return NULL;
    }

    if (nimcp_platform_mutex_init(&iterator->lock, false) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "btree_iterator_create: failed to initialize mutex");
        nimcp_free(iterator->stack);
        nimcp_free(iterator->index_stack);
        nimcp_free(iterator);
        return NULL;
    }

    return iterator;
}

void btree_iterator_destroy(btree_iterator_t* iterator)
{
    if (!iterator)
        return;

    // CRITICAL FIX: Release all held read locks before destroying iterator
    nimcp_platform_mutex_lock(&iterator->lock);
    for (int i = 0; i < iterator->stack_size; i++) {
        if (iterator->stack[i]) {
            nimcp_platform_rwlock_rdunlock(&iterator->stack[i]->lock);
        }
    }
    nimcp_platform_mutex_unlock(&iterator->lock);

    nimcp_platform_mutex_destroy(&iterator->lock);
    nimcp_free(iterator->stack);
    nimcp_free(iterator->index_stack);
    nimcp_free(iterator);
}

bool btree_iterator_next(btree_iterator_t* iterator, void** data)
{
    if (!iterator || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "btree_iterator_next: required parameter is NULL (iterator, data)");
        return false;
    }

    nimcp_platform_mutex_lock(&iterator->lock);

    if (iterator->stack_size == 0) {
        // Initialize iterator at leftmost leaf
        btree_node_t* node = iterator->tree->root;
        while (node) {
            if (acquire_read_lock(&node->lock, LOCK_TIMEOUT_MS) != 0) {
                nimcp_platform_mutex_unlock(&iterator->lock);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "btree_iterator_next: validation failed");
                return false;
            }

            iterator->stack[iterator->stack_size] = node;
            iterator->index_stack[iterator->stack_size] = 0;
            iterator->stack_size++;

            if (node->leaf)
                break;
            node = node->children[0];
        }
    }

    while (iterator->stack_size > 0) {
        btree_node_t* current = iterator->stack[iterator->stack_size - 1];
        int current_index = iterator->index_stack[iterator->stack_size - 1];

        if (current_index >= current->n) {
            nimcp_platform_rwlock_rdunlock(&current->lock);
            iterator->stack_size--;
            continue;
        }

        *data = current->keys[current_index];
        iterator->index_stack[iterator->stack_size - 1]++;

        if (!current->leaf && current->children[current_index + 1]) {
            btree_node_t* node = current->children[current_index + 1];
            while (node) {
                if (acquire_read_lock(&node->lock, LOCK_TIMEOUT_MS) != 0) {
                    nimcp_platform_mutex_unlock(&iterator->lock);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "btree_iterator_next: validation failed");
                    return false;
                }

                if (iterator->stack_size >= iterator->stack_capacity) {
                    // Resize stacks if needed
                    size_t new_capacity = iterator->stack_capacity * 2;
                    btree_node_t** new_stack =
                        nimcp_realloc(iterator->stack, new_capacity * sizeof(btree_node_t*));
                    int* new_index_stack =
                        nimcp_realloc(iterator->index_stack, new_capacity * sizeof(int));

                    if (!new_stack || !new_index_stack) {
                        nimcp_platform_rwlock_rdunlock(&node->lock);
                        nimcp_platform_mutex_unlock(&iterator->lock);
                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "btree_iterator_next: required parameter is NULL (new_stack, new_index_stack)");
                        return false;
                    }

                    iterator->stack = new_stack;
                    iterator->index_stack = new_index_stack;
                    iterator->stack_capacity = new_capacity;
                }

                iterator->stack[iterator->stack_size] = node;
                iterator->index_stack[iterator->stack_size] = 0;
                iterator->stack_size++;

                if (node->leaf)
                    break;
                node = node->children[0];
            }
        }

        nimcp_platform_mutex_unlock(&iterator->lock);
        return true;
    }

    nimcp_platform_mutex_unlock(&iterator->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "btree_iterator_next: operation failed");
    return false;
}

// Traversal
static void traverse_node(const btree_node_t* node, btree_traverse_func callback, void* user_data)
{
    if (!node)
        return;

    int i;
    for (i = 0; i < node->n; i++) {
        if (!node->leaf) {
            traverse_node(node->children[i], callback, user_data);
        }
        callback(node->keys[i], user_data);
    }

    if (!node->leaf) {
        traverse_node(node->children[i], callback, user_data);
    }
}

void btree_foreach(const btree_t* tree, btree_traverse_func callback, void* user_data)
{
    if (!tree || !callback)
        return;

    if (acquire_read_lock(&tree->root->lock, LOCK_TIMEOUT_MS) != 0) {
        return;
    }

    traverse_node(tree->root, callback, user_data);
    nimcp_platform_rwlock_rdunlock(&tree->root->lock);
}
