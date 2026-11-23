"""Spiking learners: each one learns (overfit test), each switch changes the
output (no statues), and nothing uses gradients."""
import torch

from athena.bio.learners import (Hippocampus, RSTDPReadout, STDPCortex, assign_labels,
                                 predict_assigned)
from athena.bio.snn import poisson


def _patterns(n_classes=5, n_in=36, per_class=4, noise=0.1, seed=0):
    g = torch.Generator().manual_seed(seed)
    protos = (torch.rand(n_classes, n_in, generator=g) > 0.6).float()
    y = torch.arange(n_classes).repeat_interleave(per_class)
    X = (protos[y] + noise * torch.rand(len(y), n_in, generator=g)).clamp(0, 1)
    return X, y


def test_poisson_rate():
    s = poisson(torch.tensor([0.0, 0.5, 1.0]), 4000, 0.2, torch.Generator().manual_seed(0))
    rates = s.mean(0)
    assert rates[0] == 0
    assert abs(float(rates[1]) - 0.1) < 0.02 and abs(float(rates[2]) - 0.2) < 0.02


def test_hippocampus_one_shot_memorises():
    X, y = _patterns()
    h = Hippocampus(X.shape[1], 5, n_dg=2000, steps=400, seed=0)
    h.store(h.code(X), y)                                # one exposure each
    assert (h.recall(h.code(X)) == y).float().mean() == 1.0
    empty = Hippocampus(X.shape[1], 5, n_dg=2000, steps=400, seed=0)
    assert not torch.equal(empty.recall(empty.code(X)), h.recall(h.code(X)))   # storing changes recall


def test_stdp_cortex_learns_and_labels():
    X, y = _patterns()
    c = STDPCortex(X.shape[1], n=20, seed=0)
    W0 = c.W.clone()
    c.train(X, epochs=5)
    assert c.updates > 0 and not torch.allclose(W0, c.W)   # plasticity acted
    assert torch.allclose(c.W.sum(0), torch.full((20,), c.w_sum), atol=1e-3)  # synaptic scaling
    R = c.respond(X, steps=200)
    a = assign_labels(R, y, 5)
    assert (predict_assigned(R, a, 5) == y).float().mean() >= 0.9


def test_cortex_winners_switch_changes_code():
    X, _ = _patterns()
    c = STDPCortex(X.shape[1], n=20, seed=0)
    c.train(X, epochs=2)
    one, many = c.respond(X, winners=1), c.respond(X, winners=5)
    assert many.sum() > one.sum()


def test_rstdp_learns_from_reward_only():
    X, y = _patterns(n_classes=3, per_class=3, noise=0.05)
    r = RSTDPReadout(X.shape[1], 3, seed=0)
    before = (r.predict(X) == y).float().mean()
    for _ in range(80):
        for i in torch.randperm(len(X), generator=torch.Generator().manual_seed(1)):
            r.learn(X[i], int(y[i]))
    after = (r.predict(X) == y).float().mean()
    assert after > before and after >= 0.75
