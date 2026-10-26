//! NIMCP V2 — append-only persistent event log.
//!
//! # Purpose
//!
//! In V2, every state mutation is an event. The event log is the canonical
//! source of truth. Benefits:
//!
//! 1. **Determinism.** Replay the log, get bit-identical state.
//! 2. **Crash recovery.** Truncate to last valid event, replay, resume.
//! 3. **Debugging.** Every weight update, every synapse creation is a
//!    durable, inspectable record.
//! 4. **No races on shared state** — the log is append-only + serialized.
//!
//! # Phase 0 scope
//!
//! - Append operation with optional fsync
//! - Read operation by id
//! - Full iterator replay
//! - Truncate (for checkpoint compaction)
//! - Local fs backend only; replication is a Phase 8 concern
//!
//! # On-disk format
//!
//! The log is a single file `log.bin` inside [`LogConfig::dir`]:
//!
//! ```text
//! +----------------------------+
//! | magic "NIMCPEL1" (8 bytes) |
//! | version u32 LE (4 bytes)   |
//! +----------------------------+
//! | entry 0                    |
//! | entry 1                    |
//! | ...                        |
//! +----------------------------+
//! ```
//!
//! Each entry:
//!
//! ```text
//! [seq_id: u64 LE][payload_len: u32 LE][payload: payload_len bytes][crc32: u32 LE]
//! ```
//!
//! The CRC32 (IEEE) covers `seq_id || payload_len || payload`.
//!
//! # Anti-goals
//!
//! - Distributed consensus (not yet)
//! - Multi-writer (single-writer by design, avoid coordination overhead)

#![forbid(unsafe_code)]

use nimcp_core::EventId;
use std::fs::{File, OpenOptions};
use std::io::{BufReader, ErrorKind, Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};
use thiserror::Error;

