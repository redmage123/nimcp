//! Persistence — **one** canonical, versioned format for the whole
//! [`GroundedLanguage`] engine.
//!
//! V1 carried two divergent serializers (the `.gl_lang` sidecar vs the
//! standalone `grounded_language_save`); fields added to one but not the
//! other was a recurring bug (the phrase table silently wiped every
//! restart). V2 has a single envelope: a magic string + a version + the
//! `serde`-serialized engine. Loading validates the magic/version and
//! rebuilds the lexicon's `#[serde(skip)]` form→index map.
//!
//! The wire codec here is JSON (human-inspectable, dependency-light, and
//! every field is JSON-safe — tuple-keyed maps were already given Vec
//! wire forms). A binary codec can wrap the same `serde` model later
//! without changing the engine.

use std::path::Path;

use serde::{Deserialize, Serialize};

use crate::engine::GroundedLanguage;

/// Magic tag at the head of every saved engine.
pub const FORMAT_MAGIC: &str = "NIMCP_GL_V2";

/// On-disk format version. Bump on any breaking model change.
pub const FORMAT_VERSION: u32 = 1;

/// Persistence error.
#[derive(Debug, thiserror::Error)]
pub enum PersistError {
    #[error("language persistence I/O: {0}")]
    Io(#[from] std::io::Error),
    #[error("language persistence (de)serialization: {0}")]
    Serde(#[from] serde_json::Error),
    #[error("language persistence: bad magic (expected {expected:?}, got {got:?})")]
    BadMagic { expected: String, got: String },
    #[error("language persistence: unsupported version {0}")]
    BadVersion(u32),
}

/// Versioned envelope wrapping the engine.
#[derive(Serialize, Deserialize)]
struct Envelope {
    magic: String,
    version: u32,
    engine: GroundedLanguage,
}

impl GroundedLanguage {
    /// Serialize to a canonical JSON string (magic + version + engine).
    pub fn to_json(&self) -> Result<String, PersistError> {
        let env = Envelope {
            magic: FORMAT_MAGIC.to_string(),
            version: FORMAT_VERSION,
            engine: self.clone(),
        };
        Ok(serde_json::to_string(&env)?)
    }

    /// Deserialize from a canonical JSON string. Validates the magic and
    /// version, then rebuilds the lexicon index.
    pub fn from_json(s: &str) -> Result<Self, PersistError> {
        let env: Envelope = serde_json::from_str(s)?;
        if env.magic != FORMAT_MAGIC {
            return Err(PersistError::BadMagic {
                expected: FORMAT_MAGIC.to_string(),
                got: env.magic,
            });
        }
        if env.version != FORMAT_VERSION {
            return Err(PersistError::BadVersion(env.version));
        }
        let mut engine = env.engine;
        engine.reindex();
        Ok(engine)
    }

    /// Save the engine to `path` (atomic temp-write + rename).
    pub fn save(&self, path: impl AsRef<Path>) -> Result<(), PersistError> {
        let path = path.as_ref();
        let json = self.to_json()?;
        let tmp = path.with_extension("tmp");
        std::fs::write(&tmp, json.as_bytes())?;
        std::fs::rename(&tmp, path)?;
        Ok(())
    }

    /// Load an engine from `path`.
    pub fn load(path: impl AsRef<Path>) -> Result<Self, PersistError> {
        let s = std::fs::read_to_string(path)?;
        Self::from_json(&s)
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use crate::lexicon::Modality;

    fn trained_engine() -> GroundedLanguage {
        let mut gl = GroundedLanguage::new(8, 123);
        gl.enable_bigram_spectrum(16);
        gl.ground("dog", &[1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Visual);
        gl.ground("cat", &[0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Visual);
        for _ in 0..10 {
            gl.learn_from_text("the dog chased the cat");
        }
        gl.comprehend("the dog runs");
        gl
    }

    #[test]
    fn json_round_trip_preserves_everything() {
        let gl = trained_engine();
        let json = gl.to_json().unwrap();
        let mut back = GroundedLanguage::from_json(&json).unwrap();

        assert_eq!(back.lexicon.vocab_count(), gl.lexicon.vocab_count());
        // Index rebuilt → lookups work after load.
        let dog = back.lexicon.find("dog").expect("dog present after load");
        assert_eq!(
            back.lexicon.entry(dog).bindings.len(),
            gl.lexicon.entry(gl.lexicon.find("dog").unwrap()).bindings.len()
        );
        // Phrase table survived (the V1 silent-wipe bug).
        let the = back.lexicon.find("the").unwrap();
        let dogi = back.lexicon.find("dog").unwrap();
        assert_eq!(
            back.phrases.bigram_freq(the, dogi),
            gl.phrases.bigram_freq(the, dogi)
        );
        assert!(back.phrases.bigram_freq(the, dogi) > 0);
        // Concept features survived.
        let cid = back.concepts.intern_text("dog");
        assert!(back.concept_features(cid).is_some());
        // Spectrum survived.
        assert_eq!(
            back.spectrum.as_ref().unwrap().total_events(),
            gl.spectrum.as_ref().unwrap().total_events()
        );
        // Stats survived.
        assert_eq!(back.stats.total_comprehensions, gl.stats.total_comprehensions);
    }

    #[test]
    fn produce_is_identical_after_round_trip() {
        let gl = trained_engine();
        let back = GroundedLanguage::from_json(&gl.to_json().unwrap()).unwrap();
        let intent = [0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
        assert_eq!(gl.produce(&intent, 4, 1).words, back.produce(&intent, 4, 1).words);
    }

    #[test]
    fn bad_magic_is_rejected() {
        let bad = r#"{"magic":"WRONG","version":1,"engine":{}}"#;
        assert!(matches!(
            GroundedLanguage::from_json(bad),
            Err(PersistError::BadMagic { .. }) | Err(PersistError::Serde(_))
        ));
    }

    #[test]
    fn bad_version_is_rejected() {
        let mut gl = GroundedLanguage::new(4, 1);
        gl.learn_from_text("a b c");
        let json = gl.to_json().unwrap().replace("\"version\":1", "\"version\":999");
        assert!(matches!(
            GroundedLanguage::from_json(&json),
            Err(PersistError::BadVersion(999))
        ));
    }

    #[test]
    fn file_save_load_round_trip() {
        let gl = trained_engine();
        let dir = std::env::temp_dir();
        let path = dir.join(format!("nimcp_gl_test_{}.json", std::process::id()));
        gl.save(&path).unwrap();
        let back = GroundedLanguage::load(&path).unwrap();
        assert_eq!(back.lexicon.vocab_count(), gl.lexicon.vocab_count());
        let _ = std::fs::remove_file(&path);
    }
}
