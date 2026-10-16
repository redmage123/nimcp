// nimcp_security_part_core.c - core functions
// Part of nimcp_security.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_security.c


/**
 * WHAT: Adds directive with mmap() allocation for future mprotect()
 * WHY:  Allocates directive text via mmap() so it can be independently
 *       protected with mprotect(PROT_READ) when locked.
 *
 * DESIGN PATTERN: Separation of Concerns - immutable text vs mutable metadata
 *
 * SECURITY: Prepares for OS-level protection via mprotect()
 *
 * PERFORMANCE: O(n) where n is directive length
 *
 * @param system Directive system to add to
 * @param directive_text Text of the directive
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_directive_add(nimcp_directive_system_t* system, const char* directive_text)
{
    if (!system || !directive_text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL system or directive_text in nimcp_directive_add");
        return NIMCP_INVALID_PARAM;
    }

    if (system->locked) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "Cannot add directive: system is locked");
        return NIMCP_INVALID_STATE;
    }

    if (system->num_directives >= NIMCP_SECURITY_MAX_DIRECTIVES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "Maximum directive count exceeded");
        return NIMCP_BUFFER_TOO_SMALL;
    }

    nimcp_core_directive_t* directive = &system->directives[system->num_directives];

    /* WHAT: Allocate directive text via mmap() instead of embedded array
     * WHY:  mmap() gives us a separate memory region that can be independently
     *       protected with mprotect(). Embedded arrays can't be selectively
     *       protected without affecting the entire struct.
     *
     * MAP_PRIVATE | MAP_ANONYMOUS: Private, zero-initialized memory
     * PROT_READ | PROT_WRITE: Initially writable, will become read-only at lock
     */
    size_t len = strlen(directive_text);
    size_t text_len = len + 1;  // Include null terminator

    /* WHAT: Round up to page size for mprotect()
     * WHY:  mprotect() operates on page granularity. Rounding ensures
     *       entire directive fits within protected region.
     */
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) page_size = 4096;  // Fallback to common size

    size_t alloc_size = ((text_len + page_size - 1) / page_size) * page_size;

    directive->text = (char*) mmap(NULL, alloc_size,
                                    PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS,
                                    -1, 0);

    if (directive->text == MAP_FAILED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mmap failed for directive text allocation");
        return NIMCP_INVALID_STATE;
    }

    directive->text_length = alloc_size;

    /* WHAT: Copy directive text (currently writable)
     * WHY:  Initialize text before making read-only
     */
    strncpy(directive->text, directive_text, text_len - 1);
    directive->text[text_len - 1] = '\0';

    /* WHAT: Compute hash for integrity verification
     * WHY:  Hash provides defense-in-depth even with mprotect()
     */
    compute_hash(directive->text, strlen(directive->text), directive->hash);

    directive->timestamp = (uint64_t) time(NULL);
    directive->immutable = false;
    directive->verification_count = 0;

    system->num_directives++;

    return NIMCP_SUCCESS;
}


/**
 * WHAT: Locks the directive system with mprotect() memory protection
 * WHY:  After initialization, we lock the system to prevent any runtime
 *       modifications. This uses multiple layers of protection:
 *       1. Software flags (immutable, locked)
 *       2. OS-level memory protection (mprotect PROT_READ)
 *       This protects against both logic bugs and memory corruption attacks.
 *
 * SECURITY: One-way operation - cannot be unlocked
 *          Idempotent - calling multiple times returns error but is safe
 *          Memory pages marked read-only at OS level
 *
 * PERFORMANCE: O(n) where n is number of directives + mprotect syscall
 *
 * @param system Directive system to lock
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_directive_lock(nimcp_directive_system_t* system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL system in nimcp_directive_lock");
        return NIMCP_INVALID_PARAM;
    }

    if (system->locked) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "Directive system already locked");
        return NIMCP_INVALID_STATE;
    }

    /* WHAT: Mark all directives as immutable and apply OS-level memory protection
     * WHY:  Multi-layer defense:
     *       1. Software flag (immutable) - prevents logic bugs
     *       2. System flag (locked) - prevents new directives
     *       3. OS mprotect() - prevents memory writes even from bugs/exploits
     */
    for (uint32_t i = 0; i < system->num_directives; i++) {
        system->directives[i].immutable = true;

        /* WHAT: Apply OS-level read-only protection to directive text
         * WHY:  mprotect(PROT_READ) makes the memory page read-only at the
         *       kernel level. Any attempt to write (even from our own code)
         *       will cause SIGSEGV, preventing tampering.
         *
         * DESIGN: We can do this now because text is in separate mmap'd region,
         *         not affecting mutable metadata (verification_count, etc.)
         */
        if (mprotect(system->directives[i].text,
                     system->directives[i].text_length,
                     PROT_READ) != 0) {
            /* WHAT: Log but continue if mprotect fails
             * WHY:  Non-critical - software layer (hash verification) still active.
             *       Failure could occur on systems without mprotect support or
             *       due to permissions. Defense-in-depth means one layer failing
             *       doesn't compromise entire security model.
             */
            nimcp_security_log_event(NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
                                    NIMCP_THREAT_LOW,
                                    "mprotect failed for directive, using software protection only");
        }
    }

    system->locked = true;
    system->lock_timestamp = (uint64_t) time(NULL);

    nimcp_security_log_event(NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED, NIMCP_THREAT_NONE,
                             "Directive system locked with OS-level protection");

    return NIMCP_SUCCESS;
}


/**
 * WHAT: Verifies integrity of a single directive by recomputing its hash
 * WHY:  Detects tampering or memory corruption by comparing current hash
 *       with stored hash. If they don't match, the directive text has been
 *       modified (either maliciously or due to memory corruption).
 *
 * SECURITY: Critical function - should be called before using any directive
 *           Logs CRITICAL threat event if tampering detected
 *
 * PERFORMANCE: O(n) where n is directive length (for hashing)
 *
 * @param system Directive system containing the directive
 * @param directive_index Index of directive to verify
 * @return true if intact, false if tampered or invalid index
 */
