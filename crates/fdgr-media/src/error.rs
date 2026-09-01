#![forbid(unsafe_code)]
//! Stable media-inspection errors.

use crate::FourCc;
use std::error::Error;
use std::fmt::{self, Display, Formatter};
use std::io;
use std::path::PathBuf;

/// Stable bounded ISO BMFF inspection failures.
#[derive(Debug)]
pub enum MediaError {
    /// A filesystem metadata or open operation failed.
    FileIo {
        /// Stable operation name.
        operation: &'static str,
        /// Source path.
        path: PathBuf,
        /// Operating-system error.
        source: io::Error,
    },
    /// A reader or seek operation failed at an exact byte offset.
    Io {
        /// Stable operation name.
        operation: &'static str,
        /// Byte offset associated with the operation.
        offset: u64,
        /// Reader error.
        source: io::Error,
    },
    /// Source path is a symlink.
    SourceSymlink(PathBuf),
    /// Source path is not a regular file.
    SourceNotRegular(PathBuf),
    /// The file is too short to contain one box header.
    FileTooShort {
        /// Exact file length.
        length: u64,
    },
    /// Reader length changed relative to the declared inspection snapshot.
    FileLengthChanged {
        /// Declared snapshot length.
        expected: u64,
        /// Observed final reader length.
        observed: u64,
    },
    /// A box header or required fixed-width prefix extends beyond its enclosing range.
    Truncated {
        /// Box or field byte offset.
        offset: u64,
        /// Bytes required.
        needed: u64,
        /// Bytes available in the enclosing range.
        available: u64,
    },
    /// Box size is smaller than its own header or overflows its parent range.
    InvalidBoxSize {
        /// Box offset.
        offset: u64,
        /// Declared size.
        size: u64,
        /// Header size.
        header_size: u64,
        /// Enclosing range end.
        parent_end: u64,
    },
    /// A zero-sized box appeared outside the top-level file range.
    NestedZeroSizedBox {
        /// Box offset.
        offset: u64,
        /// Box type.
        box_type: FourCc,
    },
    /// A recognized semantic box appeared under an incompatible parent.
    InvalidBoxParent {
        /// Child box type.
        box_type: FourCc,
        /// Enclosing box, or `None` at the file root.
        parent: Option<FourCc>,
    },
    /// A checked arithmetic or integer conversion overflowed.
    ArithmeticOverflow,
    /// One parser limit was zero and therefore invalid.
    InvalidLimit {
        /// Stable limit field name.
        name: &'static str,
    },
    /// Explicit traversal depth exceeded its hard limit.
    DepthLimitExceeded {
        /// Requested depth.
        actual: usize,
        /// Maximum depth.
        maximum: usize,
    },
    /// Total box count exceeded its hard limit.
    BoxLimitExceeded {
        /// Observed count at refusal.
        actual: u64,
        /// Maximum count.
        maximum: u64,
    },
    /// Track count exceeded its hard limit.
    TrackLimitExceeded {
        /// Observed count at refusal.
        actual: usize,
        /// Maximum count.
        maximum: usize,
    },
    /// A table entry count exceeded its hard limit.
    TableEntryLimitExceeded {
        /// Owning box.
        box_type: FourCc,
        /// Declared entries.
        actual: u64,
        /// Maximum entries.
        maximum: u64,
    },
    /// A table payload byte count exceeded its hard limit.
    TableByteLimitExceeded {
        /// Owning box.
        box_type: FourCc,
        /// Declared or derived bytes.
        actual: u64,
        /// Maximum bytes.
        maximum: u64,
    },
    /// Compatible brand count exceeded its hard limit.
    BrandLimitExceeded {
        /// Observed count.
        actual: usize,
        /// Maximum count.
        maximum: usize,
    },
    /// Sample-description count exceeded its hard limit.
    SampleDescriptionLimitExceeded {
        /// Observed count.
        actual: u64,
        /// Maximum count.
        maximum: u64,
    },
    /// A singleton box appeared more than once in one semantic scope.
    DuplicateBox {
        /// Duplicated box type.
        box_type: FourCc,
        /// Track scope when known.
        track_index: Option<usize>,
    },
    /// A full-box version is not supported by this reference parser.
    UnsupportedVersion {
        /// Box type.
        box_type: FourCc,
        /// Observed version.
        version: u8,
    },
    /// Required movie or track metadata is absent.
    MissingRequiredBox {
        /// Missing box type.
        box_type: FourCc,
        /// Track scope when known.
        track_index: Option<usize>,
    },
    /// Movie or media timescale was zero.
    ZeroTimescale {
        /// Box type.
        box_type: FourCc,
        /// Track scope when known.
        track_index: Option<usize>,
    },
    /// Fixed-width table size does not fit the box payload exactly.
    InvalidTableLength {
        /// Owning box.
        box_type: FourCc,
        /// Required bytes.
        expected: u64,
        /// Available bytes.
        available: u64,
    },
    /// Two sample tables disagree about sample cardinality.
    SampleCountMismatch {
        /// Track index.
        track_index: usize,
        /// First table or field.
        left_name: &'static str,
        /// First count.
        left: u64,
        /// Second table or field.
        right_name: &'static str,
        /// Second count.
        right: u64,
    },
    /// A nonempty classic sample table has no `stco` or `co64` table.
    MissingChunkOffsetTable {
        /// Track index.
        track_index: usize,
    },
    /// A chunk offset does not point inside the file.
    ChunkOffsetOutOfRange {
        /// Track index.
        track_index: usize,
        /// Zero-based chunk-table position.
        entry_index: u64,
        /// Observed file offset.
        offset: u64,
        /// Exact file length.
        file_length: u64,
    },
    /// A chunk offset lies inside the file but outside every `mdat` payload.
    ChunkOffsetOutsideMediaData {
        /// Track index.
        track_index: usize,
        /// Zero-based chunk-table position.
        entry_index: u64,
        /// Observed file offset.
        offset: u64,
    },
    /// `stsc` fields or ordering are invalid.
    InvalidSampleToChunk {
        /// Track index.
        track_index: usize,
        /// Zero-based table position.
        entry_index: u64,
        /// Explanation suitable for a stable diagnostic.
        reason: &'static str,
    },
    /// A sample-description index exceeds the `stsd` cardinality.
    SampleDescriptionOutOfRange {
        /// Track index.
        track_index: usize,
        /// Highest referenced one-based description index.
        referenced: u32,
        /// Declared description count.
        available: u32,
    },
    /// Sync-sample table is non-increasing or references an unavailable sample.
    InvalidSyncSample {
        /// Track index.
        track_index: usize,
        /// Zero-based table position.
        entry_index: u64,
        /// One-based sample number.
        sample_number: u32,
        /// Known sample count, if available.
        sample_count: Option<u64>,
    },
    /// Compact sample-size field width is unsupported.
    InvalidCompactSampleFieldSize {
        /// Observed bit width.
        field_size: u8,
    },
    /// A track declared the reserved zero track ID.
    InvalidTrackId {
        /// Zero-based track index.
        track_index: usize,
    },
    /// Two tracks declared the same nonzero track ID.
    DuplicateTrackId {
        /// Repeated track ID.
        track_id: u32,
    },
}

