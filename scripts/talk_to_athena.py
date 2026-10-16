#!/usr/bin/env python3
"""Talk to Athena — Interactive conversation with the trained brain.

Connects to the running training process via Unix socket IPC if available.
Falls back to loading the brain from checkpoint if training isn't running.

Usage:
    python3 scripts/talk_to_athena.py                    # auto-detect
    python3 scripts/talk_to_athena.py --local             # force checkpoint load
    python3 scripts/talk_to_athena.py --checkpoint path   # specific checkpoint
    python3 scripts/talk_to_athena.py --top-k 5           # show top 5 matches
"""

import argparse
import json
import logging
import math
import os
import socket
import struct
import sys
import time

import numpy as np

# Ensure scripts/ is on the path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from neural_decoder import NeuralDecoder, VocabularyBank
from claude_teacher import encode_text
from response_composer import ResponseComposer
from conversation_context import ConversationContext
from phi3_decoder import Phi3Decoder
from hybrid_decoder import HybridDecoder

logger = logging.getLogger("talk_to_athena")

ATHENA_SOCKET_PATH = "/tmp/athena_brain.sock"

# ---------------------------------------------------------------------------
# Constants (must match immerse_athena.py)
# ---------------------------------------------------------------------------

EMBED_DIM = 1024
BRAIN_INPUTS = 1024


def _normalize_embedding(e: np.ndarray) -> np.ndarray:
    """Normalize embedding values from [-1, 1] to [0, 1] for the brain."""
    return (e + 1.0) * 0.5


def _denormalize_embedding(e: np.ndarray) -> np.ndarray:
    """Reverse [0,1] normalization back to [-1, 1] for cosine similarity."""
    return e * 2.0 - 1.0


def tile_to_brain_input(embedding: np.ndarray) -> list:
    """Tile/truncate embedding to 1024-dim brain input."""
    e = np.asarray(embedding, dtype=np.float32).ravel()
    e = _normalize_embedding(e)
    reps = math.ceil(BRAIN_INPUTS / len(e))
    tiled = np.tile(e, reps)[:BRAIN_INPUTS]
    return tiled.tolist()


def extract_embedding_from_output(output_vector: np.ndarray,
                                   embed_dim: int = EMBED_DIM) -> np.ndarray:
    """Extract embedding from brain output by averaging tiled copies."""
    v = np.asarray(output_vector, dtype=np.float32).ravel()
    num_outputs = len(v)

    if num_outputs <= embed_dim:
        return _denormalize_embedding(v)

    n_full = num_outputs // embed_dim
    remainder = num_outputs % embed_dim
    chunks = [v[i * embed_dim:(i + 1) * embed_dim] for i in range(n_full)]
    if remainder > 0:
        partial = v[n_full * embed_dim:]
        for i in range(n_full):
            chunks[i][:remainder] = (chunks[i][:remainder] + partial / n_full)

    averaged = np.mean(chunks, axis=0).astype(np.float32)
    return _denormalize_embedding(averaged)


# ---------------------------------------------------------------------------
# IPC Client — connects to running training process
# ---------------------------------------------------------------------------

class AthenaIPCClient:
    """Client for the Unix socket IPC server in immerse_athena.py."""

    def __init__(self, socket_path=ATHENA_SOCKET_PATH):
        self.socket_path = socket_path

    @staticmethod
    def is_available(socket_path=ATHENA_SOCKET_PATH):
        """Check if the IPC server is running and responsive."""
        if not os.path.exists(socket_path):
            return False
        try:
            client = AthenaIPCClient(socket_path)
            resp = client.send({"cmd": "ping"})
            return resp.get("ok", False)
        except Exception:
            return False

    def send(self, request: dict) -> dict:
        """Send a request and receive the response."""
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(30.0)
        try:
            sock.connect(self.socket_path)
            # Send length-prefixed JSON
            data = json.dumps(request).encode("utf-8")
            sock.sendall(struct.pack(">I", len(data)) + data)
            # Receive length-prefixed response
            hdr = b""
            while len(hdr) < 4:
                chunk = sock.recv(4 - len(hdr))
                if not chunk:
                    return {"error": "Connection closed"}
                hdr += chunk
            length = struct.unpack(">I", hdr)[0]
            body = b""
            while len(body) < length:
                chunk = sock.recv(min(length - len(body), 65536))
                if not chunk:
                    return {"error": "Connection closed"}
                body += chunk
            return json.loads(body.decode("utf-8"))
        finally:
            sock.close()

    def decide(self, text: str, top_k: int = 5) -> dict:
        return self.send({"cmd": "decide", "text": text, "top_k": top_k})

    def status(self) -> dict:
        return self.send({"cmd": "status"})

    def transcript(self) -> dict:
        return self.send({"cmd": "transcript"})

    def stats(self) -> dict:
        return self.send({"cmd": "stats"})


