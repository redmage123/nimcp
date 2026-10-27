"""Developmental curriculum stages 4-7 for immerse_athena.py.

    Stage 4  "Why?"          3-5 yr   explanation, questions, narrative, other minds, number
    Stage 5  Literacy        5-7 yr   reading, phonics, writing, retelling, vocabulary
    Stage 6  School          7-12 yr  Socratic predict-before-learn across domains, worksheets,
                                      math and logic, explain-back, research
    Stage 7  Apprenticeship  12+ yr   self-directed (pull-based) mastery, reflection, teach-back,
                                      conversation with the user

Spec: docs/claude/curriculum-stages-4-7.md

Design notes
------------
* One STEP is one EPISODE (a 2-6 turn exchange), not one stimulus.
* Every stage follows the same skeleton (_run_stage): biological clock,
  episode mix, caregiver critic, cognitive injection, corpus drips,
  evaluation, StageGate (health criteria only — loss is never a stage-ending
  criterion), checkpoint cadence. The stage-specific part is the episode
  generator table and the probe set the gate consults.
* The trainer passes itself in as `ia` (the immerse_athena module) so the
  stages reuse its helpers (submit_multimodal, make_semantic_target,
  _save_checkpoint, corpus drips, ...) without a circular import. Tests pass
  a lightweight stand-in.
* Everything that talks to Claude or to optional brain APIs is best-effort:
  offline template pools keep every episode type runnable on the pod where
  the Claude CLI auth is unreliable.
"""
from __future__ import annotations

import itertools
import json
import os
import random
import re
import time
from dataclasses import dataclass, field
from typing import Any, Callable, Optional

import numpy as np

# ---------------------------------------------------------------------------
# Small shared helpers
# ---------------------------------------------------------------------------

_WORD_RE = re.compile(r"[a-z']+")
CAUSAL_CONNECTIVES = {"because", "so", "since", "therefore", "then", "if", "when",
                      "makes", "made", "causes", "caused", "that's why", "cause"}
NUMBER_WORDS = {"zero": 0, "one": 1, "two": 2, "three": 3, "four": 4, "five": 5,
                "six": 6, "seven": 7, "eight": 8, "nine": 9, "ten": 10,
                "eleven": 11, "twelve": 12, "thirteen": 13, "fourteen": 14, "fifteen": 15,
                "sixteen": 16, "seventeen": 17, "eighteen": 18, "nineteen": 19, "twenty": 20}


def _tokens(text: str) -> list[str]:
    return _WORD_RE.findall((text or "").lower())


def _mentions(text: str, *words: str) -> bool:
    toks = set(_tokens(text))
    return any(w.lower() in toks for w in words)


def _numbers_in(text: str) -> set[int]:
    out = set()
    for t in _tokens(text):
        if t in NUMBER_WORDS:
            out.add(NUMBER_WORDS[t])
    for m in re.findall(r"\b\d{1,3}\b", text or ""):
        out.add(int(m))
    return out


def _safe(fn: Callable, *a, default=None, **kw):
    try:
        return fn(*a, **kw)
    except Exception:
        return default


class _Brain:
    """Thin guarded facade over the brain API used by the stages."""

    def __init__(self, brain, ia, composer, decoder, parent):
        self.b = brain
        self.ia = ia
        self.composer = composer
        self.decoder = decoder
        self.parent = parent

    # -- perception / learning ----------------------------------------------
    def present(self, text: str, target_word: Optional[str] = None):
        """Stage the sensory data for `text` (real photo / speech when
        available) and return the composed feature vector."""
        _safe(self.ia.submit_multimodal, self.b, text, target_word=target_word)
        return self.composer.compose(text=text, modality="text")

    def learn(self, trainer, text: str, label: str, confidence: float = 0.5,
              lr: Optional[float] = None, features=None) -> Optional[float]:
        feats = features if features is not None else self.composer.compose(text=text, modality="text")
        target = self.ia.make_semantic_target(text)
        loss = _safe(trainer.learn, feats, target, label=label[:50], confidence=confidence,
                     learning_rate=lr)
        _safe(self.b.learn_language, text)
        return loss

    # -- production -----------------------------------------------------------
    def respond(self, prompt: str) -> tuple[str, float]:
        """Athena's reply to `prompt` via the grounded language path; falls
        back to decoding decide_full output through the decoder."""
        r = _safe(self.b.grounded_respond, prompt)
        if isinstance(r, dict) and r.get("response"):
            return str(r.get("response")), float(r.get("confidence") or 0.0)
        feats = self.composer.compose(text=prompt, modality="text")
        res = _safe(self.b.decide_full, feats, default={}) or {}
        txt = _safe(self.parent.observe_response, res, default="") or ""
        return txt, 0.0

    def reward(self, value: float, confidence: float = 0.5):
        _safe(self.b.bg_update_reward, float(value), float(confidence))

    def critic(self, prompt: str, stage: int):
        return _safe(self.parent.run_critic_eval, self.b, prompt, stage=stage)

    def similarity(self, a: str, b: str) -> float:
        """Cosine similarity of the trainer's text encoder, Jaccard fallback."""
        try:
            va = np.asarray(self.ia.encode_text(a), dtype=np.float32)
            vb = np.asarray(self.ia.encode_text(b), dtype=np.float32)
            na, nb = np.linalg.norm(va), np.linalg.norm(vb)
            if na > 0 and nb > 0:
                return float(va @ vb / (na * nb))
        except Exception:
            pass
        ta, tb = set(_tokens(a)), set(_tokens(b))
        return len(ta & tb) / max(1, len(ta | tb))

    def gaps(self, topic: str) -> dict:
        g = _safe(self.b.curiosity_detect_gaps, topic, default=None)
        return g if isinstance(g, dict) else {}


# ---------------------------------------------------------------------------
# Offline content pools (procedurally varied). Claude enrichment is layered on
# top when parent.teacher is available; these keep every episode runnable.
# ---------------------------------------------------------------------------

NAMES = ["Anna", "Ben", "Maya", "Leo", "Sam", "Zoe", "Omar", "Lily", "Noah", "Ava"]
OBJECTS = ["ball", "book", "cup", "apple", "toy car", "hat", "spoon", "block", "doll", "shoe"]
PLACES = ["box", "basket", "drawer", "bag", "cupboard", "under the bed", "behind the chair"]
ANIMALS = ["dog", "cat", "bird", "horse", "rabbit", "frog", "cow", "fish", "bee", "elephant"]
COLORS = ["red", "blue", "green", "yellow", "orange", "purple"]

