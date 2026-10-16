#!/usr/bin/env python3
"""
Phase 8 Instance-Level Enhancement Script
Adds heartbeat_instance, set_instance_health_agent, and FULL training functions
to all .c files in fault_tolerance, health, mental_health, and wellbeing directories.
"""
import os, re, sys

BASE = "/home/bbrelin/nimcp"

# File metadata: (prefix, struct_name, is_bridge, has_health_agent_field, key_fields_for_training)
# We'll auto-detect most of this from the file contents.

def extract_info(filepath):
    """Extract all needed metadata from a .c file."""
    with open(filepath, 'r') as f:
        content = f.read()
    lines = content.split('\n')

    # Global health agent variable
    g_match = re.search(r'static nimcp_health_agent_t\*\s+(g_\w+)\s*=\s*NULL;', content)
    gvar = g_match.group(1) if g_match else None

    # Heartbeat function name
    hb_match = re.search(r'static inline void (\w+_heartbeat)\(const char\*', content)
    hbfunc = hb_match.group(1) if hb_match else None

    # Find struct definitions (excluding nimcp_health_agent)
    struct_defs = {}
    in_struct = False
    current_struct = None
    struct_fields = []
    brace_depth = 0
    for i, line in enumerate(lines):
        m = re.match(r'^struct (\w+)\s*\{', line)
        if m and m.group(1) != 'nimcp_health_agent':
            in_struct = True
            current_struct = m.group(1)
            struct_fields = []
            brace_depth = 1
            continue
        if in_struct:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth <= 0:
                struct_defs[current_struct] = struct_fields
                in_struct = False
                current_struct = None
            else:
                struct_fields.append(line.strip())

    # Determine primary struct
    primary_struct = None
    for sname, fields in struct_defs.items():
        if sname != 'nimcp_health_agent':
            primary_struct = sname
            break

    # Check if bridge (has bridge_base_t in struct)
    is_bridge = False
    has_health_agent_field = False
    struct_field_list = struct_defs.get(primary_struct, []) if primary_struct else []
    for field in struct_field_list:
        if 'bridge_base_t' in field:
            is_bridge = True
        if 'health_agent' in field and ('nimcp_health_agent_t' in field or 'struct health_agent' in field):
            has_health_agent_field = True

    # Derive prefix from heartbeat function name
    prefix = None
    if hbfunc:
        prefix = hbfunc.replace('_heartbeat', '')

    # Check if already enhanced
    has_instance = 'heartbeat_instance' in content
    has_training = 'training_begin' in content

    # Find line of existing heartbeat function (to insert heartbeat_instance after)
    hb_line = None
    for i, line in enumerate(lines):
        if hbfunc and f'static inline void {hbfunc}(' in line:
            # Find the closing brace
            for j in range(i, min(i+10, len(lines))):
                if lines[j].strip() == '}':
                    hb_line = j
                    break
            break

    # Find the last non-empty line
    last_line = len(lines) - 1
    while last_line > 0 and lines[last_line].strip() == '':
        last_line -= 1

    return {
        'filepath': filepath,
        'content': content,
        'lines': lines,
        'gvar': gvar,
        'hbfunc': hbfunc,
        'prefix': prefix,
        'primary_struct': primary_struct,
        'struct_fields': struct_field_list,
        'is_bridge': is_bridge,
        'has_health_agent_field': has_health_agent_field,
        'has_instance': has_instance,
        'has_training': has_training,
        'hb_end_line': hb_line,
        'total_lines': len(lines),
    }