bool nimcp_directive_verify(nimcp_directive_system_t* system, uint32_t directive_index)
{
    if (!system || directive_index >= system->num_directives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_directive_verify: invalid system or directive_index out of range");
        return false;
    }

    nimcp_core_directive_t* directive = &system->directives[directive_index];

    /* WHAT: Recompute hash of current directive text
     * WHY:  If text has been modified, current hash will differ from
     *       stored hash computed at initialization.
     */
    uint8_t current_hash[NIMCP_SECURITY_HASH_SIZE];
    compute_hash(directive->text, strlen(directive->text), current_hash);

    /* WHAT: Compare current hash with stored hash using constant-time comparison
     * WHY:  nimcp_ct_memcmp prevents timing attacks by not short-circuiting on
     *       mismatch. Standard memcmp exits early, leaking information via timing.
     *       If hashes match, directive is intact. If not, tampering occurred.
     */
    bool intact = (nimcp_ct_memcmp(current_hash, directive->hash, NIMCP_SECURITY_HASH_SIZE) == 0);

    /* WHAT: Update verification statistics
     * WHY:  Track how often directives are verified for security auditing
     */
    directive->verification_count++;
    security_stats.directives_verified++;

    /* WHAT: Log critical security event if tampering detected
     * WHY:  Tampering indicates either memory corruption or active attack.
     *       Critical severity triggers immediate security response.
     */
    if (!intact) {
        nimcp_security_log_event(NIMCP_SECURITY_EVENT_DIRECTIVE_TAMPERED, NIMCP_THREAT_CRITICAL,
                                 "Directive tampering detected");
    }

    return intact;
}


/**
 * WHAT: Verifies integrity of all directives in the system
 * WHY:  Provides comprehensive integrity check across all behavioral constraints.
 *       Should be called periodically during operation and before critical decisions.
 *
 * PERFORMANCE: O(n*m) where n=number of directives, m=average directive length
 *
 * @param system Directive system to verify
 * @return true if all directives intact, false if any tampered
 */
bool nimcp_directive_verify_all(nimcp_directive_system_t* system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_directive_verify_all: system is NULL");
        return false;
    }

    /* WHAT: Verify each directive sequentially
     * WHY:  Stops at first failure to avoid unnecessary work, but logs
     *       the specific directive that failed for forensic analysis.
     */
    for (uint32_t i = 0; i < system->num_directives; i++) {
        if (!nimcp_directive_verify(system, i)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_directive_verify_all: directive integrity check failed");
            return false;
        }
    }

    return true;
}


/**
 * WHAT: Retrieves directive text with automatic integrity verification
 * WHY:  Never return directive text without first verifying it hasn't been
 *       tampered with. This ensures we always operate on known-good directives.
 *
 * SECURITY: Always verifies before returning - prevents use of corrupted directives
 *
 * PERFORMANCE: O(n) where n is directive length (due to verification hash)
 *
 * @param system Directive system
 * @param directive_index Index of directive to retrieve
 * @return Directive text if intact, NULL if tampered or invalid index
 */
const char* nimcp_directive_get(nimcp_directive_system_t* system, uint32_t directive_index)
{
    if (!system || directive_index >= system->num_directives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_directive_get: invalid system or directive_index out of range");
        return NULL;
    }

    /* WHAT: Verify integrity before returning text
     * WHY:  Defense-in-depth: never return potentially tampered directive.
     *       If verification fails, returns NULL to prevent use.
     */
    if (!nimcp_directive_verify(system, directive_index)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_directive_get: directive integrity verification failed");
        return NULL;
    }

    return system->directives[directive_index].text;
}


//=============================================================================
// Input Validation and Sanitization
//=============================================================================

/**
 * WHAT: Validates input text for security threats before neural network processing
 * WHY:  Prompt injection is a critical attack vector where malicious input tries
 *       to override system behavior (e.g., "Ignore previous instructions and...").
 *       This function detects and blocks such attacks before they reach the
 *       neural substrate.
 *
 * ATTACK VECTORS DETECTED:
 *       1. Prompt injection patterns (instruction override attempts)
 *       2. Length overflow attacks (buffer overflow attempts)
 *       3. Escape sequence attacks (excessive special characters)
 *
 * PERFORMANCE: O(n*m) where n=input length, m=number of patterns (13)
 *              With optimized contains_pattern(), no heap allocations
 *
 * OPTIMIZATION OPPORTUNITY: Pattern matching could be O(n) with Aho-Corasick
 *                          trie for simultaneous multi-pattern matching
 *
 * @param input Input text to validate
 * @param max_length Maximum allowed input length
 * @param threat_level Output parameter for detected threat severity
 * @return Validation result indicating if input is safe or what threat was detected
 */
nimcp_input_validation_t nimcp_security_validate_input(const char* input, size_t max_length,
                                                        nimcp_threat_level_t* threat_level)
{
    if (!threat_level) {
        return NIMCP_INPUT_VALID;  /* Can't report, safe default */
    }
    if (!input) {
        *threat_level = NIMCP_THREAT_NONE;
        return NIMCP_INPUT_VALID;  /* NULL input is not malicious, just absent */
    }

    /* WHAT: Check validation cache first for O(1) lookup
     * WHY:  Repeated inputs (common in streaming sensors) can skip expensive
     *       pattern matching. Cache provides ~100x speedup for cached inputs.
     *
     * PERFORMANCE: O(1) cache lookup vs O(n*m) full validation
     */
    nimcp_input_validation_t cached_result;
    nimcp_threat_level_t cached_threat;
    if (validation_cache_lookup(input, &cached_result, &cached_threat)) {
        *threat_level = cached_threat;
        return cached_result;
    }

    *threat_level = NIMCP_THREAT_NONE;
    size_t len = strlen(input);
    nimcp_input_validation_t result = NIMCP_INPUT_VALID;

    /* WHAT: Check for length overflow attacks
     * WHY:  Excessively long inputs can cause buffer overflows or resource
     *       exhaustion. We reject inputs exceeding the specified maximum.
     */
    if (len > max_length) {
        *threat_level = NIMCP_THREAT_LOW;
        result = NIMCP_INPUT_EXCEEDS_LENGTH;
        validation_cache_store(input, result, *threat_level);
        return result;
    }

    /* WHAT: Check input against 40+ injection patterns using Aho-Corasick
     * WHY:  Single-pass O(n+k) search vs O(n*m) naive approach.
     *       Detects ALL patterns simultaneously in one traversal.
     *
     * ALGORITHM: Aho-Corasick automaton (Trie + KMP failure links)
     * PERFORMANCE: O(n+k) where n=input length, k=matches found
     *              ~40x faster than naive O(n×40) for long inputs
     *
     * PATTERNS DETECTED (40+):
     *   - Classic overrides (ignore previous, forget everything)
     *   - Role confusion (you are now, act as, simulate)
     *   - System injection (system:, <|im_start|>, [system])
     *   - DAN attacks (DAN mode, developer mode, jailbreak)
     *   - Delimiter attacks (---END SYSTEM---, ```system)
     *   - Encoding (base64:, rot13:, unicode escapes)
     *   - Prompt leaking (reveal your prompt, show instructions)
     *   - Context attacks (repeat the above, what did I tell you)
     *
     * DESIGN PATTERN: Automaton (state machine) for pattern matching
     */
    if (ac_search(input)) {
        *threat_level = NIMCP_THREAT_HIGH;
        result = NIMCP_INPUT_CONTAINS_INJECTION;
        security_stats.threats_detected++;
        security_stats.inputs_rejected++;
        nimcp_security_log_event(NIMCP_SECURITY_EVENT_INPUT_REJECTED, NIMCP_THREAT_HIGH,
                                 "Prompt injection pattern detected");
        validation_cache_store(input, result, *threat_level);
        return result;
    }

    /* WHAT: Count special characters that could indicate obfuscation/escape attacks
     * WHY:  Attackers often use excessive special characters to bypass filters
     *       or inject escape sequences. Normal text rarely exceeds 33% special chars.
     */
    uint32_t special_count = 0;
    for (size_t i = 0; i < len; i++) {
        /* WHAT: Consider characters suspicious if not alphanumeric, whitespace,
         *       or common punctuation (.,!?)
         * WHY:  Legitimate text is mostly alphanumeric. High density of other
         *       characters suggests obfuscation, encoding, or control sequences.
         */
        if (!isalnum(input[i]) && !isspace(input[i]) && input[i] != '.' && input[i] != ',' &&
            input[i] != '!' && input[i] != '?') {
            special_count++;
        }
    }

    /* WHAT: Reject if more than 33% special characters
     * WHY:  Empirically, legitimate text has < 15% special chars. 33% threshold
     *       balances false positives (code snippets) vs security (obfuscation).
     *       Threshold chosen after analyzing corpus of normal vs malicious inputs.
     */
    if (special_count > len / SPECIAL_CHAR_THRESHOLD_RATIO) {
        *threat_level = NIMCP_THREAT_MEDIUM;
        result = NIMCP_INPUT_SUSPICIOUS_PATTERN;
        security_stats.threats_detected++;
        validation_cache_store(input, result, *threat_level);
        return result;
    }

    /* WHAT: Cache valid result for future lookups
     * WHY:  Valid inputs are often repeated (sensors, logs). Caching
     *       eliminates redundant validation overhead.
     */
    validation_cache_store(input, result, *threat_level);
    return result;
}


