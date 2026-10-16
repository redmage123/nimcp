// nimcp_security_part_helpers.c - helpers functions
// Part of nimcp_security.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_security.c


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Fast non-cryptographic hash function for directive integrity checking
 * WHY:  Uses FNV-1a algorithm which provides good distribution and collision
 *       resistance for our use case, while being ~10x faster than SHA-256.
 *       For embedded systems where cryptographic hash hardware may not be
 *       available, this provides a practical balance of security and performance.
 *
 * PERFORMANCE: O(n) where n is data length
 * SECURITY:    NOT cryptographically secure - use SHA-256 for production
 *
 * TODO(security): FNV-1a is a weak hash for integrity verification - it's fast
 *       but not collision-resistant against adversarial inputs. For security-
 *       critical integrity checking (like directive tampering detection), this
 *       should be replaced with SHA-256. The sha256_* functions in
 *       nimcp_key_derivation.c and nimcp_bbb_code_signing.c could be exposed
 *       as a public API for this purpose.
 *
 * @param data Input data to hash
 * @param len Length of input data
 * @param hash Output buffer (32 bytes) to store computed hash
 */
static void compute_hash(const char* data, size_t len, uint8_t* hash)
{
    if (!data || !hash)
        return;

    /* WHAT: FNV-1a (Fowler-Noll-Vo) hash algorithm
     * WHY:  Simple, fast, and provides good distribution for integrity checks.
     *       The algorithm XORs each byte with the hash then multiplies by a
     *       carefully chosen prime number to achieve avalanche properties.
     */
    uint64_t h = FNV_OFFSET_BASIS;

    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t) data[i];
        h *= FNV_PRIME;
    }

    /* WHAT: Expand 64-bit hash to 32 bytes by mixing with multipliers
     * WHY:  API requires 32-byte hash output. We create variations by
     *       multiplying with different constants to fill the buffer while
     *       maintaining hash quality.
     */
    for (int i = 0; i < 4; i++) {
        uint64_t mix = h * (i + 1);
        memcpy(hash + (i * 8), &mix, 8);
    }
}


/**
 * WHAT: Computes 64-bit FNV-1a hash for cache indexing
 * WHY:  Fast hash for cache lookup. 64 bits provides low collision rate
 *       while being fast to compute (single pass over input).
 *
 * PERFORMANCE: O(n) where n is input length
 *
 * @param input Input string to hash
 * @return 64-bit hash value
 */
static uint64_t compute_input_hash(const char* input)
{
    if (!input)
        return 0;

    uint64_t h = FNV_OFFSET_BASIS;
    for (const char* p = input; *p; p++) {
        /* WHAT: Case-insensitive hashing
         * WHY:  "Ignore Previous" and "ignore previous" should hash to same value
         */
        h ^= (uint8_t) tolower((unsigned char) *p);
        h *= FNV_PRIME;
    }
    return h;
}


/**
 * WHAT: Looks up input in validation cache
 * WHY:  Repeated inputs (common in streaming scenarios) can skip expensive
 *       pattern matching by using cached results. Provides O(1) lookup.
 *
 * PERFORMANCE: O(1) average case - single hash computation + array lookup
 *
 * @param input Input text to look up
 * @param result Output: cached validation result
 * @param threat_level Output: cached threat level
 * @return true if cache hit, false if cache miss
 */
static bool validation_cache_lookup(const char* input, nimcp_input_validation_t* result,
                                    nimcp_threat_level_t* threat_level)
{
    if (!input || !result || !threat_level) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validation_cache_lookup: required parameter is NULL (input, result, threat_level)");
        return false;
    }

    /* WHAT: Compute hash and find cache slot
     * WHY:  Hash & (SIZE-1) is fast modulo for power-of-2 sizes.
     *       Equivalent to hash % SIZE but uses bitwise AND.
     */
    uint64_t hash = compute_input_hash(input);
    uint32_t slot = hash & (VALIDATION_CACHE_SIZE - 1);

    /* WHAT: Check if cached entry matches this input
     * WHY:  Hash collision check - verify stored hash matches computed hash
     */
    if (validation_cache.entries[slot].input_hash == hash && hash != 0) {
        *result = validation_cache.entries[slot].result;
        *threat_level = validation_cache.entries[slot].threat_level;
        validation_cache.hits++;
        return true;
    }

    validation_cache.misses++;
    return false;  /* Cache miss is normal - input needs full validation */
}


/**
 * WHAT: Stores validation result in cache
 * WHY:  Caches result for future lookups, avoiding redundant pattern matching
 *
 * PERFORMANCE: O(1) - single array write
 *
 * @param input Input text that was validated
 * @param result Validation result to cache
 * @param threat_level Threat level to cache
 */
