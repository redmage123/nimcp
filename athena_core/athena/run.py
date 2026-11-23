"""Command line. TASKS is a comma-separated list of cifar100, cifar10, speech.

    python3 -m athena.run baselines speech
    python3 -m athena.run train cifar100 --size small --epochs 30
    python3 -m athena.run train cifar100,cifar10,speech --epochs 30
    python3 -m athena.run ablate cifar100,cifar10,speech --size tiny --epochs 5
    python3 -m athena.run board cifar100
"""
from __future__ import annotations

import argparse
import os
import time

import torch

from . import scoreboard
from .baselines import BASELINES, text_baselines
from .models import ModelConfig
from .train import LOADERS, TrainConfig, load_tasks, task_key, train


def _cfg(a) -> TrainConfig:
    c = TrainConfig(model=ModelConfig(size=a.size), epochs=a.epochs, lr=a.lr,
                    batch_size=a.batch_size, augment=not a.no_augment, seed=a.seed,
                    limit_train=a.limit_train, optimizer=a.optimizer, captions=a.captions,
                    weight_decay=a.weight_decay,
                    steps_per_epoch=a.steps_per_epoch, video_passes=a.video_passes, text_batch=a.text_batch)
    if a.device:
        c.device = a.device
    for m in a.off or []:
        c.model.modules[m] = False
    for m in a.on or []:
        c.model.modules[m] = True
    return c


def _fmt(m: dict) -> str:
    s = f"top1 {m.get('top1', 0):.3f}"
    for name, v in (m.get("test") or {}).items():
        if name == "video":
            s += "  video skill " + " ".join(f"{k} {r['skill']:+.3f} (mse {r['mse']:.4f}, copy {r['copy_mse']:.4f})"
                                             for k, r in v.items() if isinstance(r, dict))
            continue
        if "ppl" in v:
            s += f"  {name} ppl {v['ppl']:.1f}" + "".join(
                f" ({k[4:]} {x:.1f})" for k, x in v.items() if k.startswith("ppl_"))
            continue
        s += f"  {name} {v['top1']:.3f}"
        if "spike_rate" in v:
            s += f" [spikes {v['spike_rate']:.3f}]"
        if "worst_top1" in v:
            s += f" [worst prompt {v['worst_top1']:.3f}, first {v['first_top1']:.3f}]"
        if "blind_top1" in v:
            s += f" (blind {v['blind_top1']:.3f}, chance {v['chance']:.3f})"
        if "joint_top1" in v:
            s += f" (joint {v['joint_top1']:.3f})"
    cm = m.get("crossmodal")
    if cm:
        s += "  crossmodal " + " ".join(f"{k} {v:.3f}" for k, v in cm.items() if "_to_" in k)
        s += f" (chance {cm['chance']:.2f})"
    return s


