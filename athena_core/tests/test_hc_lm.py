"""Hippocampal next-word memory: one exposure is recalled, the same context
gives the same code, word order is kept, and a deterministic stream becomes
near certain."""
import torch

from athena.bio.childes import perplexity
from athena.bio.hc_lm import HcLM, HcLMConfig


def _cfg(**kw):
    return HcLMConfig(n_dg=2000, k=5, **kw)


def test_same_context_same_code():
    m = HcLM(10, _cfg())
    ids = torch.tensor([1, 2, 3, 1, 2, 3, 4, 1, 2, 3])
    for l in range(m.L):
        c = m._codes(ids, l)
        if m.prof[l] == (1.0,):                     # current word only
            assert set(c[1].tolist()) == set(c[4].tolist())
    assert not torch.equal(m._codes(ids, 1)[2].sort().values, m._codes(ids, 1)[6].sort().values)


def test_one_exposure_is_recalled():
    m = HcLM(6, _cfg())
    m.store(torch.tensor([0, 1, 2, 3, 4, 5]))       # each transition seen once
    probe = torch.tensor([0, 1, 2, 3, 4, 5])
    s_seen, n = m.nll(probe)
    fresh = HcLM(6, _cfg())
    s_fresh, _ = fresh.nll(probe)
    assert s_seen < s_fresh                          # one exposure already helps


def test_deterministic_cycle_becomes_near_certain():
    ids = torch.tensor([0, 1, 2, 3, 4] * 60)
    m = HcLM(5, _cfg(alpha=0.5))
    m.store(ids)
    m.tune(ids)
    assert perplexity(*m.nll(ids)) < 1.3


def test_word_order_matters():
    """After 0 comes 1 when 3 preceded it and 2 when 4 did: only the traces
    of the earlier word can tell, so the decayed layers must carry order."""
    ids = torch.tensor([3, 0, 1, 4, 0, 2] * 80)
    m = HcLM(5, _cfg(alpha=0.5, profiles=((1.0,), (1.0, 1.0)), combine="and"))
    m.store(ids)
    m.tune(ids)
    only_current = HcLM(5, _cfg(alpha=0.5, profiles=((1.0,),)))
    only_current.store(ids)
    only_current.tune(ids)
    # order should remove at least half of the remaining uncertainty (log-perplexity)
    import math
    assert math.log(perplexity(*m.nll(ids))) < 0.5 * math.log(perplexity(*only_current.nll(ids)))


def test_sparse_projection_rejects_unfed_winners():
    import pytest
    with pytest.raises(ValueError):
        HcLM(5, HcLMConfig(n_dg=100, k=20, fan_in=8, projection="sparse"))
