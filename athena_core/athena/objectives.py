"""What the model is trained and scored on. Each objective draws its own
batches and computes its own loss on its own output; the training loop sums
them into one optimizer step."""
from __future__ import annotations

import math

import torch
from torch import nn

from .baselines import topk_accuracy
from .data import Split, Task
from .data.text import EOS, Corpus, tokenize

# Caption templates. The scoring prompt is never trained on.
# Many phrasings, so the input has to win whatever the sentence around it
# leads the language model to expect.
TRAIN_TEMPLATES = {
    "vision": ["this is a {w} .", "look , a {w} .", "i see a {w} .", "that is a {w} .", "here is a {w} .",
               "the photo shows a {w} .", "a {w} is in this picture .", "she pointed at the {w} .",
               "we found a {w} .", "my favourite is the {w} .", "this picture is of a {w} .",
               "the thing in front of me is a {w} .", "he drew a {w} .", "an image of a {w} .",
               "that one is a {w} .", "do you see the {w} ?", "it is a {w} , i am sure .",
               "the children laughed at the {w} ."],
    "audio": ["you said {w} .", "i heard {w} .", "the word was {w} .", "that was {w} .",
              "she whispered {w} .", "he shouted {w} !", "the voice said {w} .", "listen : {w} .",
              "a child called out {w} .", "over the radio came the word {w} .", "{w} , said the voice .",
              "i think the word was {w} .", "they kept repeating {w} .", "\" {w} , \" he said .",
              "it was the word {w} .", "the recording says {w} .", "somebody spoke : {w} .",
              "again and again : {w} ."],
}
# Held-out prompts, never trained on. Captions are scored on each; the
# headline is the mean, and the worst shows how far a strong language prior
# ("it sounded like one ...") can pull the answer away from the input.
EVAL_PROMPTS = {
    "vision": ["there is a", "it looks like a", "in the picture i can see a", "what i saw was a"],
    "audio": ["it sounded like", "someone said", "what i heard was", "the sound was the word"],
}
EVAL_PROMPT = {m: p[0] for m, p in EVAL_PROMPTS.items()}


def _autocast(device: str):
    return torch.autocast(device_type="cuda", dtype=torch.bfloat16, enabled=device == "cuda")


class Objective:
    name: str
    n_train: int

    def batch_loss(self, model, step: int, perm: torch.Tensor, device: str) -> torch.Tensor:
        raise NotImplementedError

    def new_perm(self) -> torch.Tensor:
        return torch.randperm(self.n_train)

    def evaluate(self, model, which: str, device: str) -> dict:
        raise NotImplementedError


class Naming(Objective):
    """Name the input: softmax over the task's own words."""

    def __init__(self, task: Task, train: Split, val: Split, batch: int, augment: bool, smoothing: float):
        self.task, self.train, self.val = task, train, val
        self.name = task.name
        self.n_train = len(train)
        self.batch, self.augment = batch, augment
        self.loss_fn = nn.CrossEntropyLoss(label_smoothing=smoothing)

    def batch_loss(self, model, step, perm, device):
        lo = (step * self.batch) % len(perm)
        idx = perm[lo:lo + self.batch]
        x = self.task.prep(self.train.inputs[idx].to(device, non_blocking=True))
        if self.augment:
            x = self.task.augment(x)
        with _autocast(device):
            logits = model(x, self.task.modality)[:, self.task.word_ids.to(device)]
            return self.loss_fn(logits.float(), self.train.labels[idx].to(device))

    @torch.no_grad()
    def logits(self, model, split: Split, device: str, batch: int = 256) -> torch.Tensor:
        model.eval()
        out = []
        for i in range(0, len(split), batch):
            x = self.task.prep(split.inputs[i:i + batch].to(device))
            with _autocast(device):
                out.append(model(x, self.task.modality).float().cpu())
        return torch.cat(out)

    def evaluate(self, model, which, device):
        """top1/top5 among the task's own words, plus top-1 over the whole
        joint vocabulary ("joint_top1")."""
        split = self.val if which == "val" else self.task.test
        logits = self.logits(model, split, device)
        out = topk_accuracy(logits[:, self.task.word_ids], split.labels)
        out["joint_top1"] = topk_accuracy(logits, self.task.word_ids[split.labels], ks=(1,))["top1"]
        rate = getattr(model.workspace, "last_rate", None)
        if rate is not None:
            out["spike_rate"] = float(rate)   # fraction of workspace neurons spiking per step
        return out