def main() -> None:
    p = argparse.ArgumentParser(prog="athena.run")
    p.add_argument("command", choices=["baselines", "train", "ablate", "board", "generate"])
    p.add_argument("tasks", help="comma-separated: " + ", ".join(LOADERS) + ", text, video")
    p.add_argument("--size", default="small", choices=["tiny", "small", "base"])
    p.add_argument("--epochs", type=int, default=30)
    p.add_argument("--lr", type=float, default=0.1)
    p.add_argument("--optimizer", default="sgd", choices=["sgd", "adamw"])
    p.add_argument("--weight-decay", type=float, default=5e-4)
    p.add_argument("--captions", action="store_true", help="train input -> sentence captions (needs text)")
    p.add_argument("--vocab-size", type=int, default=16384)
    p.add_argument("--subwords", action="store_true", help="subword (byte-pair) vocabulary instead of words")
    p.add_argument("--extra-text", nargs="*", default=[], help="PG-19 parquet shards to add to the books")
    p.add_argument("--steps-per-epoch", type=int)
    p.add_argument("--video-passes", type=float, help="video passes per epoch in a joint run (default: every step)")
    p.add_argument("--text-batch", type=int, default=64)
    p.add_argument("--batch-size", type=int, default=256)
    p.add_argument("--no-augment", action="store_true")
    p.add_argument("--off", nargs="*", help="model modules to switch off")
    p.add_argument("--on", nargs="*", help="candidate modules to switch on (e.g. spiking_workspace)")
    p.add_argument("--limit-train", type=int)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--device")
    p.add_argument("--name", default=None)
    p.add_argument("--model", help="generate: path to a saved model (.pt)")
    p.add_argument("--prompt", default="", help="generate: text to continue")
    p.add_argument("--samples", type=int, default=5, help="generate: test items to caption")
    p.add_argument("--temperature", type=float, default=0.8)
    a = p.parse_args()
    names = a.tasks.split(",")
    cap_gb = os.environ.get("ATHENA_GPU_MEM_GB")
    if cap_gb and torch.cuda.is_available():
        # Hard cap on this process's GPU memory, for running beside other jobs.
        total = torch.cuda.get_device_properties(0).total_memory
        torch.cuda.set_per_process_memory_fraction(min(1.0, float(cap_gb) * 2**30 / total))

    if a.command == "board":
        key = "+".join(sorted(names))
        for r in scoreboard.load(task=key):
            print(f"{r['time']}  {r['kind']:<9} {r['name']:<20} {_fmt(r['metrics'])}  code {r['code']}")
        return

    if a.command == "generate":
        _generate(a, names)
        return

    tasks = load_tasks(names)
    corpus = None
    if "text" in names:
        from .data.text import load_corpus
        from .data.vocab import build_vocab
        corpus = load_corpus(vocab_size=a.vocab_size, required_words=build_vocab(load_tasks(list(LOADERS))),
                             extra=a.extra_text, subwords=a.subwords)
    video = None
    if "video" in names:
        from .data.video import load_video
        video = load_video()
    if a.command == "baselines":
        if corpus is not None:
            t0 = time.time()
            for bname, m in text_baselines(corpus).items():
                scoreboard.record(task_key([], corpus), "baseline", bname, m, seconds=time.time() - t0)
                print(f"text      {bname:<9} test ppl {m['ppl']:.1f}")
        for t in tasks:
            for bname, fn in BASELINES.items():
                t0 = time.time()
                m = fn(t)
                scoreboard.record(t.name, "baseline", bname, m, seconds=time.time() - t0)
                print(f"{t.name:<9} {bname:<9} top1 {m['top1']:.3f}  top5 {m['top5']:.3f}  ({time.time() - t0:.0f}s)")
    elif a.command == "train":
        out = train(_cfg(a), tasks, name=a.name or f"{a.size}_e{a.epochs}",
                    save_dir=scoreboard.SCOREBOARD.parent / "models", corpus=corpus, video=video)
        m = out["metrics"]
        print(f"\nresult: {_fmt(m)}  ({m['params']:,} params, {out['seconds']:.0f}s)")
        if corpus is not None:
            rows = [r for r in scoreboard.load(task=task_key([], corpus)) if r["kind"] == "baseline"]
            if rows:
                b = min(rows, key=lambda r: r["metrics"]["ppl"])
                print(f"  text: ppl {m['test']['text']['ppl']:.1f} vs best baseline {b['name']} "
                      f"{b['metrics']['ppl']:.1f}")
        for t in tasks:
            base = scoreboard.best_baseline(t.name)
            if base and "test" in m:
                print(f"  {t.name}: {m['test'][t.name]['top1']:.3f} vs best baseline {base[0]} "
                      f"{base[1]:.3f} -> margin {m['test'][t.name]['top1'] - base[1]:+.3f}")
    elif a.command == "ablate":
        from .ablate import ablate
        ablate(_cfg(a), tasks, corpus=corpus, video=video)


def _generate(a, names) -> None:
    """Show what a saved model says: continue a text prompt, and caption a
    few test items of each named task (the word chosen by the caption head,
    then a free continuation)."""
    from .data.text import EOS, Vocab, tokenize
    from .models import Brain, ModelConfig
    from .models.language import generate

    ck = torch.load(a.model, map_location="cpu", weights_only=False)
    mc = ck["config"]["model"]
    model = Brain(ModelConfig(**mc)).eval()
    model.load_state_dict(ck["model"])
    if not mc.get("n_tokens"):
        raise SystemExit("this model has no language head")
    if ck.get("tokens"):
        vocab = Vocab(ck["tokens"])
    else:   # older checkpoints: rebuild the vocabulary exactly as training did
        from .data.text import load_corpus
        from .data.vocab import build_vocab
        vocab = load_corpus(required_words=build_vocab(load_tasks(list(LOADERS)))).vocab
    assert len(vocab) == mc["n_tokens"], (len(vocab), mc["n_tokens"])
    eos = vocab.stoi[EOS]
    torch.manual_seed(a.seed)
    if a.prompt:
        out = generate(model.language, torch.tensor(vocab.encode(tokenize(a.prompt))), 60,
                       temperature=a.temperature, stop=eos)
        print(f"{a.prompt} | {vocab.decode(out)}")
    from .objectives import EVAL_PROMPT
    for t in load_tasks(names):
        if t.modality not in model.encoders:
            continue
        g = torch.Generator().manual_seed(a.seed)
        for i in torch.randperm(len(t.test), generator=g)[:a.samples].tolist():
            x = t.prep(t.test.inputs[i:i + 1])
            with torch.no_grad():
                z = model.encode(x, t.modality)
            prompt = torch.tensor(vocab.encode(tokenize(EVAL_PROMPT[t.modality])))
            out = generate(model.language, prompt, 12, prefix=z, temperature=a.temperature, stop=eos)
            print(f"[{t.name} #{i}, truly '{t.words[int(t.test.labels[i])]}'] "
                  f"{EVAL_PROMPT[t.modality]} {vocab.decode(out)}")


if __name__ == "__main__":
    main()
