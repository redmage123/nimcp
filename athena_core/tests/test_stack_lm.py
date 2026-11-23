"""Residual stack with local learning: exact where the theory says (one
block, symmetric feedback), learns context with learned feedback, no
autograd."""
import math

import torch

from athena.bio.stack_lm import StackLM, StackLMConfig, StackLMTorch


def test_one_block_symmetric_updates_equal_the_gradient():
    net = StackLM(7, StackLMConfig(d=6, n=10, layers=1, feedback="symmetric", seed=3), dtype=torch.float64)
    net.P["bu0"] += 0.5                               # keep most rates positive
    g = torch.Generator().manual_seed(0)
    S = torch.randint(0, 7, (3, 18), generator=g)
    st = net.init_state(3, learn=True)
    D = net.zero_deltas()
    for t in range(S.shape[1] - 1):
        net.step(st, S[:, t], S[:, t + 1], D)
    m = StackLMTorch(net)
    logits, _ = m(S[:, :-1])
    torch.nn.functional.cross_entropy(logits.reshape(-1, 7), S[:, 1:].reshape(-1), reduction="sum").backward()
    for k in net.P:
        if k == "E":                                  # current-step error only (documented)
            continue
        want = m.P[k].grad
        assert torch.allclose(D[k], want, atol=1e-8), (k, (D[k] - want).abs().max())
        assert want.abs().sum() > 0, k


def _order_task(net, reps=300, passes=6):
    ids = torch.tensor([3, 0, 1, 4, 0, 2] * reps)
    for _ in range(passes):
        net.train_pass(ids, batch=4)
    S = StackLM.streams(ids, 4)
    st = net.init_state(4, learn=False)
    hit = n = 0
    for t in range(S.shape[1] - 1):
        logp = net.step(st, S[:, t])
        if t > 12 and bool((S[:, t] == 0).all()):
            hit += int((logp.argmax(1) == S[:, t + 1]).sum())
            n += 4
    return hit / n


def test_learned_feedback_two_blocks_learn_context():
    assert _order_task(StackLM(5, StackLMConfig(d=16, n=32, layers=2, feedback="kp", seed=1))) >= 0.9


def test_no_autograd():
    net = StackLM(5, StackLMConfig(d=8, n=16, layers=2, seed=2))
    assert math.isfinite(net.train_pass(torch.tensor([0, 1, 2, 3, 4] * 40), batch=4))
    for v in net.P.values():
        assert not v.requires_grad and v.grad is None