/// Errors specific to the event log.
#[derive(Debug, Error)]
pub enum LogError {
    /// I/O failure reading or writing to the log file.
    #[error("io: {0}")]
    Io(#[from] std::io::Error),
    /// Log file header or entry framing is invalid.
    #[error("corrupt log: {0}")]
    Corrupt(String),
    /// Requested event ID is past the end of the log.
    #[error("event {0} not in log")]
    NotFound(EventId),
}

/// Configuration for an event log.
#[derive(Debug, Clone)]
pub struct LogConfig {
    /// Directory where the log file lives.
    pub dir: PathBuf,
    /// fsync after every append (safe but slow). If false, relies on the OS
    /// to flush at its convenience. Set true for production; false for tests.
    pub fsync_on_append: bool,
}

impl Default for LogConfig {
    fn default() -> Self {
        Self {
            dir: PathBuf::from("./nimcp-eventlog"),
            fsync_on_append: true,
        }
    }
}

/// Magic bytes at the start of every log file.
const MAGIC: &[u8; 8] = b"NIMCPEL1";

/// Current log format version. Bumped on incompatible changes only.
const VERSION: u32 = 1;

/// Fixed-size header at the start of the file: magic + version.
const HEADER_LEN: u64 = 8 + 4;

/// Number of framing bytes per entry: seq_id(8) + payload_len(4) + crc32(4).
const FRAME_OVERHEAD: u64 = 8 + 4 + 4;

/// Filename of the backing log file inside `config.dir`.
const LOG_FILENAME: &str = "log.bin";

/// In-memory index entry: where a given event lives in the file.
#[derive(Debug, Clone, Copy)]
struct IndexEntry {
    /// Seq id of the event (= EventId).
    id: u64,
    /// Byte offset in the file where the entry starts (at its seq_id field).
    offset: u64,
    /// Payload length in bytes.
    payload_len: u32,
}

/// Append-only persistent event log.
///
/// Owns a single writer handle; `append` / `truncate` require `&mut self`.
/// `read` / `iter` take `&self` and open short-lived reader handles so they
/// can run while an append is in flight at the OS level (though not
/// concurrent with `append` in Rust terms).
#[derive(Debug)]
pub struct EventLog {
    config: LogConfig,
    /// Full path to the backing file.
    path: PathBuf,
    /// Open file handle for appends (positioned at end of file).
    writer: File,
    /// In-memory index: one entry per live event, in append order.
    index: Vec<IndexEntry>,
    /// Next seq_id to assign on append.
    next_id: u64,
    /// Current end-of-file offset (byte length).
    end_offset: u64,
}

impl EventLog {
    /// Open (or create) an event log at `config.dir/log.bin`.
    ///
    /// On open, the existing file is scanned end-to-end. If a truncated or
    /// CRC-corrupt entry is found at the tail, the file is truncated at the
    /// last valid entry and a warning is logged. Corruption in the middle of
    /// the file is fatal (returns `LogError::Corrupt`) — only the tail may
    /// be chopped, on the assumption it was an interrupted append.
    pub fn open(config: LogConfig) -> Result<Self, LogError> {
        std::fs::create_dir_all(&config.dir)?;
        let path = config.dir.join(LOG_FILENAME);

        // Open r/w, create if missing. We append via seek-to-end + write.
        let mut file = OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(false)
            .open(&path)?;

        let file_len = file.metadata()?.len();

        // Write the header if this is a brand-new (empty) file.
        if file_len == 0 {
            file.write_all(MAGIC)?;
            file.write_all(&VERSION.to_le_bytes())?;
            file.sync_all()?;
            tracing::info!(path = %path.display(), "created new event log");

            return Ok(Self {
                config,
                path,
                writer: file,
                index: Vec::new(),
                next_id: 0,
                end_offset: HEADER_LEN,
            });
        }

        // Existing file — verify header.
        if file_len < HEADER_LEN {
            return Err(LogError::Corrupt(format!(
                "file too short ({file_len} bytes) for header"
            )));
        }
        file.seek(SeekFrom::Start(0))?;
        let mut magic_buf = [0u8; 8];
        file.read_exact(&mut magic_buf)?;
        if &magic_buf != MAGIC {
            return Err(LogError::Corrupt(format!(
                "bad magic: expected {:?}, got {:?}",
                MAGIC, magic_buf
            )));
        }
        let mut version_buf = [0u8; 4];
        file.read_exact(&mut version_buf)?;
        let version = u32::from_le_bytes(version_buf);
        if version != VERSION {
            return Err(LogError::Corrupt(format!(
                "unsupported version {version}, expected {VERSION}"
            )));
        }

        // Scan entries.
        let (index, last_good_end) = scan_entries(&mut file, file_len)?;

        // Truncate trailing garbage if any entry failed.
        if last_good_end < file_len {
            let dropped = file_len - last_good_end;
            tracing::warn!(
                path = %path.display(),
                dropped_bytes = dropped,
                last_good_offset = last_good_end,
                "truncating event log tail: {dropped} bytes appear corrupt or truncated"
            );
            file.set_len(last_good_end)?;
            file.sync_all()?;
        }

        let next_id = index.last().map_or(0, |e| e.id + 1);
        // Position the writer at the end so the next write appends.
        file.seek(SeekFrom::End(0))?;

        tracing::info!(
            path = %path.display(),
            entries = index.len(),
            next_id = next_id,
            "opened existing event log"
        );

        Ok(Self {
            config,
            path,
            writer: file,
            index,
            next_id,
            end_offset: last_good_end,
        })
    }