# ---------------------------------------------------------------------------
# Conversation memory
# ---------------------------------------------------------------------------

class ConversationMemory:
    """Track recent conversation turns for context."""

    def __init__(self, max_turns=10):
        self.turns = []
        self.max_turns = max_turns

    def add(self, user_text, athena_response, confidence=0.0):
        self.turns.append({
            'user': user_text,
            'athena': athena_response,
            'confidence': confidence,
            'time': time.time()
        })
        if len(self.turns) > self.max_turns:
            self.turns = self.turns[-self.max_turns:]

    def get_context_string(self, last_n=3):
        if not self.turns:
            return ""
        recent = self.turns[-last_n:]
        parts = []
        for t in recent:
            parts.append(f"User: {t['user']}")
            if t['athena']:
                parts.append(f"Athena: {t['athena']}")
        return " | ".join(parts)

    def get_context_embedding(self, last_n=3):
        ctx = self.get_context_string(last_n)
        if not ctx:
            return None
        return encode_text(ctx)


# ---------------------------------------------------------------------------
# IPC conversation loop (connects to running training process)
# ---------------------------------------------------------------------------

def run_ipc_conversation(client: AthenaIPCClient, args):
    """Conversation loop using IPC to the training process."""
    # Get training status
    status = client.status()
    vocab_size = status.get("vocab_size", 0)
    stage = status.get("stage", "?")
    step = status.get("step", "?")

    print()
    print("=" * 60)
    print("  ATHENA CONVERSATION (IPC — connected to training process)")
    print(f"  Training: Stage {stage}, Step {step}")
    print(f"  Vocabulary: {vocab_size} entries")
    print("  Type your message and press Enter.")
    print("  Commands: /quit /status /transcript /context")
    print("=" * 60)
    print()

    memory = ConversationMemory(max_turns=10)
    ctx_engine = ConversationContext(max_turns=50)
    turn = 0

    while True:
        try:
            user_input = input("You: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nGoodbye!")
            break

        if not user_input:
            continue

        if user_input.lower() in ("/quit", "/exit", "/q"):
            print("Goodbye!")
            break

        if user_input.lower() == "/status":
            status = client.status()
            print(f"[Stage {status.get('stage', '?')}, "
                  f"Step {status.get('step', '?')}, "
                  f"Vocab: {status.get('vocab_size', 0)}]")
            continue

        if user_input.lower() == "/transcript":
            resp = client.transcript()
            transcript = resp.get("transcript", [])
            if transcript:
                print(f"[Cognitive transcript ({len(transcript)} entries):]")
                for entry in transcript:
                    mod = entry.get('module', '?')
                    sal = entry.get('salience', 0.0)
                    conf = entry.get('confidence', 0.0)
                    summary = entry.get('summary', '')
                    print(f"  {mod:20s}  sal={sal:.3f}  "
                          f"conf={conf:.3f}  {summary}")
            else:
                print("[No transcript available]")
            continue

        if user_input.lower() == "/context":
            summary = ctx_engine.get_summary()
            print(f"[Conversation context:]")
            print(f"  Turns: {summary['turns']}")
            print(f"  Active topic: {', '.join(summary['active_topic']) if summary['active_topic'] else '(none)'}")
            continue

        # --- Query the brain via IPC ---
        ctx_signals = ctx_engine.process_turn(user_input, [], {})

        t0 = time.time()
        resp = client.decide(user_input, top_k=args.top_k)
        elapsed = time.time() - t0

        if "error" in resp:
            print(f"[Error: {resp['error']}]")
            continue

        confidence = resp.get("confidence", 0.0)
        coherence = resp.get("coherence", 0.0)
        output_norm = resp.get("output_norm", 0.0)
        athena_response = ""

        # Priority: grounded > phi3 > generated > decoded vocab
        grounded = resp.get("grounded_response", "")
        grounded_conf = resp.get("grounded_confidence", 0.0)
        phi3_resp = resp.get("phi3_response", "")
        generated = resp.get("generated_text", "")
        decoded = resp.get("decoded", [])
        response_text = resp.get("response", "")

        if grounded and grounded_conf >= 0.3:
            athena_response = grounded
            print(f"Athena: {grounded}")
            print(f"        [grounded | conf={grounded_conf:.4f} | "
                  f"{elapsed*1000:.0f}ms]")
        elif phi3_resp:
            athena_response = phi3_resp
            print(f"Athena: {phi3_resp}")
            vocab_hint = response_text[:40] if response_text else ""
            print(f"        [phi3 | vocab: {vocab_hint} | "
                  f"conf={confidence:.4f} | {elapsed*1000:.0f}ms]")
        elif generated:
            athena_response = generated
            print(f"Athena: {generated}")
            print(f"        [generated | conf={confidence:.4f} | "
                  f"coherence={coherence:.3f} | {elapsed*1000:.0f}ms]")
        elif response_text:
            athena_response = response_text
            print(f"Athena: {response_text}")
            sim = decoded[0]["similarity"] if decoded else 0.0
            print(f"        [vocab | sim={sim:.4f} | "
                  f"coherence={coherence:.3f} | |v|={output_norm:.2f} | "
                  f"{elapsed*1000:.0f}ms]")
        else:
            print(f"Athena: [no decodable output | conf={confidence:.4f}]")

        # Show alternatives if verbose
        if args.verbose and decoded and len(decoded) > 1:
            print("        alternatives:")
            for d in decoded[1:]:
                print(f"          {d['similarity']:.4f}  {d['text'][:80]}")

        # Show transcript highlights if verbose
        if args.verbose:
            transcript = resp.get("transcript", [])
            if transcript:
                print("        [cognitive transcript:]")
                for entry in sorted(transcript,
                                    key=lambda e: e.get('salience', 0),
                                    reverse=True)[:3]:
                    if entry.get('salience', 0) > 0.1:
                        mod = entry.get('module', '?')
                        sal = entry.get('salience', 0.0)
                        summary = entry.get('summary', '')
                        print(f"          {mod:20s}  sal={sal:.2f}  "
                              f"{summary[:60]}")

        ctx_engine.record_response(athena_response)
        memory.add(user_input, athena_response, confidence)
        print()
        turn += 1


