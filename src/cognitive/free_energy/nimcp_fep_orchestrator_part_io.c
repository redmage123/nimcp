// nimcp_fep_orchestrator_part_io.c - io functions
// Part of nimcp_fep_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_fep_orchestrator.c


/* ============================================================================
 * String Conversion API Implementation
 * ============================================================================ */

const char* fep_bridge_category_to_string(fep_bridge_category_t category) {
    if (category >= FEP_BRIDGE_CATEGORY_COUNT) return "unknown";
    return CATEGORY_NAMES[category];
}


const char* fep_orchestrator_state_to_string(fep_orchestrator_state_t state) {
    if (state > FEP_ORCHESTRATOR_ERROR) return "unknown";
    return STATE_NAMES[state];
}
