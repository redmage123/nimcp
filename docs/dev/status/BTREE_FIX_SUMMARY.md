# B-Tree Bug Fix Summary

## Overview
Fixed critical bugs in B-tree implementation causing double-free memory errors and incorrect item counts during removal operations.

## Issues Found

### Issue #1: Hardcoded should_free_key Parameter
**File:** `src/utils/containers/nimcp_btree.c:502`

**Bug:** When recursing after borrowing/merging operations, the code hardcoded `should_free_key=true` instead of passing through the parameter.

```c
// BEFORE (BUG):
return remove_from_node_internal(tree, child, key, true);

// AFTER (FIX):
return remove_from_node_internal(tree, child, key, should_free_key);
```

**Impact:** When removing a predecessor/successor key (which should NOT be freed because it was moved to parent), if borrowing/merging occurred, the key would be freed anyway, causing:
- Double-free during tree destruction
- Count mismatches

### Issue #2: Incorrect Predecessor/Successor Retrieval
**File:** `src/utils/containers/nimcp_btree.c:415-439`

**Bug:** Code incorrectly assumed predecessor key was at `pred->keys[pred->n - 1]` even when `pred` was an internal node, not a leaf.

**The Problem:**
- Predecessor is defined as the rightmost key in the left subtree (must be in a LEAF)
- Successor is defined as the leftmost key in the right subtree (must be in a LEAF)
- Old code just took keys from the immediate child without traversing to leaves

**The Fix:** Added helper functions that properly traverse to leaves:

```c
/**
 * @brief Get predecessor key (rightmost key in left subtree)
 */
static void* get_predecessor_key(btree_node_t* node)
{
    // Follow rightmost child until we reach a leaf
    while (!node->leaf) {
        node = node->children[node->n];
    }
    return node->keys[node->n - 1];
}

/**
 * @brief Get successor key (leftmost key in right subtree)
 */
static void* get_successor_key(btree_node_t* node)
{
    // Follow leftmost child until we reach a leaf
    while (!node->leaf) {
        node = node->children[0];
    }
    return node->keys[0];
}
```

**Impact:**
- Keys were being "moved" from wrong locations, causing them to remain in multiple places in the tree
- This led to double-free during tree destruction (same key pointer in multiple nodes)
- Count became incorrect (508 instead of 500) because keys weren't actually removed

## Test Results

### Before Fix:
```
BTreeTest.Stress_MixedOperations - FAILED
Expected: 500 items
Actual: 508 items
Valgrind: Invalid free() / double free detected
Custom test: 509 items freed (expected 500)
```

### After Fix:
```
BTreeTest.Stress_MixedOperations - PASSED ✓
All 35 BTree tests - PASSED ✓
Valgrind: No memory errors ✓
Custom test: 500 items freed (expected 500) ✓
```

## Files Modified

1. **src/utils/containers/nimcp_btree.c**
   - Added `get_predecessor_key()` helper (lines 400-408)
   - Added `get_successor_key()` helper (lines 420-428)
   - Updated predecessor replacement to use helper (line 458)
   - Updated successor replacement to use helper (line 470)
   - Fixed should_free_key passthrough (line 502)

## Debugging Methodology

Created comprehensive debugging suite (`debug_suite.py`) that:
1. Automatically detects symptoms (memory corruption, count mismatch, crashes)
2. Selects appropriate tools (valgrind, gdb, rr, sanitizers)
3. Parses output and provides actionable recommendations
4. Saves detailed reports for analysis

**Usage:**
```bash
./debug_suite.py --test ./src/tests/utility_tests --filter BTreeTest.Stress_MixedOperations --mode auto
```

## Root Cause Analysis Process

1. **Initial symptom:** Test failing with count=508/518 instead of 500
2. **Valgrind revealed:** Double-free during tree destruction at `destroy_node:78`
3. **Custom test showed:** 509 items freed when count said 508
4. **Analysis:** Same key pointer existed in multiple nodes
5. **Deep dive:** Predecessor/successor algorithm was incorrect - not traversing to leaves
6. **Secondary issue:** Parameter not being passed through after borrowing/merging

## Prevention

Added directive to `.claude/claude.md` requiring use of debugging suite for ALL bug investigations going forward.

## Verification

- [x] All BTree tests pass (35/35)
- [x] Valgrind shows no memory errors
- [x] Custom stress test with 100 iterations passes
- [x] Count matches expected value
- [x] No double-frees detected
- [x] All items properly freed during destruction
