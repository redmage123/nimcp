#!/bin/bash
# phantom_audit_script.sh - 2026-04-24 phantom API audit (strict)
#
# A function PROTOTYPE in a header looks like:
#   <return-type-tokens> <name>(<args>);
# possibly with NIMCP_EXPORT / extern / const prefix. The ENTIRE prototype may
# be on one line or span multiple lines, but the prototype ALWAYS ends with ");".
#
# Strategy: read each header, preprocess (strip comments + continuations),
# find every "... name(...);" statement where "name" is the first identifier
# immediately preceding "(". That gives us a reliable list of prototypes.
#
# Output: tab-separated: name\theader_path:line\tcaller_count (in src/)

set +e

HDRS_FILE=/tmp/phantom_hdrs_list.txt
NAMES_FILE=/tmp/phantom_names.txt
IMPL_TMP=/tmp/phantom_impl.txt
IMPL_NAMES=/tmp/phantom_impl_names.txt

# Collect headers
find /home/bbrelin/nimcp/include/cognitive /home/bbrelin/nimcp/include/core/brain /home/bbrelin/nimcp/include/security \
     -name "*.h" -type f ! -path "*/bak/*" ! -name "*.bak" 2>/dev/null > "$HDRS_FILE"

echo "Headers: $(wc -l < $HDRS_FILE)" >&2

# Use python for robust parsing — much easier than awk for multi-line prototypes.
python3 - "$HDRS_FILE" > "$NAMES_FILE" <<'PYEOF'
import sys, re, os
hdrs_list = open(sys.argv[1]).read().splitlines()

# Strip /* ... */ comments, // ..., and preprocessor lines. Keep newline structure.
CLINE = re.compile(r'//[^\n]*')
CBLOCK = re.compile(r'/\*.*?\*/', re.DOTALL)
PPLINE = re.compile(r'^\s*#.*$', re.MULTILINE)

# Prototype matcher: match at file-scope a statement-like chunk ending in ");"
# Something like "<ret-type-tokens> <NAME>(...);"
# Capture NAME as the identifier immediately preceding "(".
PROTO = re.compile(
    r'(?P<lead>(?:NIMCP_EXPORT\s+)?(?:extern\s+)?(?:static\s+)?(?:inline\s+)?(?:const\s+)?(?:struct\s+\w+\s*\*?\s*|union\s+\w+\s*\*?\s*|enum\s+\w+\s*\*?\s*|\w[\w\s\*]*))'
    r'\b(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*;',
    re.MULTILINE
)

# Skip names that are obviously not prototypes (keywords, macros, etc.)
SKIP = set("""if for while switch sizeof return typedef else do goto case static inline
    extern const volatile struct union enum void int float double bool char long short
    signed unsigned size_t ssize_t uint8_t uint16_t uint32_t uint64_t int8_t int16_t int32_t int64_t
    NIMCP_EXPORT""".split())

for h in hdrs_list:
    try:
        with open(h, 'r', errors='replace') as f:
            src = f.read()
    except:
        continue

    # Original line-number tracking: replace comments with spaces so line numbers remain.
    def blank_out(m):
        s = m.group(0)
        return ''.join(c if c == '\n' else ' ' for c in s)

    clean = CBLOCK.sub(blank_out, src)
    clean = CLINE.sub(lambda m: ' ' * len(m.group(0)), clean)
    clean = PPLINE.sub(lambda m: ' ' * len(m.group(0)), clean)

    for m in PROTO.finditer(clean):
        name = m.group('name')
        if name in SKIP:
            continue
        # skip all-uppercase macros
        if re.fullmatch(r'[A-Z][A-Z0-9_]+', name):
            continue
        # "lead" must be non-trivial — at least one word token besides the name
        lead = m.group('lead').strip()
        # must contain a whitespace (so it's "<type> <name>", not just "name"/"(void) name")
        if not lead or not re.search(r'\S', lead):
            continue
        # compute line number
        line = clean.count('\n', 0, m.start()) + 1
        print(f"{name}\t{h}:{line}")