static void validation_cache_store(const char* input, nimcp_input_validation_t result,
                                   nimcp_threat_level_t threat_level)
{
    if (!input)
        return;

    uint64_t hash = compute_input_hash(input);
    uint32_t slot = hash & (VALIDATION_CACHE_SIZE - 1);

    /* WHAT: Store in cache slot
     * WHY:  Ring buffer style - new entries overwrite old ones in same slot.
     *       No need to track LRU explicitly - hash distribution provides
     *       natural spreading of popular entries.
     */
    validation_cache.entries[slot].input_hash = hash;
    validation_cache.entries[slot].result = result;
    validation_cache.entries[slot].threat_level = threat_level;
    validation_cache.entries[slot].timestamp = (uint64_t) time(NULL);
}


/**
 * WHAT: Maps character to trie alphabet index
 * WHY:  Normalizes input characters to consistent indices for trie navigation.
 *       Handles case-insensitivity and maps all relevant chars to 0-31 range.
 *
 * PERFORMANCE: O(1) - simple arithmetic
 *
 * @param c Character to map
 * @return Index in range [0, AC_ALPHABET_SIZE) or -1 if invalid
 */
static int ac_char_to_index(char c)
{
    c = tolower((unsigned char) c);

    /* WHAT: Map lowercase letters to indices 0-25
     * WHY:  Primary characters in attack patterns
     */
    if (c >= 'a' && c <= 'z') {
        return c - 'a';
    }

    /* WHAT: Map common pattern characters to remaining indices
     * WHY:  Patterns contain spaces, punctuation, special chars
     */
    switch (c) {
        case ' ': return 26;
        case '-': return 27;
        case '_': return 28;
        case ':': return 29;
        case '#': return 30;
        case '|': return 31;
        default: return 26;  // Map unknowns to space index
    }
}


/**
 * WHAT: Inserts pattern into Aho-Corasick trie
 * WHY:  Builds trie structure for multi-pattern matching.
 *       Each pattern creates a path from root to a terminal node.
 *
 * PERFORMANCE: O(m) where m is pattern length
 *
 * @param pattern Pattern string to insert
 */
static void ac_insert_pattern(const char* pattern)
{
    if (!pattern || ac_trie.node_count >= AC_MAX_NODES - 50)
        return;

    int current = 0;  // Start at root

    /* WHAT: Walk/create path for each character in pattern
     * WHY:  Builds trie by following existing edges or creating new nodes
     */
    for (const char* p = pattern; *p; p++) {
        int idx = ac_char_to_index(*p);
        if (idx < 0) continue;

        /* WHAT: Create new node if edge doesn't exist
         * WHY:  Extends trie to accommodate new pattern prefix
         */
        if (ac_trie.nodes[current].children[idx] == -1) {
            int new_node = ac_trie.node_count++;
            if (new_node >= AC_MAX_NODES) return;

            // Initialize new node
            for (int i = 0; i < AC_ALPHABET_SIZE; i++) {
                ac_trie.nodes[new_node].children[i] = -1;
            }
            ac_trie.nodes[new_node].failure = 0;
            ac_trie.nodes[new_node].is_pattern_end = false;

            ac_trie.nodes[current].children[idx] = new_node;
        }

        current = ac_trie.nodes[current].children[idx];
    }

    /* WHAT: Mark final node as pattern endpoint
     * WHY:  Indicates that reaching this node means we found a complete pattern
     */
    ac_trie.nodes[current].is_pattern_end = true;
}


/**
 * WHAT: Computes failure links for Aho-Corasick automaton (KMP-style)
 * WHY:  Failure links enable efficient backtracking during search without
 *       rescanning text. Similar to KMP failure function.
 *
 * ALGORITHM: BFS traversal, computing failure links level by level
 *
 * PERFORMANCE: O(nodes × alphabet_size) = O(sum of pattern lengths)
 */