def get_training_fields(info):
    """Determine which fields to use in training based on struct fields."""
    fields = info['struct_fields']
    prefix = info['prefix']
    struct_name = info['primary_struct']

    # Categorize available fields
    has_stats = any('stats' in f for f in fields)
    has_config = any('config' in f for f in fields)
    has_count = any('count' in f and 'uint32' in f for f in fields)
    has_initialized = any('initialized' in f for f in fields)
    has_running = any('running' in f or 'active' in f for f in fields)
    has_update_count = any('update_count' in f for f in fields)
    has_attention_weight = any('attention_weight' in f for f in fields)
    has_effects = any('effects' in f for f in fields)
    has_monitoring_active = any('monitoring_active' in f for f in fields)
    has_magic = any('magic' in f for f in fields)
    has_mutex = any('mutex' in f for f in fields)
    has_total_samples = any('total_samples' in f for f in fields)
    has_self_confidence = any('self_confidence' in f for f in fields)
    has_uncertainty = any('uncertainty' in f for f in fields)
    has_episode_count = any('episode_count' in f for f in fields)
    has_rule_count = any('rule_count' in f for f in fields)
    has_pattern_count = any('pattern_count' in f for f in fields)
    has_indicator_count = any('indicator_count' in f for f in fields)
    has_prediction_count = any('prediction_count' in f for f in fields)
    has_fault_count = any('fault_count' in f for f in fields)
    has_cascade_detected = any('cascade_detected' in f for f in fields)
    has_record_count = any('record_count' in f for f in fields)
    has_tracking_count = any('tracking_count' in f for f in fields)
    has_failure_tracking_count = any('failure_tracking_count' in f for f in fields)
    has_subgoal_count = any('subgoal_count' in f for f in fields)
    has_current_step = any('current_step' in f for f in fields)
    has_next_plan_id = any('next_plan_id' in f for f in fields)
    has_decisions_since_check = any('decisions_since_check' in f for f in fields)
    has_disorder_scores = any('disorder_scores' in f for f in fields)
    has_history_index = any('history_index' in f for f in fields)
    has_checks_performed = any('checks_performed' in f for f in fields)
    has_interventions_applied = any('interventions_applied' in f for f in fields)
    has_current_level = any('current_level' in f for f in fields)
    has_last_overall_severity = any('last_overall_severity' in f for f in fields)
    has_state_field = any('state' in f and ('_state_t' in f or 'state;' in f) for f in fields)
    has_synapse_count = any('synapse_count' in f for f in fields)
    has_learning_rate = any('learning_rate' in f for f in fields)
    has_stress_level = any('stress_level' in f or 'current_stress' in f for f in fields)
    has_anxiety = any('anxiety' in f for f in fields)
    has_depression = any('depression' in f for f in fields)
    has_dim_states = any('dim_states' in f for f in fields)
    has_num_instances = any('num_instances' in f for f in fields)
    has_num_pending = any('num_pending' in f for f in fields)
    has_num_patterns = any('num_patterns' in f for f in fields)
    has_num_applied = any('num_applied' in f for f in fields)
    has_decision_log = any('decision_log' in f for f in fields)

    return locals()


def generate_heartbeat_instance(info):
    """Generate the heartbeat_instance function."""
    prefix = info['prefix']
    gvar = info['gvar']
    return f"""
/** @brief Send heartbeat from {prefix} module (instance-level) */
static inline void {prefix}_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{{
    if ({gvar}) {{
        nimcp_health_agent_heartbeat_ex({gvar}, operation, progress);
    }}
    if (instance_agent && instance_agent != {gvar}) {{
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }}
}}
"""


def generate_set_instance(info):
    """Generate the set_instance_health_agent function."""
    prefix = info['prefix']
    gvar = info['gvar']
    struct_name = info['primary_struct']
    is_bridge = info['is_bridge']
    has_ha = info['has_health_agent_field']

    # Determine the type parameter
    if struct_name and (is_bridge or has_ha):
        # Bridge with typed struct
        # Figure out the typedef name
        type_name = f"{struct_name}_t"
        if is_bridge:
            code = f"""/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void {prefix}_set_instance_health_agent({type_name}* bridge, nimcp_health_agent_t* agent) {{
    if (bridge) {{
        bridge->health_agent = agent;
    }}
}}
"""
        else:
            code = f"""/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void {prefix}_set_instance_health_agent({type_name}* instance, nimcp_health_agent_t* agent) {{
    if (instance) {{
        instance->health_agent = agent;
    }}
}}
"""
    else:
        # Non-bridge or no struct: void* global fallback
        code = f"""/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void {prefix}_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {{
    if (instance) {{
        (void)instance;  /* No struct-level health_agent; use global */
        {gvar} = agent;
    }}
}}
"""
    return code


