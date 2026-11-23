import copy

import pytest
import torch

from athena import baselines, scoreboard
from athena.ablate import modules_of, without
from athena.data import build_vocab, load_cifar100, load_speech
from athena.data.cifar import augment as image_augment
from athena.data.speech import augment as audio_augment
from athena.models import Brain, ModelConfig
from athena.train import TrainConfig, crossmodal, train


@pytest.fixture(scope="module")
def cifar():
    return load_cifar100()


@pytest.fixture(scope="module")
def speech():
    return load_speech()


def quiet(*_):
    pass


def test_cifar_shapes(cifar):
    assert cifar.train.inputs.shape == (50000, 3, 32, 32)
    assert cifar.test.inputs.shape == (10000, 3, 32, 32)
    assert cifar.train.inputs.dtype == torch.uint8
    assert len(cifar.words) == 100 and cifar.words[0] == "apple"
    assert "aquarium fish" in cifar.words
    assert torch.bincount(cifar.train.labels).unique().tolist() == [500]


def test_cifar_prep_is_standardised(cifar):
    x = cifar.prep(cifar.train.inputs[:5000])
    assert torch.allclose(x.mean(dim=(0, 2, 3)), torch.zeros(3), atol=0.05)
    assert torch.allclose(x.std(dim=(0, 2, 3)), torch.ones(3), atol=0.05)


def test_speech_splits_are_speaker_disjoint(speech):
    assert speech.train.inputs.shape[1:] == (1, 64, 101)
    assert len(speech.words) == 35 and {"cat", "dog", "bird", "bed", "house"} <= set(speech.words)
    assert (len(speech.train), len(speech.val), len(speech.test)) == (84843, 9981, 11005)
    x = speech.prep(speech.train.inputs[:5000])
    assert abs(x.mean().item()) < 0.2 and 0.7 < x.std().item() < 1.3


def test_image_augment_keeps_shape_and_changes_images(cifar):
    torch.manual_seed(0)
    x = cifar.prep(cifar.train.inputs[:64])
    y = image_augment(x)
    assert y.shape == x.shape and not torch.equal(x, y)
    assert y.abs().max() <= x.abs().max() + 1e-5


def test_audio_augment_keeps_shape_and_changes_spectrograms(speech):
    torch.manual_seed(0)
    x = speech.prep(speech.train.inputs[:64])
    y = audio_augment(x)
    assert y.shape == x.shape and not torch.equal(x, y)
    assert y.min() >= x.amin(dim=(1, 2, 3)).min() - 1e-5


@pytest.mark.parametrize("module", ["residual", "workspace_mlp"])
def test_every_model_module_changes_the_output(module):
    """A switch that changes nothing is a statue."""
    x = torch.randn(4, 3, 32, 32)
    on = ModelConfig(size="tiny", vocab_size=10)
    off = copy.deepcopy(on)
    off.modules[module] = False
    torch.manual_seed(0)
    a = Brain(on).eval()(x)
    torch.manual_seed(0)
    b = Brain(off).eval()(x)
    assert a.shape == b.shape == (4, 10)
    assert not torch.allclose(a, b)


def test_shared_words_switch_gives_each_sense_its_own_table():
    cfg = ModelConfig(size="tiny", vocab_size=10, modalities={"vision": 3, "audio": 1})
    shared = Brain(cfg)
    assert shared.head("vision") is shared.head("audio")
    cfg.modules["shared_words"] = False
    separate = Brain(cfg)
    assert separate.head("vision") is not separate.head("audio")


def test_vocab_merges_words_across_tasks(two_senses):
    vocab = build_vocab(two_senses)
    assert vocab == ["cat", "dog", "bed", "car", "cup", "yes", "no"]
    seen, heard = two_senses
    assert seen.word_ids.tolist() == [0, 1, 2, 3, 4]
    assert heard.word_ids.tolist() == [0, 1, 2, 5, 6]


