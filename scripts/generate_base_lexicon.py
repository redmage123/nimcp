#!/usr/bin/env python3
"""Generate the base lexicon fixture for grounded language bootstrap.

WHAT: Produces data/lexicon/base_lexicon_v1.json with ~1500 common English
      words (nouns/verbs/adjectives) and deterministic 128-d feature vectors.

WHY:  The grounded_language module ships with ~248 seeded function words
      (the/a/is/etc.) and learns content words from scratch. That's slow.
      This fixture lets the C bootstrap API one-shot inject ~1500 content
      words at daemon start so curriculum text immediately has anchors.

HOW:  Each word's features are deterministic — same word + class always maps
      to the same vector. The first 16 components are class axes
      (noun=0..3, verb=4..7, adj=8..11) at magnitude 0.5. The remaining 112
      components are derived from a stable hash (FNV-1a 64-bit) of
      "<class>:<word>" mapped through a tiny xorshift64* PRNG into [-0.3, 0.3].

USAGE:  python3 scripts/generate_base_lexicon.py
        Writes data/lexicon/base_lexicon_v1.json (overwrites if present).
"""

from __future__ import annotations

import json
import os
import sys
from typing import Iterable

SEMANTIC_DIM = 128
CLASS_AXIS_RANGES = {
    "noun":      (0, 4),
    "verb":      (4, 8),
    "adjective": (8, 12),
}
CLASS_AXIS_MAG = 0.5
NOISE_COMPONENTS_START = 16
NOISE_RANGE = 0.3  # [-0.3, +0.3]


# -----------------------------------------------------------------------------
# Word lists (curated, deduplicated, lowercase). Target: ~700 N + ~400 V + ~400 A.
# Curated for breadth: concrete/abstract nouns, motion/communication/cognition
# verbs, color/size/temperature/emotion adjectives, etc.
# -----------------------------------------------------------------------------