# ---------------------------------------------------------------------------
# Local conversation loop (loads brain from checkpoint — fallback)
# ---------------------------------------------------------------------------

def run_local_conversation(args):
    """Original conversation loop — loads brain from checkpoint."""
    print("[1/3] Importing NIMCP library...", end=" ", flush=True)
    t0 = time.time()
    import nimcp
    print(f"done ({time.time() - t0:.1f}s)")
    print(f"      NIMCP v{nimcp.version()} | ABI hash: {nimcp.ABI_LAYOUT_HASH}")

    if not os.path.exists(args.checkpoint):
        print(f"ERROR: Checkpoint not found: {args.checkpoint}")
        cp_dir = os.path.dirname(args.checkpoint) or "checkpoints/athena"
        if os.path.isdir(cp_dir):
            print("Available checkpoints:")
            for f in sorted(os.listdir(cp_dir)):
                if f.endswith(".bin"):
                    path = os.path.join(cp_dir, f)
                    size_mb = os.path.getsize(path) / (1024 * 1024)
                    print(f"  {path}  ({size_mb:.0f} MB)")
        sys.exit(1)

    cp_size_mb = os.path.getsize(args.checkpoint) / (1024 * 1024)
    print(f"[2/3] Loading brain from checkpoint ({cp_size_mb:.0f} MB)...",
          end=" ", flush=True)
    t0 = time.time()
    brain = nimcp.Brain("athena", checkpoint=args.checkpoint)
    print(f"done ({time.time() - t0:.1f}s)")

    print("[3/3] Loading decoder...", end=" ", flush=True)
    decoder = None
    vocab = None
    if os.path.isdir(args.decoder_dir):
        try:
            decoder = NeuralDecoder.load(args.decoder_dir)
            vocab = decoder.vocabulary
            print(f"done ({len(vocab)} vocab entries)")
        except Exception as e:
            print(f"failed ({e})")
    else:
        print("skipped (no decoder directory)")

    has_grounded = hasattr(brain, 'grounded_respond')
    has_generate = hasattr(brain, 'generate_text')

    # Initialize Phi-3 hybrid decoder
    phi3 = Phi3Decoder()
    hybrid = HybridDecoder(neural_decoder=decoder, phi3_decoder=phi3, brain=brain)

    print()
    print("=" * 60)
    print("  ATHENA CONVERSATION (LOCAL — brain loaded from checkpoint)")
    print("  Type your message and press Enter.")
    print("  Commands: /quit /status /raw /similar <text> /teach <text> "
          "/probe /transcript /context")
    print("=" * 60)
    print(f"  Inference pipeline:")
    print(f"    System 1 (fast): grounded_respond "
          f"{'[available]' if has_grounded else '[unavailable]'}")
    print(f"    System 2 (full): decide_full + "
          f"{'Phi-3 [available]' if phi3.available else 'vocab lookup'}")
    if vocab and len(vocab) > 0:
        print(f"  Vocabulary: {len(vocab)} entries")
    print()

    show_raw = False
    turn = 0
    memory = ConversationMemory(max_turns=10)
    composer = ResponseComposer(max_sentences=3, min_salience=0.15)
    ctx_engine = ConversationContext(max_turns=50)

    while True:
        try:
            user_input = input("You: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nGoodbye!")
            break

        if not user_input:
            continue

        if user_input.lower() in ("/quit", "/exit", "/q"):
            print("Goodbye!")
            break

        if user_input.lower() == "/raw":
            show_raw = not show_raw
            print(f"[Raw output display: {'ON' if show_raw else 'OFF'}]")
            continue

        if user_input.lower() == "/status":
            print(f"[Turn {turn} | Vocab: {len(vocab) if vocab else 0} | "
                  f"Checkpoint: {args.checkpoint}]")
            continue

        if user_input.lower().startswith("/similar "):
            query = user_input[9:].strip()
            if query and vocab:
                q_emb = encode_text(query)
                results = vocab.decode(q_emb, top_k=args.top_k)
                print(f"[Nearest to '{query}':]")
                for text, sim in results:
                    print(f"  {sim:.4f}  {text[:80]}")
            continue

        if user_input.lower() == "/probe":
            try:
                stats = brain.get_stats()
                print(f"[Brain stats: {stats}]")
            except Exception as e:
                print(f"[Probe error: {e}]")
            continue

        if user_input.lower() == "/transcript":
            try:
                transcript = brain.get_transcript()
                if transcript:
                    print(f"[Cognitive transcript ({len(transcript)} entries):]")
                    for entry in transcript:
                        mod = entry.get('module', '?')
                        sal = entry.get('salience', 0.0)
                        conf = entry.get('confidence', 0.0)
                        summary = entry.get('summary', '')
                        print(f"  {mod:20s}  sal={sal:.3f}  "
                              f"conf={conf:.3f}  {summary}")
                else:
                    print("[No transcript available — run a query first]")
            except Exception as e:
                print(f"[Transcript error: {e}]")
            continue

        if user_input.lower() == "/context":
            summary = ctx_engine.get_summary()
            print(f"[Conversation context:]")
            print(f"  Turns: {summary['turns']}")
            print(f"  Active topic: {', '.join(summary['active_topic']) if summary['active_topic'] else '(none)'}")
            continue

        if user_input.lower().startswith("/teach "):
            text = user_input[7:].strip()
            if text:
                emb = encode_text(text)
                features = tile_to_brain_input(emb)
                target_emb = _normalize_embedding(emb)
                reps = math.ceil(2048 / len(target_emb))
                target = np.tile(target_emb, reps)[:2048].tolist()
                try:
                    loss = brain.learn_vector(features, target, label=text[:50])
                    if vocab:
                        vocab.add(text, emb)
                    print(f"[Taught: '{text[:60]}' | loss={loss:.4f}]")
                except Exception as e:
                    print(f"[Teach error: {e}]")
            continue

        # Process context
        ctx_signals = ctx_engine.process_turn(user_input, [], {})

        # System 1: grounded language
        grounded_ok = False
        t0 = time.time()
        try:
            grounded = brain.grounded_respond(user_input)
            grounded_time = time.time() - t0
            grounded_text = grounded.get("response", "")
            grounded_conf = grounded.get("confidence", 0.0)
            if grounded_text and grounded_conf >= 0.3:
                grounded_ok = True
                print(f"Athena: {grounded_text}")
                print(f"        [grounded | conf={grounded_conf:.4f} | "
                      f"{grounded_time*1000:.0f}ms]")
        except Exception:
            pass

        if grounded_ok:
            ctx_engine.record_response(grounded_text)
            memory.add(user_input, grounded_text, grounded_conf)
            print()
            turn += 1
            continue

        # System 2: full cognitive pipeline
        athena_response = ""
        t0 = time.time()
        input_embedding = encode_text(user_input)
        ctx_emb = memory.get_context_embedding(last_n=3)
        if ctx_emb is not None:
            input_embedding = 0.8 * input_embedding + 0.2 * ctx_emb
            input_embedding = input_embedding / max(
                np.linalg.norm(input_embedding), 1e-8)
        features = tile_to_brain_input(input_embedding)
        encode_time = time.time() - t0

        t0 = time.time()
        try:
            result = brain.decide_full(features)
        except Exception as e:
            print(f"[Brain error: {e}]")
            continue
        infer_time = time.time() - t0

        output_vector = result.get("output_vector", [])
        label = result.get("label", "")
        confidence = result.get("confidence", 0.0)

        if args.verbose or show_raw:
            print(f"[System 2 | encode={encode_time*1000:.0f}ms, "
                  f"infer={infer_time*1000:.0f}ms, "
                  f"label='{label}', conf={confidence:.4f}]")

        if show_raw and output_vector:
            ov = np.array(output_vector)
            print(f"[output: len={len(ov)}, min={ov.min():.4f}, "
                  f"max={ov.max():.4f}, mean={ov.mean():.4f}, "
                  f"std={ov.std():.4f}]")

        # Decode
        vocab_response = ""
        gen_text = ""
        best_sim = 0.0
        results = []

        if output_vector:
            try:
                gen_result = brain.generate_text(list(output_vector))
                gt = gen_result.get("text", "")
                if gt and len(gt.strip()) > 2:
                    gen_text = gt
            except Exception:
                pass

        if output_vector and vocab and len(vocab) > 0:
            brain_emb = extract_embedding_from_output(np.array(output_vector))
            results = vocab.decode(brain_emb, top_k=args.top_k)
            best_text, best_sim = results[0]
            vocab_response = best_text

        # Transcript
        transcript = []
        try:
            transcript = brain.get_transcript() or []
        except Exception:
            pass

        # Compose response
        best_base = gen_text or vocab_response
        composed = composer.compose(
            transcript, result,
            user_input=user_input,
            vocab_response=best_base if confidence > 0.1 else "",
            context=ctx_signals)

        if composed:
            athena_response = composed
            print(f"Athena: {composed}")
            sources = []
            for entry in sorted(transcript,
                                key=lambda e: e.get('salience', 0),
                                reverse=True)[:3]:
                if entry.get('salience', 0) > 0.15:
                    sources.append(entry['module'])
            src_str = ", ".join(sources) if sources else "cognitive"
            phase = ctx_signals.conversation_phase
            print(f"        [composed from: {src_str} | "
                  f"conf={confidence:.4f} | phase={phase} | "
                  f"{(encode_time + infer_time)*1000:.0f}ms]")
        elif phi3.available and output_vector:
            # Phi-3 language cortex: brain-state conditioned generation
            phi3_result = hybrid.decode(output_vector, user_text=user_input)
            if phi3_result and phi3_result.get('source') == 'phi3':
                athena_response = phi3_result['text']
                print(f"Athena: {phi3_result['text']}")
                vocab_hint = vocab_response[:40] if vocab_response else ""
                print(f"        [phi3 | vocab: {vocab_hint} | "
                      f"conf={confidence:.4f} | "
                      f"{(encode_time + infer_time)*1000:.0f}ms]")
            elif vocab_response:
                athena_response = vocab_response
                print(f"Athena: {vocab_response}")
                print(f"        [vocab lookup | similarity={best_sim:.4f}]")
            else:
                athena_response = label or ""
                print(f"Athena: [{label}] (confidence: {confidence:.4f})")
        elif gen_text:
            athena_response = gen_text
            print(f"Athena: {gen_text}")
            print(f"        [generated | conf={confidence:.4f}]")
        elif vocab_response:
            athena_response = vocab_response
            print(f"Athena: {vocab_response}")
            print(f"        [vocab lookup | similarity={best_sim:.4f}]")
        elif label:
            athena_response = label
            print(f"Athena: [{label}] (confidence: {confidence:.4f})")
        else:
            athena_response = ""
            print("Athena: [no decodable output]")

        if args.verbose or show_raw:
            if vocab_response:
                print(f"        [vocab: '{vocab_response[:60]}' "
                      f"sim={best_sim:.4f}]")
                if args.top_k > 1 and len(results) > 1:
                    print("        alternatives:")
                    for text, sim in results[1:]:
                        print(f"          {sim:.4f}  {text[:80]}")
            if transcript:
                print("        [cognitive transcript:]")
                for entry in transcript:
                    mod = entry.get('module', '?')
                    sal = entry.get('salience', 0.0)
                    if sal > 0.1:
                        summary = entry.get('summary', '')
                        print(f"          {mod:20s}  sal={sal:.2f}  "
                              f"{summary[:60]}")

        ctx_engine.record_response(athena_response)
        memory.add(user_input, athena_response, confidence)
        print()
        turn += 1


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Talk to Athena")
    parser.add_argument("--checkpoint", type=str,
                        default="checkpoints/athena/athena_immersive.bin",
                        help="Path to brain checkpoint (for local mode)")
    parser.add_argument("--decoder-dir", type=str,
                        default="checkpoints/athena/decoder",
                        help="Path to decoder state directory (for local mode)")
    parser.add_argument("--top-k", type=int, default=5,
                        help="Number of nearest matches to show")
    parser.add_argument("--verbose", action="store_true",
                        help="Show timing and debug info")
    parser.add_argument("--local", action="store_true",
                        help="Force local mode (load brain from checkpoint)")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s"
    )

    # Try IPC first (unless --local)
    if not args.local and AthenaIPCClient.is_available():
        print("Connected to running Athena training process via IPC.")
        print("(No extra memory needed — using the brain already in memory)")
        client = AthenaIPCClient()
        run_ipc_conversation(client, args)
    else:
        if not args.local:
            print("Training process not running — loading brain from "
                  "checkpoint.")
            print("(This will use ~50+ GB RAM. Use --local to force this "
                  "mode.)")
            print()
        run_local_conversation(args)


if __name__ == "__main__":
    main()
