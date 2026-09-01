#![forbid(unsafe_code)]
//! Public bounded sample-window contract and orchestration.

mod scan;
mod window;
#[cfg(test)]
mod tests;

use crate::{IsoBmffSummary, MediaError, ParseLimits, inspect_iso_bmff};
use std::error::Error;
use std::fmt::{self, Display, Formatter};
use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom};
use std::path::Path;

/// Default maximum number of records returned by one sample-window request.
pub const DEFAULT_SAMPLE_WINDOW_RECORDS: usize = 4096;
/// Default maximum table-entry reads spent locating and expanding one sample window.
pub const DEFAULT_SAMPLE_INDEX_SCAN_BUDGET: u64 = 1_000_000;

/// Requested zero-based sample interval on one exact track.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct SampleWindowRequest {
    /// Nonzero ISO BMFF track identity.
    pub track_id: u32,
    /// Zero-based first sample.
    pub start_sample: u64,
    /// Maximum records to return.
    pub max_samples: usize,
}

/// Independent work ceilings for exact sample-index expansion.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct SampleWindowLimits {
    /// Maximum records permitted in one response.
    pub max_records: usize,
    /// Maximum classic-table entries read while locating and expanding the response.
    pub max_index_entries_scanned: u64,
}

impl Default for SampleWindowLimits {
    fn default() -> Self {
        Self {
            max_records: DEFAULT_SAMPLE_WINDOW_RECORDS,
            max_index_entries_scanned: DEFAULT_SAMPLE_INDEX_SCAN_BUDGET,
        }
    }
}

/// One exact classic-table sample record.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SampleRecord {
    /// Zero-based sample index within the track.
    pub sample_index: u64,
    /// Decode timestamp in track timescale units.
    pub decode_time: u64,
    /// Composition timestamp in track timescale units; version-1 `ctts` may make this negative.
    pub composition_time: i128,
    /// Decode duration in track timescale units.
    pub duration: u32,
    /// Exact source-file byte offset.
    pub byte_offset: u64,
    /// Exact encoded sample length.
    pub byte_length: u32,
    /// Whether the sample is a sync sample according to `stss`, or all-sync when `stss` is absent.
    pub is_sync: bool,
    /// One-based sample-description index selected by `stsc`.
    pub sample_description_index: u32,
}

/// Bounded exact sample window and its evidence/accounting context.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TrackSampleWindow {
    /// Track identity.
    pub track_id: u32,
    /// Track timescale.
    pub timescale: u32,
    /// Total validated classic-table sample count.
    pub total_samples: u64,
    /// Requested zero-based start.
    pub start_sample: u64,
    /// Requested maximum records.
    pub requested_max_samples: usize,
    /// Whether the returned window reaches the end of the classic sample table.
    pub complete: bool,
    /// Table entries charged to the expansion budget.
    pub index_entries_scanned: u64,
    /// Ordered sample records.
    pub samples: Vec<SampleRecord>,
}

/// Stable sample-index failures separate from container-inspection failures.
#[derive(Debug)]
pub enum SampleIndexError {
    /// The underlying bounded container inspection failed.
    Media(MediaError),
    /// A request or response limit was zero or exceeded its ceiling.
    InvalidWindowLimit {
        /// Requested records.
        requested: usize,
        /// Maximum records.
        maximum: usize,
    },
    /// Table reads required to answer the request exceeded the explicit budget.
    IndexScanBudgetExceeded {
        /// Reads requested at refusal.
        requested: u64,
        /// Maximum reads.
        maximum: u64,
    },
    /// The named track does not exist in the validated movie.
    TrackNotFound {
        /// Requested track identity.
        track_id: u32,
    },
    /// The requested start lies beyond the validated sample count.
    SampleStartOutOfRange {
        /// Track identity.
        track_id: u32,
        /// Requested zero-based start.
        start_sample: u64,
        /// Validated sample count.
        total_samples: u64,
    },
    /// A required classic sample table is absent from the focused index.
    ClassicTableUnavailable {
        /// Track identity.
        track_id: u32,
        /// Stable table name.
        table: &'static str,
    },
    /// The focused index disagrees with the already validated summary.
    InconsistentIndex {
        /// Track identity when known.
        track_id: Option<u32>,
        /// Stable explanation.
        reason: &'static str,
    },
    /// A derived sample byte interval is not fully contained in one `mdat` payload.
    SampleOutsideMediaData {
        /// Track identity.
        track_id: u32,
        /// Zero-based sample index.
        sample_index: u64,
        /// Derived byte offset.
        byte_offset: u64,
        /// Derived byte length.
        byte_length: u32,
    },
}