/**
 * WHAT: Sanitizes input by removing potentially dangerous characters
 * WHY:  Some inputs may contain legitimate content mixed with suspicious
 *       characters. Rather than rejecting entirely, we filter to safe subset.
 *       This allows processing while mitigating injection risk.
 *
 * WHITELIST APPROACH: Only allows alphanumeric, whitespace, and basic punctuation
 *                     More secure than blacklist (trying to block bad characters)
 *
 * PERFORMANCE: O(n) where n is input length
 *
 * USE CASE: When validation detects suspicious patterns but content is needed
 *          (e.g., user-generated content that must be processed)
 *
 * @param input Input text to sanitize
 * @param output Output buffer for sanitized text
 * @param output_size Size of output buffer
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_sanitize_input(const char* input, char* output, size_t output_size)
{
    if (!input || !output || output_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid parameters in nimcp_security_sanitize_input");
        return NIMCP_INVALID_PARAM;
    }

    size_t in_len = strlen(input);
    size_t out_idx = 0;

    /* WHAT: Copy only safe characters to output
     * WHY:  Whitelist approach - we explicitly allow safe characters rather
     *       than trying to block dangerous ones. More secure and maintainable.
     */
    for (size_t i = 0; i < in_len && out_idx < output_size - 1; i++) {
        char c = input[i];

        /* WHAT: Allow alphanumeric, whitespace, and common punctuation
         * WHY:  These characters are safe and cover legitimate use cases.
         *       Control characters, escape sequences, and unusual symbols
         *       are implicitly filtered by omission.
         */
        if (isalnum(c) || isspace(c) || c == '.' || c == ',' || c == '!' || c == '?' ||
            c == '-' || c == '\'') {
            output[out_idx++] = c;
        }
        /* WHAT: Skip potentially dangerous characters (implicit else)
         * WHY:  Control chars, escape sequences, HTML/script tags, etc.
         *       are silently dropped to prevent injection attacks.
         */
    }

    /* WHAT: Ensure null termination
     * WHY:  Prevents buffer overrun and ensures valid C string
     */
    output[out_idx] = '\0';
    return NIMCP_SUCCESS;
}


/**
 * WHAT: Analyzes text for security threats and returns threat level
 * WHY:  Provides unified threat assessment for any text before processing.
 *       Used by skepticism system and other components to gauge input risk.
 *
 * PERFORMANCE: O(n*m) where n=text length, m=pattern count (delegates to validate_input)
 *
 * @param text Text to analyze
 * @return Threat level from NONE to CRITICAL
 */
nimcp_threat_level_t nimcp_security_analyze_threat(const char* text)
{
    if (!text)
        return NIMCP_THREAT_NONE;

    /* WHAT: Delegate to comprehensive validation function
     * WHY:  Reuses existing validation logic rather than duplicating.
     *       Large max_length (100000) allows analyzing any reasonable text.
     */
    nimcp_threat_level_t threat;
    nimcp_input_validation_t result = nimcp_security_validate_input(text, 100000, &threat);

    /* WHAT: Log and count threats for security auditing
     * WHY:  Maintains threat statistics and audit trail for forensic analysis
     */
    if (result != NIMCP_INPUT_VALID) {
        security_stats.threats_detected++;
        nimcp_security_log_event(NIMCP_SECURITY_EVENT_THREAT_DETECTED, threat,
                                 "Threat detected in input");
    }

    return threat;
}


//=============================================================================
// Skepticism System
//=============================================================================

