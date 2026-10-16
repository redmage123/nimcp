/**
 * @file nimcp_darray.c
 * @brief Generic dynamic array implementation
 */

#include "utils/containers/nimcp_darray.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(darray)

/**
 * @brief Internal dynamic array structure
 */
struct nimcp_darray_t {
    void* data;                           /**< Contiguous element storage */
    size_t size;                          /**< Current number of elements */
    size_t capacity;                      /**< Allocated capacity */
    size_t element_size;                  /**< Size of each element in bytes */
    nimcp_darray_destructor_t destructor; /**< Optional element destructor */
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get pointer to element at index (unchecked)
 */
static inline void* darray_get_ptr(nimcp_darray_t* arr, size_t index) {
    return (char*)arr->data + (index * arr->element_size);
}

/**
 * @brief Get const pointer to element at index (unchecked)
 */
static inline const void* darray_get_ptr_const(const nimcp_darray_t* arr, size_t index) {
    return (const char*)arr->data + (index * arr->element_size);
}

/**
 * @brief Ensure capacity for at least one more element
 */
static bool darray_ensure_capacity(nimcp_darray_t* arr) {
    if (arr->size < arr->capacity) {
        return true;
    }

    /* Grow by GROWTH_FACTOR */
    size_t new_capacity = arr->capacity * NIMCP_DARRAY_GROWTH_FACTOR;
    if (new_capacity < NIMCP_DARRAY_DEFAULT_CAPACITY) {
        new_capacity = NIMCP_DARRAY_DEFAULT_CAPACITY;
    }

    void* new_data = nimcp_realloc(arr->data, new_capacity * arr->element_size);
    if (!new_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "darray_ensure_capacity: new_data is NULL");
        return false;
    }

    arr->data = new_data;
    arr->capacity = new_capacity;
    return true;
}

/**
 * @brief Call destructor for element if destructor is set
 */