# (scene, why, what happens next)
CAUSAL_SCENES = [
    ("The ice cream is melting in the sun", "the sun is hot and heat melts ice cream",
     "it drips and makes a sticky puddle"),
    ("The ball rolled down the hill", "the hill is sloped and gravity pulls the ball down",
     "it stops at the bottom where the ground is flat"),
    ("The plant on the windowsill is drooping", "it has not had water for days",
     "it perks up after it gets a drink of water"),
    ("The baby is crying", "the baby is hungry or tired", "the baby calms down after being fed"),
    ("The puddle on the path is gone", "the sun dried the water up", "the path stays dry until it rains again"),
    ("The cat ran away from the vacuum cleaner", "the noise scared the cat", "the cat comes back when it is quiet"),
    ("The tower of blocks fell over", "it was built too tall and wobbly", "the blocks lie scattered on the floor"),
    ("The window is wet on the inside", "warm breath made water on the cold glass", "it dries when the room warms up"),
    ("The dog is panting after the walk", "running makes the dog hot and panting cools it down",
     "the dog drinks water and lies down"),
    ("The lights went out in the house", "the storm knocked out the power", "we light candles until the power returns"),
    ("The bread has gone hard", "it was left out and the air dried it", "it is thrown away or made into crumbs"),
    ("The leaves are falling off the tree", "it is autumn and the days are getting colder", "the tree is bare all winter"),
    ("The soup is steaming", "it is very hot", "it cools down and the steam stops"),
    ("The kite flew up high", "the wind pushed against it", "it comes down when the wind stops"),
    ("The snowman is smaller today", "the sun is warming the snow and it melts", "it becomes a puddle"),
    ("The flowers opened this morning", "the sunlight warmed them", "bees come to visit them"),
    ("The glass fell and broke", "it was knocked off the edge of the table", "someone sweeps up the pieces"),
    ("The boy is wearing a coat", "it is cold outside", "he takes it off when he comes inside"),
    ("The car would not start", "the battery is flat", "someone brings a new battery"),
    ("The cookies smell good", "they are baking in the oven", "we eat them when they cool"),
    ("The river is very high", "it rained for three days", "it goes down when the rain stops"),
    ("The bird flew away from the cat", "it was scared the cat would catch it", "it comes back when the cat leaves"),
    ("The shadow is long on the ground", "the sun is low in the sky", "it gets shorter at midday"),
    ("The boat is floating on the pond", "it is light and the water holds it up", "it drifts to the other side"),
    ("The candle went out", "the wind blew on the flame", "someone lights it again"),
    ("The paper airplane crashed", "it was thrown too hard and bent", "we fold a new one"),
    ("The milk tastes bad", "it was left out of the fridge too long", "we pour it away"),
    ("The grass is wet in the morning", "dew formed in the cool night", "it dries when the sun comes up"),
    ("The girl is laughing", "her brother told a funny joke", "she tells the joke to her friend"),
    ("The tomatoes turned red", "they ripened in the sun", "we pick them and eat them"),
]

STORIES = [
    ["{n1} found a {obj} in the garden", "{n1} showed it to {n2}", "{n2} wanted to play with it",
     "they played with the {obj} together until dinner"],
    ["A {animal} was lost in the park", "{n1} saw it shivering under a bench", "{n1} brought it some food",
     "the {animal} followed {n1} home"],
    ["It started to rain while {n1} was walking", "{n1} had no umbrella", "{n2} shared an umbrella",
     "they both stayed dry"],
    ["{n1} planted a seed in a pot", "every day {n1} gave it water", "a green shoot came up",
     "a {color} flower opened in the spring"],
    ["{n1} built a tall tower of blocks", "the {animal} bumped the table", "the tower fell down",
     "{n1} built it again, even taller"],
    ["{n1} and {n2} went to the beach", "they found a big shell", "they heard the sea inside it",
     "they took the shell home"],
    ["{n1} could not find the {obj}", "{n2} looked in the {place}", "the {obj} was there all along",
     "{n1} said thank you"],
    ["The {animal} was hungry", "it looked everywhere for food", "{n1} gave it an apple",
     "the {animal} was happy"],
    ["{n1} woke up early", "the sky was still dark", "slowly the sun came up", "the birds began to sing"],
    ["{n1} drew a picture of a {animal}", "{n1} used {color} and {color2} crayons", "{n2} said it looked real",
     "{n1} hung it on the wall"],
]

# Theory of mind scripts. {n1} leaves, {n2} moves the object. The correct
# answer to the false-belief question is where n1 LEFT it (place1).
TOM_SCRIPTS = [
    ("{n1} puts the {obj} in the {place1} and goes outside to play. While {n1} is away, {n2} moves the "
     "{obj} to the {place2}. {n1} comes back. Where will {n1} look for the {obj}?", "place1"),
    ("{n1} hides a {obj} in the {place1}. {n1} leaves the room. {n2} finds the {obj} and puts it in the "
     "{place2}. When {n1} returns, where does {n1} think the {obj} is?", "place1"),
    ("{n1} sees a {obj} in the {place1}. Then {n1} goes to sleep. {n2} takes the {obj} and puts it in the "
     "{place2}. Where will {n1} look first when {n1} wakes up?", "place1"),
    ("{n2} watches {n1} put the {obj} in the {place1}. {n2} then moves it to the {place2} while {n1} "
     "watches. Where will {n1} look for the {obj}?", "place2"),
]

CATEGORIES = {
    "animal": ANIMALS,
    "food": ["apple", "bread", "cheese", "banana", "carrot", "soup", "rice", "egg"],
    "vehicle": ["car", "bus", "train", "bicycle", "boat", "airplane", "truck"],
    "clothing": ["hat", "coat", "shoe", "sock", "shirt", "glove", "scarf"],
    "furniture": ["chair", "table", "bed", "couch", "lamp", "shelf"],
    "plant": ["tree", "flower", "grass", "bush", "mushroom", "rose"],
}

PHONICS_WORDS = ["cat", "dog", "sun", "hat", "cup", "bed", "pig", "box", "map", "red", "fish",
                 "ship", "tree", "frog", "milk", "hand", "jump", "sing", "book", "rain", "boat",
                 "star", "moon", "duck", "bell", "ring", "ball", "cake", "bike", "nest"]

FALLBACK_PASSAGES = [
    "The little fox lived at the edge of the wood. Every morning she went down to the stream to drink. "
    "One day the stream was frozen. The fox tapped the ice with her paw until it cracked, and then she drank.",
    "Tom had a red kite. He took it to the top of the hill where the wind was strong. The kite climbed "
    "higher and higher until it was a small red dot. Then the string broke and the kite sailed away over the town.",
    "Grandmother kept bees at the bottom of the garden. The bees flew to the apple trees and the clover. "
    "In the autumn she opened the hive and took out the honey, and the whole house smelled sweet.",
    "The river was wide and slow. A heron stood in the shallow water, very still, waiting for a fish. "
    "When the fish came close the heron struck, quick as a flash, and swallowed it whole.",
    "Mira built a boat out of a cardboard box. She painted it blue and wrote her name on the side. "
    "In the bath the boat floated for a minute, then the cardboard went soft and the boat sank.",
    "At night the owl hunts. Its eyes are large so it can see in the dark, and its feathers are soft "
    "so it makes no sound. The mice in the field do not hear it coming.",
]

MATH_TEMPLATES = [
    ("If you have {a} {objs} and someone gives you {b} more, how many do you have?", lambda a, b: a + b),
    ("There are {a} {objs} on the table. {b} are taken away. How many are left?", lambda a, b: a - b),
    ("{n1} has {a} {objs} and {n2} has {b}. How many do they have together?", lambda a, b: a + b),
    ("A box holds {a} {objs}. How many {objs} are in {b} boxes?", lambda a, b: a * b),
    ("{n1} had {a} {objs} and ate {b}. How many are left?", lambda a, b: a - b),
]