impl Display for MediaError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::FileIo {
                operation,
                path,
                source,
            } => write!(
                formatter,
                "media file operation {operation} failed at {}: {source}",
                path.display()
            ),
            Self::Io {
                operation,
                offset,
                source,
            } => write!(
                formatter,
                "media reader operation {operation} failed at byte {offset}: {source}"
            ),
            Self::SourceSymlink(path) => {
                write!(formatter, "media source must not be a symlink: {}", path.display())
            }
            Self::SourceNotRegular(path) => {
                write!(formatter, "media source is not a regular file: {}", path.display())
            }
            Self::FileTooShort { length } => {
                write!(formatter, "media file is only {length} bytes; at least 8 are required")
            }
            Self::FileLengthChanged { expected, observed } => write!(
                formatter,
                "media length changed during inspection: expected {expected}, observed {observed}"
            ),
            Self::Truncated {
                offset,
                needed,
                available,
            } => write!(
                formatter,
                "media structure at byte {offset} needs {needed} bytes; {available} remain"
            ),
            Self::InvalidBoxSize {
                offset,
                size,
                header_size,
                parent_end,
            } => write!(
                formatter,
                "box at byte {offset} declares size {size} with header {header_size} inside parent ending at {parent_end}"
            ),
            Self::NestedZeroSizedBox { offset, box_type } => write!(
                formatter,
                "nested box {box_type} at byte {offset} uses forbidden size zero"
            ),
            Self::InvalidBoxParent { box_type, parent } => write!(
                formatter,
                "box {box_type} is not valid under parent {parent:?}"
            ),
            Self::ArithmeticOverflow => formatter.write_str("media arithmetic overflow"),
            Self::InvalidLimit { name } => {
                write!(formatter, "parse limit {name} must be positive")
            }
            Self::DepthLimitExceeded { actual, maximum } => write!(
                formatter,
                "media nesting depth {actual} exceeds maximum {maximum}"
            ),
            Self::BoxLimitExceeded { actual, maximum } => {
                write!(formatter, "media box count {actual} exceeds maximum {maximum}")
            }
            Self::TrackLimitExceeded { actual, maximum } => {
                write!(formatter, "media track count {actual} exceeds maximum {maximum}")
            }
            Self::TableEntryLimitExceeded {
                box_type,
                actual,
                maximum,
            } => write!(
                formatter,
                "{box_type} declares {actual} entries; maximum is {maximum}"
            ),
            Self::TableByteLimitExceeded {
                box_type,
                actual,
                maximum,
            } => write!(
                formatter,
                "{box_type} requires {actual} table bytes; maximum is {maximum}"
            ),
            Self::BrandLimitExceeded { actual, maximum } => write!(
                formatter,
                "compatible brand count {actual} exceeds maximum {maximum}"
            ),
            Self::SampleDescriptionLimitExceeded { actual, maximum } => write!(
                formatter,
                "sample-description count {actual} exceeds maximum {maximum}"
            ),
            Self::DuplicateBox {
                box_type,
                track_index,
            } => write!(
                formatter,
                "duplicate {box_type} in track scope {track_index:?}"
            ),
            Self::UnsupportedVersion {
                box_type,
                version,
            } => write!(formatter, "unsupported {box_type} version {version}"),
            Self::MissingRequiredBox {
                box_type,
                track_index,
            } => write!(
                formatter,
                "required {box_type} is missing in track scope {track_index:?}"
            ),
            Self::ZeroTimescale {
                box_type,
                track_index,
            } => write!(
                formatter,
                "{box_type} timescale is zero in track scope {track_index:?}"
            ),
            Self::InvalidTableLength {
                box_type,
                expected,
                available,
            } => write!(
                formatter,
                "{box_type} table requires {expected} bytes; {available} are available"
            ),
            Self::SampleCountMismatch {
                track_index,
                left_name,
                left,
                right_name,
                right,
            } => write!(
                formatter,
                "track {track_index} sample count mismatch: {left_name}={left}, {right_name}={right}"
            ),
            Self::MissingChunkOffsetTable { track_index } => write!(
                formatter,
                "track {track_index} has samples but no stco or co64 table"
            ),
            Self::ChunkOffsetOutOfRange {
                track_index,
                entry_index,
                offset,
                file_length,
            } => write!(
                formatter,
                "track {track_index} chunk offset entry {entry_index} points to {offset}, outside file length {file_length}"
            ),
            Self::ChunkOffsetOutsideMediaData {
                track_index,
                entry_index,
                offset,
            } => write!(
                formatter,
                "track {track_index} chunk offset entry {entry_index} points to {offset}, outside every mdat payload"
            ),
            Self::InvalidSampleToChunk {
                track_index,
                entry_index,
                reason,
            } => write!(
                formatter,
                "track {track_index} stsc entry {entry_index} is invalid: {reason}"
            ),
            Self::SampleDescriptionOutOfRange {
                track_index,
                referenced,
                available,
            } => write!(
                formatter,
                "track {track_index} references sample description {referenced}, but stsd declares {available}"
            ),
            Self::InvalidSyncSample {
                track_index,
                entry_index,
                sample_number,
                sample_count,
            } => write!(
                formatter,
                "track {track_index} stss entry {entry_index} references sample {sample_number} with known count {sample_count:?}"
            ),
            Self::InvalidCompactSampleFieldSize { field_size } => write!(
                formatter,
                "stz2 field size must be 4, 8, or 16 bits; observed {field_size}"
            ),
            Self::InvalidTrackId { track_index } => {
                write!(formatter, "track {track_index} declares reserved track ID zero")
            }
            Self::DuplicateTrackId { track_id } => {
                write!(formatter, "duplicate nonzero track ID {track_id}")
            }
        }
    }
}

impl Error for MediaError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::FileIo { source, .. } | Self::Io { source, .. } => Some(source),
            _ => None,
        }
    }
}
