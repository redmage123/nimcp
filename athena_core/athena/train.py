"""One training loop for every objective: each step takes a batch from every
objective, backpropagates each loss as it is built, and makes one optimizer
step on their sum."""
from __future__ import annotations

import math
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path

import torch

from . import scoreboard
from .data import Split, Task, build_vocab, load_cifar10, load_cifar100, load_speech
from .data.text import Corpus, tokenize
from .models import Brain, ModelConfig
from .objectives import Captions, Naming, Objective, Prediction, Text, _autocast

LOADERS = {"cifar100": load_cifar100, "cifar10": load_cifar10, "speech": load_speech}


def load_tasks(names: list[str]) -> list[Task]:
    return [LOADERS[n]() for n in names if n in LOADERS]


def task_key(tasks: list[Task], corpus: Corpus | None = None, captions: bool = False,
             video=None) -> str:
    parts = sorted(t.name for t in tasks)
    if corpus is not None:
        parts.append("text/sw" if corpus.vocab.subword else "text")
    if video is not None:
        parts.append("video")
    if captions:
        parts.append("captions")
    return "+".join(parts)


@dataclass
class TrainConfig:
    model: ModelConfig = field(default_factory=ModelConfig)
    epochs: int = 30
    batch_size: int = 256          # per naming/caption objective, per step
    text_batch: int = 64           # sequences per step for the text objective
    ctx: int = 128                 # text window
    optimizer: str = "sgd"         # "sgd" | "adamw"
    lr: float = 0.1
    weight_decay: float = 5e-4
    label_smoothing: float = 0.1
    augment: bool = True           # each task's own augmentation (ablatable)
    captions: bool = False         # train input -> sentence captions (needs a corpus)
    steps_per_epoch: int | None = None  # cap (a pass over a large corpus would swamp everything else)
    video_passes: float | None = None   # video passes per epoch (None: one batch every step).
                                        # Video data is small and overfits; spread evenly.
    val_size: int = 5000           # carved from train when a task has no validation split
    limit_train: int | None = None # memorisation mode: train and score on the first N items
    seed: int = 0
    device: str = "cuda" if torch.cuda.is_available() else "cpu"


def train_val(task: Task, cfg: TrainConfig) -> tuple[Split, Split]:
    if cfg.limit_train:
        s = task.train.subset(torch.arange(min(cfg.limit_train, len(task.train))))
        return s, s
    if task.val is not None:
        return task.train, task.val
    perm = torch.randperm(len(task.train), generator=torch.Generator().manual_seed(cfg.seed))
    return task.train.subset(perm[cfg.val_size:]), task.train.subset(perm[:cfg.val_size])


@torch.no_grad()
def encode_split(model: Brain, task: Task, split: Split, device: str, batch: int = 256) -> torch.Tensor:
    model.eval()
    out = []
    for i in range(0, len(split), batch):
        x = task.prep(split.inputs[i:i + batch].to(device))
        with _autocast(device):
            out.append(model.encode(x, task.modality).float().cpu())
    return torch.cat(out)


@torch.no_grad()
def crossmodal(model: Brain, tasks: list[Task], device: str, per_word: int = 200) -> dict | None:
    """Grounding test. For the words both senses know, does a heard word's
    workspace vector lie closest to the pictures of the same word (and the
    other way round)? Chance is 1 / (number of shared words)."""
    by_mod: dict[str, list[Task]] = {}
    for t in tasks:
        by_mod.setdefault(t.modality, []).append(t)
    if len(by_mod) < 2:
        return None
    (ma, ta), (mb, tb) = list(by_mod.items())[:2]
    words = sorted({w for t in ta for w in t.words} & {w for t in tb for w in t.words})
    if len(words) < 2:
        return None

    def vectors(group: list[Task]) -> dict[str, torch.Tensor]:
        out = {}
        for t in group:
            for w in words:
                if w in t.words and w not in out:
                    idx = (t.test.labels == t.words.index(w)).nonzero().flatten()[:per_word]
                    z = encode_split(model, t, t.test.subset(idx), device)
                    out[w] = torch.nn.functional.normalize(z, dim=1)
        return out

    va, vb = vectors(ta), vectors(tb)

    def match(src: dict, dst: dict) -> float:
        cent = torch.nn.functional.normalize(torch.stack([dst[w].mean(0) for w in words]), dim=1)
        hits = [((src[w] @ cent.t()).argmax(1) == i).float() for i, w in enumerate(words)]
        return torch.cat(hits).mean().item()

    return {"words": words, "chance": 1 / len(words),
            f"{mb}_to_{ma}": match(vb, va), f"{ma}_to_{mb}": match(va, vb)}


def build_objectives(cfg: TrainConfig, tasks: list[Task], corpus: Corpus | None,
                     video=None) -> list[Objective]:
    objs: list[Objective] = []
    for t in tasks:
        tr, va = train_val(t, cfg)
        objs.append(Naming(t, tr, va, cfg.batch_size, cfg.augment, cfg.label_smoothing))
        if cfg.captions and corpus is not None:
            objs.append(Captions(t, tr, va, corpus, cfg.batch_size, cfg.augment))
    if corpus is not None:
        objs.append(Text(corpus, cfg.text_batch, cfg.ctx))
    if video is not None:
        objs.append(Prediction(video, cfg.batch_size))
    return objs