LOGIC_ITEMS = [
    ("All birds have feathers. A robin is a bird. Does a robin have feathers?", ["yes"]),
    ("If it is raining the ground gets wet. It is raining. Is the ground wet?", ["yes"]),
    ("Every cat is an animal. Rex is a dog. Is Rex a cat?", ["no"]),
    ("Which is bigger, an elephant or a mouse?", ["elephant"]),
    ("Which is heavier, a feather or a stone?", ["stone"]),
    ("If today is Monday, what day is tomorrow?", ["tuesday"]),
    ("A square has four sides. A triangle has three sides. Which has more sides?", ["square"]),
    ("Water freezes when it is very cold. Is ice warm or cold?", ["cold"]),
]

SCHOOL_DOMAINS = ["science", "mathematics", "philosophy", "psychology", "technology",
                  "history", "geography", "biology", "physics", "chemistry", "literature",
                  "music", "art", "ethics", "economics", "astronomy", "ecology", "language",
                  "health", "engineering", "logic", "computing", "society", "nature"]


def _fill(template: str) -> str:
    n1, n2 = random.sample(NAMES, 2)
    c1, c2 = random.sample(COLORS, 2)
    return template.format(n1=n1, n2=n2, obj=random.choice(OBJECTS), animal=random.choice(ANIMALS),
                           place=random.choice(PLACES), color=c1, color2=c2)


# ---------------------------------------------------------------------------
# Episode results
# ---------------------------------------------------------------------------

@dataclass
class Episode:
    kind: str
    prompt: str = ""
    response: str = ""
    target: str = ""
    score: float = 0.0          # 0..1 task score (probe-able)
    loss: Optional[float] = None
    turns: int = 1
    details: dict = field(default_factory=dict)


@dataclass
class StageStats:
    counts: dict = field(default_factory=dict)
    scores: dict = field(default_factory=dict)     # kind -> list of recent scores

    def add(self, ep: Episode):
        self.counts[ep.kind] = self.counts.get(ep.kind, 0) + 1
        self.scores.setdefault(ep.kind, []).append(ep.score)
        if len(self.scores[ep.kind]) > 200:
            self.scores[ep.kind] = self.scores[ep.kind][-200:]

    def mean(self, kind: str, last: int = 100) -> float:
        s = self.scores.get(kind, [])[-last:]
        return float(np.mean(s)) if s else 0.0

    def summary(self) -> str:
        return " | ".join(f"{k}:{self.counts[k]} ({self.mean(k):.2f})" for k in sorted(self.counts))


# ---------------------------------------------------------------------------
# Stage 4 episodes — "Why?"
# ---------------------------------------------------------------------------

def ep_causal(B: _Brain, trainer, stage: int, lr: float) -> Episode:
    scene, why, nxt = random.choice(CAUSAL_SCENES)
    feats = B.present(scene)
    B.learn(trainer, scene, label="s4_scene", confidence=0.6, lr=lr, features=feats)
    q = f"{scene}. Why?"
    resp, conf = B.respond(q)
    connective = any(c in (resp or "").lower() for c in CAUSAL_CONNECTIVES)
    sim = B.similarity(resp, why)
    score = min(1.0, 0.5 * sim + (0.5 if connective else 0.0))
    B.critic(q, stage)
    recast = f"{scene} because {why}."
    loss = B.learn(trainer, recast, label="s4_why", confidence=0.7, lr=lr)
    B.reward(0.3 + 0.6 * score, 0.6)
    q2 = f"{scene}. What happens next?"
    resp2, _ = B.respond(q2)
    sim2 = B.similarity(resp2, nxt)
    B.learn(trainer, f"After that, {nxt}.", label="s4_next", confidence=0.6, lr=lr)
    B.reward(0.3 + 0.6 * sim2, 0.5)
    return Episode("causal", q, resp, why, score=(score + sim2) / 2, loss=loss, turns=4,
                   details={"connective": connective, "sim_why": sim, "sim_next": sim2})


def ep_athena_asks(B: _Brain, trainer, stage: int, lr: float, source) -> Episode:
    topic = random.choice(["animals", "weather", "plants", "people", "machines", "the sky", "water", "food"])
    g = B.gaps(topic)
    questions = g.get("questions") or []
    q = None
    if questions:
        q = str(random.choice(questions))
    if not q:
        # Athena generates the question herself
        q, _ = B.respond(f"Ask me a question about {topic}.")
    asked = bool(q) and ("?" in q or _mentions(q, "why", "what", "how", "where", "who", "when"))
    if not asked:
        q = f"Why do we see {topic} every day?"
    # Parent answers from the fact pool (preferring the topic) or Claude.
    fact, _dom = _safe(source.get_fact, default=("Everything we see has a reason we can find out.", "general")) \
        or ("Everything we see has a reason we can find out.", "general")
    answer = fact
    teacher = getattr(B.parent, "teacher", None)
    if teacher is not None:
        turn = _safe(teacher.generate_dialogue_turn, f"Child asks: {q}", default=None)
        if isinstance(turn, tuple) and turn and isinstance(turn[0], str) and len(turn[0]) > 10:
            answer = turn[0]
    B.learn(trainer, f"{q} {answer}", label="s4_ask", confidence=0.7, lr=lr)
    B.reward(0.8 if asked else 0.3, 0.6)
    return Episode("asks", q, answer, "", score=1.0 if asked else 0.0, turns=2,
                   details={"topic": topic, "from_curiosity": bool(questions)})


def ep_narrative(B: _Brain, trainer, stage: int, lr: float) -> Episode:
    story = [_fill(s) for s in random.choice(STORIES)]
    for s in story:
        feats = B.present(s)
        B.learn(trainer, s, label="s4_story", confidence=0.6, lr=lr, features=feats)
    q = "What happened first?"
    r1, _ = B.respond(q)
    q3 = "What happened last?"
    r3, _ = B.respond(q3)
    s_first = B.similarity(r1, story[0])
    s_last = B.similarity(r3, story[-1])
    retell, _ = B.respond("Tell me the whole story.")
    s_retell = np.mean([B.similarity(retell, s) for s in story]) if retell else 0.0
    B.critic("Tell me the whole story.", stage)
    full = " ".join(s + "." for s in story)
    loss = B.learn(trainer, full, label="s4_retell", confidence=0.7, lr=lr)
    score = float((s_first + s_last + s_retell) / 3)
    B.reward(0.3 + 0.6 * score, 0.5)
    return Episode("narrative", q, r1, story[0], score=score, loss=loss, turns=5,
                   details={"first": s_first, "last": s_last, "retell": s_retell})