impl Display for SampleIndexError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Media(error) => write!(formatter, "sample index media error: {error}"),
            Self::InvalidWindowLimit { requested, maximum } => write!(
                formatter,
                "sample window requests {requested} records; maximum is {maximum}"
            ),
            Self::IndexScanBudgetExceeded { requested, maximum } => write!(
                formatter,
                "sample index requires {requested} table reads; budget is {maximum}"
            ),
            Self::TrackNotFound { track_id } => {
                write!(formatter, "sample-index track {track_id} was not found")
            }
            Self::SampleStartOutOfRange {
                track_id,
                start_sample,
                total_samples,
            } => write!(
                formatter,
                "sample-index start {start_sample} exceeds track {track_id} sample count {total_samples}"
            ),
            Self::ClassicTableUnavailable { track_id, table } => write!(
                formatter,
                "track {track_id} has no validated classic {table} table"
            ),
            Self::InconsistentIndex { track_id, reason } => write!(
                formatter,
                "focused sample index for track {track_id:?} is inconsistent: {reason}"
            ),
            Self::SampleOutsideMediaData {
                track_id,
                sample_index,
                byte_offset,
                byte_length,
            } => write!(
                formatter,
                "track {track_id} sample {sample_index} byte range {byte_offset}+{byte_length} is outside mdat"
            ),
        }
    }
}

impl Error for SampleIndexError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Media(error) => Some(error),
            _ => None,
        }
    }
}

