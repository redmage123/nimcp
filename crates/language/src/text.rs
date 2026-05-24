//! Tokenization — port of V1's `tokenize_text`.
//!
//! Lowercases and splits on anything that is not an ASCII alphanumeric or
//! an apostrophe (so contractions like `don't` survive for the later
//! negation pass). Empty tokens are dropped.

/// Split `text` into lowercased word tokens.
#[must_use]
pub fn tokenize(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut cur = String::new();
    for ch in text.chars() {
        if ch.is_ascii_alphanumeric() || ch == '\'' {
            cur.push(ch.to_ascii_lowercase());
        } else if !cur.is_empty() {
            out.push(std::mem::take(&mut cur));
        }
    }
    if !cur.is_empty() {
        out.push(cur);
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn splits_and_lowercases() {
        assert_eq!(tokenize("The Dog runs."), vec!["the", "dog", "runs"]);
    }

    #[test]
    fn keeps_contractions() {
        assert_eq!(tokenize("don't stop"), vec!["don't", "stop"]);
    }

    #[test]
    fn collapses_punctuation_and_whitespace() {
        assert_eq!(tokenize("  a,,b   c! "), vec!["a", "b", "c"]);
    }

    #[test]
    fn empty_is_empty() {
        assert!(tokenize("   ,.!  ").is_empty());
        assert!(tokenize("").is_empty());
    }
}
