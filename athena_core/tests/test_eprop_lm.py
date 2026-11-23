"""E-prop language model: learns a deterministic word sequence (overfit
test), uses context beyond the current word, the learning signal is not a
statue, and nothing uses autograd. Plus the n-gram baseline and data
helpers."""
import math

import torch

from athena.bio.childes import InterpolatedNgram, Vocab, perplexity
from athena.bio.eprop_lm import EpropLM, EpropLMConfig


def _stream(pattern, reps):
    return torch.tensor(pattern * reps, dtype=torch.long)


def _acc(net, ids, batch=4):
    S = EpropLM.streams(ids, batch)
    st = net.init_state(batch, learn=False)
    hit = n = 0
    for t in range(S.shape[1] - 1):
        logp = net.step_word(st, S[:, t], None, None)
        if t > 8:
            hit += int((logp.argmax(1) == S[:, t + 1]).sum())
            n += batch
    return hit / n


def test_memorises_a_word_cycle():
    ids = _stream([0, 1, 2, 3, 4, 5], 200)
    net = EpropLM(6, EpropLMConfig(n_rec=64, seed=0))
    for _ in range(3):
        net.train_pass(ids, batch=4)
    assert _acc(net, ids) >= 0.95


def test_uses_context_beyond_the_current_word():
    """After 0 comes 1 or 2 depending on the word before; a bigram model is
    at chance on those positions, the recurrent network must use context."""
    ids = _stream([3, 0, 1, 4, 0, 2], 300)
    net = EpropLM(5, EpropLMConfig(n_rec=96, seed=1))
    for _ in range(4):
        net.train_pass(ids, batch=4)
    S = EpropLM.streams(ids, 4)
    st = net.init_state(4, learn=False)
    hit = n = 0
    for t in range(S.shape[1] - 1):
        logp = net.step_word(st, S[:, t], None, None)
        if t > 12 and bool((S[:, t] == 0).all()):
            hit += int((logp.argmax(1) == S[:, t + 1]).sum())
            n += 4
    assert hit / n >= 0.9


def test_learning_signal_trains_the_hidden_weights():
    ids = _stream([0, 1, 2, 3], 50)
    for B_scale, moves in ((0.0, False), (1.0, True)):
        net = EpropLM(4, EpropLMConfig(n_rec=32, reg=0.0, seed=2))
        net.B *= B_scale
        w0 = net.W_rec.clone()
        net.train_pass(ids, batch=4)
        assert bool((net.W_rec - w0).abs().sum() > 0) == moves


def test_adaptive_neurons_run_and_no_autograd():
    ids = _stream([0, 1, 2, 3], 50)
    net = EpropLM(4, EpropLMConfig(n_rec=32, adaptive_frac=0.5, seed=3))
    nll = net.train_pass(ids, batch=4)
    assert math.isfinite(nll)
    for w in (net.W_in, net.W_rec, net.W_out, net.b_out):
        assert not w.requires_grad and w.grad is None


def test_ngram_and_vocab():
    v = Vocab.build("a b a c a b".split(), 3)
    assert v.itos[0] == "a" and v.itos[-1] == "<unk>"
    assert v.encode(["a", "zzz"]).tolist() == [0, v.unk]
    ids = _stream([0, 1, 2], 100)
    g = InterpolatedNgram(ids, 3)
    g.tune(ids)
    assert perplexity(*g.nll(ids)) < 1.2           # a deterministic cycle is almost certain