impl From<MediaError> for SampleIndexError {
    fn from(error: MediaError) -> Self {
        Self::Media(error)
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub(super) struct ByteRange {
    pub(super) start: u64,
    pub(super) end: u64,
}

#[derive(Clone, Copy, Debug)]
pub(super) struct FixedTable {
    pub(super) entries_start: u64,
    pub(super) count: u64,
    pub(super) entry_bytes: u64,
}

#[derive(Clone, Copy, Debug)]
pub(super) struct CompositionTable {
    pub(super) table: FixedTable,
    pub(super) version: u8,
}

#[derive(Clone, Copy, Debug)]
pub(super) enum SampleSizeTable {
    Constant { count: u64, sample_size: u32 },
    U32 { entries_start: u64, count: u64 },
    Compact {
        entries_start: u64,
        count: u64,
        field_size: u8,
    },
}

impl SampleSizeTable {
    pub(super) const fn count(self) -> u64 {
        match self {
            Self::Constant { count, .. } | Self::U32 { count, .. } | Self::Compact { count, .. } => {
                count
            }
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub(super) struct SampleToChunkEntry {
    pub(super) first_chunk: u32,
    pub(super) samples_per_chunk: u32,
    pub(super) sample_description_index: u32,
}

#[derive(Clone, Debug, Default)]
pub(super) struct TrackIndex {
    pub(super) track_id: Option<u32>,
    pub(super) timescale: Option<u32>,
    pub(super) stts: Option<FixedTable>,
    pub(super) ctts: Option<CompositionTable>,
    pub(super) sample_sizes: Option<SampleSizeTable>,
    pub(super) chunk_offsets: Option<FixedTable>,
    pub(super) sample_to_chunk: Vec<SampleToChunkEntry>,
    pub(super) sync_samples: Option<FixedTable>,
}

#[derive(Clone, Debug, Default)]
pub(super) struct FocusedIndex {
    pub(super) tracks: Vec<TrackIndex>,
    pub(super) media_data_ranges: Vec<ByteRange>,
}

#[derive(Clone, Copy, Debug)]
pub(super) struct ScanBudget {
    pub(super) used: u64,
    maximum: u64,
}

impl ScanBudget {
    pub(super) const fn new(maximum: u64) -> Self {
        Self { used: 0, maximum }
    }

    pub(super) fn charge(&mut self, amount: u64) -> Result<(), SampleIndexError> {
        let requested = self
            .used
            .checked_add(amount)
            .ok_or(MediaError::ArithmeticOverflow)?;
        if requested > self.maximum {
            return Err(SampleIndexError::IndexScanBudgetExceeded {
                requested,
                maximum: self.maximum,
            });
        }
        self.used = requested;
        Ok(())
    }
}

/// Reads one exact sample window from a regular non-symlink file.
///
/// # Errors
///
/// Returns a typed filesystem, container, table, request, budget, or byte-range error.
pub fn read_classic_sample_window_file(
    path: impl AsRef<Path>,
    request: SampleWindowRequest,
    parse_limits: ParseLimits,
    window_limits: SampleWindowLimits,
) -> Result<(IsoBmffSummary, TrackSampleWindow), SampleIndexError> {
    let path = path.as_ref();
    let metadata = fs::symlink_metadata(path).map_err(|source| MediaError::FileIo {
        operation: "sample_index_source_metadata",
        path: path.to_path_buf(),
        source,
    })?;
    if metadata.file_type().is_symlink() {
        return Err(MediaError::SourceSymlink(path.to_path_buf()).into());
    }
    if !metadata.is_file() {
        return Err(MediaError::SourceNotRegular(path.to_path_buf()).into());
    }
    let mut file = File::open(path).map_err(|source| MediaError::FileIo {
        operation: "open_sample_index_source",
        path: path.to_path_buf(),
        source,
    })?;
    read_classic_sample_window(
        &mut file,
        metadata.len(),
        request,
        parse_limits,
        window_limits,
    )
}

/// Validates a seekable ISO BMFF input, then expands one bounded classic sample window.
///
/// # Errors
///
/// Returns a typed container, table, request, budget, or byte-range error. Fragmented `moof`/`trun`
/// samples are deliberately outside this API; only validated classic `stbl` samples are returned.
pub fn read_classic_sample_window<R: Read + Seek>(
    reader: &mut R,
    file_length: u64,
    request: SampleWindowRequest,
    parse_limits: ParseLimits,
    window_limits: SampleWindowLimits,
) -> Result<(IsoBmffSummary, TrackSampleWindow), SampleIndexError> {
    validate_window_limits(request, window_limits)?;
    let summary = inspect_iso_bmff(reader, file_length, parse_limits)?;
    let focused = scan::scan_focused_index(reader, file_length, parse_limits)?;
    if focused.tracks.len() != summary.tracks.len() {
        return inconsistent(None, "track cardinality differs from validated summary");
    }
    let (track_index, track_summary) = summary
        .tracks
        .iter()
        .enumerate()
        .find(|(_, track)| track.track_id == request.track_id)
        .ok_or(SampleIndexError::TrackNotFound {
            track_id: request.track_id,
        })?;
    let track_timescale = track_summary.timescale;
    let total_samples = track_summary.sample_count.ok_or(
        SampleIndexError::ClassicTableUnavailable {
            track_id: request.track_id,
            table: "stsz_or_stz2",
        },
    )?;
    let track = focused
        .tracks
        .get(track_index)
        .ok_or(SampleIndexError::InconsistentIndex {
            track_id: Some(request.track_id),
            reason: "validated track has no focused descriptor",
        })?;
    if track.track_id != Some(request.track_id) || track.timescale != Some(track_timescale) {
        return inconsistent(
            Some(request.track_id),
            "track identity or timescale differs from validated summary",
        );
    }
    if request.start_sample > total_samples {
        return Err(SampleIndexError::SampleStartOutOfRange {
            track_id: request.track_id,
            start_sample: request.start_sample,
            total_samples,
        });
    }
    let remaining = total_samples.saturating_sub(request.start_sample);
    let requested_u64 = u64::try_from(request.max_samples)
        .map_err(|_| MediaError::ArithmeticOverflow)?;
    let returned_u64 = remaining.min(requested_u64);
    if returned_u64 == 0 {
        return Ok((
            summary,
            TrackSampleWindow {
                track_id: request.track_id,
                timescale: track_timescale,
                total_samples,
                start_sample: request.start_sample,
                requested_max_samples: request.max_samples,
                complete: true,
                index_entries_scanned: 0,
                samples: Vec::new(),
            },
        ));
    }
    let mut budget = ScanBudget::new(window_limits.max_index_entries_scanned);
    let samples = window::expand_window(
        reader,
        request,
        total_samples,
        track,
        &focused.media_data_ranges,
        returned_u64,
        &mut budget,
    )?;
    let end = request
        .start_sample
        .checked_add(returned_u64)
        .ok_or(MediaError::ArithmeticOverflow)?;
    let observed_length = reader.seek(SeekFrom::End(0)).map_err(|source| MediaError::Io {
        operation: "sample_index_seek_end",
        offset: file_length,
        source,
    })?;
    if observed_length != file_length {
        return Err(MediaError::FileLengthChanged {
            expected: file_length,
            observed: observed_length,
        }
        .into());
    }
    Ok((
        summary,
        TrackSampleWindow {
            track_id: request.track_id,
            timescale: track_timescale,
            total_samples,
            start_sample: request.start_sample,
            requested_max_samples: request.max_samples,
            complete: end == total_samples,
            index_entries_scanned: budget.used,
            samples,
        },
    ))
}

fn validate_window_limits(
    request: SampleWindowRequest,
    limits: SampleWindowLimits,
) -> Result<(), SampleIndexError> {
    if request.max_samples == 0
        || limits.max_records == 0
        || request.max_samples > limits.max_records
    {
        return Err(SampleIndexError::InvalidWindowLimit {
            requested: request.max_samples,
            maximum: limits.max_records,
        });
    }
    if limits.max_index_entries_scanned == 0 {
        return Err(SampleIndexError::IndexScanBudgetExceeded {
            requested: 1,
            maximum: 0,
        });
    }
    Ok(())
}

pub(super) fn inconsistent<T>(
    track_id: Option<u32>,
    reason: &'static str,
) -> Result<T, SampleIndexError> {
    Err(SampleIndexError::InconsistentIndex { track_id, reason })
}