class Text(Objective):
    """Predict the next token of the books."""

    def __init__(self, corpus: Corpus, batch: int, ctx: int):
        self.corpus, self.batch, self.ctx = corpus, batch, ctx
        self.name = "text"
        self.n_train = (len(corpus.train) - 1) // ctx      # one epoch = one pass over the text

    def batch_loss(self, model, step, perm, device):
        starts = torch.randint(0, len(self.corpus.train) - self.ctx - 1, (self.batch,))
        win = torch.stack([self.corpus.train[s:s + self.ctx + 1] for s in starts.tolist()]).long().to(device)
        with _autocast(device):
            logits = model.language(win[:, :-1])
            return nn.functional.cross_entropy(logits.flatten(0, 1).float(), win[:, 1:].flatten())

    def evaluate(self, model, which, device, max_tokens: int = 200_000):
        """Perplexity on held-out text: the canonical books ("ppl", comparable
        across runs) and each extra source ("ppl_<source>")."""
        by = self.corpus.val_by_source if which == "val" else self.corpus.test_by_source
        main = self._ppl(model, self.corpus.val if which == "val" else self.corpus.test, device, max_tokens)
        for name, ids in (by or {}).items():
            if name != "canonical":
                main[f"ppl_{name}"] = self._ppl(model, ids, device, max_tokens)["ppl"]
        return main

    @torch.no_grad()
    def _ppl(self, model, ids, device, max_tokens):
        """Perplexity per word over non-overlapping windows: the summed
        log-loss of every token, divided by the number of words (tokens that
        start a word). With a word vocabulary that is the usual per-token
        perplexity; with subwords it stays comparable to it."""
        model.eval()
        starts = self.corpus.vocab.word_starts()
        ids = ids[:max_tokens].long()
        n = (len(ids) - 1) // self.ctx
        x = ids[:n * self.ctx + 1]
        inp = x[:-1].view(n, self.ctx)
        tgt = x[1:].view(n, self.ctx)
        total, tokens, words = 0.0, 0, 0
        for i in range(0, n, 64):
            with _autocast(device):
                logits = model.language(inp[i:i + 64].to(device))
            total += nn.functional.cross_entropy(logits.flatten(0, 1).float(), tgt[i:i + 64].flatten().to(device),
                                                 reduction="sum").item()
            tokens += tgt[i:i + 64].numel()
            words += int(starts[tgt[i:i + 64]].sum())
        out = {"ppl": math.exp(total / words), "nll": total / words}
        if self.corpus.vocab.subword:
            out["ppl_token"] = math.exp(total / tokens)
        return out