PYEOF

echo "Decl candidates: $(wc -l < $NAMES_FILE)" >&2

# Step 2: implementations in src/ AND in-header impls — any top-level function definition.
python3 - > "$IMPL_NAMES" <<'PYEOF'
import os, re, sys

# Match: start-of-line <return-type-tokens> <name>( ... ) {
# Supports multi-line: find "name(" then scan for matching ")" then "{"
# A more forgiving regex: lines that begin with a return type token and a "name("
# and DO NOT end with ";" and we find "{" on that line or a following line.
FN_DEF = re.compile(
    r'^(?:NIMCP_EXPORT\s+)?(?:extern\s+)?(?:static\s+)?(?:inline\s+)?(?:const\s+)?'
    r'(?:struct\s+\w+\s*\*?\s*|union\s+\w+\s*\*?\s*|enum\s+\w+\s*\*?\s*|\w[\w\s\*]*)\s*'
    r'\b(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*\)\s*(?:\{|$)',
    re.MULTILINE
)

# Also a generator-friendly: just find "NAME(" that is followed by opening brace
# within a reasonable window — captures macro-based CONNECT_IMPL patterns.
MACRO_IMPL = re.compile(
    r'^\s*([A-Z_][A-Z0-9_]*)\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,',
    re.MULTILINE
)

roots = ["/home/bbrelin/nimcp/src", "/home/bbrelin/nimcp/include"]
names = set()
for root in roots:
    for dp, dns, fns in os.walk(root):
        if "/bak" in dp: continue
        for fn in fns:
            if not (fn.endswith(".c") or fn.endswith(".h")): continue
            p = os.path.join(dp, fn)
            try:
                with open(p, 'r', errors='replace') as f:
                    src = f.read()
            except:
                continue
            # strip block comments (preserve newlines for line numbering)
            src2 = re.sub(r'/\*.*?\*/', lambda m: ''.join(c if c == '\n' else ' ' for c in m.group(0)), src, flags=re.DOTALL)
            src2 = re.sub(r'//[^\n]*', '', src2)
            for m in FN_DEF.finditer(src2):
                name = m.group('name')
                names.add(name)
            # Also pick up macro-generated impls (e.g. CONNECT_IMPL(name, ...))
            # Include the second argument after a known macro invocation,
            # BUT also just add the second identifier whenever we see a macro
            # pattern that looks like: MACRO_NAME(IDENTIFIER, ...
            # This may over-collect; we accept that.
            for m in MACRO_IMPL.finditer(src2):
                # only if the macro name suggests an impl generator
                macro = m.group(1)
                ident = m.group(2)
                if macro.endswith('_IMPL') or macro.endswith('IMPL') or \
                   'IMPL' in macro or 'DEFINE' in macro or \
                   macro.startswith('FN_') or macro.startswith('FUNC_') or \
                   'GENERATE' in macro or 'HOOK_SETTER' in macro:
                    names.add(ident)

            # Known code-gen macro invocations: BRIDGE_DEFINE_BIO_ASYNC_FUNCS*
            # generates <prefix>_connect_bio_async, <prefix>_disconnect_bio_async, <prefix>_is_bio_async_connected
            for m in re.finditer(r'BRIDGE_DEFINE_BIO_ASYNC_FUNCS(?:_TYPE|_OPAQUE)?\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)', src2):
                prefix = m.group(1)
                for suffix in ('_connect_bio_async', '_disconnect_bio_async', '_is_bio_async_connected'):
                    names.add(prefix + suffix)

            # Hook setter macros: HOOK_SETTER(ethics, type) -> octopus_set_ethics_hook
            # But the prefix ("octopus_") depends on scope — look around for the #define
            # We instead broadly also add <something>_set_<arg1>_hook when HOOK_SETTER seen
            # with the textual prefix derived from the macro definition scope.
            for m in re.finditer(r'\bHOOK_SETTER\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)', src2):
                # Look earlier in the file for the HOOK_SETTER #define line to guess prefix
                # For now: add <arg>_set_hook and also octopus_set_<arg>_hook patterns
                hook_arg = m.group(1)
                # Find the #define line for HOOK_SETTER
                defm = re.search(r'#\s*define\s+HOOK_SETTER\s*\([^)]*\)[^\n]*(?:\\[^\n]*\n[^\n]*)*', src2)
                # The file is nimcp_octopus.c -> prefix is "octopus"
                # Use file basename to guess
                base = os.path.basename(p)
                base_prefix = base.replace('nimcp_', '').replace('.c', '').replace('.h', '')
                names.add(f'{base_prefix}_set_{hook_arg}_hook')

            # CONNECT_IMPL style: explicit first-arg is the generated name
            for m in re.finditer(r'\bCONNECT_IMPL\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)', src2):
                names.add(m.group(1))

            # BRIDGE_DEFINE_SECURITY_SETTERS(prefix) -> prefix_set_{bbb,ethics,lgss,coordinator}
            for m in re.finditer(r'BRIDGE_DEFINE_SECURITY_SETTERS(?:_TYPE)?\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)', src2):
                prefix = m.group(1)
                for suffix in ('_set_bbb', '_set_ethics', '_set_lgss', '_set_coordinator'):
                    names.add(prefix + suffix)

            # BRIDGE_DEFINE_MESH_REGISTRATION(MODULE, CATEGORY) -> MODULE_register_with_mesh, MODULE_unregister_from_mesh
            for m in re.finditer(r'BRIDGE_DEFINE_MESH_REGISTRATION\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)', src2):
                prefix = m.group(1)
                for suffix in ('_register_with_mesh', '_unregister_from_mesh'):
                    names.add(prefix + suffix)

            # Generic FOO_SETTER(name, ...) → <module>_set_<name> pattern.
            # For files that define a local macro FOO_SETTER(name, ...) with pattern like
            # <prefix>_set_##name, find all invocations and add <prefix>_set_<arg>.
            # Extract prefix from the macro definition line.
            setter_def_rx = re.compile(r'#\s*define\s+([A-Z_][A-Z0-9_]*_SETTER)\s*\(\s*name\s*,[^)]*\)\s*\\?\n(?:[^\n]*\\\n)*?[^\n]*\b([a-z_][a-z0-9_]*)_set_\#\#\s*name', re.MULTILINE)
            prefix_map = {}
            for m in setter_def_rx.finditer(src2):
                macro_name = m.group(1)
                set_prefix = m.group(2)
                prefix_map[macro_name] = set_prefix
            for macro_name, set_prefix in prefix_map.items():
                for m in re.finditer(rf'\b{macro_name}\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)', src2):
                    names.add(f"{set_prefix}_set_{m.group(1)}")