NOUNS: list[str] = sorted(set([
    # People / kinship / social roles
    "person", "people", "man", "woman", "child", "boy", "girl", "baby", "kid",
    "adult", "parent", "father", "mother", "dad", "mom", "son", "daughter",
    "brother", "sister", "uncle", "aunt", "cousin", "nephew", "niece",
    "grandmother", "grandfather", "grandparent", "grandchild", "family",
    "friend", "neighbor", "stranger", "guest", "host", "teacher", "student",
    "doctor", "nurse", "patient", "lawyer", "judge", "soldier", "officer",
    "police", "firefighter", "farmer", "worker", "employee", "boss",
    "manager", "engineer", "scientist", "artist", "writer", "musician",
    "singer", "actor", "dancer", "athlete", "player", "coach", "captain",
    "leader", "king", "queen", "prince", "princess", "president", "minister",
    "citizen", "resident", "tourist", "traveler", "driver", "pilot", "sailor",
    "chef", "cook", "waiter", "baker", "tailor", "carpenter", "plumber",

    # Body parts
    "body", "head", "face", "hair", "eye", "ear", "nose", "mouth", "lip",
    "tooth", "tongue", "neck", "shoulder", "arm", "elbow", "wrist", "hand",
    "finger", "thumb", "nail", "chest", "back", "stomach", "waist", "hip",
    "leg", "knee", "ankle", "foot", "toe", "skin", "bone", "muscle", "blood",
    "heart", "lung", "brain", "liver", "kidney", "spine", "rib", "skull",

    # Animals
    "animal", "dog", "puppy", "cat", "kitten", "horse", "cow", "pig", "sheep",
    "goat", "chicken", "duck", "goose", "turkey", "rabbit", "mouse", "rat",
    "squirrel", "deer", "bear", "wolf", "fox", "lion", "tiger", "elephant",
    "giraffe", "zebra", "monkey", "ape", "gorilla", "kangaroo", "koala",
    "panda", "whale", "dolphin", "shark", "fish", "salmon", "tuna", "bird",
    "eagle", "hawk", "owl", "crow", "sparrow", "robin", "pigeon", "parrot",
    "penguin", "ostrich", "snake", "lizard", "turtle", "frog", "toad",
    "spider", "ant", "bee", "wasp", "fly", "mosquito", "butterfly", "moth",
    "beetle", "worm", "crab", "lobster", "shrimp", "octopus", "jellyfish",

    # Plants / nature
    "plant", "tree", "bush", "flower", "rose", "lily", "tulip", "daisy",
    "leaf", "branch", "root", "stem", "seed", "fruit", "grass", "moss",
    "fern", "mushroom", "vine", "grain", "wheat", "rice", "corn",

    # Food / drink
    "food", "meal", "breakfast", "lunch", "dinner", "snack", "dessert",
    "bread", "toast", "sandwich", "pizza", "pasta", "noodle", "soup", "salad",
    "rice", "potato", "tomato", "onion", "garlic", "carrot", "celery",
    "lettuce", "cabbage", "spinach", "broccoli", "pepper", "cucumber",
    "apple", "banana", "orange", "grape", "lemon", "lime", "peach", "pear",
    "plum", "cherry", "strawberry", "blueberry", "watermelon", "pineapple",
    "mango", "coconut", "meat", "beef", "pork", "chicken", "lamb", "turkey",
    "fish", "egg", "cheese", "milk", "butter", "yogurt", "cream", "sugar",
    "salt", "honey", "syrup", "jam", "oil", "vinegar", "sauce", "spice",
    "water", "juice", "coffee", "tea", "soda", "wine", "beer", "drink",

    # Places / built environment
    "place", "home", "house", "apartment", "room", "kitchen", "bedroom",
    "bathroom", "living-room", "office", "garage", "basement", "attic",
    "yard", "garden", "park", "playground", "school", "classroom",
    "library", "museum", "theater", "cinema", "stadium", "gym", "store",
    "shop", "market", "mall", "restaurant", "cafe", "hotel", "motel",
    "hospital", "clinic", "pharmacy", "bank", "post-office", "church",
    "temple", "mosque", "factory", "farm", "ranch", "village", "town",
    "city", "country", "state", "region", "neighborhood", "street", "road",
    "highway", "bridge", "tunnel", "airport", "station", "harbor", "port",
    "beach", "coast", "shore", "island", "mountain", "hill", "valley",
    "river", "lake", "ocean", "sea", "pond", "forest", "jungle", "desert",
    "field", "meadow", "cave", "cliff", "canyon",

    # Transportation
    "vehicle", "car", "truck", "bus", "van", "taxi", "train", "subway",
    "tram", "boat", "ship", "yacht", "canoe", "kayak", "raft", "plane",
    "helicopter", "rocket", "bicycle", "bike", "motorcycle", "scooter",
    "wheel", "tire", "engine", "fuel", "gasoline", "battery",

    # Tools / objects / household
    "tool", "hammer", "saw", "screwdriver", "wrench", "drill", "knife",
    "fork", "spoon", "plate", "bowl", "cup", "glass", "mug", "bottle",
    "jar", "can", "box", "bag", "basket", "bucket", "pan", "pot", "kettle",
    "oven", "stove", "fridge", "freezer", "microwave", "dishwasher",
    "washer", "dryer", "vacuum", "broom", "mop", "sponge", "towel", "soap",
    "shampoo", "toothbrush", "toothpaste", "comb", "brush", "razor",
    "mirror", "lamp", "candle", "clock", "watch", "calendar", "key", "lock",
    "door", "window", "wall", "floor", "ceiling", "roof", "stairs", "table",
    "chair", "sofa", "couch", "bed", "pillow", "blanket", "sheet", "rug",
    "curtain", "shelf", "cabinet", "drawer", "desk", "bookcase",

    # Clothing
    "clothes", "clothing", "shirt", "pants", "trousers", "jeans", "skirt",
    "dress", "suit", "uniform", "coat", "jacket", "sweater", "vest",
    "shorts", "underwear", "sock", "shoe", "boot", "sandal", "sneaker",
    "hat", "cap", "scarf", "glove", "mitten", "belt", "tie", "watch",
    "ring", "necklace", "bracelet", "earring", "glasses",

    # Tech / media / education
    "computer", "laptop", "phone", "tablet", "screen", "keyboard", "mouse",
    "printer", "camera", "television", "radio", "speaker", "headphone",
    "microphone", "internet", "website", "email", "message", "video",
    "movie", "picture", "photo", "image", "drawing", "painting", "sculpture",
    "book", "novel", "story", "poem", "song", "music", "album", "newspaper",
    "magazine", "letter", "postcard", "card", "ticket", "receipt", "bill",
    "money", "coin", "cash", "card", "wallet", "purse",

    # Time
    "time", "moment", "second", "minute", "hour", "day", "night", "morning",
    "afternoon", "evening", "noon", "midnight", "week", "weekend", "month",
    "year", "decade", "century", "season", "spring", "summer", "autumn",
    "winter", "today", "tomorrow", "yesterday",

    # Math / science / abstract
    "number", "zero", "one", "two", "three", "four", "five", "six", "seven",
    "eight", "nine", "ten", "hundred", "thousand", "million", "half",
    "quarter", "addition", "subtraction", "multiplication", "division",
    "sum", "difference", "product", "quotient", "ratio", "fraction",
    "percent", "average", "total", "count", "amount", "quantity", "size",
    "length", "width", "height", "depth", "weight", "mass", "speed",
    "velocity", "acceleration", "force", "energy", "power", "pressure",
    "temperature", "heat", "light", "sound", "color", "shape", "circle",
    "square", "triangle", "rectangle", "sphere", "cube", "line", "point",
    "angle", "curve", "edge", "surface", "volume", "area",
    "atom", "molecule", "element", "compound", "cell", "tissue", "organ",
    "system", "gene", "species", "habitat", "ecosystem", "evolution",
    "gravity", "magnet", "electricity", "wave", "particle", "matter",

    # Emotion / mind / abstract concepts
    "feeling", "emotion", "happiness", "joy", "love", "hope", "pride",
    "sadness", "grief", "anger", "fear", "worry", "anxiety", "stress",
    "surprise", "curiosity", "boredom", "loneliness", "shame", "guilt",
    "trust", "doubt", "courage", "patience", "kindness", "compassion",
    "jealousy", "envy", "regret", "gratitude", "respect",
    "thought", "idea", "memory", "dream", "wish", "plan", "goal", "purpose",
    "reason", "cause", "effect", "result", "consequence", "problem",
    "question", "answer", "decision", "choice", "opinion", "belief",
    "knowledge", "fact", "truth", "lie", "secret", "story", "rumor",
    "advice", "warning", "promise", "rule", "law", "right", "freedom",
    "duty", "responsibility", "honor", "justice", "peace", "war", "conflict",

    # Time/abstract experiential
    "life", "death", "birth", "childhood", "youth", "age", "future", "past",
    "present", "history", "tradition", "culture", "language", "word",
    "sentence", "name", "title", "label", "sign", "symbol",
]))