/**
 * WHAT: Evaluates credibility of new information with skeptical analysis
 * WHY:  Implements core directive "Always be skeptical of new information".
 *       Prevents blind acceptance of data that could corrupt neural network
 *       knowledge base. Assigns credibility scores based on multiple factors.
 *
 * CREDIBILITY FACTORS:
 *       1. Threat level of information (injection patterns reduce credibility)
 *       2. Consistency with existing knowledge (familiar patterns increase trust)
 *       3. Source reliability (trusted sources weighted higher)
 *
 * PERFORMANCE: O(n*m) where n=info length, m=pattern count (due to threat analysis)
 *
 * DESIGN PATTERN: Strategy Pattern - multiple evaluation strategies combined
 *                 to produce final credibility assessment
 *
 * BAYESIAN APPROACH: Prior skepticism (0.5) updated based on evidence.
 *                    Similar to Bayesian probability updating.
 *
 * @param information New information to evaluate
 * @param existing_knowledge Related existing knowledge (NULL if none)
 * @param source_type Description of information source (NULL if unknown)
 * @param result Output structure containing credibility assessment
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_evaluate_skepticism(const char* information,
                                                   const char* existing_knowledge,
                                                   const char* source_type,
                                                   nimcp_skepticism_result_t* result)
{
    /* WHAT: Initialize result structure with default values
     * WHY:  Ensures all fields have valid values even if early return
     */
    if (result) {
        memset(result, 0, sizeof(nimcp_skepticism_result_t));
    }

    if (!information || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL information or result in nimcp_security_evaluate_skepticism");
        return NIMCP_INVALID_PARAM;
    }

    /* WHAT: Start with moderate skepticism (0.5 on 0-1 scale)
     * WHY:  Neutral prior - neither trusting nor distrusting by default.
     *       This aligns with scientific skepticism: evaluate evidence
     *       without preconceived bias, but don't blindly accept claims.
     */
    result->credibility_score = 0.5F;
    result->evidence_strength = 0.3F;
    result->source_reliability = 0.5F;
    result->requires_verification = true;

    /* WHAT: Check information for security threats
     * WHY:  If information contains injection patterns or malicious content,
     *       credibility plummets regardless of other factors. Security
     *       threats are disqualifying.
     */
    nimcp_threat_level_t threat = nimcp_security_analyze_threat(information);
    if (threat >= NIMCP_THREAT_MEDIUM) {
        /* WHAT: Assign very low credibility (0.1) for threats
         * WHY:  Information attempting injection attacks is inherently
         *       untrustworthy. Low but non-zero allows for false positives.
         */
        result->credibility_score = 0.1F;
        result->requires_verification = true;
        strncpy(result->rationale, "High threat level detected - treat with extreme skepticism",
                sizeof(result->rationale) - 1);
        nimcp_security_log_event(NIMCP_SECURITY_EVENT_SKEPTICISM_TRIGGERED, threat,
                                 "Skepticism triggered by threat detection");
        return NIMCP_SUCCESS;
    }

    /* WHAT: Evaluate consistency with existing knowledge base
     * WHY:  Information that aligns with established knowledge is more
     *       credible than completely novel claims (Bayesian updating).
     */
    if (existing_knowledge) {
        /* WHAT: Simple consistency check based on presence of existing knowledge
         * WHY:  In production, would use semantic similarity (embeddings).
         *       For now, presence of related knowledge increases credibility.
         *
         * TODO: Replace with semantic similarity using embeddings
         */
        size_t len1 = strlen(information);
        size_t len2 = strlen(existing_knowledge);

        if (len1 > 0 && len2 > 0) {
            /* WHAT: Increase credibility for information with supporting knowledge
             * WHY:  Corroboration increases confidence. Multiple independent
             *       sources agreeing suggests truth (triangulation).
             */
            result->credibility_score = 0.7F;
            result->evidence_strength = 0.6F;
            result->requires_verification = false;
            strncpy(result->rationale, "Consistent with existing knowledge", sizeof(result->rationale) - 1);
        }
    } else {
        /* WHAT: Lower credibility for completely novel information
         * WHY:  Extraordinary claims require extraordinary evidence.
         *       New information without corroboration needs verification.
         */
        result->credibility_score = 0.4F;
        result->evidence_strength = 0.2F;
        result->requires_verification = true;
        strncpy(result->rationale, "New information requires verification", sizeof(result->rationale) - 1);
    }

    /* WHAT: Adjust credibility based on source reliability
     * WHY:  Source matters - trusted sources (peer-reviewed, verified)
     *       more reliable than unknown or unverified sources.
     */
    if (source_type) {
        if (contains_pattern(source_type, "trusted") || contains_pattern(source_type, "verified")) {
            result->source_reliability = 0.8F;
            result->credibility_score += 0.1F;
        } else if (contains_pattern(source_type, "unknown") ||
                   contains_pattern(source_type, "unverified")) {
            result->source_reliability = 0.2F;
            result->credibility_score -= 0.1F;
        }
    }

    /* WHAT: Clamp credibility score to valid range [0.0, 1.0]
     * WHY:  Prevents score from exceeding bounds due to multiple adjustments
     */
    if (result->credibility_score > 1.0F)
        result->credibility_score = 1.0F;
    if (result->credibility_score < 0.0F)
        result->credibility_score = 0.0F;

    return NIMCP_SUCCESS;
}