def ep_other_minds(B: _Brain, trainer, stage: int, lr: float) -> Episode:
    tmpl, key = random.choice(TOM_SCRIPTS)
    n1, n2 = random.sample(NAMES, 2)
    p1, p2 = random.sample(PLACES, 2)
    obj = random.choice(OBJECTS)
    script = tmpl.format(n1=n1, n2=n2, obj=obj, place1=p1, place2=p2)
    correct = p1 if key == "place1" else p2
    wrong = p2 if key == "place1" else p1
    B.present(script)
    resp, _ = B.respond(script)
    ok = _mentions(resp, *_tokens(correct)) and not (_mentions(resp, *_tokens(wrong)) and not _mentions(resp, *_tokens(correct)))
    explanation = (f"{n1} will look in the {correct}, because {n1} did not see the {obj} being moved."
                   if key == "place1" else f"{n1} will look in the {correct}, because {n1} saw it being moved.")
    loss = B.learn(trainer, f"{script} {explanation}", label="s4_tom", confidence=0.7, lr=lr)
    B.reward(0.9 if ok else 0.2, 0.6)
    return Episode("other_minds", script, resp, correct, score=1.0 if ok else 0.0, loss=loss, turns=2)


def ep_number_category(B: _Brain, trainer, stage: int, lr: float) -> Episode:
    if random.random() < 0.5:
        n = random.randint(1, 6)
        thing = random.choice(ANIMALS + OBJECTS)
        scene = f"{['one','two','three','four','five','six'][n-1]} {thing}s on the grass"
        B.present(scene)
        resp, _ = B.respond(f"How many {thing}s are there?")
        ok = n in _numbers_in(resp)
        loss = B.learn(trainer, f"There are {n} {thing}s. {scene}.", label="s4_count", confidence=0.7, lr=lr)
        B.reward(0.9 if ok else 0.2, 0.6)
        return Episode("number", scene, resp, str(n), score=1.0 if ok else 0.0, loss=loss, turns=2)
    cat, members = random.choice(list(CATEGORIES.items()))
    other_cat = random.choice([c for c in CATEGORIES if c != cat])
    items = random.sample(members, 3) + [random.choice(CATEGORIES[other_cat])]
    random.shuffle(items)
    odd = [x for x in items if x not in members][0]
    q = f"Which one does not belong: {', '.join(items)}?"
    resp, _ = B.respond(q)
    ok = _mentions(resp, *_tokens(odd))
    loss = B.learn(trainer, f"A {items[0]} is a {cat}. The {odd} does not belong because it is not a {cat}.",
                   label="s4_category", confidence=0.7, lr=lr)
    B.reward(0.9 if ok else 0.2, 0.6)
    return Episode("category", q, resp, odd, score=1.0 if ok else 0.0, loss=loss, turns=2)


# ---------------------------------------------------------------------------
# Stage 5 episodes — Literacy
# ---------------------------------------------------------------------------

def _passages(ia) -> list[str]:
    """Passages from the canonical corpus when present, else fallbacks."""
    root = None
    cfg = getattr(ia, "_CANONICAL_CFG", None)
    if cfg and cfg.get("root") and os.path.isdir(cfg["root"]):
        root = cfg["root"]
    if root:
        try:
            files = [os.path.join(root, f) for f in os.listdir(root) if f.endswith(".txt")]
            if files:
                with open(random.choice(files), encoding="utf-8", errors="ignore") as fh:
                    txt = fh.read()
                if len(txt) > 600:
                    start = random.randint(0, max(0, len(txt) - 600))
                    chunk = txt[start:start + 600]
                    chunk = chunk[chunk.find(". ") + 2:] if ". " in chunk[:200] else chunk
                    return [chunk.strip()]
        except Exception:
            pass
    return FALLBACK_PASSAGES


def ep_read_aloud(B: _Brain, trainer, stage: int, lr: float, ia) -> Episode:
    passage = random.choice(_passages(ia))
    sents = [s.strip() for s in re.split(r"(?<=[.!?])\s+", passage) if s.strip()]
    for s in sents[:6]:
        B.present(s)
        B.learn(trainer, s, label="s5_read", confidence=0.6, lr=lr)
    # comprehension question: who/what from the first sentence
    first = sents[0] if sents else passage
    q = f"In the story, {first.split()[0] if first else 'who'}... what did they do?"
    resp, _ = B.respond(f"{passage} Question: {q}")
    sim = B.similarity(resp, first)
    B.critic(q, stage)
    B.reward(0.3 + 0.6 * sim, 0.5)
    return Episode("read", q, resp, first, score=float(sim), turns=len(sents[:6]) + 1)


def ep_phonics(B: _Brain, trainer, stage: int, lr: float) -> Episode:
    word = random.choice(PHONICS_WORDS)
    spelled = " ".join(word)
    B.present(f"the word {word}", target_word=word)
    resp, _ = B.respond(f"Spell the word {word}.")
    letters = [t for t in _tokens(resp) if len(t) == 1]
    ok = "".join(letters) == word or _mentions(resp, word)
    loss = B.learn(trainer, f"The word {word} is spelled {spelled}. {word}.", label="s5_phonics", confidence=0.7, lr=lr)
    resp2, _ = B.respond(f"Say the word that is spelled {spelled}.")
    ok2 = _mentions(resp2, word)
    B.reward(0.9 if (ok or ok2) else 0.2, 0.6)
    return Episode("phonics", word, resp, spelled, score=float(ok or ok2), loss=loss, turns=2)


def ep_write(B: _Brain, trainer, stage: int, lr: float) -> Episode:
    subj = random.choice(ANIMALS + OBJECTS)
    scene = f"a {random.choice(COLORS)} {subj} in the garden"
    B.present(scene)
    resp, _ = B.respond(f"Write one sentence about the {subj} you see.")
    toks = _tokens(resp)
    has_subject = _mentions(resp, subj)
    long_enough = len(toks) >= 5
    v = B.critic(f"Write one sentence about the {subj}.", stage)
    score = (0.5 if has_subject else 0.0) + (0.5 if long_enough else 0.0)
    model = f"The {subj} is sitting in the garden under the warm sun."
    loss = B.learn(trainer, model, label="s5_write", confidence=0.6, lr=lr)
    B.reward(0.3 + 0.6 * score, 0.5)
    return Episode("write", scene, resp, model, score=score, loss=loss, turns=2,
                   details={"words": len(toks), "critic": getattr(v, "score", None)})


def ep_retell_passage(B: _Brain, trainer, stage: int, lr: float, ia) -> Episode:
    passage = random.choice(_passages(ia))
    B.learn(trainer, passage, label="s5_passage", confidence=0.6, lr=lr)
    resp, _ = B.respond(f"{passage} Now tell me that story in your own words.")
    sim = B.similarity(resp, passage)
    B.critic("Tell me the story in your own words.", stage)
    B.reward(0.3 + 0.6 * sim, 0.5)
    return Episode("retell", "retell", resp, passage[:60], score=float(sim), turns=2)