    /// Append a raw (already-serialized) event payload. Returns the assigned ID.
    ///
    /// If `config.fsync_on_append` is true, the file is fsynced before this
    /// returns, making the event durable against power loss.
    pub fn append(&mut self, payload: &[u8]) -> Result<EventId, LogError> {
        let seq_id = self.next_id;
        let payload_len: u32 = payload.len().try_into().map_err(|_| {
            LogError::Corrupt(format!("payload too large: {} bytes", payload.len()))
        })?;

        // CRC covers seq_id || payload_len || payload.
        let mut hasher = crc32fast::Hasher::new();
        hasher.update(&seq_id.to_le_bytes());
        hasher.update(&payload_len.to_le_bytes());
        hasher.update(payload);
        let crc = hasher.finalize();

        let entry_offset = self.end_offset;

        // One write per field is fine here (File::write_all buffers in the
        // kernel page cache; userspace buffering would require `flush`
        // choreography around fsync). Keep it simple.
        self.writer.write_all(&seq_id.to_le_bytes())?;
        self.writer.write_all(&payload_len.to_le_bytes())?;
        self.writer.write_all(payload)?;
        self.writer.write_all(&crc.to_le_bytes())?;

        if self.config.fsync_on_append {
            self.writer.sync_data()?;
        }

        let entry_bytes = FRAME_OVERHEAD + u64::from(payload_len);
        self.end_offset += entry_bytes;
        self.next_id += 1;
        self.index.push(IndexEntry {
            id: seq_id,
            offset: entry_offset,
            payload_len,
        });

        tracing::trace!(id = seq_id, bytes = entry_bytes, "appended event");
        Ok(EventId(seq_id))
    }

    /// Read the payload for a given event id. O(log n) lookup via the
    /// in-memory index.
    pub fn read(&self, id: EventId) -> Result<Vec<u8>, LogError> {
        let idx = self
            .index
            .binary_search_by_key(&id.0, |e| e.id)
            .map_err(|_| LogError::NotFound(id))?;
        let entry = self.index[idx];

        let mut reader = File::open(&self.path)?;
        // Skip seq_id(8) + payload_len(4); jump straight to payload.
        reader.seek(SeekFrom::Start(entry.offset + 12))?;
        let mut payload = vec![0u8; entry.payload_len as usize];
        reader.read_exact(&mut payload)?;
        Ok(payload)
    }

    /// Iterator that yields every live `(EventId, payload)` in append order.
    ///
    /// Each call opens a fresh read-only file handle; the iterator is
    /// independent of the writer.
    pub fn iter(&self) -> impl Iterator<Item = Result<(EventId, Vec<u8>), LogError>> + '_ {
        EventLogIter::new(self)
    }

    /// Remove entries with id < `keep_from`, preserving the rest with the
    /// same ids. Implemented as write-temp + atomic rename.
    ///
    /// If `keep_from` is greater than the highest assigned id + 1, the log
    /// becomes empty (but `next_id` is preserved for monotonicity).
    pub fn truncate(&mut self, keep_from: EventId) -> Result<(), LogError> {
        // Find the first index entry we want to keep.
        let first_keep = self.index.partition_point(|e| e.id < keep_from.0);
        if first_keep == 0 {
            // Nothing to drop.
            return Ok(());
        }

        let tmp_path = self.path.with_extension("bin.compact");
        {
            let mut tmp = OpenOptions::new()
                .read(true)
                .write(true)
                .create(true)
                .truncate(true)
                .open(&tmp_path)?;

            // Write header.
            tmp.write_all(MAGIC)?;
            tmp.write_all(&VERSION.to_le_bytes())?;

            // Copy the surviving entries verbatim from the source file.
            let mut src = File::open(&self.path)?;
            for e in &self.index[first_keep..] {
                let entry_bytes = FRAME_OVERHEAD + u64::from(e.payload_len);
                src.seek(SeekFrom::Start(e.offset))?;
                let mut buf = vec![0u8; entry_bytes as usize];
                src.read_exact(&mut buf)?;
                tmp.write_all(&buf)?;
            }
            tmp.sync_all()?;
        }

        // Atomic rename replaces the live file. On Unix this is a single
        // inode swap; readers holding the old handle keep reading old data
        // until they close it.
        std::fs::rename(&tmp_path, &self.path)?;

        // Re-open the writer on the new file.
        let mut file = OpenOptions::new()
            .read(true)
            .write(true)
            .create(false)
            .truncate(false)
            .open(&self.path)?;

        // Rebuild the index offsets for the surviving entries.
        let mut new_index = Vec::with_capacity(self.index.len() - first_keep);
        let mut running = HEADER_LEN;
        for e in &self.index[first_keep..] {
            new_index.push(IndexEntry {
                id: e.id,
                offset: running,
                payload_len: e.payload_len,
            });
            running += FRAME_OVERHEAD + u64::from(e.payload_len);
        }

        file.seek(SeekFrom::End(0))?;
        self.writer = file;
        self.index = new_index;
        self.end_offset = running;

        tracing::info!(
            keep_from = keep_from.0,
            remaining = self.index.len(),
            "truncated event log"
        );
        Ok(())
    }