for n in sorted(names):
    print(n)
PYEOF

echo "Impl names: $(wc -l < $IMPL_NAMES)" >&2

# Step 3: diff
python3 - "$NAMES_FILE" "$IMPL_NAMES" <<'PYEOF'
import sys, os, re, subprocess
names_file = sys.argv[1]
impl_names_file = sys.argv[2]

impl = set(open(impl_names_file).read().splitlines())

phantoms = {}
with open(names_file) as f:
    for line in f:
        parts = line.rstrip('\n').split('\t')
        if len(parts) < 2:
            continue
        name, loc = parts[0], parts[1]
        if name in impl:
            continue
        phantoms.setdefault(name, []).append(loc)

# Count callers in src/ per phantom.
# One grep per phantom name is slow but manageable (~hundreds).
# Use `grep -rE "\bname\s*\("` in src/.
for name, locs in sorted(phantoms.items()):
    # caller count
    try:
        r = subprocess.run(
            ['grep', '-rlE', r'\b' + re.escape(name) + r'\s*\(',
             '/home/bbrelin/nimcp/src', '--include=*.c'],
            capture_output=True, text=True, timeout=30)
        callers = len(r.stdout.splitlines())
    except:
        callers = -1
    print(f"{name}\t{'|'.join(locs)}\t{callers}")
PYEOF