def test_ablation_covers_every_module(pattern_task, two_senses):
    cfg = TrainConfig(model=ModelConfig(size="tiny"))
    assert set(modules_of(cfg, [pattern_task])) == {"augment", "residual", "workspace_mlp"}
    assert "shared_words" in modules_of(cfg, two_senses)
    assert "motion_context" not in modules_of(cfg, two_senses)
    assert "motion_context" in modules_of(cfg, [], video=object())
    assert without(cfg, "augment").augment is False
    assert without(cfg, "residual").model.modules["residual"] is False
    assert cfg.model.modules["residual"] is True   # the original is untouched


def test_memorises_a_small_set(cifar):
    """The gate nimcp's pipeline never passed: learn 64 items to 100%."""
    cfg = TrainConfig(model=ModelConfig(size="tiny"), epochs=40, batch_size=32, lr=0.05,
                      augment=False, label_smoothing=0.0, limit_train=64, device="cpu")
    out = train(cfg, [cifar], name="memorise", log=quiet)
    assert out["metrics"]["top1"] == 1.0


def test_learns_a_simple_task(pattern_task):
    cfg = TrainConfig(model=ModelConfig(size="tiny"), epochs=8, batch_size=32, lr=0.05,
                      augment=False, val_size=50, device="cpu")
    out = train(cfg, [pattern_task], name="patterns", log=quiet)
    assert out["metrics"]["test"]["seen"]["top1"] > 0.9


def test_two_senses_name_their_words_and_align(two_senses):
    """With one word table, a heard word lands nearest the pictures of the
    same word (and vice versa)."""
    cfg = TrainConfig(model=ModelConfig(size="tiny"), epochs=30, batch_size=32, lr=0.05,
                      augment=False, val_size=50, device="cpu")
    m = train(cfg, two_senses, name="senses", log=quiet)["metrics"]
    assert m["test"]["seen"]["top1"] > 0.9 and m["test"]["heard"]["top1"] > 0.9
    cm = m["crossmodal"]
    assert cm["words"] == ["bed", "cat", "dog"]
    assert cm["audio_to_vision"] > 0.9 and cm["vision_to_audio"] > 0.9


def test_crossmodal_needs_two_senses(pattern_task):
    model = Brain(ModelConfig(size="tiny", vocab_size=5))
    assert crossmodal(model, [pattern_task], "cpu") is None


def test_baselines_on_easy_task(pattern_task):
    assert baselines.chance(pattern_task)["top1"] == pytest.approx(0.2)
    assert baselines.ridge(pattern_task)["top1"] > 0.95
    assert baselines.knn(pattern_task, k=1)["top1"] > 0.95


def test_scoreboard_round_trip(tmp_scoreboard):
    scoreboard.record("cifar100", "baseline", "a", {"top1": 0.1})
    scoreboard.record("cifar100", "baseline", "b", {"top1": 0.3})
    scoreboard.record("cifar100", "train", "c", {"top1": 0.9})
    scoreboard.record("speech", "baseline", "d", {"top1": 0.99})
    assert len(scoreboard.load(task="cifar100")) == 3
    assert scoreboard.best_baseline("cifar100") == ("b", 0.3)


def test_spiking_workspace_is_a_real_swap_and_trains(pattern_task):
    """The candidate replaces the workspace, spikes sparsely, passes
    gradients through its surrogate, and can learn a simple task."""
    from athena.models.spiking import SpikingWorkspace

    cfg = ModelConfig(size="tiny", vocab_size=5, modules={"spiking_workspace": True})
    b = Brain(cfg)
    assert isinstance(b.workspace, SpikingWorkspace)
    x = torch.randn(8, 3, 32, 32, requires_grad=True)
    b(x).sum().backward()
    assert b.workspace.inp.weight.grad.abs().sum() > 0
    assert 0.0 < float(b.workspace.last_rate) < 1.0

    tc = TrainConfig(model=ModelConfig(size="tiny", modules={"spiking_workspace": True}), epochs=10,
                     batch_size=32, lr=0.05, augment=False, val_size=50, device="cpu")
    m = train(tc, [pattern_task], name="spiking", log=quiet)["metrics"]
    assert m["test"]["seen"]["top1"] > 0.9
    assert 0.0 < m["test"]["seen"]["spike_rate"] < 1.0