VERBS: list[str] = sorted(set([
    # Motion
    "go", "come", "arrive", "leave", "depart", "return", "enter", "exit",
    "walk", "run", "jog", "sprint", "march", "stride", "wander", "stroll",
    "hike", "climb", "descend", "rise", "fall", "drop", "tumble", "slip",
    "trip", "stumble", "jump", "leap", "hop", "skip", "bounce", "fly",
    "soar", "glide", "swim", "dive", "float", "drift", "sink", "drown",
    "drive", "ride", "sail", "row", "paddle", "bike", "skate", "ski",
    "crawl", "creep", "slide", "roll", "spin", "turn", "twist", "rotate",
    "swing", "sway", "shake", "tremble", "shiver", "wave", "flap", "flutter",
    "move", "shift", "carry", "bring", "take", "fetch", "deliver", "send",
    "push", "pull", "drag", "lift", "raise", "lower", "throw", "toss",
    "catch", "drop", "kick", "punch", "hit", "strike", "tap", "knock",
    "press", "squeeze", "crush", "smash", "break", "snap", "bend", "fold",
    "stretch", "tug", "yank", "shove",

    # Communication / language
    "say", "tell", "speak", "talk", "shout", "yell", "scream", "whisper",
    "mumble", "mutter", "answer", "reply", "respond", "ask", "question",
    "request", "demand", "command", "order", "beg", "plead", "promise",
    "swear", "lie", "confess", "explain", "describe", "discuss", "argue",
    "debate", "agree", "disagree", "praise", "blame", "criticize", "scold",
    "thank", "apologize", "greet", "introduce", "call", "name", "label",
    "announce", "declare", "confirm", "deny", "warn", "advise", "suggest",
    "recommend", "propose", "offer", "invite", "welcome", "say", "read",
    "write", "type", "print", "publish", "translate", "spell",

    # Cognition / mental
    "think", "wonder", "imagine", "dream", "consider", "ponder", "reflect",
    "remember", "recall", "forget", "memorize", "learn", "study", "review",
    "understand", "comprehend", "realize", "recognize", "identify", "know",
    "believe", "doubt", "trust", "suspect", "guess", "predict", "expect",
    "hope", "wish", "want", "need", "desire", "prefer", "choose", "decide",
    "plan", "intend", "mean", "judge", "evaluate", "compare", "analyze",
    "solve", "calculate", "count", "measure", "estimate", "weigh", "test",
    "experiment", "discover", "invent", "create", "design",

    # Perception / sensory
    "see", "look", "watch", "observe", "stare", "glance", "peek", "notice",
    "spot", "view", "witness", "hear", "listen", "smell", "sniff", "taste",
    "feel", "touch", "sense", "perceive", "appear", "seem", "show", "reveal",
    "hide", "conceal", "display", "exhibit", "demonstrate",

    # Change-of-state / transformation
    "become", "turn", "grow", "shrink", "expand", "contract", "increase",
    "decrease", "rise", "fall", "improve", "worsen", "develop", "evolve",
    "change", "transform", "alter", "modify", "adjust", "adapt", "fix",
    "repair", "mend", "heal", "cure", "harm", "hurt", "injure", "damage",
    "destroy", "ruin", "build", "construct", "create", "make", "produce",
    "manufacture", "form", "shape", "carve", "mold", "cook", "bake", "boil",
    "fry", "roast", "grill", "steam", "freeze", "melt", "burn", "dry", "wet",
    "soak", "wash", "clean", "scrub", "rinse", "polish", "paint", "color",
    "dye", "decorate", "open", "close", "shut", "lock", "unlock", "begin",
    "start", "end", "finish", "stop", "pause", "continue", "resume", "wait",
    "delay", "hurry", "rush",

    # Action / interaction / social
    "do", "make", "use", "try", "attempt", "manage", "succeed", "fail",
    "win", "lose", "compete", "race", "play", "perform", "act", "pretend",
    "rest", "relax", "sleep", "wake", "yawn", "stretch", "smile", "laugh",
    "giggle", "cry", "weep", "sob", "frown", "blush", "blink", "wink",
    "nod", "shake", "shrug", "point", "wave", "clap", "hug", "kiss", "hold",
    "grab", "grasp", "release", "let", "give", "receive", "accept", "refuse",
    "share", "lend", "borrow", "owe", "pay", "buy", "sell", "trade", "spend",
    "save", "earn", "owe", "cost", "find", "lose", "search", "seek", "hunt",
    "chase", "follow", "lead", "guide", "help", "assist", "support",
    "protect", "defend", "attack", "fight", "argue", "punish", "reward",
    "teach", "train", "coach", "study", "practice", "rehearse",
    "celebrate", "cheer", "mourn", "grieve", "bury", "marry", "divorce",
    "love", "like", "hate", "dislike", "enjoy", "suffer", "tolerate",
    "ignore", "remember", "miss", "visit", "meet", "greet", "introduce",
    "vote", "elect", "rule", "govern", "manage", "direct", "control",
    "lead", "obey", "follow", "join", "leave", "quit",
    "eat", "drink", "swallow", "chew", "bite", "sip", "lick", "spit",
    "breathe", "cough", "sneeze", "hiccup", "snore", "gasp",
    "live", "die", "exist", "happen", "occur", "appear", "disappear",
    "stay", "remain", "stand", "sit", "lie", "lean", "kneel", "crouch",
]))