class Captions(Objective):
    """Describe the input in words: the input's workspace vector comes first,
    then a sentence naming it. Scored on a phrasing never trained on."""

    def __init__(self, task: Task, train: Split, val: Split, corpus: Corpus, batch: int, augment: bool):
        self.task, self.train, self.val = task, train, val
        self.name = f"captions_{task.name}"
        self.n_train = len(train)
        self.batch, self.augment = batch, augment
        v = corpus.vocab
        self.vocab = v
        self.word_ids_tok = [v.encode(tokenize(w)) for w in task.words]
        self.templates = [[v.encode(tokenize(part)) for part in tpl.split("{w}")]
                          for tpl in TRAIN_TEMPLATES[task.modality]]
        self.prompts = [v.encode(tokenize(p)) for p in EVAL_PROMPTS[task.modality]]
        self.eos = v.stoi[EOS]

    def _sentences(self, labels: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
        """Token ids (padded with <eos>) and a mask of positions that count."""
        seqs = []
        for lab in labels.tolist():
            pre, post = self.templates[torch.randint(len(self.templates), (1,)).item()]
            seqs.append(pre + self.word_ids_tok[lab] + post + [self.eos])
        width = max(len(s) for s in seqs)
        ids = torch.full((len(seqs), width), self.eos, dtype=torch.long)
        mask = torch.zeros(len(seqs), width)
        for i, s in enumerate(seqs):
            ids[i, :len(s)] = torch.tensor(s)
            mask[i, :len(s)] = 1
        return ids, mask

    def batch_loss(self, model, step, perm, device):
        lo = (step * self.batch) % len(perm)
        idx = perm[lo:lo + self.batch]
        x = self.task.prep(self.train.inputs[idx].to(device, non_blocking=True))
        if self.augment:
            x = self.task.augment(x)
        ids, mask = self._sentences(self.train.labels[idx])
        ids, mask = ids.to(device), mask.to(device)
        with _autocast(device):
            z = model.encode(x, self.task.modality)
            logits = model.language(ids, prefix=z)[:, :-1]          # position i predicts ids[:, i]
            nll = nn.functional.cross_entropy(logits.flatten(0, 1).float(), ids.flatten(), reduction="none")
            return (nll * mask.flatten()).sum() / mask.sum()

    @torch.no_grad()
    def word_scores(self, model, split: Split, device: str, blind: bool = False,
                    budget: float = 1.0e8, prompt: list[int] | None = None) -> torch.Tensor:
        """log p(word | input, prompt) for every one of the task's words
        (prompt: token ids, default the first held-out prompt).
        blind=True replaces the input with a zero vector (the language prior)."""
        model.eval()
        n_words = len(self.word_ids_tok)
        width = max(len(w) for w in self.word_ids_tok)
        cand = torch.full((n_words, width), self.eos, dtype=torch.long)
        cmask = torch.zeros(n_words, width)
        for i, w in enumerate(self.word_ids_tok):
            cand[i, :len(w)] = torch.tensor(w)
            cmask[i, :len(w)] = 1
        prompt = torch.tensor(self.prompts[0] if prompt is None else prompt, dtype=torch.long)
        p = len(prompt)
        seqs = torch.cat([prompt.expand(n_words, p), cand], 1).to(device)      # W x (P + width)
        cmask = cmask.to(device)
        # Each item scores every word over the full vocabulary: keep the
        # logits of one chunk under `budget` floats.
        batch = max(1, int(budget // (n_words * seqs.shape[1] * len(self.vocab))))
        out = []
        for i in range(0, len(split), batch):
            x = self.task.prep(split.inputs[i:i + batch].to(device))
            with _autocast(device):
                z = model.encode(x, self.task.modality)
            if blind:
                z = torch.zeros_like(z)
            b = len(z)
            zz = z.repeat_interleave(n_words, 0)                                # (b*W) x dim
            ss = seqs.repeat(b, 1)
            with _autocast(device):
                logits = model.language(ss, prefix=zz)[:, :-1].float()          # predicts ss
            lp = logits.log_softmax(-1).gather(2, ss[..., None])[..., 0]        # (b*W) x L
            lp = (lp[:, p:] * cmask.repeat(b, 1)).sum(1).view(b, n_words)
            out.append(lp.cpu())
        return torch.cat(out)

    def evaluate(self, model, which, device, limit: int = 2000):
        split = self.val if which == "val" else self.task.test
        g = torch.Generator().manual_seed(0)
        idx = torch.randperm(len(split), generator=g)[:limit]
        sub = split.subset(idx)
        seen = [topk_accuracy(self.word_scores(model, sub, device, prompt=p), sub.labels) for p in self.prompts]
        # With the input blanked out the answer is the same for every item:
        # its accuracy is how often that word is the label.
        one = sub.subset(torch.arange(1))
        blind = [(sub.labels == self.word_scores(model, one, device, blind=True, prompt=p)[0].argmax()).float()
                 .mean().item() for p in self.prompts]
        top1 = [r["top1"] for r in seen]
        return {"top1": sum(top1) / len(top1), "worst_top1": min(top1), "first_top1": top1[0],
                "top5": sum(r["top5"] for r in seen) / len(seen), "blind_top1": max(blind),
                "chance": 1 / len(self.word_ids_tok)}


class Prediction(Objective):
    """Predict the next video frame from the last few. Scored per source as
    mean squared error against two baselines: copying the last frame, and
    continuing the last change linearly. skill = 1 - mse / copy_mse (above 0
    only by predicting motion)."""

    def __init__(self, video, batch: int, ctx: int = 4):
        from .data.video import windows

        self.video, self.batch, self.ctx = video, batch, ctx
        self.name = "video"
        self.wins = {src: windows(c, ctx) for src, c in video.train.items()}
        self.order = [(src, i) for src, w in self.wins.items() for i in range(len(w))]
        self.n_train = len(self.order)

    def batch_loss(self, model, step, perm, device):
        from .data.video import gather

        lo = (step * self.batch) % len(perm)
        picks = perm[lo:lo + self.batch].tolist()
        loss, n = 0.0, 0
        for src in self.wins:
            rows = [self.order[p][1] for p in picks if self.order[p][0] == src]
            if not rows:
                continue
            ctx, nxt = gather(self.video.train[src], self.wins[src][rows], self.ctx, device)
            with _autocast(device):
                pred = model.predict_next(ctx)
            loss = loss + ((pred.float() - nxt) ** 2).mean() * len(rows)
            n += len(rows)
        return loss / n

    @torch.no_grad()
    def evaluate(self, model, which, device):
        from .data.video import gather, windows

        model.eval()
        clips = self.video.val if which == "val" else self.video.test
        out = {}
        for src, c in clips.items():
            w = windows(c, self.ctx)
            se = {"mse": 0.0, "copy_mse": 0.0, "linear_mse": 0.0}
            for i in range(0, len(w), 128):
                ctx, nxt = gather(c, w[i:i + 512], self.ctx, device)
                with _autocast(device):
                    pred = model.predict_next(ctx).float()
                se["mse"] += ((pred - nxt) ** 2).mean((1, 2, 3)).sum().item()
                se["copy_mse"] += ((ctx[:, -1] - nxt) ** 2).mean((1, 2, 3)).sum().item()
                se["linear_mse"] += ((2 * ctx[:, -1] - ctx[:, -2] - nxt) ** 2).mean((1, 2, 3)).sum().item()
            r = {k: v / len(w) for k, v in se.items()}
            r["skill"] = 1 - r["mse"] / r["copy_mse"]
            out[src] = r
        out["skill"] = sum(v["skill"] for v in out.values() if isinstance(v, dict)) / len(clips)
        return out