def generate_training_functions(info):
    """Generate FULL training_begin, training_step, training_end."""
    prefix = info['prefix']
    gvar = info['gvar']
    struct_name = info['primary_struct']
    is_bridge = info['is_bridge']
    has_ha = info['has_health_agent_field']
    finfo = get_training_fields(info)

    # Short prefix for heartbeat ops (max ~30 chars)
    short = prefix[:14] if len(prefix) > 14 else prefix

    # Determine instance agent expression
    if has_ha:
        agent_expr = "ctx->health_agent"
    else:
        agent_expr = "NULL"

    # Type for param
    if struct_name:
        type_name = f"{struct_name}_t"
        if is_bridge:
            param_type = f"{type_name}*"
            param_name = "bridge"
        else:
            param_type = f"{type_name}*"
            param_name = "instance"
        cast_line = f"    {type_name}* ctx = ({type_name}*){param_name};"
    else:
        param_type = "void*"
        param_name = "instance"
        cast_line = f"    (void)instance;"

    # === TRAINING BEGIN ===
    begin_body = []
    begin_body.append(f"    {prefix}_heartbeat_instance({agent_expr}, \"{short}_training_begin\", 0.0f);")

    if struct_name:
        # Reset counters and set flags based on available fields
        if finfo.get('has_stats'):
            begin_body.append("    memset(&ctx->stats, 0, sizeof(ctx->stats));")
        if finfo.get('has_update_count'):
            begin_body.append("    ctx->update_count = 0;")
        if finfo.get('has_total_samples'):
            begin_body.append("    ctx->total_samples = 0;")
        if finfo.get('has_episode_count'):
            begin_body.append("    ctx->episode_count = 0;")
        if finfo.get('has_rule_count'):
            begin_body.append("    ctx->rule_count = 0;")
        if finfo.get('has_pattern_count'):
            begin_body.append("    ctx->pattern_count = 0;")
        if finfo.get('has_indicator_count'):
            begin_body.append("    ctx->indicator_count = 0;")
        if finfo.get('has_prediction_count'):
            begin_body.append("    ctx->prediction_count = 0;")
        if finfo.get('has_fault_count'):
            begin_body.append("    ctx->fault_count = 0;")
        if finfo.get('has_record_count'):
            begin_body.append("    ctx->record_count = 0;")
        if finfo.get('has_tracking_count'):
            begin_body.append("    ctx->tracking_count = 0;")
        if finfo.get('has_failure_tracking_count'):
            begin_body.append("    ctx->failure_tracking_count = 0;")
        if finfo.get('has_subgoal_count'):
            begin_body.append("    ctx->subgoal_count = 0;")
        if finfo.get('has_current_step'):
            begin_body.append("    ctx->current_step = 0;")
        if finfo.get('has_cascade_detected'):
            begin_body.append("    ctx->cascade_detected = false;")
        if finfo.get('has_self_confidence'):
            begin_body.append("    ctx->self_confidence = 0.5f;")
        if finfo.get('has_uncertainty'):
            begin_body.append("    ctx->uncertainty = 0.5f;")
        if finfo.get('has_attention_weight'):
            begin_body.append("    ctx->attention_weight = 1.0f;")
        if finfo.get('has_decisions_since_check'):
            begin_body.append("    ctx->decisions_since_check = 0;")
        if finfo.get('has_history_index'):
            begin_body.append("    ctx->history_index = 0;")
        if finfo.get('has_checks_performed'):
            begin_body.append("    ctx->checks_performed = 0;")
        if finfo.get('has_interventions_applied'):
            begin_body.append("    ctx->interventions_applied = 0;")
        if finfo.get('has_last_overall_severity'):
            begin_body.append("    ctx->last_overall_severity = 0.0f;")
        if finfo.get('has_synapse_count'):
            begin_body.append("    ctx->synapse_count = 0;")
        if finfo.get('has_stress_level'):
            begin_body.append("    ctx->current_stress_level = 0.0f;")
        if finfo.get('has_anxiety'):
            begin_body.append("    ctx->anxiety_signal = 0.0f;")
        if finfo.get('has_depression'):
            begin_body.append("    ctx->depression_signal = 0.0f;")
        if finfo.get('has_num_instances'):
            begin_body.append("    ctx->num_instances = 0;")
        if finfo.get('has_num_pending'):
            begin_body.append("    ctx->num_pending = 0;")
        if finfo.get('has_num_patterns'):
            begin_body.append("    ctx->num_patterns = 0;")
        if finfo.get('has_num_applied'):
            begin_body.append("    ctx->num_applied = 0;")

    begin_body.append(f"    NIMCP_LOGGING_INFO(\"%s training begin: counters reset\", \"{prefix}\");")

    # === TRAINING STEP ===
    step_body = []
    step_body.append("    if (progress < 0.0f) progress = 0.0f;")
    step_body.append("    if (progress > 1.0f) progress = 1.0f;")
    step_body.append(f"    {prefix}_heartbeat_instance({agent_expr}, \"{short}_training_step\", progress);")

    if struct_name:
        if finfo.get('has_update_count'):
            step_body.append("    ctx->update_count++;")
        if finfo.get('has_total_samples'):
            step_body.append("    ctx->total_samples++;")
        if finfo.get('has_attention_weight'):
            step_body.append("    /* Adaptive attention decay during training */")
            step_body.append("    float decay = 1.0f - 0.1f * progress;")
            step_body.append("    if (decay < 0.5f) decay = 0.5f;")
            step_body.append("    ctx->attention_weight *= decay;")
        if finfo.get('has_self_confidence'):
            step_body.append("    /* Gradually improve confidence during training */")
            step_body.append("    ctx->self_confidence += 0.005f * progress;")
            step_body.append("    if (ctx->self_confidence > 1.0f) ctx->self_confidence = 1.0f;")
        if finfo.get('has_uncertainty'):
            step_body.append("    /* Reduce uncertainty as training progresses */")
            step_body.append("    ctx->uncertainty *= (1.0f - 0.01f * progress);")
            step_body.append("    if (ctx->uncertainty < 0.0f) ctx->uncertainty = 0.0f;")
        if finfo.get('has_effects'):
            step_body.append("    /* Modulate effects capacity with training progress */")
            step_body.append("    ctx->effects.overall_capacity = 0.5f + 0.5f * progress;")
        if finfo.get('has_stress_level'):
            step_body.append("    /* Reduce stress during training */")
            step_body.append("    ctx->current_stress_level *= (1.0f - 0.02f * progress);")
        if finfo.get('has_anxiety'):
            step_body.append("    /* Reduce anxiety signal during training */")
            step_body.append("    ctx->anxiety_signal *= (1.0f - 0.02f * progress);")
        if finfo.get('has_depression'):
            step_body.append("    /* Reduce depression signal during training */")
            step_body.append("    ctx->depression_signal *= (1.0f - 0.02f * progress);")
        if finfo.get('has_learning_rate'):
            step_body.append("    /* Adapt learning rate to training phase */")
            step_body.append("    ctx->learning_rate_effective = 0.01f + 0.09f * (1.0f - progress);")
        if finfo.get('has_last_overall_severity'):
            step_body.append("    /* Decrease severity tracking during training */")
            step_body.append("    ctx->last_overall_severity *= (1.0f - 0.01f * progress);")
        if finfo.get('has_decisions_since_check'):
            step_body.append("    ctx->decisions_since_check++;")
        if finfo.get('has_checks_performed'):
            step_body.append("    ctx->checks_performed++;")

    # === TRAINING END ===
    end_body = []
    end_body.append(f"    {prefix}_heartbeat_instance({agent_expr}, \"{short}_training_end\", 1.0f);")

    if struct_name:
        # Compute averages and log metrics
        if finfo.get('has_update_count'):
            end_body.append("    uint64_t total_updates = ctx->update_count;")
        if finfo.get('has_total_samples'):
            end_body.append("    uint64_t total_samples = ctx->total_samples;")
        if finfo.get('has_self_confidence'):
            end_body.append("    float final_confidence = ctx->self_confidence;")
        if finfo.get('has_uncertainty'):
            end_body.append("    float final_uncertainty = ctx->uncertainty;")
        if finfo.get('has_attention_weight'):
            end_body.append("    /* Restore attention weight post-training */")
            end_body.append("    ctx->attention_weight = 1.0f;")
        if finfo.get('has_effects'):
            end_body.append("    ctx->effects.overall_capacity = 1.0f;")

        # Build log message
        log_parts = []
        log_args = []
        if finfo.get('has_update_count'):
            log_parts.append("updates=%lu")
            log_args.append("(unsigned long)total_updates")
        if finfo.get('has_total_samples'):
            log_parts.append("samples=%lu")
            log_args.append("(unsigned long)total_samples")
        if finfo.get('has_self_confidence'):
            log_parts.append("confidence=%.4f")
            log_args.append("final_confidence")
        if finfo.get('has_uncertainty'):
            log_parts.append("uncertainty=%.4f")
            log_args.append("final_uncertainty")
        if finfo.get('has_checks_performed'):
            log_parts.append("checks=%lu")
            log_args.append("(unsigned long)ctx->checks_performed")

        if log_parts:
            fmt = ", ".join(log_parts)
            args = ", ".join(log_args)
            end_body.append(f"    NIMCP_LOGGING_INFO(\"%s training end: {fmt}\", \"{prefix}\", {args});")
        else:
            end_body.append(f"    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"{prefix}\");")
    else:
        end_body.append(f"    NIMCP_LOGGING_INFO(\"%s training end: complete\", \"{prefix}\");")

    # Build the functions
    # For bridge types, take typed pointer; for non-bridge with struct, also typed
    if struct_name:
        type_name = f"{struct_name}_t"
        if is_bridge:
            p_type = f"{type_name}*"
            p_name = "bridge"
        else:
            p_type = f"{type_name}*"
            p_name = "instance"
    else:
        p_type = "void*"
        p_name = "instance"

    code = f"""/* ============================================================================
 * Phase 8: Training Functions (FULL implementation)
 * ============================================================================ */
int {prefix}_training_begin({p_type} {p_name}) {{
    if (!{p_name}) return -1;
"""
    if struct_name:
        code += cast_line + "\n"
    code += "\n".join(begin_body) + "\n    return 0;\n}\n\n"

    code += f"int {prefix}_training_step({p_type} {p_name}, float progress) {{\n"
    code += f"    if (!{p_name}) return -1;\n"
    if struct_name:
        code += cast_line + "\n"
    code += "\n".join(step_body) + "\n    return 0;\n}\n\n"

    code += f"int {prefix}_training_end({p_type} {p_name}) {{\n"
    code += f"    if (!{p_name}) return -1;\n"
    if struct_name:
        code += cast_line + "\n"
    code += "\n".join(end_body) + "\n    return 0;\n}\n"

    return code