ADJECTIVES: list[str] = sorted(set([
    # Color
    "red", "orange", "yellow", "green", "blue", "purple", "violet", "pink",
    "brown", "tan", "beige", "black", "white", "gray", "grey", "silver",
    "gold", "golden", "bronze", "copper", "crimson", "scarlet", "maroon",
    "navy", "turquoise", "teal", "aqua", "magenta", "indigo", "lavender",
    "ivory", "cream", "amber", "ruby", "emerald", "sapphire", "colorful",
    "colorless", "pale", "bright", "dark", "light", "vivid", "dull",

    # Size / dimension
    "big", "large", "huge", "enormous", "giant", "gigantic", "massive",
    "vast", "immense", "tremendous", "small", "little", "tiny", "minute",
    "miniature", "petite", "short", "tall", "long", "narrow", "wide",
    "thin", "thick", "slim", "slender", "broad", "deep", "shallow", "high",
    "low", "fat", "skinny", "plump", "lean", "heavy", "light", "compact",
    "spacious", "cramped", "roomy",

    # Temperature / weather / texture
    "hot", "warm", "cool", "cold", "freezing", "boiling", "scorching",
    "icy", "chilly", "lukewarm", "tepid", "humid", "dry", "wet", "moist",
    "damp", "soggy", "soaked", "dusty", "muddy", "sticky", "slippery",
    "smooth", "rough", "soft", "hard", "firm", "tough", "tender", "fragile",
    "delicate", "sturdy", "solid", "hollow", "soft", "fluffy", "fuzzy",
    "sharp", "dull", "blunt", "pointy", "jagged", "bumpy", "flat", "even",
    "uneven", "wrinkled", "smooth", "shiny", "glossy", "matte", "polished",
    "sunny", "cloudy", "rainy", "snowy", "windy", "stormy", "foggy",
    "misty", "clear", "overcast",

    # Emotion / personality
    "happy", "joyful", "cheerful", "merry", "glad", "delighted", "ecstatic",
    "content", "pleased", "satisfied", "sad", "unhappy", "depressed", "blue",
    "miserable", "gloomy", "melancholy", "sorrowful", "angry", "mad",
    "furious", "irate", "annoyed", "irritated", "cross", "calm", "serene",
    "peaceful", "tranquil", "relaxed", "tense", "anxious", "nervous",
    "worried", "afraid", "scared", "frightened", "terrified", "fearful",
    "brave", "bold", "courageous", "fearless", "shy", "timid", "bashful",
    "confident", "proud", "humble", "modest", "arrogant", "vain", "loud",
    "quiet", "noisy", "silent", "talkative", "chatty", "kind", "gentle",
    "tender", "loving", "caring", "warm", "cold", "harsh", "cruel", "mean",
    "rude", "polite", "courteous", "respectful", "friendly", "hostile",
    "sociable", "outgoing", "introverted", "honest", "truthful", "sincere",
    "dishonest", "deceitful", "loyal", "faithful", "trustworthy", "reliable",
    "responsible", "lazy", "hardworking", "diligent", "energetic",
    "lively", "dynamic", "tired", "exhausted", "weary", "sleepy", "drowsy",
    "alert", "active", "passive", "patient", "impatient", "curious",
    "interested", "bored", "excited", "thrilled", "eager", "enthusiastic",
    "indifferent", "jealous", "envious", "greedy", "generous", "selfish",
    "selfless", "thoughtful", "considerate", "thoughtless", "sensitive",
    "insensitive",

    # Evaluation / quality
    "good", "great", "excellent", "wonderful", "fantastic", "amazing",
    "awesome", "marvelous", "superb", "splendid", "fine", "okay", "fair",
    "decent", "average", "ordinary", "common", "typical", "normal",
    "regular", "standard", "usual", "unusual", "uncommon", "rare", "unique",
    "special", "extraordinary", "remarkable", "outstanding", "perfect",
    "flawless", "ideal", "bad", "awful", "terrible", "horrible", "dreadful",
    "atrocious", "poor", "lousy", "mediocre", "subpar", "inferior",
    "superior", "better", "best", "worse", "worst", "cheap", "expensive",
    "costly", "valuable", "worthless", "priceless", "free", "paid",
    "wealthy", "rich", "poor", "needy", "broke", "successful", "famous",
    "popular", "obscure", "unknown", "important", "significant", "trivial",
    "minor", "major", "main", "primary", "secondary", "essential",
    "necessary", "vital", "crucial", "optional", "useful", "useless",
    "helpful", "unhelpful", "effective", "ineffective", "efficient",
    "inefficient", "convenient", "inconvenient", "easy", "simple",
    "straightforward", "hard", "difficult", "tough", "challenging",
    "complex", "complicated", "intricate", "clear", "obvious", "evident",
    "vague", "ambiguous", "confusing", "interesting", "fascinating",
    "intriguing", "boring", "dull", "tedious", "fun", "enjoyable",
    "pleasant", "unpleasant", "delightful", "disgusting", "gross",
    "horrible", "lovely", "beautiful", "pretty", "gorgeous", "stunning",
    "attractive", "handsome", "cute", "adorable", "ugly", "hideous",
    "plain", "elegant", "graceful", "clumsy", "awkward",

    # Age / time / state
    "new", "old", "ancient", "modern", "current", "recent", "fresh", "stale",
    "young", "youthful", "elderly", "aged", "mature", "juvenile", "old-fashioned",
    "outdated", "obsolete", "vintage", "antique", "early", "late", "punctual",
    "tardy", "fast", "quick", "rapid", "swift", "slow", "sluggish", "brief",
    "short", "long", "lengthy", "lasting", "permanent", "temporary",
    "eternal", "endless", "finite", "infinite", "alive", "dead", "lifeless",
    "active", "inactive", "open", "closed", "shut", "broken", "fixed",
    "whole", "complete", "incomplete", "partial", "full", "empty", "vacant",
    "occupied", "busy", "idle", "ready", "unprepared", "available",
    "unavailable", "present", "absent",

    # Quantity
    "many", "much", "few", "little", "several", "numerous", "countless",
    "single", "double", "triple", "multiple", "every", "each", "all", "some",
    "any", "none", "enough", "plenty", "ample", "scarce", "abundant",
    "sparse", "dense", "crowded", "empty",

    # Misc property
    "true", "false", "real", "fake", "genuine", "authentic", "artificial",
    "natural", "original", "copied", "wild", "tame", "domestic", "foreign",
    "local", "global", "public", "private", "secret", "hidden", "visible",
    "invisible", "obvious", "subtle", "loud", "soft", "smelly", "fragrant",
    "stinky", "tasty", "delicious", "yummy", "bland", "bitter", "sweet",
    "sour", "salty", "spicy", "savory", "rotten", "fresh", "ripe", "raw",
    "cooked", "burnt", "frozen", "melted", "wet", "dry", "clean", "dirty",
    "messy", "tidy", "neat", "organized", "chaotic", "orderly",
    "safe", "dangerous", "risky", "secure", "unsafe", "harmful", "harmless",
    "healthy", "sick", "ill", "well", "fit", "strong", "weak", "powerful",
    "feeble", "energetic", "lethargic", "smart", "intelligent", "brilliant",
    "clever", "wise", "foolish", "stupid", "dumb", "ignorant", "educated",
    "literate", "skilled", "talented", "gifted", "amateur", "expert",
    "professional", "right", "correct", "wrong", "incorrect", "accurate",
    "inaccurate", "precise", "imprecise", "exact", "approximate",
    "perfect", "flawed", "broken", "intact",
]))