def ep_vocabulary(B: _Brain, trainer, stage: int, lr: float, source) -> Episode:
    fact, dom = _safe(source.get_fact, default=("A river is water that flows across the land.", "nature")) \
        or ("A river is water that flows across the land.", "nature")
    content = [t for t in _tokens(fact) if len(t) > 5]
    word = random.choice(content) if content else "river"
    resp, _ = B.respond(f"{fact} What does the word {word} mean?")
    sim = B.similarity(resp, fact)
    loss = B.learn(trainer, fact, label=f"s5_vocab_{dom}", confidence=0.6, lr=lr)
    B.reward(0.3 + 0.6 * sim, 0.5)
    return Episode("vocabulary", word, resp, fact, score=float(sim), loss=loss, turns=2)


# ---------------------------------------------------------------------------
# Stage 6 episodes — School
# ---------------------------------------------------------------------------

def ep_socratic(B: _Brain, trainer, stage: int, lr: float, source, mastery, domains) -> Episode:
    domain = _safe(mastery.select_domain, domains, default=random.choice(domains)) or random.choice(domains)
    topic_text, dname = _safe(source.get_advanced, domain, default=(None, domain)) or (None, domain)
    if not topic_text:
        topic_text, dname = _safe(source.get_fact, default=("The Earth goes around the Sun once a year.", "science")) \
            or ("The Earth goes around the Sun once a year.", "science")
    # predict-before-learn: ask first, then reveal
    q = f"What do you know about this: {topic_text[:80]}?"
    resp, conf = B.respond(q)
    sim = B.similarity(resp, topic_text)
    loss = B.learn(trainer, topic_text, label=f"s6_{dname}", confidence=0.6 + 0.3 * sim, lr=lr)
    _safe(mastery.record, dname, loss if loss is not None else 1.0, sim)
    B.reward(0.3 + 0.6 * sim, 0.6)
    return Episode("socratic", q, resp, topic_text, score=float(sim), loss=loss, turns=2,
                   details={"domain": dname})


def ep_worksheet(B: _Brain, trainer, stage: int, lr: float, source, mastery, domains) -> Episode:
    """Active-learning ladder keyed to domain mastery: worksheet (<0.4) →
    explain (0.4-0.6) → research (0.6-0.8) → create (>0.8)."""
    domain = random.choice(domains)
    m = float(_safe(mastery.get_mastery, domain, default=0.0) or 0.0)
    topic_text, dname = _safe(source.get_advanced, domain, default=(None, domain)) or (None, domain)
    if not topic_text:
        topic_text = f"Something important about {domain}."
    if m < 0.4:
        q, kind = f"Fill in the idea: {topic_text[:60]} ... ?", "worksheet"
    elif m < 0.6:
        q, kind = f"Explain in your own words: {topic_text[:80]}", "explain"
    elif m < 0.8:
        q, kind = f"What else would you want to find out about this: {topic_text[:80]}?", "research"
    else:
        q, kind = f"Make up a new example that shows this idea: {topic_text[:80]}", "create"
    resp, _ = B.respond(q)
    sim = B.similarity(resp, topic_text)
    score = sim if kind in ("worksheet", "explain") else min(1.0, 0.5 + 0.5 * (len(_tokens(resp)) >= 8))
    loss = B.learn(trainer, topic_text, label=f"s6_{kind}_{dname}", confidence=0.6, lr=lr)
    _safe(mastery.record, dname, loss if loss is not None else 1.0, score)
    B.reward(0.3 + 0.6 * score, 0.5)
    return Episode("worksheet", q, resp, topic_text, score=float(score), loss=loss, turns=2,
                   details={"ladder": kind, "mastery": m})


def ep_math_logic(B: _Brain, trainer, stage: int, lr: float) -> Episode:
    if random.random() < 0.6:
        tmpl, fn = random.choice(MATH_TEMPLATES)
        a, b = random.randint(2, 12), random.randint(1, 9)
        if fn(a, b) < 0:
            a, b = b, a
        objs = random.choice(["apples", "stones", "coins", "cups", "cards"])
        n1, n2 = random.sample(NAMES, 2)
        q = tmpl.format(a=a, b=b, objs=objs, n1=n1, n2=n2)
        ans = fn(a, b)
        resp, _ = B.respond(q)
        ok = ans in _numbers_in(resp)
        loss = B.learn(trainer, f"{q} The answer is {ans}.", label="s6_math", confidence=0.7, lr=lr)
        B.reward(0.9 if ok else 0.2, 0.6)
        return Episode("math", q, resp, str(ans), score=float(ok), loss=loss, turns=2)
    q, answers = random.choice(LOGIC_ITEMS)
    resp, _ = B.respond(q)
    ok = _mentions(resp, *answers)
    loss = B.learn(trainer, f"{q} {answers[0].capitalize()}.", label="s6_logic", confidence=0.7, lr=lr)
    B.reward(0.9 if ok else 0.2, 0.6)
    return Episode("logic", q, resp, answers[0], score=float(ok), loss=loss, turns=2)


def ep_explain_back(B: _Brain, trainer, stage: int, lr: float, source) -> Episode:
    fact, dom = _safe(source.get_fact, default=("Plants make food from sunlight.", "science")) \
        or ("Plants make food from sunlight.", "science")
    B.learn(trainer, fact, label=f"s6_lesson_{dom}", confidence=0.6, lr=lr)
    resp, _ = B.respond(f"I told you: {fact} Now explain it back to me as if I were younger than you.")
    score = B.similarity(resp, fact)
    teacher = getattr(B.parent, "teacher", None)
    if teacher is not None:
        ev = _safe(teacher.evaluate_response, fact, resp, default=None)
        if isinstance(ev, tuple) and ev and isinstance(ev[0], (int, float)):
            score = float(max(0.0, min(1.0, ev[0])))
    B.critic("Explain it back to me.", stage)
    B.reward(0.3 + 0.6 * score, 0.5)
    return Episode("explain_back", fact, resp, fact, score=float(score), turns=2)


def ep_research(B: _Brain, trainer, stage: int, lr: float, source, ia) -> Episode:
    """Go and find out: curiosity gap → corpus/stream material → learn → self-test."""
    g = B.gaps("general")
    topic = str(g.get("suggested_topic") or random.choice(SCHOOL_DOMAINS))
    # Material: streaming corpus when configured, else advanced topics.
    material = None
    cfg = getattr(ia, "_STREAMING_CFG", None)
    if cfg and cfg.get("enabled") and cfg.get("kinds"):
        _safe(ia._ingest_streaming_corpus, B.b, stage=stage, kind=random.choice(cfg["kinds"]),
              chunk_chars=cfg.get("chunk_chars", 1200), max_chunks_per_call=3,
              checkpoint_dir=getattr(ia, "CHECKPOINT_DIR", "checkpoints/athena"))
    tt, dname = _safe(source.get_advanced, None, default=(None, topic)) or (None, topic)
    material = tt or f"Let's learn about {topic}."
    loss = B.learn(trainer, material, label=f"s6_research_{dname}", confidence=0.5, lr=lr)
    resp, _ = B.respond(f"What did you just learn about {dname}?")
    sim = B.similarity(resp, material)
    B.reward(0.3 + 0.6 * sim, 0.5)
    return Episode("research", topic, resp, material[:60], score=float(sim), loss=loss, turns=3,
                   details={"domain": dname})