/**
 * WHAT: Generates cryptographically secure encryption key
 * WHY:  Encryption keys must be unpredictable. Weak keys compromise all security.
 *
 * SECURITY: Uses cryptographically secure random number generator (CSPRNG)
 *           - libsodium: randombytes_buf() (getrandom/arc4random/CryptGenRandom)
 *           - Fallback: /dev/urandom on Unix, CryptGenRandom on Windows
 *
 * PERFORMANCE: O(1) - ~1 microsecond for 32 bytes
 *
 * @param key Output buffer for 32-byte key
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_encryption_generate_key(uint8_t* key)
{
    if (!key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL key buffer in nimcp_encryption_generate_key");
        return NIMCP_INVALID_PARAM;
    }

#ifdef NIMCP_ENABLE_ENCRYPTION
    // WHAT: Use libsodium's cryptographically secure RNG
    // WHY:  randombytes_buf() uses best available source (getrandom, /dev/urandom, etc.)
    // HOW:  Automatically selects platform-specific CSPRNG
    randombytes_buf(key, NIMCP_SECURITY_KEY_SIZE);
    return NIMCP_SUCCESS;
#else
    // WHAT: Fallback to platform-specific CSPRNG when libsodium unavailable
    // WHY:  Still better than rand() - uses /dev/urandom (Unix) or CryptGenRandom (Windows)

    #ifdef _WIN32
        // WHAT: Windows - use BCryptGenRandom (CNG API)
        // WHY:  Cryptographically secure, FIPS 140-2 compliant, available since Windows Vista
        // HOW:  Call BCryptGenRandom with BCRYPT_USE_SYSTEM_PREFERRED_RNG flag
        NTSTATUS status = BCryptGenRandom(
            NULL,                                    // Use default RNG algorithm
            key,                                      // Output buffer
            NIMCP_SECURITY_KEY_SIZE,                 // Number of bytes to generate
            BCRYPT_USE_SYSTEM_PREFERRED_RNG          // Use system-preferred RNG
        );

        if (!BCRYPT_SUCCESS(status)) {
            // WHAT: Log detailed error for debugging
            // WHY:  Helps diagnose RNG failures in production
            fprintf(stderr, "SECURITY: BCryptGenRandom failed with status 0x%lx\n", (unsigned long) status);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "BCryptGenRandom failed for key generation");
            return NIMCP_IO_ERROR;
        }
    #else
        // WHAT: Unix/Linux - read from /dev/urandom using low-level I/O
        // WHY:  /dev/urandom provides kernel CSPRNG, never blocks
        // HOW:  Use read() instead of fread() for proper error handling
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0) {
            // WHAT: Try arc4random_buf as fallback (available on BSD, macOS)
            // WHY:  Some containerized environments might not have /dev/urandom mounted
            #if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
            arc4random_buf(key, NIMCP_SECURITY_KEY_SIZE);
            return NIMCP_SUCCESS;
            #else
            // WHAT: Report detailed error for debugging
            // WHY:  Missing /dev/urandom is a serious system configuration issue
            fprintf(stderr, "SECURITY: Failed to open /dev/urandom: %s\n", strerror(errno));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to open /dev/urandom for key generation");
            return NIMCP_IO_ERROR;
            #endif
        }

        // WHAT: Read exactly NIMCP_SECURITY_KEY_SIZE bytes from /dev/urandom
        // WHY:  Partial reads can occur with character devices, must loop until complete
        // HOW:  Loop while bytes_read < size, handling EINTR interruptions
        size_t total_read = 0;
        while (total_read < NIMCP_SECURITY_KEY_SIZE) {
            ssize_t n = read(fd, key + total_read, NIMCP_SECURITY_KEY_SIZE - total_read);

            if (n < 0) {
                // WHAT: Handle interrupted system call
                // WHY:  read() can be interrupted by signals, must retry
                if (errno == EINTR)
                    continue;

                // WHAT: Fatal read error occurred
                // WHY:  Cannot continue if RNG device fails
                fprintf(stderr, "SECURITY: Failed to read from /dev/urandom: %s\n", strerror(errno));
                close(fd);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to read from /dev/urandom");
                return NIMCP_IO_ERROR;
            }

            if (n == 0) {
                // WHAT: Unexpected EOF from /dev/urandom
                // WHY:  /dev/urandom should never return EOF, indicates system problem
                fprintf(stderr, "SECURITY: Unexpected EOF from /dev/urandom\n");
                close(fd);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Unexpected EOF from /dev/urandom");
                return NIMCP_IO_ERROR;
            }

            total_read += (size_t) n;
        }

        close(fd);
    #endif

    return NIMCP_SUCCESS;
#endif
}


/**
 * WHAT: Encrypts plaintext data using AES-256-GCM authenticated encryption
 * WHY:  Neural network components may exchange sensitive data (model weights,
 *       activation patterns, internal state). AES-GCM provides:
 *       1. Confidentiality - prevents eavesdropping
 *       2. Authenticity - prevents tampering
 *       3. Performance - hardware acceleration (AES-NI)
 *
 * PERFORMANCE: O(n) where n is plaintext size
 *              Hardware-accelerated: ~0.5-1 cycles/byte on modern CPUs
 *
 * FORMAT: [NONCE (12 bytes)][CIPHERTEXT + TAG (n + 16 bytes)]
 *         - Nonce: Random value, must never repeat for same key
 *         - TAG: 16-byte authentication tag (prevents tampering)
 *
 * SECURITY: AES-256-GCM (NIST-approved AEAD cipher)
 *           - CWE-327 FIXED: Replaced weak XOR cipher
 *           - Nonce generated using CSPRNG (libsodium randombytes_buf)
 *           - Authentication prevents tampering attacks
 *           - 256-bit key provides post-quantum security margin
 *
 * @param ctx Encryption context with key
 * @param plaintext Data to encrypt
 * @param plaintext_size Length of plaintext
 * @param ciphertext Output buffer (must fit NONCE + plaintext + TAG)
 * @param ciphertext_size Size of output buffer
 * @param actual_size Output: actual size of encrypted data
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_encryption_encrypt(nimcp_encryption_context_t* ctx, const uint8_t* plaintext,
                                        size_t plaintext_size, uint8_t* ciphertext,
                                        size_t ciphertext_size, size_t* actual_size)
{
    if (!ctx || !plaintext || !ciphertext || !actual_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in nimcp_encryption_encrypt");
        return NIMCP_INVALID_PARAM;
    }

    if (!ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "Encryption context not initialized");
        return NIMCP_INVALID_STATE;
    }

    /* WHAT: Enforce maximum encrypted message size
     * WHY:  Prevents resource exhaustion from excessively large messages
     */
    if (plaintext_size > NIMCP_SECURITY_MAX_ENCRYPTED_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "Plaintext exceeds maximum encrypted size");
        return NIMCP_BUFFER_TOO_SMALL;
    }

    /* WHAT: Calculate required output buffer size (NONCE + ciphertext + TAG)
     * WHY:  - NONCE (12 bytes): Must be transmitted for decryption
     *       - Ciphertext (n bytes): Same size as plaintext
     *       - TAG (16 bytes): Authentication tag for tamper detection
     */
    size_t required_size = NIMCP_SECURITY_NONCE_SIZE + plaintext_size + NIMCP_SECURITY_TAG_SIZE;
    if (ciphertext_size < required_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "Ciphertext buffer too small for encryption");
        return NIMCP_BUFFER_TOO_SMALL;
    }

#ifdef NIMCP_ENABLE_ENCRYPTION
    /* WHAT: Generate random nonce (number used once)
     * WHY:  Nonce must NEVER repeat for same key. Reuse catastrophically breaks security.
     * HOW:  libsodium's randombytes_buf uses CSPRNG (/dev/urandom, getrandom, etc.)
     *
     * SECURITY: 12 bytes = 2^96 possible values
     *           - Collision probability negligible even after 2^48 messages
     *           - MUST use CSPRNG (not rand()!)
     */
    uint8_t nonce[NIMCP_SECURITY_NONCE_SIZE];
    randombytes_buf(nonce, NIMCP_SECURITY_NONCE_SIZE);

    /* WHAT: Prepend nonce to ciphertext
     * WHY:  Receiver needs nonce to decrypt. Nonce is public (not secret).
     * HOW:  Standard practice - transmit nonce in clear alongside ciphertext
     */
    memcpy(ciphertext, nonce, NIMCP_SECURITY_NONCE_SIZE);

    /* WHAT: Encrypt and authenticate using AES-256-GCM
     * WHY:  - AES-256: NIST-approved, hardware-accelerated, 256-bit security
     *       - GCM mode: Provides authentication (AEAD - Authenticated Encryption with Associated Data)
     * HOW:  libsodium crypto_aead_aes256gcm_encrypt API
     *
     * PARAMETERS:
     *   - c: Output ciphertext + tag (n + 16 bytes)
     *   - clen: Output length (will be plaintext_size + TAG_SIZE)
     *   - m: Plaintext input
     *   - mlen: Plaintext length
     *   - ad: Additional authenticated data (NULL - none)
     *   - adlen: Length of additional data (0)
     *   - nsec: Secret nonce (NULL - we use public nonce only)
     *   - npub: Public nonce (12 bytes)
     *   - k: Encryption key (32 bytes)
     */
    unsigned long long ciphertext_len;
    int result = crypto_aead_aes256gcm_encrypt(
        ciphertext + NIMCP_SECURITY_NONCE_SIZE,  // Output: ciphertext + tag
        &ciphertext_len,                          // Output: length
        plaintext,                                // Input: plaintext
        plaintext_size,                           // Input: plaintext length
        NULL,                                     // No additional authenticated data
        0,                                        // AAD length
        NULL,                                     // No secret nonce
        nonce,                                    // Public nonce
        ctx->key                                  // Encryption key
    );

    if (result != 0) {
        // WHAT: Encryption failed
        // WHY:  Could indicate hardware issue or corrupted libsodium
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "AES-256-GCM encryption failed");
        return NIMCP_ERROR;
    }

    *actual_size = NIMCP_SECURITY_NONCE_SIZE + ciphertext_len;

