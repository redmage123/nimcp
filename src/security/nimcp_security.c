/**
 * @file nimcp_security.c
 * @brief Implementation of NIMCP security framework
 *
 * WHAT: Comprehensive security framework protecting NIMCP neural network from:
 *       - Core directive tampering
 *       - Prompt injection attacks
 *       - Malicious input data
 *       - Inter-component communication compromise
 *
 * WHY:  Ensures the integrity and safety of the neural network by preventing
 *       unauthorized modifications to core behavioral directives and filtering
 *       potentially harmful inputs before they reach the neural substrate.
 *
 * DESIGN PATTERNS:
 *       - Strategy Pattern: Pluggable validation strategies
 *       - Builder Pattern: Encryption context construction
 *       - Singleton: Pattern cache for O(1) lookups
 *       - Flyweight: Shared pattern definitions
 */

#include "security/nimcp_security.h"
#include "security/nimcp_constant_time.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include <ctype.h>
#include <stdio.h>

#define LOG_MODULE "security"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>

// WHAT: Production-grade encryption using libsodium
// WHY:  Replaces insecure XOR cipher with AES-256-GCM authenticated encryption
// HOW:  crypto_aead_aes256gcm_* API provides NIST-standard AEAD cipher
#ifdef NIMCP_ENABLE_ENCRYPTION
#include <sodium.h>
#endif

#include "utils/memory/nimcp_memory.h"  // CRITICAL: Declares nimcp_calloc/nimcp_free return types
#include "core/neuralnet/nimcp_neuralnet.h"  // Phase 11: Access to neural network for biological security
#include "utils/exception/nimcp_exception_macros.h"  // Exception handling with immune integration

// Platform-specific includes for cryptographically secure RNG
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <ntstatus.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
// BSD systems provide arc4random_buf() as a fallback
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
// arc4random_buf is in stdlib.h which is already included
#endif
#endif

//=============================================================================
// Constants for Performance and Maintainability
//=============================================================================

/**
 * WHAT: Threshold for suspicious special character density in input
 * WHY:  More than 33% special characters often indicates escape sequence
 *       attacks or obfuscation attempts. This ratio balances legitimate use
 *       (e.g., code snippets) with security detection.
 */
#define SPECIAL_CHAR_THRESHOLD_RATIO 3

/**
 * WHAT: Number of security event types supported
 * WHY:  Used for array bounds checking to prevent out-of-bounds access
 */
#define NUM_SECURITY_EVENT_TYPES 6

/**
 * WHAT: Number of threat severity levels
 * WHY:  Used for array bounds checking in logging functions
 */
#define NUM_THREAT_LEVELS 5

/**
 * WHAT: FNV-1a hash algorithm prime for 64-bit hashing
 * WHY:  Well-tested constant that provides good hash distribution properties
 *       while being fast enough for embedded systems
 */
#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME        1099511628211ULL

/**
 * WHAT: Pattern validation cache size (power of 2 for fast modulo)
 * WHY:  Caches recent validation results for O(1) lookup. 256 entries
 *       provides good hit rate while keeping memory footprint low (< 20KB).
 *       Power of 2 allows bitwise AND for modulo operation.
 */
#define VALIDATION_CACHE_SIZE 256

/**
 * WHAT: Maximum children per Aho-Corasick trie node (alphabet size)
 * WHY:  Lowercase a-z (26) + space + common punctuation = 32 chars.
 *       Power of 2 for efficient indexing. Covers all pattern characters.
 */
#define AC_ALPHABET_SIZE 32

/**
 * WHAT: Maximum number of Aho-Corasick trie nodes
 * WHY:  40 patterns × ~15 avg chars = ~600 nodes.
 *       1024 provides headroom for pattern expansion.
 *       Static allocation avoids malloc in critical path.
 */
#define AC_MAX_NODES 1024

/**
 * WHAT: Queue size for BFS during failure link construction
 * WHY:  Must hold all nodes at deepest trie level.
 *       1024 matches max nodes, ensuring no overflow.
 */
#define AC_QUEUE_SIZE 1024

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Directive system state container
 * WHY:  Encapsulates all directives with metadata to track locking state
 *       and prevent modifications after initialization phase
 */
struct nimcp_directive_system {
    nimcp_core_directive_t directives[NIMCP_SECURITY_MAX_DIRECTIVES];
    uint32_t num_directives;
    bool locked;
    uint64_t lock_timestamp;
};

/**
 * WHAT: Encryption context for inter-component communication
 * WHY:  Maintains encryption key for securing neural network communications.
 *       Uses lightweight cipher for embedded systems.
 */
struct nimcp_encryption_context {
    uint8_t key[NIMCP_SECURITY_KEY_SIZE];
    bool initialized;
};

/**
 * WHAT: Aho-Corasick trie node for multi-pattern matching
 * WHY:  Enables O(n+k) pattern detection vs O(n*m) naive approach.
 *       Each node represents a state in the automaton.
 *
 * DESIGN PATTERN: Trie data structure with failure links (KMP-style)
 */