static inline void darray_destroy_element(nimcp_darray_t* arr, size_t index) {
    if (arr->destructor) {
        arr->destructor(darray_get_ptr(arr, index));
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

nimcp_darray_t* nimcp_darray_create(size_t element_size, size_t initial_capacity) {
    return nimcp_darray_create_with_destructor(element_size, initial_capacity, NULL);
}

nimcp_darray_t* nimcp_darray_create_with_destructor(
    size_t element_size,
    size_t initial_capacity,
    nimcp_darray_destructor_t destructor
) {
    if (element_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_darray_create_with_destructor: element_size is 0");
        return NULL;
    }

    nimcp_darray_t* arr = (nimcp_darray_t*)nimcp_malloc(sizeof(nimcp_darray_t));
    if (!arr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_darray_create_with_destructor: failed to allocate darray");
        return NULL;
    }

    if (initial_capacity == 0) {
        initial_capacity = NIMCP_DARRAY_DEFAULT_CAPACITY;
    }

    arr->data = nimcp_calloc(initial_capacity, element_size);
    if (!arr->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_darray_create_with_destructor: failed to allocate data array");
        nimcp_free(arr);
        return NULL;
    }

    arr->size = 0;
    arr->capacity = initial_capacity;
    arr->element_size = element_size;
    arr->destructor = destructor;

    return arr;
}

void nimcp_darray_destroy(nimcp_darray_t* arr) {
    if (!arr) return;

    /* Call destructor for all elements */
    if (arr->destructor) {
        for (size_t i = 0; i < arr->size; i++) {
            arr->destructor(darray_get_ptr(arr, i));
        }
    }

    if (arr->data) {
        nimcp_free(arr->data);
    }
    nimcp_free(arr);
}

/* ============================================================================
 * Element Access
 * ============================================================================ */

void* nimcp_darray_at(nimcp_darray_t* arr, size_t index) {
    if (!arr || index >= arr->size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_at: arr is NULL");
        return NULL;
    }
    return darray_get_ptr(arr, index);
}

const void* nimcp_darray_at_const(const nimcp_darray_t* arr, size_t index) {
    if (!arr || index >= arr->size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_at_const: arr is NULL");
        return NULL;
    }
    return darray_get_ptr_const(arr, index);
}

void* nimcp_darray_front(nimcp_darray_t* arr) {
    if (!arr || arr->size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_front: arr is NULL");
        return NULL;
    }
    return arr->data;
}

void* nimcp_darray_back(nimcp_darray_t* arr) {
    if (!arr || arr->size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_back: arr is NULL");
        return NULL;
    }
    return darray_get_ptr(arr, arr->size - 1);
}

void* nimcp_darray_data(nimcp_darray_t* arr) {
    if (!arr || arr->size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_data: arr is NULL");
        return NULL;
    }
    return arr->data;
}

/* ============================================================================
 * Modification Functions
 * ============================================================================ */

bool nimcp_darray_push_back(nimcp_darray_t* arr, const void* element) {
    if (!arr || !element) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_darray_push_back: required parameter is NULL (arr, element)");
        return false;
    }

    if (!darray_ensure_capacity(arr)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_push_back: darray_ensure_capacity is NULL");
        return false;
    }

    memcpy(darray_get_ptr(arr, arr->size), element, arr->element_size);
    arr->size++;
    return true;
}

bool nimcp_darray_pop_back(nimcp_darray_t* arr, void* out_element) {
    if (!arr || arr->size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_pop_back: arr is NULL");
        return false;
    }

    arr->size--;

    if (out_element) {
        memcpy(out_element, darray_get_ptr(arr, arr->size), arr->element_size);
    }

    /* Call destructor after copying out (if copying) */
    if (!out_element) {
        darray_destroy_element(arr, arr->size);
    }

    return true;
}

bool nimcp_darray_set(nimcp_darray_t* arr, size_t index, const void* element) {
    if (!arr || !element || index >= arr->size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_darray_set: required parameter is NULL (arr, element)");
        return false;
    }

    /* Destroy old element first */
    darray_destroy_element(arr, index);

    memcpy(darray_get_ptr(arr, index), element, arr->element_size);
    return true;
}

bool nimcp_darray_insert(nimcp_darray_t* arr, size_t index, const void* element) {
    if (!arr || !element || index > arr->size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_darray_insert: required parameter is NULL (arr, element)");
        return false;
    }

    if (!darray_ensure_capacity(arr)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_insert: darray_ensure_capacity is NULL");
        return false;
    }

    /* Shift elements right */
    if (index < arr->size) {
        memmove(
            darray_get_ptr(arr, index + 1),
            darray_get_ptr(arr, index),
            (arr->size - index) * arr->element_size
        );
    }

    memcpy(darray_get_ptr(arr, index), element, arr->element_size);
    arr->size++;
    return true;
}

bool nimcp_darray_remove_at(nimcp_darray_t* arr, size_t index, void* out_element) {
    if (!arr || index >= arr->size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_remove_at: arr is NULL");
        return false;
    }

    if (out_element) {
        memcpy(out_element, darray_get_ptr(arr, index), arr->element_size);
    } else {
        darray_destroy_element(arr, index);
    }

    /* Shift elements left */
    if (index < arr->size - 1) {
        memmove(
            darray_get_ptr(arr, index),
            darray_get_ptr(arr, index + 1),
            (arr->size - index - 1) * arr->element_size
        );
    }

    arr->size--;
    return true;
}

/* ============================================================================
 * Size and Capacity Functions
 * ============================================================================ */

size_t nimcp_darray_size(const nimcp_darray_t* arr) {
    return arr ? arr->size : 0;
}

size_t nimcp_darray_capacity(const nimcp_darray_t* arr) {
    return arr ? arr->capacity : 0;
}

size_t nimcp_darray_element_size(const nimcp_darray_t* arr) {
    return arr ? arr->element_size : 0;
}

bool nimcp_darray_is_empty(const nimcp_darray_t* arr) {
    return !arr || arr->size == 0;
}

void nimcp_darray_clear(nimcp_darray_t* arr) {
    if (!arr) return;

    /* Call destructor for all elements */
    if (arr->destructor) {
        for (size_t i = 0; i < arr->size; i++) {
            arr->destructor(darray_get_ptr(arr, i));
        }
    }

    arr->size = 0;
}

bool nimcp_darray_reserve(nimcp_darray_t* arr, size_t capacity) {
    if (!arr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_darray_reserve: arr is NULL");
        return false;
    }

    if (capacity <= arr->capacity) {
        return true;  /* Already have enough */
    }

    void* new_data = nimcp_realloc(arr->data, capacity * arr->element_size);
    if (!new_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_darray_reserve: new_data is NULL");
        return false;
    }

    arr->data = new_data;
    arr->capacity = capacity;
    return true;
}

bool nimcp_darray_shrink_to_fit(nimcp_darray_t* arr) {
    if (!arr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_darray_shrink_to_fit: arr is NULL");
        return false;
    }

    if (arr->size == 0) {
        /* Shrink to minimum */
        void* new_data = nimcp_realloc(arr->data, arr->element_size);
        if (new_data) {
            arr->data = new_data;
            arr->capacity = 1;
        }
        return true;
    }

    if (arr->size == arr->capacity) {
        return true;  /* Already at minimum */
    }

    void* new_data = nimcp_realloc(arr->data, arr->size * arr->element_size);
    if (!new_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_darray_shrink_to_fit: new_data is NULL");
        return false;
    }

    arr->data = new_data;
    arr->capacity = arr->size;
    return true;
}

bool nimcp_darray_resize(nimcp_darray_t* arr, size_t new_size) {
    if (!arr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_darray_resize: arr is NULL");
        return false;
    }

    if (new_size == arr->size) {
        return true;
    }

    if (new_size < arr->size) {
        /* Shrinking - destroy excess elements */
        if (arr->destructor) {
            for (size_t i = new_size; i < arr->size; i++) {
                arr->destructor(darray_get_ptr(arr, i));
            }
        }
        arr->size = new_size;
        return true;
    }

    /* Growing - ensure capacity */
    if (new_size > arr->capacity) {
        if (!nimcp_darray_reserve(arr, new_size)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_resize: nimcp_darray_reserve is NULL");
            return false;
        }
    }

    /* Zero-initialize new elements */
    memset(darray_get_ptr(arr, arr->size), 0,
           (new_size - arr->size) * arr->element_size);

    arr->size = new_size;
    return true;
}

bool nimcp_darray_swap(nimcp_darray_t* arr1, nimcp_darray_t* arr2) {
    if (!arr1 || !arr2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_darray_swap: required parameter is NULL (arr1, arr2)");
        return false;
    }

    if (arr1->element_size != arr2->element_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_darray_swap: validation failed");
        return false;  /* Cannot swap arrays with different element sizes */
    }

    /* Swap all fields */
    void* tmp_data = arr1->data;
    size_t tmp_size = arr1->size;
    size_t tmp_capacity = arr1->capacity;
    nimcp_darray_destructor_t tmp_destructor = arr1->destructor;

    arr1->data = arr2->data;
    arr1->size = arr2->size;
    arr1->capacity = arr2->capacity;
    arr1->destructor = arr2->destructor;

    arr2->data = tmp_data;
    arr2->size = tmp_size;
    arr2->capacity = tmp_capacity;
    arr2->destructor = tmp_destructor;

    return true;
}
