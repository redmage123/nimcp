#!/usr/bin/env python3
"""Live sweep: SNN generation seed-count x params, show grounded_respond output."""
import json, socket, struct, sys

SOCK = "/var/run/athena/brain.sock"

def send(cmd, **kw):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(120.0)
    s.connect(SOCK)
    payload = json.dumps({"cmd": cmd, **kw}).encode()
    s.sendall(struct.pack(">I", len(payload)) + payload)
    hdr = b""
    while len(hdr) < 4:
        hdr += s.recv(4 - len(hdr))
    n = struct.unpack(">I", hdr)[0]
    buf = b""
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            break
        buf += chunk
    s.close()
    return json.loads(buf.decode())

PROMPTS = [
    "tell me about a cat",
    "what is water",
    "the dog runs fast",
    "I feel happy today",
    "the sun is bright",
]

# Make sure SNN produce path is on.
print("ping:", send("ping").get("ok"))
print("set_produce_via_snn:", send("set_produce_via_snn", enabled=True))

for max_c in (1, 2, 3):
    print("\n" + "=" * 70)
    print(f"  max_concepts={max_c}  n_steps=16  inject=40")
    print("=" * 70)
    r = send("set_snn_gen_params", n_steps=16, inject=40.0, max_concepts=max_c)
    print("  set_snn_gen_params ->", r.get("ok", r))
    for p in PROMPTS:
        resp = send("grounded_respond", text=p)
        txt = resp.get("response") or resp.get("text") or resp
        conf = resp.get("confidence")
        print(f"  [{p!r}] -> {txt!r}  (conf={conf})")
