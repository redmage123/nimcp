"""Run a config, then the same config with each module switched off, and
report what each module is worth."""
from __future__ import annotations

import copy

from .data import Task
from .models.brain import default_modules
from .train import TrainConfig, train


def modules_of(cfg: TrainConfig, tasks: list[Task], video=None) -> list[str]:
    """The switches that matter for this run: shared_words only with two or
    more senses, motion_context only when predicting video."""
    mods = {**default_modules(), **cfg.model.modules}
    multi = len({t.modality for t in tasks}) > 1
    skip = set()
    if not multi:
        skip.add("shared_words")
    if video is None:
        skip.add("motion_context")
    if not tasks:
        skip.add("workspace_mlp")            # prediction works on the encoder map, not the workspace
        if video is None:
            skip.add("residual")
    candidates = {"spiking_workspace"}           # phase-4 swaps are compared with --on, not removed
    names = [k for k, on in mods.items() if on and k not in skip | candidates]
    return (["augment"] if tasks else []) + sorted(names)


def without(cfg: TrainConfig, module: str) -> TrainConfig:
    c = copy.deepcopy(cfg)
    if module == "augment":
        c.augment = False
    else:
        c.model.modules[module] = False
    return c


def _summary(metrics: dict) -> dict:
    out = {"top1": metrics["top1"]}
    cm = metrics.get("crossmodal")
    if cm:
        out["crossmodal"] = sum(v for k, v in cm.items() if "_to_" in k) / 2
    return out


def ablate(cfg: TrainConfig, tasks: list[Task], log=print, corpus=None, video=None) -> list[dict]:
    def run(c, name):
        return _summary(train(c, tasks, name=name, kind="ablation", log=log, corpus=corpus, video=video)["metrics"])

    log("== full model")
    full = run(cfg, "full")
    rows = [{"module": "(none removed)", **full}]
    for m in modules_of(cfg, tasks, video):
        log(f"== without {m}")
        rows.append({"module": m, **run(without(cfg, m), f"no_{m}")})
    has_cm = "crossmodal" in full
    log("\nmodule            score   worth" + ("    crossmodal  worth" if has_cm else ""))
    for r in rows:
        line = f"{r['module']:<16} {r['top1']:.3f}   {full['top1'] - r['top1']:+.3f}"
        if has_cm:
            line += f"    {r['crossmodal']:.3f}      {full['crossmodal'] - r['crossmodal']:+.3f}"
        log(line)
    return rows