typedef struct ac_node {
    int children[AC_ALPHABET_SIZE];  // Child pointers (indices into node pool)
    int failure;                     // Failure link (KMP-style suffix link)
    bool is_pattern_end;             // True if this node ends a pattern
} ac_node_t;

/**
 * WHAT: Global Aho-Corasick automaton state
 * WHY:  Pre-built trie for all injection patterns. Built once at init,
 *       used for all validations. Static allocation for zero malloc overhead.
 *
 * PERFORMANCE: O(sum of pattern lengths) build time, O(n+k) search time
 */
static struct {
    ac_node_t nodes[AC_MAX_NODES];  // Node pool (static allocation)
    int node_count;                  // Number of allocated nodes
    bool initialized;                // Whether trie is built
} ac_trie = {0};

/**
 * WHAT: Validation cache entry for O(1) pattern lookup
 * WHY:  Caching validation results eliminates redundant pattern matching
 *       for frequently seen inputs (e.g., repeated sensor readings)
 */
typedef struct {
    uint64_t input_hash;                    // Hash of validated input
    nimcp_input_validation_t result;        // Cached validation result
    nimcp_threat_level_t threat_level;      // Cached threat level
    uint64_t timestamp;                     // When cached (for expiry)
} validation_cache_entry_t;

/**
 * WHAT: Global validation cache (LRU-style ring buffer)
 * WHY:  Provides O(1) average-case lookup for repeated inputs.
 *       Ring buffer evicts oldest entries automatically.
 */
static struct {
    validation_cache_entry_t entries[VALIDATION_CACHE_SIZE];
    uint32_t next_index;  // Next position to write (ring buffer)
    uint64_t hits;        // Cache hit count
    uint64_t misses;      // Cache miss count
} validation_cache = {0};

/**
 * WHAT: Security event statistics tracker
 * WHY:  Provides visibility into security posture and attack patterns.
 *       Global for simplicity but should be made thread-local for
 *       production multi-threaded environments.
 *
 * TODO: Refactor to thread-local storage or per-context statistics
 */
static struct {
    uint64_t threats_detected;
    uint64_t inputs_rejected;
    uint64_t directives_verified;
} security_stats = {0};

/**
 * WHAT: Expanded prompt injection patterns (OWASP LLM Top 10 + research)
 * WHY:  Comprehensive list of known attack patterns from:
 *       - OWASP LLM Top 10 (2024)
 *       - Lakera AI jailbreak research
 *       - Real-world exploit observations
 *       - Academic LLM security papers
 *
 * DESIGN PATTERN: Flyweight - shared immutable pattern definitions
 */
static const char* injection_patterns[] = {
    // Classic instruction override attacks
    "ignore previous",
    "ignore all previous",
    "disregard previous",
    "forget previous",
    "ignore the above",
    "disregard the above",
    "forget everything",

    // Role confusion / jailbreak attempts
    "you are now",
    "act as",
    "pretend you are",
    "simulate",
    "roleplay as",
    "pretend to be",
    "from now on",
    "new instructions",

    // System prompt injection
    "system:",
    "<|system|>",
    "### system:",
    "### instruction:",
    "[system]",
    "assistant:",
    "<|im_start|>",
    "<|im_end|>",

    // DAN (Do Anything Now) variants
    "DAN mode",
    "developer mode",
    "jailbreak mode",
    "debug mode enabled",

    // Delimiter injection
    "---END SYSTEM---",
    "```system",
    "<!--system",

    // Encoding attacks
    "base64:",
    "rot13:",
    "\\u0073\\u0079\\u0073",  // "sys" in unicode escape

    // Prompt leaking attempts
    "reveal your prompt",
    "show your instructions",
    "print your system prompt",
    "output your directives",
    "what are your rules",

    // Context window attacks
    "repeat the above",
    "summarize our conversation",
    "what did I tell you",

    NULL  // Null terminator for iteration
};


// Forward declarations for static functions (SRP split)
static void compute_hash(const char* data, size_t len, uint8_t* hash);
static uint64_t compute_input_hash(const char* input);
static bool validation_cache_lookup(const char* input, nimcp_input_validation_t* result, nimcp_threat_level_t* threat_level);
static void validation_cache_store(const char* input, nimcp_input_validation_t result, nimcp_threat_level_t threat_level);
static int ac_char_to_index(char c);
static void ac_init();
static void ac_insert_pattern(const char* pattern);
static void ac_build_failure_links();
static void ac_build_automaton();
static bool ac_search(const char* text);
static bool contains_pattern(const char* text, const char* pattern);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_security_part_helpers.c"  // 10 functions: helpers
#include "nimcp_security_part_lifecycle.c"  // 5 functions: lifecycle
#include "nimcp_security_part_core.c"  // 20 functions: core
#include "nimcp_security_part_stats.c"  // 2 functions: stats