def headline(results: dict) -> float:
    """One number per run: mean naming top-1 (or caption top-1, or minus log
    perplexity when nothing else is trained)."""
    naming = [v["top1"] for k, v in results.items()
              if not k.startswith("captions_") and isinstance(v, dict) and "top1" in v]
    if naming:
        return sum(naming) / len(naming)
    caps = [v["top1"] for k, v in results.items() if k.startswith("captions_")]
    if caps:
        return sum(caps) / len(caps)
    if "text" in results:
        return -math.log(results["text"]["ppl"])
    return results["video"]["skill"]


def train(cfg: TrainConfig, tasks: list[Task], name: str = "train", kind: str = "train",
          save_dir: Path | None = None, log=print, corpus: Corpus | None = None,
          video=None) -> dict:
    torch.manual_seed(cfg.seed)
    device = cfg.device
    vocab = build_vocab(tasks)
    cfg.model.vocab_size = len(vocab)
    cfg.model.modalities = {t.modality: t.in_channels for t in tasks}
    if video is not None or not cfg.model.modalities:
        cfg.model.modalities.setdefault("vision", 3)
    cfg.model.video = video is not None
    if corpus is not None:
        cfg.model.n_tokens = len(corpus.vocab)
        cfg.model.word_tokens = [corpus.vocab.encode(tokenize(w)) for w in vocab] if vocab else [[0]]
    model = Brain(cfg.model).to(device)
    params_n = sum(p.numel() for p in model.parameters())

    objs = build_objectives(cfg, tasks, corpus, video)
    steps_per_epoch = max(math.ceil(o.n_train / (o.batch if hasattr(o, "batch") else 1)) for o in objs)
    if cfg.steps_per_epoch:
        steps_per_epoch = min(steps_per_epoch, cfg.steps_per_epoch)
    if cfg.optimizer == "adamw":
        opt = torch.optim.AdamW(model.parameters(), lr=cfg.lr, weight_decay=cfg.weight_decay)
    else:
        opt = torch.optim.SGD(model.parameters(), lr=cfg.lr, momentum=0.9, nesterov=True,
                              weight_decay=cfg.weight_decay)
    total = cfg.epochs * steps_per_epoch
    warmup = min(total // 10, 2 * steps_per_epoch)
    sched = torch.optim.lr_scheduler.LambdaLR(
        opt, lambda s: (s + 1) / max(1, warmup) if s < warmup
        else 0.5 * (1 + math.cos(math.pi * (s - warmup) / max(1, total - warmup))))

    n_steps = {o.name: sum(_slot(o, i, steps_per_epoch, cfg) is not None for i in range(steps_per_epoch))
               for o in objs}
    t0 = time.time()
    history = []
    for epoch in range(cfg.epochs):
        model.train()
        perms = {o.name: o.new_perm() for o in objs}
        run = {o.name: 0.0 for o in objs}
        for i in range(steps_per_epoch):
            opt.zero_grad(set_to_none=True)
            for o in objs:
                j = _slot(o, i, steps_per_epoch, cfg)
                if j is None:
                    continue
                loss = o.batch_loss(model, j, perms[o.name], device)
                loss.backward()
                run[o.name] += loss.item()
            opt.step()
            sched.step()
        val = {o.name: o.evaluate(model, "val", device) for o in objs}
        history.append({"epoch": epoch + 1, "loss": {k: v / max(1, n_steps[k]) for k, v in run.items()}, "val": val})
        log(f"  epoch {epoch + 1:3d}  " + "  ".join(_short(k, v) for k, v in val.items())
            + f"  {time.time() - t0:.0f}s")

    metrics: dict = {"val_final": history[-1]["val"], "params": params_n}
    if cfg.limit_train:
        metrics["top1"] = headline(history[-1]["val"])
    else:
        metrics["test"] = {o.name: o.evaluate(model, "test", device) for o in objs}
        metrics["top1"] = headline(metrics["test"])
        cm = crossmodal(model, tasks, device)
        if cm:
            metrics["crossmodal"] = cm
    secs = time.time() - t0
    conf = asdict(cfg)
    conf["model"].pop("word_tokens", None)
    scoreboard.record(task_key(tasks, corpus, cfg.captions, video), kind, name, metrics, conf, secs)
    if save_dir is not None:
        save_dir.mkdir(parents=True, exist_ok=True)
        torch.save({"model": model.state_dict(), "config": asdict(cfg), "vocab": vocab,
                    "tokens": corpus.vocab.itos if corpus is not None else None,
                    "tasks": [t.name for t in tasks]},
                   save_dir / f"{name}.pt")
    return {"metrics": metrics, "history": history, "model": model, "seconds": secs, "vocab": vocab}


def _slot(o: Objective, i: int, steps: int, cfg: TrainConfig) -> int | None:
    """Batch index objective `o` trains on at step i, or None to sit the step out.
    Only video is rationed: `video_passes` passes per epoch, spread evenly."""
    if o.name != "video" or cfg.video_passes is None:
        return i
    own = max(1, round(cfg.video_passes * math.ceil(o.n_train / o.batch)))
    if own >= steps:
        return i
    j = i * own // steps
    return j if i == 0 or (i - 1) * own // steps != j else None


def _short(name: str, v: dict) -> str:
    if name == "video":
        return "video skill " + " ".join(f"{k} {r['skill']:+.3f}" for k, r in v.items() if isinstance(r, dict))
    if "ppl" in v:
        return f"{name} ppl {v['ppl']:.1f}"
    s = f"{name} {v['top1']:.3f}"
    if "worst_top1" in v:
        s += f" worst {v['worst_top1']:.3f}"
    if "blind_top1" in v:
        s += f" (blind {v['blind_top1']:.3f})"
    return s