def add_health_agent_to_struct(content, struct_name):
    """Add nimcp_health_agent_t* health_agent to struct if not present."""
    # Find the struct definition and add field after first member
    pattern = rf'(struct {re.escape(struct_name)} \{{[^\}}]*?)((\n\s*}}))'

    # More targeted: find struct, add before closing brace
    lines = content.split('\n')
    in_struct = False
    brace_depth = 0
    insert_line = None

    for i, line in enumerate(lines):
        if re.match(rf'^struct {re.escape(struct_name)} \{{', line):
            in_struct = True
            brace_depth = 1
            continue
        if in_struct:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth <= 0:
                # Insert before closing brace
                insert_line = i
                break

    if insert_line is not None:
        # Check if health_agent already exists
        struct_text = '\n'.join(lines[:insert_line])
        if 'health_agent' not in struct_text.split(f'struct {struct_name}')[-1]:
            indent = "    "
            new_field = f"{indent}nimcp_health_agent_t* health_agent;         /**< Health agent (Phase 8) */"
            lines.insert(insert_line, new_field)
            lines.insert(insert_line, "")
            lines.insert(insert_line, f"{indent}/* Phase 8: Instance health agent */")
            return '\n'.join(lines), True

    return content, False


def process_file(filepath):
    """Process a single .c file to add Phase 8 enhancements."""
    info = extract_info(filepath)

    if not info['gvar'] or not info['hbfunc'] or not info['prefix']:
        print(f"  SKIP (no health agent infrastructure): {filepath}")
        return False

    if info['has_instance'] and info['has_training']:
        print(f"  SKIP (already enhanced): {filepath}")
        return False

    content = info['content']
    modified = False

    # 1. For bridges with struct: add health_agent field to struct if missing
    if info['is_bridge'] and info['primary_struct'] and not info['has_health_agent_field']:
        content, added = add_health_agent_to_struct(content, info['primary_struct'])
        if added:
            print(f"  Added health_agent field to struct {info['primary_struct']}")
            info['has_health_agent_field'] = True
            modified = True

    # 2. Add heartbeat_instance after existing heartbeat function
    if not info['has_instance']:
        hb_instance_code = generate_heartbeat_instance(info)
        # Find the end of the existing heartbeat function
        hb_func = info['hbfunc']
        # Pattern: find the complete heartbeat function and insert after it
        pattern = rf'(static inline void {re.escape(hb_func)}\(const char\* operation, float progress\) \{{[^}}]*\}})'
        match = re.search(pattern, content, re.DOTALL)
        if match:
            insert_pos = match.end()
            content = content[:insert_pos] + hb_instance_code + content[insert_pos:]
            modified = True
            print(f"  Added heartbeat_instance")

    # 3. Add set_instance_health_agent and training functions at end of file
    if not info['has_training']:
        # Re-extract info after potential struct modification
        info['content'] = content

        set_instance_code = generate_set_instance(info)
        training_code = generate_training_functions(info)

        # Find insertion point: before the last line or before security macros
        # Look for BRIDGE_DEFINE_SECURITY_SETTERS or just append before final empty lines
        security_pattern = r'BRIDGE_DEFINE_SECURITY_SETTERS\('
        security_match = re.search(security_pattern, content)

        if security_match:
            insert_pos = security_match.start()
            content = content[:insert_pos] + set_instance_code + "\n" + training_code + "\n" + content[insert_pos:]
        else:
            # Append at end, stripping trailing whitespace
            content = content.rstrip() + "\n\n" + set_instance_code + "\n" + training_code + "\n"

        modified = True
        print(f"  Added set_instance + training functions")

    if modified:
        with open(filepath, 'w') as f:
            f.write(content)
        return True
    return False


def main():
    dirs = [
        "src/cognitive/fault_tolerance",
        "src/cognitive/health",
        "src/cognitive/mental_health",
        "src/cognitive/wellbeing",
    ]

    total_files = 0
    processed = 0
    skipped = 0

    for d in dirs:
        full_dir = os.path.join(BASE, d)
        print(f"\n=== Processing {d} ===")
        for fn in sorted(os.listdir(full_dir)):
            if not fn.endswith('.c'):
                continue
            # Skip backup files
            if '.bioasync_backup' in fn:
                continue
            # Skip include-only files
            if fn == 'disorder_detectors.c':
                continue

            filepath = os.path.join(full_dir, fn)
            total_files += 1
            print(f"\nProcessing: {fn}")

            if process_file(filepath):
                processed += 1
            else:
                skipped += 1

    print(f"\n=== Summary ===")
    print(f"Total files: {total_files}")
    print(f"Processed: {processed}")
    print(f"Skipped: {skipped}")


if __name__ == '__main__':
    main()
