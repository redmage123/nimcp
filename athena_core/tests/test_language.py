import pytest
import torch

from athena.baselines import text_baselines
from athena.data import build_vocab
from athena.data.text import Vocab, load_corpus, strip_gutenberg, tokenize
from athena.models import Brain, ModelConfig
from athena.train import TrainConfig, train

BOOK = ("*** START OF THE PROJECT GUTENBERG EBOOK TEST ***\n"
        "{body}\n*** END OF THE PROJECT GUTENBERG EBOOK TEST ***\nlicense text")


def quiet(*_):
    pass


LINES = ["the cat sat on the bed .", "a dog ran to the car .", "i heard you say yes .",
         "she drank from the cup .", "no , said the dog ."]


@pytest.fixture
def tiny_corpus(tmp_path, two_senses):
    """Two short 'books' of repeated sentences, plus every naming word."""
    for i in range(2):
        (tmp_path / f"{i + 1}.txt").write_text(BOOK.format(body=" ".join(LINES * 60)))
    return load_corpus(vocab_size=64, required_words=build_vocab(two_senses), corpus_dir=tmp_path)


@pytest.fixture
def subword_corpus(tmp_path, two_senses, monkeypatch):
    """The same books with a subword vocabulary too small to hold every word."""
    import athena.data.text as text

    monkeypatch.setattr(text, "CACHE_DIR", tmp_path / "cache")
    lines = LINES + ["the sleepy caterpillar sat on the bedstead .", "dogs and cats ran to the cars ."]
    for i in range(2):
        (tmp_path / f"{i + 1}.txt").write_text(BOOK.format(body=" ".join(lines * 60)))
    return load_corpus(vocab_size=100, required_words=build_vocab(two_senses), corpus_dir=tmp_path,
                       subwords=True)


def test_tokenize_and_strip():
    assert tokenize("Look, a CAT! It's here.") == ["look", ",", "a", "cat", "!", "it's", "here", "."]
    assert strip_gutenberg(BOOK.format(body="hello world")).strip() == "hello world"


def test_vocab_keeps_required_words_even_if_rare():
    from collections import Counter
    v = Vocab.build(Counter({"the": 10, "a": 5}), size=7, required=["aquarium fish"])
    assert {"aquarium", "fish", "the"} <= set(v.itos) and "a" not in v.itos   # 4 specials + 2 required + 1
    assert len(Vocab.build(Counter({"the": 10}), size=6, required=["aquarium fish"])) == 6
    assert v.encode(["zebra"]) == [v.stoi["<unk>"]]


def test_subwords_spell_any_word_and_keep_naming_words_whole():
    from collections import Counter
    counts = Counter({"the": 50, "caterpillar": 5, "cater": 3, "pillar": 3, "cats": 9, "cat": 9})
    v = Vocab.learn_subwords(counts, size=110, required=["aquarium fish"])
    assert v.subword and len(v) <= 110        # stops early when nothing is left to merge
    assert v.encode(["aquarium"]) == [v.stoi["aquarium"]] and v.encode(["the"]) == [v.stoi["the"]]
    unk = v.stoi["<unk>"]
    for w in ["caterpillar", "zebra", "qwxz", "it's", "1999"]:
        ids = v.encode([w])
        assert unk not in ids and v.decode(ids) == w, (w, ids)
    sent = tokenize("The zebra's caterpillars, 12 of them!")
    assert v.decode(v.encode(sent)) == " ".join(sent)
    starts = v.word_starts()
    assert int(starts[torch.tensor(v.encode(sent))].sum()) == len(sent)


def test_word_vocab_is_unchanged_by_subword_support():
    v = Vocab(["<pad>", "<unk>", "<bos>", "<eos>", "the", "cat"])
    assert not v.subword and v.encode(["the", "caterpillar"]) == [4, 1]
    assert bool(v.word_starts().all())


def test_corpus_holds_out_the_end_of_every_book(tiny_corpus):
    c = tiny_corpus
    total = len(c.train) + len(c.val) + len(c.test)
    assert 0.85 < len(c.train) / total < 0.95
    assert c.unk_rate("val") == 0.0


def test_language_model_is_causal():
    cfg = ModelConfig(size="tiny", vocab_size=2, n_tokens=30, word_tokens=[[4], [5]])
    lm = Brain(cfg).eval().language
    x = torch.randint(0, 30, (1, 12))
    y = x.clone()
    y[0, 8:] = (y[0, 8:] + 1) % 30
    a, b = lm(x), lm(y)
    assert torch.allclose(a[0, :8], b[0, :8], atol=1e-5)
    assert not torch.allclose(a[0, 8:], b[0, 8:])