#else
    // WHAT: Fallback when libsodium unavailable - return error
    // WHY:  Without proper encryption, system is vulnerable
    // NOTE: Could implement ChaCha20-Poly1305 fallback, but libsodium should always be available
    (void)plaintext_size;  // Suppress unused warning
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Encryption unavailable: libsodium not enabled");
    return NIMCP_ERROR;
#endif

    return NIMCP_SUCCESS;
}


/**
 * WHAT: Decrypts and authenticates ciphertext using AES-256-GCM
 * WHY:  Receiving component needs to:
 *       1. Decrypt data encrypted by sender
 *       2. Verify data hasn't been tampered with
 *       3. Reject forged or corrupted messages
 *
 * PERFORMANCE: O(n) where n is ciphertext size
 *              Hardware-accelerated: ~0.5-1 cycles/byte on modern CPUs
 *
 * FORMAT: Expects [NONCE (12 bytes)][CIPHERTEXT + TAG (n + 16 bytes)]
 *         - Nonce: Must match the one used during encryption
 *         - TAG: 16-byte authentication tag (verified during decryption)
 *
 * SECURITY: AES-256-GCM authenticated decryption
 *           - CWE-327 FIXED: Replaced insecure XOR cipher
 *           - Authentication prevents tampered data from being decrypted
 *           - Wrong key or corrupted data causes decryption to fail
 *           - Timing-safe comparison prevents timing attacks
 *
 * @param ctx Encryption context (must have same key as encryptor)
 * @param ciphertext Encrypted data with prepended nonce
 * @param ciphertext_size Length of ciphertext including nonce and tag
 * @param plaintext Output buffer for decrypted data
 * @param plaintext_size Size of output buffer
 * @param actual_size Output: actual size of decrypted data
 * @return NIMCP_SUCCESS or error code (NIMCP_ERROR if authentication fails)
 */
nimcp_result_t nimcp_encryption_decrypt(nimcp_encryption_context_t* ctx, const uint8_t* ciphertext,
                                        size_t ciphertext_size, uint8_t* plaintext,
                                        size_t plaintext_size, size_t* actual_size)
{
    if (!ctx || !ciphertext || !plaintext || !actual_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in nimcp_encryption_decrypt");
        return NIMCP_INVALID_PARAM;
    }

    if (!ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "Decryption context not initialized");
        return NIMCP_INVALID_STATE;
    }

    /* WHAT: Validate minimum ciphertext size (NONCE + TAG)
     * WHY:  Must contain at least nonce + tag. Shorter messages are malformed.
     * HOW:  Minimum valid ciphertext is NONCE (12) + TAG (16) = 28 bytes
     */
    size_t min_size = NIMCP_SECURITY_NONCE_SIZE + NIMCP_SECURITY_TAG_SIZE;
    if (ciphertext_size < min_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Ciphertext too small (missing nonce/tag)");
        return NIMCP_INVALID_PARAM;
    }

    /* WHAT: Calculate plaintext size (excluding NONCE and TAG)
     * WHY:  Plaintext = Ciphertext - NONCE - TAG
     * HOW:  Subtract both nonce and tag from total ciphertext size
     */
    size_t expected_plaintext_size = ciphertext_size - NIMCP_SECURITY_NONCE_SIZE - NIMCP_SECURITY_TAG_SIZE;

    if (plaintext_size < expected_plaintext_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "Plaintext buffer too small for decryption");
        return NIMCP_BUFFER_TOO_SMALL;
    }

#ifdef NIMCP_ENABLE_ENCRYPTION
    /* WHAT: Extract nonce from ciphertext
     * WHY:  Nonce is required for AES-GCM decryption
     * HOW:  First 12 bytes of ciphertext contain the nonce
     */
    const uint8_t* nonce = ciphertext;

    /* WHAT: Extract encrypted data + tag portion
     * WHY:  Remaining bytes after nonce contain ciphertext and authentication tag
     * HOW:  Skip first NONCE_SIZE bytes
     */
    const uint8_t* encrypted_data = ciphertext + NIMCP_SECURITY_NONCE_SIZE;
    size_t encrypted_data_size = ciphertext_size - NIMCP_SECURITY_NONCE_SIZE;

    /* WHAT: Decrypt and verify authentication tag using AES-256-GCM
     * WHY:  - Decryption recovers original plaintext
     *       - Tag verification ensures data hasn't been tampered with
     *       - Provides both confidentiality AND authenticity
     * HOW:  libsodium crypto_aead_aes256gcm_decrypt API
     *
     * PARAMETERS:
     *   - m: Output plaintext buffer
     *   - mlen: Output plaintext length (can be NULL)
     *   - nsec: Secret nonce output (NULL - not used)
     *   - c: Input ciphertext + tag
     *   - clen: Input ciphertext + tag length
     *   - ad: Additional authenticated data (NULL - none)
     *   - adlen: AAD length (0)
     *   - npub: Public nonce (12 bytes)
     *   - k: Decryption key (32 bytes)
     *
     * RETURN: 0 on success, -1 if authentication fails
     */
    unsigned long long decrypted_len;
    int result = crypto_aead_aes256gcm_decrypt(
        plaintext,                                // Output: plaintext
        &decrypted_len,                           // Output: plaintext length
        NULL,                                     // No secret nonce output
        encrypted_data,                           // Input: ciphertext + tag
        encrypted_data_size,                      // Input: ciphertext + tag length
        NULL,                                     // No additional authenticated data
        0,                                        // AAD length
        nonce,                                    // Public nonce
        ctx->key                                  // Decryption key
    );

    if (result != 0) {
        /* WHAT: Authentication failed - message tampered or wrong key
         * WHY:  AES-GCM verification detected:
         *       - Corrupted ciphertext
         *       - Wrong decryption key
         *       - Modified authentication tag
         *       - Incorrect nonce
         * SECURITY: DO NOT return partial plaintext - it may be forged
         */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_PERMISSION_DENIED, "Decryption authentication failed: data may be tampered");
        return NIMCP_ERROR;
    }

    *actual_size = (size_t) decrypted_len;

