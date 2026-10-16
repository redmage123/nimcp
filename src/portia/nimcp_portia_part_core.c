// nimcp_portia_part_core.c - core functions
// Part of nimcp_portia.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_portia.c


//=============================================================================
// Utility Functions
//=============================================================================

const char* portia_power_state_name(portia_power_state_t state) {
    switch (state) {
        case PORTIA_POWER_AC: return "AC";
        case PORTIA_POWER_BATTERY_FULL: return "BATTERY_FULL";
        case PORTIA_POWER_BATTERY_MID: return "BATTERY_MID";
        case PORTIA_POWER_BATTERY_LOW: return "BATTERY_LOW";
        case PORTIA_POWER_BATTERY_CRITICAL: return "BATTERY_CRITICAL";
        case PORTIA_POWER_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}


const char* portia_thermal_state_name(portia_thermal_state_t state) {
    switch (state) {
        case PORTIA_THERMAL_NOMINAL: return "NOMINAL";
        case PORTIA_THERMAL_WARM: return "WARM";
        case PORTIA_THERMAL_HOT: return "HOT";
        case PORTIA_THERMAL_THROTTLED: return "THROTTLED";
        case PORTIA_THERMAL_CRITICAL: return "CRITICAL";
        default: return "INVALID";
    }
}


const char* portia_accel_type_name(portia_accelerator_type_t type) {
    switch (type) {
        case PORTIA_ACCEL_NONE: return "NONE";
        case PORTIA_ACCEL_GPU: return "GPU";
        case PORTIA_ACCEL_NPU: return "NPU";
        case PORTIA_ACCEL_TPU: return "TPU";
        case PORTIA_ACCEL_DSP: return "DSP";
        case PORTIA_ACCEL_FPGA: return "FPGA";
        case PORTIA_ACCEL_ASIC: return "ASIC";
        default: return "INVALID";
    }
}


const char* portia_workload_type_name(portia_workload_type_t type) {
    switch (type) {
        case PORTIA_WORKLOAD_TRAINING: return "TRAINING";
        case PORTIA_WORKLOAD_INFERENCE: return "INFERENCE";
        case PORTIA_WORKLOAD_MONITORING: return "MONITORING";
        case PORTIA_WORKLOAD_IDLE: return "IDLE";
        case PORTIA_WORKLOAD_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}


const char* portia_degradation_level_name(portia_degradation_level_t level) {
    switch (level) {
        case PORTIA_DEGRADATION_NONE: return "NONE";
        case PORTIA_DEGRADATION_MINOR: return "MINOR";
        case PORTIA_DEGRADATION_MODERATE: return "MODERATE";
        case PORTIA_DEGRADATION_SEVERE: return "SEVERE";
        case PORTIA_DEGRADATION_EMERGENCY: return "EMERGENCY";
        default: return "INVALID";
    }
}