static void ac_build_failure_links()
{
    /* WHAT: BFS queue for level-order traversal
     * WHY:  Must process parents before children to compute failure links
     */
    int queue[AC_QUEUE_SIZE];
    int queue_front = 0, queue_back = 0;

    /* WHAT: Initialize root's children failure links to root
     * WHY:  Base case - when at depth 1, failure returns to root
     */
    for (int i = 0; i < AC_ALPHABET_SIZE; i++) {
        int child = ac_trie.nodes[0].children[i];
        if (child != -1) {
            ac_trie.nodes[child].failure = 0;
            queue[queue_back++] = child;
        }
    }

    /* WHAT: BFS to compute failure links
     * WHY:  Each node's failure link points to longest proper suffix
     *       that is also a prefix of some pattern
     */
    while (queue_front < queue_back) {
        int current = queue[queue_front++];

        for (int i = 0; i < AC_ALPHABET_SIZE; i++) {
            int child = ac_trie.nodes[current].children[i];
            if (child == -1) continue;

            /* WHAT: Compute failure link for child
             * WHY:  Follow parent's failure chain until finding node with
             *       matching edge, or reaching root
             */
            int failure = ac_trie.nodes[current].failure;
            while (failure != 0 && ac_trie.nodes[failure].children[i] == -1) {
                failure = ac_trie.nodes[failure].failure;
            }

            if (ac_trie.nodes[failure].children[i] != -1 &&
                ac_trie.nodes[failure].children[i] != child) {
                failure = ac_trie.nodes[failure].children[i];
            }

            ac_trie.nodes[child].failure = failure;

            /* WHAT: Inherit pattern matches from failure link
             * WHY:  If failure link is pattern end, this node represents
             *       multiple overlapping patterns
             */
            if (ac_trie.nodes[failure].is_pattern_end) {
                ac_trie.nodes[child].is_pattern_end = true;
            }

            queue[queue_back++] = child;
        }
    }
}


/**
 * WHAT: Builds complete Aho-Corasick automaton from pattern list
 * WHY:  One-time construction enables O(n+k) searches.
 *       Called once at startup or first validation.
 *
 * PERFORMANCE: O(sum of all pattern lengths) one-time cost
 */
static void ac_build_automaton()
{
    if (ac_trie.initialized)
        return;

    ac_init();

    /* WHAT: Insert all injection patterns into trie
     * WHY:  Builds the basic trie structure before adding failure links
     */
    for (int i = 0; injection_patterns[i]; i++) {
        ac_insert_pattern(injection_patterns[i]);
    }

    /* WHAT: Compute failure links for efficient matching
     * WHY:  Transforms trie into full Aho-Corasick automaton
     */
    ac_build_failure_links();

    ac_trie.initialized = true;
}


/**
 * WHAT: Searches text for ANY injection pattern using Aho-Corasick
 * WHY:  Single pass through text detects all 40+ patterns simultaneously.
 *       O(n+k) vs O(n*m) naive approach - massive speedup for long inputs.
 *
 * PERFORMANCE: O(n+k) where n=text length, k=number of matches found
 *              40x faster than naive O(n*m) approach
 *
 * DESIGN PATTERN: Automaton pattern - state machine with failure transitions
 *
 * @param text Input text to search
 * @return true if ANY pattern found, false otherwise
 */
static bool ac_search(const char* text)
{
    if (!text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ac_search: text is NULL");
        return false;
    }

    /* WHAT: Build automaton on first use (lazy initialization)
     * WHY:  Defers construction cost until first validation.
     *       Subsequent calls reuse pre-built automaton.
     */
    if (!ac_trie.initialized) {
        ac_build_automaton();
    }

    int current = 0;  // Start at root

    /* WHAT: Single pass through input text
     * WHY:  Automaton transitions handle all patterns simultaneously.
     *       No need to restart search for each pattern.
     */
    for (const char* p = text; *p; p++) {
        int idx = ac_char_to_index(*p);
        if (idx < 0) continue;

        /* WHAT: Follow failure links until finding valid transition
         * WHY:  When no edge exists, failure link provides best alternative
         *       state without rescanning text (like KMP)
         */
        while (current != 0 && ac_trie.nodes[current].children[idx] == -1) {
            current = ac_trie.nodes[current].failure;
        }

        /* WHAT: Take transition if it exists
         * WHY:  Advance automaton state along pattern match
         */
        if (ac_trie.nodes[current].children[idx] != -1) {
            current = ac_trie.nodes[current].children[idx];
        }

        /* WHAT: Check if current state represents pattern match
         * WHY:  Terminal states indicate complete pattern found in text
         */
        if (ac_trie.nodes[current].is_pattern_end) {
            return true;  // Found injection pattern
        }
    }

    return false;  /* No injection patterns found - input is clean */
}


/**
 * WHAT: Simple case-insensitive substring search
 * WHY:  Used for non-critical path searches (e.g., skepticism source checking).
 *       For critical validation path, use ac_search() instead.
 *
 * PERFORMANCE: O(n*m) - acceptable for infrequent use
 *
 * @param text Haystack
 * @param pattern Needle
 * @return true if pattern found
 */
static bool contains_pattern(const char* text, const char* pattern)
{
    if (!text || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contains_pattern: required parameter is NULL (text, pattern)");
        return false;
    }

    const char* text_pos = text;
    while (*text_pos) {
        const char* t = text_pos;
        const char* p = pattern;

        while (*p && *t && (tolower((unsigned char)*t) == tolower((unsigned char)*p))) {
            t++;
            p++;
        }

        if (*p == '\0')
            return true;

        text_pos++;
    }

    return false;  /* Pattern not found in text */
}
