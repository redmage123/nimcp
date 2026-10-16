// nimcp_security_part_stats.c - stats functions
// Part of nimcp_security.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_security.c


/**
 * WHAT: Returns number of directives in the system
 * WHY:  Enables iteration over directives and validation that expected
 *       directives are present.
 *
 * PERFORMANCE: O(1)
 *
 * @param system Directive system
 * @return Number of directives, or 0 if system is NULL
 */
uint32_t nimcp_directive_count(nimcp_directive_system_t* system)
{
    return system ? system->num_directives : 0;
}


//=============================================================================
// Security Audit and Logging
//=============================================================================

/**
 * WHAT: Logs security events for auditing and forensic analysis
 * WHY:  Security monitoring requires comprehensive audit trail. Logging enables:
 *       - Real-time threat detection and alerting
 *       - Forensic analysis after incidents
 *       - Compliance with security standards (audit requirements)
 *       - Pattern analysis to identify attack trends
 *
 * PERFORMANCE: O(1) - simple fprintf operation
 *
 * OUTPUT: Logs to stderr for immediate visibility
 *         Format: [SECURITY] [SEVERITY] [EVENT_TYPE] Details
 *
 * DESIGN PATTERN: Observer Pattern - centralized event notification
 *                 Could extend to multiple log destinations (file, syslog, network)
 *
 * @param event Type of security event
 * @param severity Threat severity level
 * @param details Human-readable event description
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_log_event(nimcp_security_event_t event,
                                        nimcp_threat_level_t severity, const char* details)
{
    /* WHAT: Human-readable names for security event types
     * WHY:  Enum values are numbers - need strings for logging.
     *       Array indexed by enum provides O(1) lookup.
     */
    const char* event_names[] = {"DIRECTIVE_VERIFIED", "DIRECTIVE_TAMPERED", "INPUT_REJECTED",
                                 "THREAT_DETECTED",    "ENCRYPTION_FAILED",  "SKEPTICISM_TRIGGERED"};

    /* WHAT: Human-readable names for threat severity levels
     * WHY:  Enables quick visual scanning of logs to identify critical events
     */
    const char* severity_names[] = {"NONE", "LOW", "MEDIUM", "HIGH", "CRITICAL"};

    /* WHAT: Bounds checking with named constants instead of magic numbers
     * WHY:  Prevents out-of-bounds array access if enum values are invalid.
     *       Using named constants (NUM_SECURITY_EVENT_TYPES, NUM_THREAT_LEVELS)
     *       makes code maintainable - adding new types updates constant.
     */
    const char* event_name = (event < NUM_SECURITY_EVENT_TYPES) ? event_names[event] : "UNKNOWN";
    const char* severity_name = (severity < NUM_THREAT_LEVELS) ? severity_names[severity] : "UNKNOWN";

    /* WHAT: Log to stderr with structured format
     * WHY:  stderr ensures immediate output (unbuffered by default).
     *       Structured format enables parsing by log analysis tools.
     *
     * FORMAT: [SECURITY] - identifies security subsystem
     *         [SEVERITY] - enables filtering by threat level
     *         [EVENT] - specific event type
     *         Details - context-specific information
     *
     * TODO: Add timestamp, process ID, and thread ID for production
     * TODO: Support configurable log destinations (file, syslog, network)
     */
    fprintf(stderr, "[SECURITY] [%s] [%s] %s\n", severity_name, event_name,
            details ? details : "");

    return NIMCP_SUCCESS;
}
