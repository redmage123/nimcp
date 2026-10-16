// nimcp_security_part_lifecycle.c - lifecycle functions
// Part of nimcp_security.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_security.c


/**
 * WHAT: Initializes Aho-Corasick trie with root node
 * WHY:  Prepares trie for pattern insertion. Root is node 0.
 *
 * PERFORMANCE: O(1)
 */
static void ac_init()
{
    memset(&ac_trie, 0, sizeof(ac_trie));

    /* WHAT: Initialize root node (index 0)
     * WHY:  All patterns start from root. Children initialized to -1 (no child).
     */
    for (int i = 0; i < AC_ALPHABET_SIZE; i++) {
        ac_trie.nodes[0].children[i] = -1;
    }
    ac_trie.nodes[0].failure = 0;  // Root's failure link points to itself
    ac_trie.node_count = 1;
}


//=============================================================================
// Core Directive Protection
//=============================================================================

/**
 * WHAT: Creates a new directive protection system
 * WHY:  Core directives define fundamental behavioral constraints that must
 *       never be altered by external input or prompt injection. This system
 *       provides cryptographic integrity protection to detect any tampering.
 *
 * DESIGN PATTERN: Factory Pattern - centralized creation of directive systems
 *
 * PERFORMANCE: O(1) allocation
 *
 * @return New directive system or NULL on allocation failure
 */
nimcp_directive_system_t* nimcp_directive_system_create(void)
{
    nimcp_directive_system_t* system = (nimcp_directive_system_t*) nimcp_calloc(1, sizeof(nimcp_directive_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate directive system");
        return NULL;
    }

    /* WHAT: Initialize to unlocked state with zero directives
     * WHY:  System starts in configuration mode where directives can be added.
     *       After initialization, nimcp_directive_lock() makes them immutable.
     */
    system->num_directives = 0;
    system->locked = false;
    system->lock_timestamp = 0;

    return system;
}


/**
 * WHAT: Destroys directive system and securely erases all data
 * WHY:  Prevents directive data from lingering in memory where it could
 *       be recovered or inspected by unauthorized code. Final verification
 *       ensures we log any tampering before destruction.
 *
 * SECURITY: Zeros memory before freeing to prevent data leakage
 *           Final integrity check provides last audit trail
 *
 * PERFORMANCE: O(n*m) where n=directives, m=average length (for final verify)
 *
 * @param system Directive system to destroy
 */
void nimcp_directive_system_destroy(nimcp_directive_system_t* system)
{
    if (!system)
        return;

    /* WHAT: Verify all directives one final time before destruction
     * WHY:  Provides final audit trail. If tampering occurred during
     *       system lifetime, we log it before destroying evidence.
     */
    nimcp_directive_verify_all(system);

    /* WHAT: Unmap all directive text regions
     * WHY:  These were allocated via mmap() in nimcp_directive_add(),
     *       so we must munmap() them to avoid memory leaks. munmap()
     *       automatically removes any mprotect() restrictions.
     *
     * DESIGN: We iterate through all directives and munmap their text.
     *         After unmapping, we zero the pointer for safety.
     */
    for (uint32_t i = 0; i < system->num_directives; i++) {
        if (system->directives[i].text) {
            /* WHAT: Remove mmap'd memory region
             * WHY:  Returns pages to OS. No need to mprotect(PROT_WRITE) first -
             *       munmap() works regardless of protection level.
             */
            munmap(system->directives[i].text, system->directives[i].text_length);
            system->directives[i].text = NULL;  // Safety: prevent double-free
        }
    }

    /* WHAT: Zero out remaining directive metadata before freeing
     * WHY:  Prevents hash values and metadata from remaining in freed memory.
     *       Note: directive text was already unmapped above.
     */
    memset(system, 0, sizeof(nimcp_directive_system_t));
    nimcp_free(system);
}


//=============================================================================
// Encryption for Inter-Component Communication
//=============================================================================

/**
 * WHAT: Creates encryption context for securing inter-component communication
 * WHY:  Neural network components exchange sensitive data. Context manages
 *       encryption key material for message encryption operations.
 *
 * DESIGN PATTERN: Builder Pattern - centralized context creation
 *
 * PERFORMANCE: O(1) - simple allocation
 *
 * SECURITY: Initializes libsodium on first use (thread-safe, idempotent)
 *
 * @param key 32-byte encryption key
 * @return Encryption context or NULL on failure
 */
nimcp_encryption_context_t* nimcp_encryption_create(const uint8_t* key)
{
    if (!key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL key in nimcp_encryption_create");
        return NULL;
    }

#ifdef NIMCP_ENABLE_ENCRYPTION
    // WHAT: Initialize libsodium cryptographic library
    // WHY:  Required before using any libsodium functions
    // HOW:  sodium_init() is thread-safe and idempotent (safe to call multiple times)
    if (sodium_init() < 0) {
        // WHAT: Initialization failed - libsodium unavailable
        // WHY:  Could indicate missing CPU features (AES-NI) or library corruption
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "libsodium initialization failed");
        return NULL;
    }

    // WHAT: Verify AES-256-GCM is available on this CPU
    // WHY:  AES-GCM requires hardware AES support (AES-NI on x86)
    // HOW:  crypto_aead_aes256gcm_is_available() checks CPU capabilities
    if (crypto_aead_aes256gcm_is_available() == 0) {
        // WHAT: AES-GCM not supported - CPU lacks AES-NI instructions
        // WHY:  Fallback could use ChaCha20-Poly1305, but we require AES-256-GCM
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "AES-256-GCM not available on this CPU");
        return NULL;
    }
#endif

    nimcp_encryption_context_t* ctx =
        (nimcp_encryption_context_t*) nimcp_calloc(1, sizeof(nimcp_encryption_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate encryption context");
        return NULL;
    }

    memcpy(ctx->key, key, NIMCP_SECURITY_KEY_SIZE);
    ctx->initialized = true;

    return ctx;
}


/**
 * WHAT: Securely destroys encryption context and erases key material
 * WHY:  Prevents encryption keys from lingering in memory after use.
 *       If keys remain in memory, they could be recovered by:
 *       - Memory dumps
 *       - Swap files
 *       - Cold boot attacks
 *       - Memory scanning malware
 *
 * SECURITY: Overwrites key with zeros before freeing
 *          Best practice: zero then free, never just free
 *
 * PERFORMANCE: O(1) - constant time key zeroing
 *
 * @param ctx Encryption context to destroy
 */
void nimcp_encryption_destroy(nimcp_encryption_context_t* ctx)
{
    if (!ctx)
        return;

    /* WHAT: Zero out all key bytes before freeing memory
     * WHY:  Freed memory may not be immediately reused. Zeroing ensures
     *       key cannot be recovered from heap. Defense-in-depth against
     *       memory disclosure vulnerabilities.
     */
    nimcp_secure_zero(ctx->key, NIMCP_SECURITY_KEY_SIZE);
    ctx->initialized = false;

    nimcp_free(ctx);
}