# ---------------------------------------------------------------------------
# Stage 7 episodes — Apprenticeship (self-directed)
# ---------------------------------------------------------------------------

INBOX_FILE = "checkpoints/athena/conversation_inbox.jsonl"   # user → Athena, consumed as episodes
TRANSCRIPT_FILE = "checkpoints/athena/conversation_transcript.jsonl"


def ep_self_directed(B: _Brain, trainer, stage: int, lr: float, source, ia, mastery) -> Episode:
    """Athena picks the topic (curiosity gaps ranked by novelty), studies it,
    tests herself, and the result feeds mastery."""
    candidates = []
    for t in random.sample(SCHOOL_DOMAINS, 4):
        g = B.gaps(t)
        candidates.append((float(g.get("novelty_score") or g.get("gap_size") or random.random()), t, g))
    candidates.sort(reverse=True)
    novelty, topic, g = candidates[0]
    ep = ep_research(B, trainer, stage, lr, source, ia)
    ep.kind = "self_directed"
    ep.details.update({"chosen_topic": topic, "novelty": novelty})
    _safe(mastery.record, topic, ep.loss if ep.loss is not None else 1.0, ep.score)
    return ep


def ep_reflect(B: _Brain, trainer, stage: int, lr: float) -> Episode:
    q = random.choice(["What did you learn today that surprised you?",
                       "What do you still not understand?",
                       "What would you like to learn next, and why?",
                       "Tell me something you believe and why you believe it."])
    resp, conf = B.respond(q)
    reasoning = _safe(B.b.ti_reason, q, default=None)
    toks = _tokens(resp)
    score = min(1.0, len(toks) / 15.0) * (1.0 if any(c in (resp or "").lower() for c in CAUSAL_CONNECTIVES) else 0.6)
    B.critic(q, stage)
    B.reward(0.3 + 0.6 * score, 0.4)
    return Episode("reflect", q, resp, "", score=float(score), turns=1,
                   details={"reasoning": bool(reasoning)})


def ep_teach_back(B: _Brain, trainer, stage: int, lr: float, source) -> Episode:
    fact, dom = _safe(source.get_fact, default=("The heart pumps blood around the body.", "biology")) \
        or ("The heart pumps blood around the body.", "biology")
    resp, _ = B.respond(f"Teach me about this in your own words: {fact}")
    score = B.similarity(resp, fact)
    teacher = getattr(B.parent, "teacher", None)
    if teacher is not None:
        ev = _safe(teacher.evaluate_response, fact, resp, default=None)
        if isinstance(ev, tuple) and ev and isinstance(ev[0], (int, float)):
            score = float(max(0.0, min(1.0, ev[0])))
    B.critic("Teach me.", stage)
    B.reward(0.3 + 0.6 * score, 0.5)
    return Episode("teach_back", fact, resp, fact, score=float(score), turns=2)


def ep_conversation(B: _Brain, trainer, stage: int, lr: float, ia) -> Optional[Episode]:
    """Consume one user message from the inbox (if any) as a dialogue
    episode; append the exchange to the transcript. Returns None when the
    inbox is empty so the caller picks another episode type."""
    path = os.path.join(os.getcwd(), INBOX_FILE)
    if not os.path.exists(path) or os.path.getsize(path) == 0:
        return None
    try:
        with open(path, "r+", encoding="utf-8") as fh:
            lines = fh.readlines()
            fh.seek(0); fh.truncate()
            fh.writelines(lines[1:])
        msg = json.loads(lines[0]).get("user", "") if lines else ""
    except Exception:
        return None
    if not msg:
        return None
    feats = B.present(msg)
    resp, conf = B.respond(msg)
    B.critic(msg, stage)
    B.learn(trainer, msg, label="s7_user", confidence=0.7, lr=lr, features=feats)
    try:
        with open(os.path.join(os.getcwd(), TRANSCRIPT_FILE), "a", encoding="utf-8") as fh:
            fh.write(json.dumps({"t": time.time(), "user": msg, "athena": resp, "conf": conf}) + "\n")
    except Exception:
        pass
    return Episode("conversation", msg, resp, "", score=min(1.0, len(_tokens(resp)) / 8.0), turns=2)


# ---------------------------------------------------------------------------
# Probe sets: stage-specific capability metrics the gate consults in addition
# to the StageGate health criteria. Each returns (metric_name, value in 0..1).
# ---------------------------------------------------------------------------

def probe_stage4(B: _Brain) -> dict[str, float]:
    tom = []
    for _ in range(6):
        tmpl, key = random.choice(TOM_SCRIPTS[:3])
        n1, n2 = random.sample(NAMES, 2); p1, p2 = random.sample(PLACES, 2)
        s = tmpl.format(n1=n1, n2=n2, obj=random.choice(OBJECTS), place1=p1, place2=p2)
        resp, _ = B.respond(s)
        tom.append(float(_mentions(resp, *_tokens(p1)) and not _mentions(resp, *_tokens(p2))))
    why = []
    for scene, w, _ in random.sample(CAUSAL_SCENES, 6):
        resp, _ = B.respond(f"{scene}. Why?")
        why.append(float(any(c in (resp or "").lower() for c in CAUSAL_CONNECTIVES)))
    cnt = []
    for _ in range(6):
        n = random.randint(1, 6); thing = random.choice(ANIMALS)
        resp, _ = B.respond(f"How many {thing}s: {', '.join([thing]*n)}?")
        cnt.append(float(n in _numbers_in(resp)))
    return {"tom_false_belief": float(np.mean(tom)), "why_connective": float(np.mean(why)),
            "counting": float(np.mean(cnt))}


def probe_stage5(B: _Brain, ia) -> dict[str, float]:
    sp = []
    for w in random.sample(PHONICS_WORDS, 6):
        resp, _ = B.respond(f"Spell the word {w}.")
        letters = "".join(t for t in _tokens(resp) if len(t) == 1)
        sp.append(float(letters == w or _mentions(resp, w)))
    comp = []
    for p in random.sample(FALLBACK_PASSAGES, 3):
        first = re.split(r"(?<=[.!?])\s+", p)[0]
        resp, _ = B.respond(f"{p} What happened at the start?")
        comp.append(B.similarity(resp, first))
    wr = []
    for subj in random.sample(ANIMALS, 4):
        resp, _ = B.respond(f"Write one sentence about a {subj}.")
        wr.append(float(_mentions(resp, subj) and len(_tokens(resp)) >= 5))
    return {"spelling": float(np.mean(sp)), "comprehension": float(np.mean(comp)),
            "writing": float(np.mean(wr))}


def probe_stage6(B: _Brain) -> dict[str, float]:
    m = []
    for _ in range(6):
        tmpl, fn = random.choice(MATH_TEMPLATES); a, b = random.randint(2, 12), random.randint(1, 9)
        if fn(a, b) < 0: a, b = b, a
        n1, n2 = random.sample(NAMES, 2)
        q = tmpl.format(a=a, b=b, objs="stones", n1=n1, n2=n2)
        resp, _ = B.respond(q)
        m.append(float(fn(a, b) in _numbers_in(resp)))
    lg = []
    for q, answers in random.sample(LOGIC_ITEMS, 5):
        resp, _ = B.respond(q)
        lg.append(float(_mentions(resp, *answers)))
    return {"arithmetic": float(np.mean(m)), "logic": float(np.mean(lg))}