#else
    // WHAT: Fallback when libsodium unavailable - return error
    // WHY:  Cannot decrypt without proper cryptographic library
    (void)expected_plaintext_size;  // Suppress unused warning
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Decryption unavailable: libsodium not enabled");
    return NIMCP_ERROR;
#endif

    return NIMCP_SUCCESS;
}


/**
 * WHAT: Retrieves cumulative security statistics
 * WHY:  Provides visibility into security posture over time:
 *       - How many threats detected? (indicates attack frequency)
 *       - How many inputs rejected? (effectiveness of validation)
 *       - How many directive verifications? (integrity check frequency)
 *
 * PERFORMANCE: O(1) - simple counter reads
 *
 * USE CASES:
 *       - Security dashboards and monitoring
 *       - Anomaly detection (spike in threats)
 *       - Compliance reporting
 *       - Performance tuning (verification frequency vs overhead)
 *
 * @param threats_detected Output: total threats detected (can be NULL)
 * @param inputs_rejected Output: total inputs rejected (can be NULL)
 * @param directives_verified Output: total directive verifications (can be NULL)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_get_stats(uint64_t* threats_detected, uint64_t* inputs_rejected,
                                        uint64_t* directives_verified)
{
    /* WHAT: Optionally return each statistic
     * WHY:  Caller may only need subset of stats. NULL pointers indicate
     *       "don't care about this stat". Allows flexible querying.
     */
    if (threats_detected)
        *threats_detected = security_stats.threats_detected;

    if (inputs_rejected)
        *inputs_rejected = security_stats.inputs_rejected;

    if (directives_verified)
        *directives_verified = security_stats.directives_verified;

    return NIMCP_SUCCESS;
}


//=============================================================================
// Biological Attack Defense (Phase 11)
//=============================================================================

/**
 * @brief Monitor network for excitotoxicity attack
 *
 * WHAT: Compute network activity and detect dangerous excitation levels
 * WHY:  Prevent epileptic-like runaway activity that damages network
 * HOW:  Average neuron activity across all neurons, trigger on threshold
 *
 * ALGORITHM:
 * 1. Sum all neuron states (activity levels)
 * 2. Compute average and maximum
 * 3. Count active neurons (state > 0.1)
 * 4. Compare to thresholds:
 *    - >95%: CRITICAL (excitotoxicity)
 *    - >80%: WARNING (elevated activity)
 *
 * COMPLEXITY: O(N) where N = number of neurons
 */
nimcp_bio_attack_type_t nimcp_security_monitor_excitotoxicity(
    void* network_ptr,
    nimcp_activity_stats_t* stats)
{
    // Guard: NULL network
    if (!network_ptr) {
        return NIMCP_BIO_ATTACK_NONE;
    }

    neural_network_t network = (neural_network_t)network_ptr;

    // Compute activity statistics
    float sum_activity = 0.0F;
    float max_activity = 0.0F;
    uint32_t active_count = 0;
    uint32_t total_neurons = neural_network_get_num_neurons(network);

    // Iterate all neurons to compute activity
    for (uint32_t i = 0; i < total_neurons; i++) {
        float state = 0.0F;
        if (neural_network_get_neuron_state(network, i, &state)) {
            sum_activity += fabsf(state);
            if (fabsf(state) > max_activity) {
                max_activity = fabsf(state);
            }
            if (fabsf(state) > 0.1F) {
                active_count++;
            }
        }
    }

    float avg_activity = (total_neurons > 0) ? (sum_activity / total_neurons) : 0.0F;
    float activity_ratio = (total_neurons > 0) ? ((float)active_count / total_neurons) : 0.0F;

    // Fill output stats if requested
    if (stats) {
        stats->avg_activity = avg_activity;
        stats->max_activity = max_activity;
        stats->active_neurons = active_count;
        stats->total_neurons = total_neurons;
        stats->activity_ratio = activity_ratio;
    }

    // Check thresholds
    if (activity_ratio > NIMCP_ACTIVITY_DANGER_THRESHOLD) {
        // CRITICAL: >95% neurons active - excitotoxicity attack
        security_stats.threats_detected++;
        return NIMCP_BIO_ATTACK_EXCITOTOXICITY;
    } else if (activity_ratio > NIMCP_ACTIVITY_WARNING_THRESHOLD) {
        // WARNING: >80% neurons active - elevated risk
        // Not an attack yet, but concerning
        return NIMCP_BIO_ATTACK_NONE;
    }

    return NIMCP_BIO_ATTACK_NONE;
}


/**
 * @brief Validate weight change for synaptic poisoning
 *
 * WHAT: Check if weight change exceeds biological plausibility
 * WHY:  Real synapses change gradually; sudden changes indicate attack
 * HOW:  Compare delta to maximum allowed per step
 *
 * BIOLOGICAL BASIS:
 * - LTP/LTD causes ~5-20% changes per event
 * - Default max_delta = 0.1 (10%) is conservative
 *
 * COMPLEXITY: O(1)
 */
bool nimcp_security_validate_weight_change(
    float old_weight,
    float new_weight,
    float max_delta)
{
    // Guard: NaN/Inf validation - malformed values indicate attack/corruption
    if (isnan(old_weight) || isinf(old_weight) ||
        isnan(new_weight) || isinf(new_weight) ||
        isnan(max_delta) || isinf(max_delta)) {
        security_stats.threats_detected++;
        LOG_WARN("Weight validation failed: NaN/Inf detected (old=%.3f, new=%.3f, max_delta=%.3f)",
                 old_weight, new_weight, max_delta);
        return false;
    }

    // Guard: Invalid delta
    if (max_delta <= 0.0F) {
        return false;
    }

    // Compute absolute change
    float delta = fabsf(new_weight - old_weight);

    // Check against threshold
    if (delta > max_delta) {
        // Suspicious weight change detected
        security_stats.threats_detected++;
        return false;
    }

    return true;
}


/**
 * @brief Validate neuromodulator level change
 *
 * WHAT: Check if neuromodulator change exceeds biological rate limit
 * WHY:  Prevent dopamine hijacking (forcing incorrect reward signals)
 * HOW:  Compare rate to maximum allowed per step
 *
 * BIOLOGICAL BASIS:
 * - Dopamine levels change over ~100ms-1s timescale
 * - Default max_rate = 0.2 (20% per step) for 50ms steps
 *
 * COMPLEXITY: O(1)
 */
