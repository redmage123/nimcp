#!/usr/bin/env python3
"""Multi-stream file transfer over ssh for large checkpoint sidecars.

A single TCP stream on the long, lossy pod<->Hetzner path is throttled by
loss recovery (measured 2026-11-01: 0.07 MB/s with cubic, 2.5 MB/s with
BBR); throughput on such paths scales roughly with the number of
concurrent streams. This tool splits a file into byte-range chunks and
moves them over N parallel ssh streams with dd, retries failed chunks,
verifies every chunk by sha256 on both ends, re-sends mismatches, and only
then renames the temporary file into place (the destination is never a
partial file).

Usage:
    parallel_xfer.py pull HOST:REMOTE_PATH LOCAL_PATH [options]
    parallel_xfer.py push LOCAL_PATH HOST:REMOTE_PATH [options]
Options:
    --streams N       concurrent streams (default 12)
    --chunk-mb M      chunk size in MiB (default 256)
    --ssh "ARGS"      extra ssh arguments, e.g. "-p 54213 -i ~/.ssh/key"
    --retries R       attempts per chunk (default 5)
Exit status 0 only when the destination exists with the source's size and
every chunk hash matches.
"""
import argparse
import concurrent.futures as cf
import hashlib
import os
import shlex
import subprocess
import sys
import time

MiB = 1024 * 1024


def ssh_cmd(host, ssh_args, remote_cmd):
    return ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=20",
            "-o", "ServerAliveInterval=30", "-o", "StrictHostKeyChecking=no",
            *shlex.split(ssh_args or ""), host, remote_cmd]


def remote_size(host, ssh_args, path):
    out = subprocess.run(ssh_cmd(host, ssh_args, f"stat -c%s {shlex.quote(path)}"),
                         capture_output=True, text=True, timeout=120)
    if out.returncode != 0:
        raise RuntimeError(f"remote stat failed: {out.stderr.strip()[-200:]}")
    return int(out.stdout.strip().splitlines()[-1])


def chunk_hash_local(path, off, n):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        f.seek(off)
        left = n
        while left > 0:
            b = f.read(min(8 * MiB, left))
            if not b:
                break
            h.update(b)
            left -= len(b)
    return h.hexdigest()


def chunk_hashes_remote(host, ssh_args, path, chunks, chunk_bytes):
    """sha256 of every chunk on the remote side in one ssh call."""
    q = shlex.quote(path)
    cb = chunk_bytes // MiB
    script = ("for i in " + " ".join(str(i) for i, _, _ in chunks) +
              f"; do dd if={q} bs=1M skip=$((i*{cb})) count={cb} status=none | sha256sum | cut -c1-64; done")
    out = subprocess.run(ssh_cmd(host, ssh_args, script), capture_output=True, text=True,
                         timeout=7200)
    if out.returncode != 0:
        raise RuntimeError(f"remote hashing failed: {out.stderr.strip()[-200:]}")
    hs = [l.strip() for l in out.stdout.splitlines() if len(l.strip()) == 64]
    return {chunks[k][0]: hs[k] for k in range(min(len(hs), len(chunks)))}