def probe_stage7(B: _Brain, stats: StageStats) -> dict[str, float]:
    return {"self_directed": stats.mean("self_directed"), "teach_back": stats.mean("teach_back"),
            "reflect": stats.mean("reflect")}


# Gate floors per stage for the probe metrics (health/capability, not loss).
PROBE_FLOORS = {
    4: {"tom_false_belief": 0.6, "why_connective": 0.6, "counting": 0.6},
    5: {"spelling": 0.6, "comprehension": 0.3, "writing": 0.6},
    6: {"arithmetic": 0.6, "logic": 0.6},
    7: {},   # open-ended: no transition out of stage 7
}

STAGE_NAMES = {4: "Why? — Dialogue, explanation and curiosity", 5: "Literacy — reading and writing",
               6: "School — Socratic learning across domains", 7: "Apprenticeship — self-directed mastery"}


# ---------------------------------------------------------------------------
# Generic stage runner
# ---------------------------------------------------------------------------

def _run_stage(ia, stage: int, brain, composer, parent, clock, source, decoder,
               num_episodes: int, start_from: int, mix: list[tuple[float, Callable]],
               probe_fn: Optional[Callable], max_episodes: int = 0,
               progress_every: int = 100, eval_every: int = 500, gate_every: int = 200,
               mastery=None):
    name = STAGE_NAMES[stage]
    print("\n" + "=" * 60)
    print(f"  STAGE {stage}: {name}")
    print("=" * 60)
    _safe(parent.pre_generate_content, stage=stage, num_stimuli=num_episodes)
    _safe(brain.set_plasticity_state, "MAINTENANCE")

    B = _Brain(brain, ia, composer, decoder, parent)
    trainer = ia.MiniBatchTrainer(brain, batch_size=8)
    lr_sched = ia.CosineAnnealingLR(lr_max=1.0, lr_min=0.3, warmup_steps=200,
                                    t_max=max(num_episodes, 1000))
    stats = StageStats()
    losses: list[float] = []

    gate = None
    gate_passed = False
    probe_ok = not probe_fn
    if getattr(ia, "STAGE_GATE_ENABLED", True) and stage < 7:
        from stage_gate import stage_gate_for, log_gate_event
        gate = stage_gate_for(stage)
        gate.chat_log_path = getattr(ia, "_CHAT_LOG_FILE", None)
        print(f"  [StageGate {stage}] active — min {num_episodes} episodes, then health criteria "
              f"+ probe floors {PROBE_FLOORS.get(stage)} 3x consecutively; ceiling: "
              f"{max_episodes or 'none'}", flush=True)
    elif stage == 7:
        print("  [Stage 7] open-ended — runs until stopped (no transition out)", flush=True)

    weights = [w for w, _ in mix]
    fns = [f for _, f in mix]

    for i in itertools.count(start_from):
        ia.TRAINING_PROGRESS[0], ia.TRAINING_PROGRESS[1] = stage, i
        if num_episodes and i >= num_episodes:
            if gate is None or (gate_passed and probe_ok):
                if stage < 7:
                    break
            if max_episodes and i >= max_episodes:
                print(f"  [StageGate {stage}] SAFETY CEILING {max_episodes} reached — advancing WITHOUT a gate pass",
                      flush=True)
                break

        action = _safe(clock.tick, brain)
        if action == "sleep":
            _safe(clock.do_sleep, brain, parent)
        elif action == "consolidate":
            _safe(clock.do_consolidate, brain)

        lr = lr_sched.get_lr()
        fn = random.choices(fns, weights=weights, k=1)[0]
        try:
            ep = fn(B, trainer, lr)
        except Exception as e:
            print(f"  [Stage {stage}] episode {getattr(fn, '__name__', '?')} failed: {e}", flush=True)
            ep = None
        if ep is None:
            # conversation inbox empty etc. → fall back to the first generator
            try:
                ep = fns[0](B, trainer, lr)
            except Exception as e:
                print(f"  [Stage {stage}] fallback episode failed: {e}", flush=True)
                ep = Episode("failed")
        stats.add(ep)
        if ep.loss is not None:
            losses.append(float(ep.loss))
        else:
            losses.append(0.0 if not ep.response else max(0.001, 1.0 - ep.score))
        lr_sched.step()

        # Cognitive module injection + IDK (same cadence as stage 3)
        if (i + 1) % getattr(ia, "COGNITIVE_TRAIN_INTERVAL", 5) == 0:
            cog = _safe(ia._inject_cognitive_training, brain, composer, step=i, learning_rate=lr,
                        spectral_splitter=getattr(ia, "_spectral_splitter", None),
                        fold_idx=getattr(ia, "_current_fold", 0), curriculum=None)
            if cog is not None:
                losses.append(float(cog))
            _safe(ia._inject_idk_unanswerable, brain, composer, step=i, learning_rate=lr, stage=stage)

        # Corpus drips every 200 (canonical / math / streaming / probe drips)
        if (i + 1) % 200 == 0:
            for cfg_name, fn_name, extra in (("_CANONICAL_CFG", "_ingest_canonical_corpus", {"corpus_root": "data/canonical_corpus"}),
                                             ("_MATH_CFG", "_ingest_math_corpus", {"corpus_root": "data/math_corpus"})):
                cfg = getattr(ia, cfg_name, None)
                if cfg and cfg.get("enabled") and hasattr(ia, fn_name):
                    kw = dict(extra); kw["corpus_root"] = cfg.get("root", kw["corpus_root"])
                    _safe(getattr(ia, fn_name), brain, stage=stage, chunk_chars=cfg.get("chunk_chars", 1200),
                          max_chunks_per_call=20, checkpoint_dir=getattr(ia, "CHECKPOINT_DIR", "checkpoints/athena"),
                          **kw)
            scfg = getattr(ia, "_STREAMING_CFG", None)
            if scfg and scfg.get("enabled") and hasattr(ia, "_ingest_streaming_corpus"):
                for kind in scfg.get("kinds", []):
                    _safe(ia._ingest_streaming_corpus, brain, stage=stage, kind=kind,
                          chunk_chars=scfg.get("chunk_chars", 1200), max_chunks_per_call=20,
                          checkpoint_dir=getattr(ia, "CHECKPOINT_DIR", "checkpoints/athena"))
            if getattr(ia, "_PROBE_DRIPS_ENABLED", False) and hasattr(ia, "_run_probe_drips"):
                _safe(ia._run_probe_drips, brain, stage=stage, composer=composer)

        # Progress
        if (i + 1) % progress_every == 0:
            avg = float(np.mean(losses[-progress_every:])) if losses else 0.0
            print(f"\n  [Stage {stage}] {i+1}/{num_episodes or '∞'} — avg_loss={avg:.4f} lr={lr:.3f}")
            print(f"    episodes: {stats.summary()}")
            rs = _safe(ia._get_real_sensory, default=None) if hasattr(ia, "_get_real_sensory") else None
            if rs:
                print(f"    [RealSensory] {rs.stats_str()}")
            _safe(ia._print_utm_health, brain)
            _safe(ia._print_bio_stats, brain)
            if mastery is not None:
                _safe(mastery.save, getattr(ia, "MASTERY_FILE", "checkpoints/athena/mastery_state.json"))

        # Evaluation
        if (i + 1) % eval_every == 0:
            if decoder:
                _safe(decoder.force_refit)
            _safe(ia.evaluate_performance, brain, composer, decoder, stage=stage, step=i + 1)

        # Gate: health criteria (StageGate) AND stage probe floors
        if gate is not None and (i + 1) % gate_every == 0:
            try:
                gr = gate.check(i + 1, losses, brain, composer, decoder, chat_eval_fn=ia.chat_eval)
            except Exception as e:
                from stage_gate import GateResult
                gr = GateResult(False, f"gate_exception:{e}")
            metrics = {}
            if probe_fn is not None:
                try:
                    metrics = probe_fn()
                except Exception as e:
                    metrics = {"probe_error": 0.0}
                    print(f"    [Probe {stage}] failed: {e}")
                floors = PROBE_FLOORS.get(stage, {})
                probe_ok = all(metrics.get(k, 0.0) >= v for k, v in floors.items())
                gr.details["probes"] = metrics
                gr.details["probe_ok"] = probe_ok
            gate_passed = gr.passed and probe_ok
            print(f"    [StageGate {stage}] step {i+1}: {gr} | probes={{"
                  + ", ".join(f"{k}:{v:.2f}" for k, v in metrics.items()) + f"}} probe_ok={probe_ok}",
                  flush=True)
            try:
                log_gate_event(getattr(ia, "STAGE_GATE_LOG", "checkpoints/athena/stage_gate_log.jsonl"),
                               stage, i + 1, gr)
            except Exception:
                pass

        # Deep consolidation + prune every 2000
        if (i + 1) % 2000 == 0:
            _safe(clock.do_sleep, brain, parent)
            pruned = _safe(brain.prune_synapses, 0.01, default=0)
            if pruned:
                print(f"    [Prune] Removed {pruned} weak synapses")

        # Checkpoint cadence (shared with the trainer's deferred-save logic)
        if (i + 1) % 50 == 0 or getattr(ia, "_checkpoint_pending", False):
            ia._save_checkpoint(brain, decoder, stage=stage, step=i + 1)

        # Chat eval every 2000 + on demand
        if (i + 1) % 2000 == 0 or os.path.exists("/tmp/athena_chat_now"):
            if os.path.exists("/tmp/athena_chat_now"):
                _safe(os.remove, "/tmp/athena_chat_now")
            _safe(ia.chat_eval, brain, composer, decoder, stage=stage, step=i + 1)

    _safe(trainer.flush)
    if mastery is not None:
        _safe(mastery.save, getattr(ia, "MASTERY_FILE", "checkpoints/athena/mastery_state.json"))
    return losses


