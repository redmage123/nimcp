"""Build (input, target) pairs for ann_clip_test's `real` task from the
trainer's own embedding cache.

input  = cached 1024-dim sentence embedding of a text (what encode_text returns)
target = make_semantic_target(text): TargetDiversifier.diversify (copied out of
         immerse_athena.py by AST, not imported) tiled to 4096.

Usage: python3 make_real_pairs.py <immerse_athena.py> <embed_cache.npz> <out.bin> [N=768] [seed=0]
Output: uint32 N, in_dim, out_dim; then N*in_dim float32 inputs; then N*out_dim float32 targets.
"""
import ast
import struct
import sys
import threading
from functools import lru_cache

import numpy as np

src_path, cache_path, out_path = sys.argv[1:4]
N = int(sys.argv[4]) if len(sys.argv) > 4 else 768
seed = int(sys.argv[5]) if len(sys.argv) > 5 else 0

tree = ast.parse(open(src_path).read())
ns = {"np": np, "threading": threading, "lru_cache": lru_cache}
nodes = []
for node in tree.body:
    if isinstance(node, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "BRAIN_OUTPUT_DIM" for t in node.targets):
        nodes.append(node)
    if isinstance(node, ast.ClassDef) and node.name == "TargetDiversifier":
        nodes.append(node)
assert len(nodes) == 2, "BRAIN_OUTPUT_DIM / TargetDiversifier not found"
exec(compile(ast.Module(body=nodes, type_ignores=[]), src_path, "exec"), ns)
out_dim = ns["BRAIN_OUTPUT_DIM"]
div = ns["TargetDiversifier"]()

z = np.load(cache_path, allow_pickle=True)
E, T = z["embeddings"].astype(np.float32), z["texts"]
idx = np.random.default_rng(seed).choice(len(E), N, replace=False)
X = E[idx]
in_dim = X.shape[1]

Y = np.empty((N, out_dim), np.float32)
for r, i in enumerate(idx):
    emb = div.diversify(E[i], str(T[i]))
    reps = (out_dim + len(emb) - 1) // len(emb)
    Y[r] = np.tile(emb, reps)[:out_dim]


def pair_cos(A):
    An = A / np.linalg.norm(A, axis=1, keepdims=True)
    C = An @ An.T
    return C[np.triu_indices(len(A), 1)].mean()


print(f"N={N} in={in_dim} out={out_dim}  input pairwise cos {pair_cos(X):.3f}  "
      f"target pairwise cos {pair_cos(Y):.3f}  |target| {np.linalg.norm(Y, axis=1).mean():.3f}")
with open(out_path, "wb") as f:
    f.write(struct.pack("<III", N, in_dim, out_dim))
    f.write(np.ascontiguousarray(X).tobytes())
    f.write(np.ascontiguousarray(Y).tobytes())
print("wrote", out_path)