def test_one_token_table_for_naming_and_language():
    cfg = ModelConfig(size="tiny", vocab_size=2, n_tokens=30, word_tokens=[[4], [5, 6]])
    b = Brain(cfg)
    assert b.language.table is b.tokens and b.naming.table is b.tokens
    w = b.naming.word_emb
    assert torch.allclose(w[1], (b.tokens.weight[5] + b.tokens.weight[6]) / 2)
    with torch.no_grad():
        b.tokens.weight[6] += 1.0      # changing a token changes the naming word built from it
    assert not torch.allclose(b.naming.word_emb[1], w[1])


def test_subword_corpus_has_no_unknowns_and_learns(subword_corpus):
    c = subword_corpus
    assert c.vocab.subword and c.unk_rate("train") == 0.0 and c.unk_rate("val") == 0.0
    assert len(c.vocab.encode(["caterpillar"])) > 1          # rare: spelled from pieces
    cfg = TrainConfig(model=ModelConfig(size="tiny"), epochs=4, text_batch=16, ctx=32,
                      optimizer="adamw", lr=3e-3, weight_decay=0.01, device="cpu")
    m = train(cfg, [], name="lm", log=quiet, corpus=c)["metrics"]["test"]["text"]
    base = text_baselines(c)
    assert m["ppl"] < base["bigram"]["ppl"] < base["unigram"]["ppl"]
    assert m["ppl_token"] < m["ppl"]                  # more tokens than words


def test_text_baselines_bigram_beats_unigram(tiny_corpus):
    base = text_baselines(tiny_corpus)
    assert base["bigram"]["ppl"] < base["unigram"]["ppl"]


def test_language_model_learns_the_text(tiny_corpus):
    cfg = TrainConfig(model=ModelConfig(size="tiny"), epochs=4, text_batch=16, ctx=32,
                      optimizer="adamw", lr=3e-3, weight_decay=0.01, device="cpu")
    m = train(cfg, [], name="lm", log=quiet, corpus=tiny_corpus)["metrics"]
    bigram = text_baselines(tiny_corpus)["bigram"]["ppl"]
    assert m["test"]["text"]["ppl"] < bigram


def test_captions_come_from_the_input(two_senses, tiny_corpus):
    """Naming in a sentence: with the picture or sound, the right word wins;
    with the input blanked out, the model can only guess."""
    cfg = TrainConfig(model=ModelConfig(size="tiny"), epochs=25, batch_size=32, text_batch=8, ctx=32,
                      optimizer="adamw", lr=3e-3, weight_decay=0.01, augment=False,
                      captions=True, val_size=50, device="cpu")
    m = train(cfg, two_senses, name="captions", log=quiet, corpus=tiny_corpus)["metrics"]
    for task in ("seen", "heard"):
        cap = m["test"][f"captions_{task}"]
        assert cap["top1"] > 0.9, (task, cap)
        assert cap["blind_top1"] < 0.5, (task, cap)


def test_generate_continues_the_learned_text(tiny_corpus):
    from athena.models.language import generate

    cfg = TrainConfig(model=ModelConfig(size="tiny"), epochs=4, text_batch=16, ctx=32,
                      optimizer="adamw", lr=3e-3, weight_decay=0.01, device="cpu")
    model = train(cfg, [], name="lm", log=quiet, corpus=tiny_corpus)["model"]
    v = tiny_corpus.vocab
    out = generate(model.language, torch.tensor(v.encode(tokenize("the cat sat on"))), 2, temperature=0)
    assert v.decode(out) == "the bed"


def test_caption_eval_prompts_are_never_trained_on():
    from athena.objectives import EVAL_PROMPTS, TRAIN_TEMPLATES
    for modality, prompts in EVAL_PROMPTS.items():
        assert len(prompts) >= 3
        for p in prompts:
            for tpl in TRAIN_TEMPLATES[modality]:
                assert p not in tpl.split("{w}")[0], (p, tpl)


def test_video_passes_ration_video_steps_evenly():
    from types import SimpleNamespace
    from athena.train import TrainConfig, _slot
    vid = SimpleNamespace(name="video", n_train=62 * 256 - 10, batch=256)
    other = SimpleNamespace(name="cifar10", n_train=10, batch=256)
    cfg = TrainConfig(video_passes=1.0)
    slots = [_slot(vid, i, 1000, cfg) for i in range(1000)]
    used = [j for j in slots if j is not None]
    assert used == list(range(62))                       # one pass, each batch once, in order
    gaps = [i for i, j in enumerate(slots) if j is not None]
    assert max(b - a for a, b in zip(gaps, gaps[1:])) <= 17   # spread across the epoch
    assert all(_slot(other, i, 1000, cfg) == i for i in range(1000))
    assert all(_slot(vid, i, 1000, TrainConfig()) == i for i in range(1000))