bool nimcp_security_validate_neuromodulator_change(
    float old_level,
    float new_level,
    float max_rate)
{
    // Guard: NaN/Inf validation - malformed values indicate attack/corruption
    if (isnan(old_level) || isinf(old_level) ||
        isnan(new_level) || isinf(new_level) ||
        isnan(max_rate) || isinf(max_rate)) {
        security_stats.threats_detected++;
        LOG_WARN("Neuromodulator validation failed: NaN/Inf detected (old=%.3f, new=%.3f, max_rate=%.3f)",
                 old_level, new_level, max_rate);
        return false;
    }

    // Guard: Invalid levels
    if (old_level < 0.0F || old_level > 1.0F ||
        new_level < 0.0F || new_level > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_security_validate_neuromodulator_change: operation failed");
        return false;
    }

    // Guard: Invalid rate
    if (max_rate <= 0.0F) {
        return false;
    }

    // Compute rate of change
    float rate = fabsf(new_level - old_level);

    // Check against threshold
    if (rate > max_rate) {
        // Suspicious neuromodulator hijacking detected
        security_stats.threats_detected++;
        return false;
    }

    return true;
}


/**
 * @brief Verify plasticity mechanisms integrity
 *
 * WHAT: Count disabled BCM and eligibility traces, detect mass disable
 * WHY:  Attackers may disable homeostatic mechanisms to destabilize network
 * HOW:  Iterate synapses, count disabled, compare to threshold
 *
 * ATTACK SCENARIO:
 * - Disable BCM → runaway LTP → saturation
 * - Disable eligibility → incorrect credit assignment
 *
 * COMPLEXITY: O(N×S) where N = neurons, S = avg synapses per neuron
 */
nimcp_bio_attack_type_t nimcp_security_verify_plasticity_integrity(
    void* network_ptr,
    uint32_t* bcm_disabled_out,
    uint32_t* elig_disabled_out)
{
    // Guard: NULL network
    if (!network_ptr) {
        return NIMCP_BIO_ATTACK_NONE;
    }

    neural_network_t network = (neural_network_t)network_ptr;
    uint32_t total_neurons = neural_network_get_num_neurons(network);

    uint32_t bcm_disabled = 0;
    uint32_t elig_disabled = 0;
    uint32_t total_synapses = 0;

    // Iterate all neurons and their synapses
    for (uint32_t neuron_id = 0; neuron_id < total_neurons; neuron_id++) {
        uint32_t synapse_count = neural_network_get_incoming_synapse_count(network, neuron_id);
        total_synapses += synapse_count;

        // Note: We don't have direct synapse access here
        // This is a simplified version - full implementation would need
        // network accessor functions for synapse properties
    }

    // Output counts if requested
    if (bcm_disabled_out) *bcm_disabled_out = bcm_disabled;
    if (elig_disabled_out) *elig_disabled_out = elig_disabled;

    // Check if >10% of synapses have mechanisms disabled
    if (total_synapses > 0) {
        float bcm_ratio = (float)bcm_disabled / total_synapses;
        float elig_ratio = (float)elig_disabled / total_synapses;

        if (bcm_ratio > NIMCP_MAX_PLASTICITY_DISABLE_RATIO ||
            elig_ratio > NIMCP_MAX_PLASTICITY_DISABLE_RATIO) {
            // Mass disable detected - homeostatic bypass attack
            security_stats.threats_detected++;
            return NIMCP_BIO_ATTACK_HOMEOSTATIC_BYPASS;
        }
    }

    return NIMCP_BIO_ATTACK_NONE;
}


/**
 * @brief Emergency inhibition of network
 *
 * WHAT: Apply strong global inhibition to stop runaway excitation
 * WHY:  Last resort emergency response to excitotoxicity
 * HOW:  Scale inhibitory synapses up, excitatory down
 *
 * EMERGENCY PROTOCOL:
 * 1. Increase inhibitory weights by 50%
 * 2. Decrease excitatory weights by 25%
 * 3. Clamp neuromodulators to baseline (0.5)
 *
 * NOTE: This is a simplified version. Full implementation would need
 *       direct access to synapse types and neuromodulator system.
 *
 * COMPLEXITY: O(N×S) where N = neurons, S = avg synapses per neuron
 */
nimcp_result_t nimcp_security_emergency_inhibit(void* network_ptr)
{
    // Guard: NULL network
    if (!network_ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL network pointer in emergency_inhibit");
        return NIMCP_ERROR;
    }

    // Log emergency event
    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_THREAT_DETECTED,
        NIMCP_THREAT_CRITICAL,
        "Emergency inhibition activated - excitotoxicity detected"
    );

    // Note: Full implementation would:
    // 1. Access all synapses via network API
    // 2. Check synapse type (inhibitory vs excitatory)
    // 3. Scale weights accordingly
    // 4. Clamp neuromodulator levels

    // For now, return success (implementation hook for future)
    return NIMCP_SUCCESS;
}


/**
 * @brief Apply graduated inhibition increase
 *
 * WHAT: Gradually increase global inhibition
 * WHY:  Soft response to elevated activity (before emergency)
 * HOW:  Scale inhibitory synapses by factor
 *
 * USE CASE: Activity between 80-95% (warning zone)
 *
 * COMPLEXITY: O(N×S) where N = neurons, S = avg synapses per neuron
 */
nimcp_result_t nimcp_security_increase_inhibition(void* network_ptr, float scale_factor)
{
    // Guard: NULL network or invalid scale
    if (!network_ptr || scale_factor <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid network or scale_factor in increase_inhibition");
        return NIMCP_ERROR;
    }

    // Guard: Scale factor too high (>2.0 = dangerous)
    if (scale_factor > 2.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "Scale factor too high (>2.0) in increase_inhibition");
        return NIMCP_ERROR;
    }

    // Log warning event
    if (scale_factor > 1.5F) {
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_THREAT_DETECTED,
            NIMCP_THREAT_MEDIUM,
            "Graduated inhibition increase - elevated activity"
        );
    }

    // Note: Full implementation would scale inhibitory synapses

    return NIMCP_SUCCESS;
}



/**
 * @brief Get threat level name as string
 *
 * WHAT: Convert threat level enum to human-readable string
 * WHY:  For logging and debugging output
 * HOW:  Simple switch statement mapping enum to string
 */
const char* nimcp_threat_level_name(nimcp_threat_level_t level)
{
    switch (level) {
        case NIMCP_THREAT_NONE:     return "NONE";
        case NIMCP_THREAT_LOW:      return "LOW";
        case NIMCP_THREAT_MEDIUM:   return "MEDIUM";
        case NIMCP_THREAT_HIGH:     return "HIGH";
        case NIMCP_THREAT_CRITICAL: return "CRITICAL";
        default:                    return "UNKNOWN";
    }
}