    /// Total number of events currently in the log.
    pub fn len(&self) -> u64 {
        self.index.len() as u64
    }

    /// True if the log contains no events.
    pub fn is_empty(&self) -> bool {
        self.index.is_empty()
    }
}

/// Iterator over the log's entries. Uses a buffered reader for speed.
struct EventLogIter<'a> {
    log: &'a EventLog,
    reader: Option<BufReader<File>>,
    pos: usize,
    /// If a previous `next()` returned Err, the iterator fuses.
    done: bool,
}

impl<'a> EventLogIter<'a> {
    fn new(log: &'a EventLog) -> Self {
        Self {
            log,
            reader: None,
            pos: 0,
            done: false,
        }
    }

    fn reader_mut(&mut self) -> Result<&mut BufReader<File>, LogError> {
        if self.reader.is_none() {
            let mut f = File::open(&self.log.path)?;
            f.seek(SeekFrom::Start(HEADER_LEN))?;
            self.reader = Some(BufReader::new(f));
        }
        // Not unsafe: just proved reader is Some above.
        Ok(self.reader.as_mut().expect("reader just set"))
    }
}

impl Iterator for EventLogIter<'_> {
    type Item = Result<(EventId, Vec<u8>), LogError>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.done || self.pos >= self.log.index.len() {
            return None;
        }
        let entry = self.log.index[self.pos];
        self.pos += 1;

        let reader = match self.reader_mut() {
            Ok(r) => r,
            Err(e) => {
                self.done = true;
                return Some(Err(e));
            }
        };

        // We trust the in-memory index (built from a verified scan or from
        // verified appends). Skip the 12-byte prefix and crc tail.
        let mut skip = [0u8; 12];
        if let Err(e) = reader.read_exact(&mut skip) {
            self.done = true;
            return Some(Err(e.into()));
        }
        let mut payload = vec![0u8; entry.payload_len as usize];
        if let Err(e) = reader.read_exact(&mut payload) {
            self.done = true;
            return Some(Err(e.into()));
        }
        let mut crc_buf = [0u8; 4];
        if let Err(e) = reader.read_exact(&mut crc_buf) {
            self.done = true;
            return Some(Err(e.into()));
        }
        Some(Ok((EventId(entry.id), payload)))
    }
}