def move_chunk(mode, host, ssh_args, remote, local, idx, off, n, chunk_bytes, retries):
    cb = chunk_bytes // MiB
    rq = shlex.quote(remote)
    for attempt in range(1, retries + 1):
        try:
            if mode == "pull":
                src = subprocess.Popen(ssh_cmd(host, ssh_args,
                                               f"dd if={rq} bs=1M skip={idx * cb} count={cb} status=none"),
                                       stdout=subprocess.PIPE)
                with open(local, "r+b") as f:
                    f.seek(off)
                    got = 0
                    while True:
                        b = src.stdout.read(4 * MiB)
                        if not b:
                            break
                        f.write(b)
                        got += len(b)
                rc = src.wait(timeout=60)
                if rc == 0 and got == n:
                    return idx, attempt
            else:
                dst = subprocess.Popen(ssh_cmd(host, ssh_args,
                                               f"dd of={rq} bs=1M seek={idx * cb} conv=notrunc status=none"),
                                       stdin=subprocess.PIPE)
                with open(local, "rb") as f:
                    f.seek(off)
                    left = n
                    while left > 0:
                        b = f.read(min(4 * MiB, left))
                        if not b:
                            break
                        dst.stdin.write(b)
                        left -= len(b)
                dst.stdin.close()
                if dst.wait(timeout=600) == 0 and left == 0:
                    return idx, attempt
        except Exception:
            pass
        time.sleep(min(30, 3 * attempt))
    raise RuntimeError(f"chunk {idx} failed after {retries} attempts")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["pull", "push"])
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--streams", type=int, default=12)
    ap.add_argument("--chunk-mb", type=int, default=256)
    ap.add_argument("--ssh", default="")
    ap.add_argument("--retries", type=int, default=5)
    a = ap.parse_args()

    if a.mode == "pull":
        host, remote = a.src.split(":", 1)
        final_local = a.dst if not os.path.isdir(a.dst) else os.path.join(a.dst, os.path.basename(remote))
        size = remote_size(host, a.ssh, remote)
        work_local = os.path.join(os.path.dirname(final_local) or ".",
                                  "." + os.path.basename(final_local) + ".pxfer")
        with open(work_local, "ab") as f:
            f.truncate(size)
        local = work_local
        work_remote = remote
    else:
        host, remote = a.dst.split(":", 1)
        local = a.src
        size = os.path.getsize(local)
        rdir, rbase = os.path.dirname(remote) or ".", os.path.basename(remote)
        if remote.endswith("/"):
            rdir, rbase = remote.rstrip("/"), os.path.basename(local)
        final_remote = f"{rdir}/{rbase}"
        work_remote = f"{rdir}/.{rbase}.pxfer"
        subprocess.run(ssh_cmd(host, a.ssh, f"mkdir -p {shlex.quote(rdir)} && "
                                            f"truncate -s {size} {shlex.quote(work_remote)}"),
                       check=True, timeout=120)

    chunk_bytes = a.chunk_mb * MiB
    chunks = [(i, i * chunk_bytes, min(chunk_bytes, size - i * chunk_bytes))
              for i in range((size + chunk_bytes - 1) // chunk_bytes)]
    t0 = time.time()

    # Already there? (the emergency ship re-sends a canonical Hetzner often
    # already holds; rsync's quick-check made that instant.) Same size +
    # every chunk hash equal -> nothing to send.
    if a.mode == "push":
        try:
            fsz = remote_size(host, a.ssh, final_remote)
        except Exception:
            fsz = -1
        if fsz == size:
            rh = chunk_hashes_remote(host, a.ssh, final_remote, chunks, chunk_bytes)
            if all(rh.get(i) == chunk_hash_local(local, off, n) for i, off, n in chunks):
                subprocess.run(ssh_cmd(host, a.ssh, f"rm -f {shlex.quote(work_remote)}"), timeout=120)
                print(f"[pxfer] OK identical copy already at destination ({size / 1e9:.2f} GB, "
                      f"verified in {time.time() - t0:.0f}s)", flush=True)
                return 0
    print(f"[pxfer] {a.mode} {size / 1e9:.2f} GB in {len(chunks)} chunks over {a.streams} streams",
          flush=True)

    def run(chs):
        done = 0
        with cf.ThreadPoolExecutor(max_workers=a.streams) as ex:
            futs = [ex.submit(move_chunk, a.mode, host, a.ssh, work_remote, local, i, off, n,
                              chunk_bytes, a.retries) for i, off, n in chs]
            for f in cf.as_completed(futs):
                f.result()
                done += 1
                if done % max(1, len(chs) // 10) == 0 or done == len(chs):
                    el = time.time() - t0
                    print(f"[pxfer] {done}/{len(chs)} chunks, {el:.0f}s", flush=True)

    run(chunks)
    # Verify every chunk; re-send mismatches (up to 3 rounds).
    for rnd in range(3):
        rh = chunk_hashes_remote(host, a.ssh, work_remote, chunks, chunk_bytes)
        bad = [c for c in chunks if rh.get(c[0]) != chunk_hash_local(local, c[1], c[2])]
        if not bad:
            break
        print(f"[pxfer] verify round {rnd + 1}: {len(bad)} chunk(s) differ, re-sending", flush=True)
        run(bad)
    else:
        print("[pxfer] FAILED: chunks still differ after 3 rounds", file=sys.stderr)
        return 2

    if a.mode == "pull":
        os.replace(local, final_local)
    else:
        subprocess.run(ssh_cmd(host, a.ssh, f"mv -f {shlex.quote(work_remote)} {shlex.quote(final_remote)}"),
                       check=True, timeout=120)
    el = time.time() - t0
    print(f"[pxfer] OK {size / 1e9:.2f} GB in {el:.0f}s = {size / MiB / max(el, 1e-6):.1f} MB/s (verified)",
          flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