def fnv1a_64(s: str) -> int:
    """Stable 64-bit FNV-1a hash. Same input -> same output across machines."""
    h = 0xcbf29ce484222325
    for ch in s.encode("utf-8"):
        h ^= ch
        h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def xorshift64star(state: int) -> tuple[int, int]:
    """xorshift64* PRNG step. Returns (value, new_state). Stateless wrt globals."""
    state &= 0xFFFFFFFFFFFFFFFF
    state ^= (state >> 12) & 0xFFFFFFFFFFFFFFFF
    state ^= (state << 25) & 0xFFFFFFFFFFFFFFFF
    state ^= (state >> 27) & 0xFFFFFFFFFFFFFFFF
    state &= 0xFFFFFFFFFFFFFFFF
    val = (state * 0x2545F4914F6CDD1D) & 0xFFFFFFFFFFFFFFFF
    return val, state


def features_for(word: str, klass: str) -> list[float]:
    """Build deterministic 128-d feature vector for (word, class)."""
    features = [0.0] * SEMANTIC_DIM

    # Class axes (first 16 components, four per class).
    lo, hi = CLASS_AXIS_RANGES[klass]
    for axis in range(lo, hi):
        features[axis] = CLASS_AXIS_MAG

    # Noise-derived semantic components from a stable hash.
    seed = fnv1a_64(f"{klass}:{word}")
    if seed == 0:
        seed = 0x9E3779B97F4A7C15  # avoid zero state for xorshift
    state = seed
    for i in range(NOISE_COMPONENTS_START, SEMANTIC_DIM):
        val, state = xorshift64star(state)
        # Map 64-bit unsigned -> [-NOISE_RANGE, +NOISE_RANGE] uniformly.
        u = val / float(0xFFFFFFFFFFFFFFFF)  # [0, 1]
        features[i] = (u * 2.0 - 1.0) * NOISE_RANGE

    return features


def emit(words: Iterable[str], klass: str) -> list[dict]:
    out = []
    for w in words:
        out.append({
            "form": w,
            "class": klass,
            "features": features_for(w, klass),
        })
    return out


def main() -> int:
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(repo_root, "data", "lexicon")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "base_lexicon_v1.json")

    entries: list[dict] = []
    entries.extend(emit(NOUNS, "noun"))
    entries.extend(emit(VERBS, "verb"))
    entries.extend(emit(ADJECTIVES, "adjective"))

    payload = {
        "version": 1,
        "semantic_dim_hint": SEMANTIC_DIM,
        "words": entries,
    }

    # Pretty-print but keep features compact (one number per slot, no scientific
    # notation surprises): use float repr and let json handle it.
    with open(out_path, "w") as f:
        json.dump(payload, f, indent=1, ensure_ascii=False)

    print(f"Wrote {out_path}")
    print(f"  nouns:      {len(NOUNS)}")
    print(f"  verbs:      {len(VERBS)}")
    print(f"  adjectives: {len(ADJECTIVES)}")
    print(f"  total:      {len(entries)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