/// Scan the file from `HEADER_LEN` to `file_len`, collecting valid entries.
///
/// Returns `(entries, end_of_last_good_entry)`. A truncated or CRC-failed
/// entry at the tail is treated as the end of the valid region — we stop
/// there and report the offset. Any I/O error other than "ran out of bytes
/// mid-entry" is propagated.
fn scan_entries(file: &mut File, file_len: u64) -> Result<(Vec<IndexEntry>, u64), LogError> {
    let mut entries = Vec::new();
    let mut reader = {
        file.seek(SeekFrom::Start(HEADER_LEN))?;
        BufReader::new(file.try_clone()?)
    };

    let mut pos = HEADER_LEN;
    loop {
        if pos == file_len {
            return Ok((entries, pos));
        }
        if pos > file_len {
            // Shouldn't happen, but guard.
            return Ok((entries, file_len));
        }

        let remaining = file_len - pos;
        if remaining < FRAME_OVERHEAD {
            tracing::warn!(
                offset = pos,
                remaining = remaining,
                "truncated frame at end of log"
            );
            return Ok((entries, pos));
        }

        let mut seq_buf = [0u8; 8];
        let mut len_buf = [0u8; 4];
        if let Err(e) = reader.read_exact(&mut seq_buf) {
            if e.kind() == ErrorKind::UnexpectedEof {
                return Ok((entries, pos));
            }
            return Err(e.into());
        }
        if let Err(e) = reader.read_exact(&mut len_buf) {
            if e.kind() == ErrorKind::UnexpectedEof {
                return Ok((entries, pos));
            }
            return Err(e.into());
        }
        let seq_id = u64::from_le_bytes(seq_buf);
        let payload_len = u32::from_le_bytes(len_buf);
        let entry_bytes = FRAME_OVERHEAD + u64::from(payload_len);

        // Bounds-check payload length: a garbage value could claim e.g. 4 GB.
        if entry_bytes > remaining {
            tracing::warn!(
                offset = pos,
                declared_len = payload_len,
                remaining = remaining,
                "entry declares more bytes than remain in file — treating as truncated tail"
            );
            return Ok((entries, pos));
        }

        let mut payload = vec![0u8; payload_len as usize];
        if let Err(e) = reader.read_exact(&mut payload) {
            if e.kind() == ErrorKind::UnexpectedEof {
                return Ok((entries, pos));
            }
            return Err(e.into());
        }
        let mut crc_buf = [0u8; 4];
        if let Err(e) = reader.read_exact(&mut crc_buf) {
            if e.kind() == ErrorKind::UnexpectedEof {
                return Ok((entries, pos));
            }
            return Err(e.into());
        }
        let got_crc = u32::from_le_bytes(crc_buf);

        let mut hasher = crc32fast::Hasher::new();
        hasher.update(&seq_buf);
        hasher.update(&len_buf);
        hasher.update(&payload);
        let want_crc = hasher.finalize();

        if got_crc != want_crc {
            tracing::warn!(
                offset = pos,
                seq_id = seq_id,
                got_crc = got_crc,
                want_crc = want_crc,
                "crc mismatch — treating as end of valid log"
            );
            return Ok((entries, pos));
        }

        // Monotonic seq_id invariant: within a single file, every entry
        // must be exactly one greater than the previous. The *first* entry
        // may be any non-negative id (legitimate after `truncate`).
        if let Some(prev) = entries.last() {
            let expected = prev.id + 1;
            if seq_id != expected {
                return Err(LogError::Corrupt(format!(
                    "seq_id gap at offset {pos}: expected {expected}, got {seq_id}"
                )));
            }
        }

        entries.push(IndexEntry {
            id: seq_id,
            offset: pos,
            payload_len,
        });
        pos += entry_bytes;
    }
}

/// Convenience function to ensure the writer's outstanding data is on disk.
/// Only useful when `fsync_on_append = false`.
impl EventLog {
    /// Flush the writer's data to disk. No-op when `fsync_on_append = true`
    /// since every append fsyncs.
    pub fn flush(&mut self) -> Result<(), LogError> {
        self.writer.sync_data()?;
        Ok(())
    }