# ---------------------------------------------------------------------------
# Public entry points
# ---------------------------------------------------------------------------

def run_stage_4(ia, brain, composer, parent, clock, source, decoder,
                num_episodes=50000, start_from=0, max_episodes=0):
    mix = [
        (0.30, lambda B, t, lr: ep_causal(B, t, 4, lr)),
        (0.20, lambda B, t, lr: ep_athena_asks(B, t, 4, lr, source)),
        (0.20, lambda B, t, lr: ep_narrative(B, t, 4, lr)),
        (0.15, lambda B, t, lr: ep_other_minds(B, t, 4, lr)),
        (0.15, lambda B, t, lr: ep_number_category(B, t, 4, lr)),
    ]
    B = _Brain(brain, ia, composer, decoder, parent)
    return _run_stage(ia, 4, brain, composer, parent, clock, source, decoder,
                      num_episodes, start_from, mix, lambda: probe_stage4(B), max_episodes)


def run_stage_5(ia, brain, composer, parent, clock, source, decoder,
                num_episodes=50000, start_from=0, max_episodes=0):
    mix = [
        (0.30, lambda B, t, lr: ep_read_aloud(B, t, 5, lr, ia)),
        (0.20, lambda B, t, lr: ep_phonics(B, t, 5, lr)),
        (0.20, lambda B, t, lr: ep_write(B, t, 5, lr)),
        (0.15, lambda B, t, lr: ep_retell_passage(B, t, 5, lr, ia)),
        (0.15, lambda B, t, lr: ep_vocabulary(B, t, 5, lr, source)),
    ]
    B = _Brain(brain, ia, composer, decoder, parent)
    return _run_stage(ia, 5, brain, composer, parent, clock, source, decoder,
                      num_episodes, start_from, mix, lambda: probe_stage5(B, ia), max_episodes)


def run_stage_6(ia, brain, composer, parent, clock, source, decoder,
                num_episodes=100000, start_from=0, max_episodes=0):
    mastery = ia.MasteryTracker()
    _safe(mastery.load, getattr(ia, "MASTERY_FILE", "checkpoints/athena/mastery_state.json"))
    domains = list(getattr(source, "ADVANCED_TOPICS", {}).keys()) or SCHOOL_DOMAINS
    mix = [
        (0.30, lambda B, t, lr: ep_socratic(B, t, 6, lr, source, mastery, domains)),
        (0.20, lambda B, t, lr: ep_worksheet(B, t, 6, lr, source, mastery, domains)),
        (0.20, lambda B, t, lr: ep_math_logic(B, t, 6, lr)),
        (0.15, lambda B, t, lr: ep_explain_back(B, t, 6, lr, source)),
        (0.15, lambda B, t, lr: ep_research(B, t, 6, lr, source, ia)),
    ]
    B = _Brain(brain, ia, composer, decoder, parent)
    return _run_stage(ia, 6, brain, composer, parent, clock, source, decoder,
                      num_episodes, start_from, mix, lambda: probe_stage6(B), max_episodes,
                      mastery=mastery)


def run_stage_7(ia, brain, composer, parent, clock, source, decoder,
                num_episodes=0, start_from=0, max_episodes=0):
    """Open-ended: num_episodes=0 means run until stopped. The conversation
    inbox (checkpoints/athena/conversation_inbox.jsonl, one {"user": ...}
    per line) is drained first whenever it has content."""
    mastery = ia.MasteryTracker()
    _safe(mastery.load, getattr(ia, "MASTERY_FILE", "checkpoints/athena/mastery_state.json"))
    mix = [
        (0.40, lambda B, t, lr: ep_self_directed(B, t, 7, lr, source, ia, mastery)),
        (0.25, lambda B, t, lr: ep_conversation(B, t, 7, lr, ia)),
        (0.20, lambda B, t, lr: ep_teach_back(B, t, 7, lr, source)),
        (0.15, lambda B, t, lr: ep_reflect(B, t, 7, lr)),
    ]
    return _run_stage(ia, 7, brain, composer, parent, clock, source, decoder,
                      num_episodes, start_from, mix, None, max_episodes, mastery=mastery)