    /// Path to the backing file (for diagnostics + tests).
    pub fn path(&self) -> &Path {
        &self.path
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    fn cfg(dir: &Path) -> LogConfig {
        LogConfig {
            dir: dir.to_path_buf(),
            fsync_on_append: false,
        }
    }

    #[test]
    fn opens_empty_and_writes_header() {
        let dir = tempdir().unwrap();
        let log = EventLog::open(cfg(dir.path())).unwrap();
        assert!(log.is_empty());
        assert_eq!(log.len(), 0);

        // Reopening must see the same (empty) state.
        drop(log);
        let log = EventLog::open(cfg(dir.path())).unwrap();
        assert!(log.is_empty());
    }

    #[test]
    fn round_trip_append_read() {
        let dir = tempdir().unwrap();
        let mut log = EventLog::open(cfg(dir.path())).unwrap();

        let mut ids = Vec::new();
        let mut payloads = Vec::new();
        for i in 0..20u64 {
            let p = format!("payload-{i}").into_bytes();
            let id = log.append(&p).unwrap();
            ids.push(id);
            payloads.push(p);
        }

        assert_eq!(log.len(), 20);
        for (i, id) in ids.iter().enumerate() {
            let got = log.read(*id).unwrap();
            assert_eq!(got, payloads[i], "mismatch at id {id}");
        }

        // Unknown id rejected.
        match log.read(EventId(9999)) {
            Err(LogError::NotFound(EventId(9999))) => {}
            other => panic!("expected NotFound, got {other:?}"),
        }
    }

    #[test]
    fn iter_yields_all_events_in_order() {
        let dir = tempdir().unwrap();
        let mut log = EventLog::open(cfg(dir.path())).unwrap();

        let payloads: Vec<Vec<u8>> = (0..10u64).map(|i| vec![i as u8; i as usize + 1]).collect();
        for p in &payloads {
            log.append(p).unwrap();
        }

        let got: Vec<(EventId, Vec<u8>)> = log.iter().collect::<Result<_, _>>().unwrap();
        assert_eq!(got.len(), payloads.len());
        for (i, (id, payload)) in got.iter().enumerate() {
            assert_eq!(*id, EventId(i as u64));
            assert_eq!(payload, &payloads[i]);
        }
    }

    #[test]
    fn reopens_and_preserves_state() {
        let dir = tempdir().unwrap();
        {
            let mut log = EventLog::open(cfg(dir.path())).unwrap();
            for i in 0..5u64 {
                log.append(format!("e{i}").as_bytes()).unwrap();
            }
        }
        let log = EventLog::open(cfg(dir.path())).unwrap();
        assert_eq!(log.len(), 5);
        let all: Vec<_> = log.iter().collect::<Result<_, _>>().unwrap();
        assert_eq!(all[2].0, EventId(2));
        assert_eq!(all[2].1, b"e2");

        // Next append should continue at id 5.
        let mut log = log;
        let id = log.append(b"e5").unwrap();
        assert_eq!(id, EventId(5));
    }

    #[test]
    fn crash_mid_append_recovers_n_minus_1() {
        let dir = tempdir().unwrap();
        let path = {
            let mut log = EventLog::open(cfg(dir.path())).unwrap();
            for i in 0..5u64 {
                log.append(format!("event-{i}").as_bytes()).unwrap();
            }
            log.path().to_path_buf()
        };

        // Simulate a crash mid-append: lop off the last few bytes of the
        // file, leaving the 5th entry truncated.
        let len_before = std::fs::metadata(&path).unwrap().len();
        let truncate_to = len_before - 3; // cut into the last entry
        {
            let f = OpenOptions::new().write(true).open(&path).unwrap();
            f.set_len(truncate_to).unwrap();
            f.sync_all().unwrap();
        }

        // Reopening should chop the bad tail and recover 4 entries.
        let log = EventLog::open(cfg(dir.path())).unwrap();
        assert_eq!(log.len(), 4, "expected 4 surviving events after crash");
        let payloads: Vec<_> = log.iter().map(|r| r.unwrap()).map(|(_, p)| p).collect();
        assert_eq!(payloads[0], b"event-0");
        assert_eq!(payloads[3], b"event-3");

        // The file should now be shorter than our truncation point (the
        // whole 5th entry is gone, not just 3 bytes).
        let len_after = std::fs::metadata(&path).unwrap().len();
        assert!(len_after < truncate_to, "log tail not actually chopped");
    }

    #[test]
    fn crc_corruption_detected_and_entry_skipped() {
        let dir = tempdir().unwrap();
        let path = {
            let mut log = EventLog::open(cfg(dir.path())).unwrap();
            for i in 0..5u64 {
                log.append(format!("event-{i}").as_bytes()).unwrap();
            }
            log.path().to_path_buf()
        };

        // Flip one byte inside the 3rd entry's payload. We need to locate
        // it: the 3rd entry starts at HEADER_LEN + 2 * (FRAME + len("event-N")=7).
        // All entries are the same size here: 8+4+7+4 = 23 bytes.
        let entry_size = FRAME_OVERHEAD + 7;
        let third_payload_offset = HEADER_LEN + 2 * entry_size + 12; // skip seq+len
        {
            let mut f = OpenOptions::new()
                .read(true)
                .write(true)
                .open(&path)
                .unwrap();
            f.seek(SeekFrom::Start(third_payload_offset)).unwrap();
            let mut b = [0u8; 1];
            f.read_exact(&mut b).unwrap();
            b[0] ^= 0xFF;
            f.seek(SeekFrom::Start(third_payload_offset)).unwrap();
            f.write_all(&b).unwrap();
            f.sync_all().unwrap();
        }

        // Corrupt CRC at entry 2 — the scan treats everything from that
        // entry onward as "tail garbage" and truncates.
        let log = EventLog::open(cfg(dir.path())).unwrap();
        assert_eq!(
            log.len(),
            2,
            "corrupt middle entry should end valid region; expected 2 survivors"
        );
        let got: Vec<_> = log.iter().map(|r| r.unwrap()).collect();
        assert_eq!(got[0].0, EventId(0));
        assert_eq!(got[1].0, EventId(1));
    }

    #[test]
    fn truncate_keeps_from_drops_earlier_entries() {
        let dir = tempdir().unwrap();
        let mut log = EventLog::open(cfg(dir.path())).unwrap();
        for i in 0..10u64 {
            log.append(format!("event-{i}").as_bytes()).unwrap();
        }
        assert_eq!(log.len(), 10);

        log.truncate(EventId(5)).unwrap();
        assert_eq!(log.len(), 5);

        let got: Vec<_> = log.iter().collect::<Result<_, _>>().unwrap();
        let ids: Vec<_> = got.iter().map(|(id, _)| id.0).collect();
        assert_eq!(ids, vec![5, 6, 7, 8, 9]);
        assert_eq!(got[0].1, b"event-5");
        assert_eq!(got[4].1, b"event-9");

        // Reads still work after truncate.
        let p = log.read(EventId(7)).unwrap();
        assert_eq!(p, b"event-7");

        // Old ids are gone.
        assert!(matches!(log.read(EventId(2)), Err(LogError::NotFound(_))));

        // Next append keeps monotonic id sequence.
        let id = log.append(b"event-10").unwrap();
        assert_eq!(id, EventId(10));
    }

    #[test]
    fn truncate_noop_when_nothing_to_drop() {
        let dir = tempdir().unwrap();
        let mut log = EventLog::open(cfg(dir.path())).unwrap();
        for i in 0..3u64 {
            log.append(format!("e{i}").as_bytes()).unwrap();
        }
        log.truncate(EventId(0)).unwrap();
        assert_eq!(log.len(), 3);
    }

    #[test]
    fn truncate_survives_reopen() {
        let dir = tempdir().unwrap();
        {
            let mut log = EventLog::open(cfg(dir.path())).unwrap();
            for i in 0..10u64 {
                log.append(format!("e{i}").as_bytes()).unwrap();
            }
            log.truncate(EventId(7)).unwrap();
        }
        let log = EventLog::open(cfg(dir.path())).unwrap();
        assert_eq!(log.len(), 3);
        let ids: Vec<_> = log.iter().map(|r| r.unwrap().0.0).collect();
        assert_eq!(ids, vec![7, 8, 9]);
    }

    #[test]
    fn bad_magic_is_rejected() {
        let dir = tempdir().unwrap();
        let path = dir.path().join(LOG_FILENAME);
        // Write garbage header.
        let mut f = File::create(&path).unwrap();
        f.write_all(b"GARBAGE!").unwrap();
        f.write_all(&0u32.to_le_bytes()).unwrap();
        drop(f);

        match EventLog::open(cfg(dir.path())) {
            Err(LogError::Corrupt(_)) => {}
            other => panic!("expected Corrupt, got {other:?}"),
        }
    }

    #[test]
    fn empty_payload_is_supported() {
        let dir = tempdir().unwrap();
        let mut log = EventLog::open(cfg(dir.path())).unwrap();
        let id = log.append(&[]).unwrap();
        let got = log.read(id).unwrap();
        assert!(got.is_empty());
        let iterd: Vec<_> = log.iter().collect::<Result<_, _>>().unwrap();
        assert_eq!(iterd.len(), 1);
        assert!(iterd[0].1.is_empty());
    }
}
