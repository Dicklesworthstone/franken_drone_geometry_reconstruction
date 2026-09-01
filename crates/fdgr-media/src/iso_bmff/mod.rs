#![forbid(unsafe_code)]
#![allow(clippy::too_many_lines)]
//! Nonrecursive bounded ISO BMFF metadata and classic sample-table inspection.

mod reader;
mod tables;
#[cfg(test)]
mod tests;

use crate::{FourCc, MediaError};
use reader::{read_box_header, require_version_zero};
use std::collections::BTreeSet;
use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom};
use std::path::Path;
use tables::{parse_leaf, validate_chunk_offset_tables};

/// Hard parser limits applied before allocation or traversal expansion.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ParseLimits {
    /// Maximum total number of boxes.
    pub max_boxes: u64,
    /// Maximum explicit nesting depth.
    pub max_depth: usize,
    /// Maximum number of tracks.
    pub max_tracks: usize,
    /// Maximum entries in any classic sample table.
    pub max_table_entries: u64,
    /// Maximum bytes read into memory for one bounded table.
    pub max_table_bytes: u64,
    /// Maximum compatible brands retained from `ftyp`.
    pub max_compatible_brands: usize,
    /// Maximum sample descriptions in one `stsd`.
    pub max_sample_descriptions: u64,
}

impl Default for ParseLimits {
    fn default() -> Self {
        Self {
            max_boxes: 100_000,
            max_depth: 24,
            max_tracks: 64,
            max_table_entries: 5_000_000,
            max_table_bytes: 64 * 1024 * 1024,
            max_compatible_brands: 64,
            max_sample_descriptions: 256,
        }
    }
}

/// Bounded classic sample-table summary for one track.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TrackSummary {
    /// Nonzero track identity from `tkhd`.
    pub track_id: u32,
    /// Handler type such as `vide`, `soun`, or `meta`.
    pub handler_type: FourCc,
    /// First sample-entry type from `stsd`, when present.
    pub codec: Option<FourCc>,
    /// Media timescale from `mdhd`.
    pub timescale: u32,
    /// Media duration in timescale units from `mdhd`.
    pub duration: u64,
    /// Width as raw unsigned 16.16 fixed point from `tkhd`.
    pub width_fixed_16_16: u32,
    /// Height as raw unsigned 16.16 fixed point from `tkhd`.
    pub height_fixed_16_16: u32,
    /// Sample count from `stsz` or `stz2`, when present.
    pub sample_count: Option<u64>,
    /// Sum of `stts.sample_count * sample_delta`, when present.
    pub decode_duration: Option<u64>,
    /// Sum of `ctts.sample_count`, when present.
    pub composition_sample_count: Option<u64>,
    /// Sum of sample sizes from `stsz` or `stz2`, when present.
    pub total_sample_bytes: Option<u64>,
    /// Constant sample size from `stsz`, when nonzero.
    pub constant_sample_size: Option<u32>,
    /// Number of chunk offsets from `stco` or `co64`, when present.
    pub chunk_count: Option<u64>,
    /// Number of sync samples from `stss`, when present.
    pub sync_sample_count: Option<u64>,
    /// Number of sample descriptions declared by `stsd`, when present.
    pub sample_description_count: Option<u32>,
    /// Number of `stsc` entries, when present.
    pub sample_to_chunk_entry_count: Option<u64>,
}

impl TrackSummary {
    /// Returns the integer pixel width by truncating the raw 16.16 value.
    #[must_use]
    pub const fn width_pixels(&self) -> u32 {
        self.width_fixed_16_16 >> 16
    }

    /// Returns the integer pixel height by truncating the raw 16.16 value.
    #[must_use]
    pub const fn height_pixels(&self) -> u32 {
        self.height_fixed_16_16 >> 16
    }
}

/// Bounded native ISO BMFF inspection result.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct IsoBmffSummary {
    /// Exact file length in bytes.
    pub file_length: u64,
    /// Major brand from `ftyp`, when present.
    pub major_brand: Option<FourCc>,
    /// Minor version from `ftyp`, when present.
    pub minor_version: Option<u32>,
    /// Compatible brands in encoded order.
    pub compatible_brands: Vec<FourCc>,
    /// Movie timescale from `mvhd`.
    pub movie_timescale: u32,
    /// Movie duration in movie-timescale units.
    pub movie_duration: u64,
    /// Tracks in source `trak` order.
    pub tracks: Vec<TrackSummary>,
    /// Whether a top-level `moof` was observed.
    pub fragmented: bool,
    /// Total boxes visited by the bounded parser.
    pub boxes_visited: u64,
}

#[derive(Clone, Copy, Debug)]
struct BoxHeader {
    box_type: FourCc,
    payload_start: u64,
    end: u64,
}

#[derive(Clone, Copy, Debug)]
struct Frame {
    cursor: u64,
    end: u64,
    depth: usize,
    container: Option<FourCc>,
    track_index: Option<usize>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct ByteRange {
    start: u64,
    end: u64,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct ChunkOffsetTable {
    box_type: FourCc,
    entries_start: u64,
    count: u64,
    entry_bytes: u64,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct SampleToChunkEntry {
    first_chunk: u32,
    samples_per_chunk: u32,
    sample_description_index: u32,
}

#[derive(Clone, Debug, Default)]
struct MovieBuilder {
    major_brand: Option<FourCc>,
    minor_version: Option<u32>,
    compatible_brands: Vec<FourCc>,
    movie_timescale: Option<u32>,
    movie_duration: Option<u64>,
    tracks: Vec<TrackBuilder>,
    saw_moov: bool,
    fragmented: bool,
    boxes_visited: u64,
    media_data_ranges: Vec<ByteRange>,
}

#[derive(Clone, Debug, Default)]
struct TrackBuilder {
    track_id: Option<u32>,
    handler_type: Option<FourCc>,
    codec: Option<FourCc>,
    timescale: Option<u32>,
    duration: Option<u64>,
    width_fixed_16_16: Option<u32>,
    height_fixed_16_16: Option<u32>,
    sample_count: Option<u64>,
    stts_sample_count: Option<u64>,
    decode_duration: Option<u64>,
    composition_sample_count: Option<u64>,
    total_sample_bytes: Option<u64>,
    constant_sample_size: Option<u32>,
    chunk_count: Option<u64>,
    chunk_offset_table: Option<ChunkOffsetTable>,
    sync_sample_count: Option<u64>,
    max_sync_sample: Option<u32>,
    sample_description_count: Option<u32>,
    sample_to_chunk_entry_count: Option<u64>,
    sample_to_chunk_entries: Vec<SampleToChunkEntry>,
}

impl TrackBuilder {
    fn finish(self, track_index: usize) -> Result<TrackSummary, MediaError> {
        let track_id = self.track_id.ok_or(MediaError::MissingRequiredBox {
            box_type: FourCc::TKHD,
            track_index: Some(track_index),
        })?;
        if track_id == 0 {
            return Err(MediaError::InvalidTrackId { track_index });
        }
        let handler_type = self.handler_type.ok_or(MediaError::MissingRequiredBox {
            box_type: FourCc::HDLR,
            track_index: Some(track_index),
        })?;
        let timescale = self.timescale.ok_or(MediaError::MissingRequiredBox {
            box_type: FourCc::MDHD,
            track_index: Some(track_index),
        })?;
        if timescale == 0 {
            return Err(MediaError::ZeroTimescale {
                box_type: FourCc::MDHD,
                track_index: Some(track_index),
            });
        }
        let duration = self.duration.ok_or(MediaError::MissingRequiredBox {
            box_type: FourCc::MDHD,
            track_index: Some(track_index),
        })?;

        if let (Some(stts), Some(samples)) = (self.stts_sample_count, self.sample_count) {
            require_same_sample_count(track_index, "stts", stts, "stsz_or_stz2", samples)?;
        }
        if let (Some(ctts), Some(samples)) = (self.composition_sample_count, self.sample_count) {
            require_same_sample_count(track_index, "ctts", ctts, "stsz_or_stz2", samples)?;
        }
        if let Some(max_sync_sample) = self.max_sync_sample {
            if max_sync_sample == 0
                || self
                    .sample_count
                    .is_some_and(|count| u64::from(max_sync_sample) > count)
            {
                return Err(MediaError::InvalidSyncSample {
                    track_index,
                    entry_index: self.sync_sample_count.unwrap_or(1).saturating_sub(1),
                    sample_number: max_sync_sample,
                    sample_count: self.sample_count,
                });
            }
        }
        if self.sample_count.is_some_and(|count| count > 0) && self.chunk_count.is_none() {
            return Err(MediaError::MissingChunkOffsetTable { track_index });
        }
        if self.sample_count == Some(0) && self.chunk_count.is_some_and(|count| count > 0) {
            return Err(MediaError::InvalidSampleToChunk {
                track_index,
                entry_index: 0,
                reason: "zero samples cannot occupy nonzero chunks",
            });
        }
        if self.sample_count.is_some_and(|count| count > 0) && self.chunk_count == Some(0) {
            return Err(MediaError::InvalidSampleToChunk {
                track_index,
                entry_index: 0,
                reason: "nonzero samples require at least one chunk",
            });
        }

        let entries = &self.sample_to_chunk_entries;
        if let Some(entry_count) = self.sample_to_chunk_entry_count {
            let actual = u64::try_from(entries.len()).map_err(|_| MediaError::ArithmeticOverflow)?;
            if actual != entry_count {
                return Err(MediaError::InvalidSampleToChunk {
                    track_index,
                    entry_index: actual,
                    reason: "retained entry count differs from declared count",
                });
            }
        }
        if !entries.is_empty() {
            let chunk_count = self.chunk_count.ok_or(MediaError::InvalidSampleToChunk {
                track_index,
                entry_index: 0,
                reason: "stsc exists without stco or co64",
            })?;
            if entries.first().is_none_or(|entry| entry.first_chunk != 1) {
                return Err(MediaError::InvalidSampleToChunk {
                    track_index,
                    entry_index: 0,
                    reason: "first entry must begin at chunk 1",
                });
            }
            let mut expanded_samples = 0_u64;
            for (entry_index, entry) in entries.iter().enumerate() {
                let start = u64::from(entry.first_chunk);
                let end = entries
                    .get(entry_index.saturating_add(1))
                    .map_or_else(
                        || chunk_count.checked_add(1).ok_or(MediaError::ArithmeticOverflow),
                        |next| Ok(u64::from(next.first_chunk)),
                    )?;
                if start > chunk_count || end <= start {
                    return Err(MediaError::InvalidSampleToChunk {
                        track_index,
                        entry_index: u64::try_from(entry_index)
                            .map_err(|_| MediaError::ArithmeticOverflow)?,
                        reason: "entry range falls outside the chunk table",
                    });
                }
                let run_chunks = end.saturating_sub(start);
                expanded_samples = expanded_samples
                    .checked_add(
                        run_chunks
                            .checked_mul(u64::from(entry.samples_per_chunk))
                            .ok_or(MediaError::ArithmeticOverflow)?,
                    )
                    .ok_or(MediaError::ArithmeticOverflow)?;
            }
            if let Some(samples) = self.sample_count {
                require_same_sample_count(
                    track_index,
                    "stsc_expanded",
                    expanded_samples,
                    "stsz_or_stz2",
                    samples,
                )?;
            }
            let referenced = entries
                .iter()
                .map(|entry| entry.sample_description_index)
                .max()
                .unwrap_or(0);
            let available = self.sample_description_count.unwrap_or(0);
            if referenced > available {
                return Err(MediaError::SampleDescriptionOutOfRange {
                    track_index,
                    referenced,
                    available,
                });
            }
        } else if self.sample_count.is_some_and(|count| count > 0)
            && self.chunk_count.is_some_and(|count| count > 0)
        {
            return Err(MediaError::MissingRequiredBox {
                box_type: FourCc::STSC,
                track_index: Some(track_index),
            });
        }

        Ok(TrackSummary {
            track_id,
            handler_type,
            codec: self.codec,
            timescale,
            duration,
            width_fixed_16_16: self.width_fixed_16_16.unwrap_or(0),
            height_fixed_16_16: self.height_fixed_16_16.unwrap_or(0),
            sample_count: self.sample_count,
            decode_duration: self.decode_duration,
            composition_sample_count: self.composition_sample_count,
            total_sample_bytes: self.total_sample_bytes,
            constant_sample_size: self.constant_sample_size,
            chunk_count: self.chunk_count,
            sync_sample_count: self.sync_sample_count,
            sample_description_count: self.sample_description_count,
            sample_to_chunk_entry_count: self.sample_to_chunk_entry_count,
        })
    }
}

/// Inspects an exact regular file without following a source symlink.
///
/// # Errors
///
/// Returns a typed filesystem, bounds, structure, table, or semantic consistency error.
pub fn inspect_iso_bmff_file(
    path: impl AsRef<Path>,
    limits: ParseLimits,
) -> Result<IsoBmffSummary, MediaError> {
    let path = path.as_ref();
    let metadata = fs::symlink_metadata(path).map_err(|source| MediaError::FileIo {
        operation: "source_metadata",
        path: path.to_path_buf(),
        source,
    })?;
    if metadata.file_type().is_symlink() {
        return Err(MediaError::SourceSymlink(path.to_path_buf()));
    }
    if !metadata.is_file() {
        return Err(MediaError::SourceNotRegular(path.to_path_buf()));
    }
    let mut file = File::open(path).map_err(|source| MediaError::FileIo {
        operation: "open_source",
        path: path.to_path_buf(),
        source,
    })?;
    inspect_iso_bmff(&mut file, metadata.len(), limits)
}

/// Inspects ISO BMFF metadata from a bounded seekable reader.
///
/// # Errors
///
/// Returns a typed I/O, bounds, structure, table, or semantic consistency error. The parser is
/// explicit-stack based and does not recurse over attacker-controlled nesting.
pub fn inspect_iso_bmff<R: Read + Seek>(
    reader: &mut R,
    file_length: u64,
    limits: ParseLimits,
) -> Result<IsoBmffSummary, MediaError> {
    if file_length < 8 {
        return Err(MediaError::FileTooShort {
            length: file_length,
        });
    }
    validate_limits(limits)?;
    let mut movie = MovieBuilder::default();
    let mut stack = vec![Frame {
        cursor: 0,
        end: file_length,
        depth: 0,
        container: None,
        track_index: None,
    }];

    while !stack.is_empty() {
        let Some(frame) = stack.last().copied() else {
            break;
        };
        if frame.cursor == frame.end {
            let _ = stack.pop();
            continue;
        }
        if frame.cursor > frame.end {
            return Err(MediaError::InvalidBoxSize {
                offset: frame.cursor,
                size: 0,
                header_size: 0,
                parent_end: frame.end,
            });
        }
        let header = read_box_header(reader, frame.cursor, frame.end, frame.depth == 0)?;
        validate_parent(header.box_type, frame.container)?;
        movie.boxes_visited = movie
            .boxes_visited
            .checked_add(1)
            .ok_or(MediaError::ArithmeticOverflow)?;
        if movie.boxes_visited > limits.max_boxes {
            return Err(MediaError::BoxLimitExceeded {
                actual: movie.boxes_visited,
                maximum: limits.max_boxes,
            });
        }
        if let Some(current) = stack.last_mut() {
            current.cursor = header.end;
        }

        let mut child_track = frame.track_index;
        if header.box_type == FourCc::TRAK {
            let actual = movie.tracks.len().saturating_add(1);
            if actual > limits.max_tracks {
                return Err(MediaError::TrackLimitExceeded {
                    actual,
                    maximum: limits.max_tracks,
                });
            }
            movie.tracks.push(TrackBuilder::default());
            child_track = movie.tracks.len().checked_sub(1);
        } else if header.box_type == FourCc::META || header.box_type == FourCc::MOOF {
            child_track = None;
        }

        if header.box_type == FourCc::MDAT {
            movie.media_data_ranges.push(ByteRange {
                start: header.payload_start,
                end: header.end,
            });
        }

        if let Some(prefix_bytes) = container_prefix(header.box_type) {
            if header.box_type == FourCc::META || header.box_type == FourCc::DREF {
                require_version_zero(reader, header)?;
            }
            if header.box_type == FourCc::MOOV {
                if movie.saw_moov {
                    return Err(MediaError::DuplicateBox {
                        box_type: FourCc::MOOV,
                        track_index: None,
                    });
                }
                movie.saw_moov = true;
            }
            if header.box_type == FourCc::MOOF {
                movie.fragmented = true;
            }
            let child_start = header
                .payload_start
                .checked_add(prefix_bytes)
                .ok_or(MediaError::ArithmeticOverflow)?;
            if child_start > header.end {
                return Err(MediaError::Truncated {
                    offset: header.payload_start,
                    needed: prefix_bytes,
                    available: header.end.saturating_sub(header.payload_start),
                });
            }
            let depth = frame
                .depth
                .checked_add(1)
                .ok_or(MediaError::ArithmeticOverflow)?;
            if depth > limits.max_depth {
                return Err(MediaError::DepthLimitExceeded {
                    actual: depth,
                    maximum: limits.max_depth,
                });
            }
            stack.push(Frame {
                cursor: child_start,
                end: header.end,
                depth,
                container: Some(header.box_type),
                track_index: child_track,
            });
            continue;
        }

        parse_leaf(
            reader,
            header,
            frame.container,
            frame.track_index,
            limits,
            &mut movie,
        )?;
    }

    validate_chunk_offset_tables(reader, &movie, file_length)?;
    let observed_length = reader
        .seek(SeekFrom::End(0))
        .map_err(|source| MediaError::Io {
            operation: "seek_end",
            offset: file_length,
            source,
        })?;
    if observed_length != file_length {
        return Err(MediaError::FileLengthChanged {
            expected: file_length,
            observed: observed_length,
        });
    }
    finish_movie(movie, file_length)
}

fn validate_limits(limits: ParseLimits) -> Result<(), MediaError> {
    for (name, value) in [
        ("max_boxes", limits.max_boxes),
        (
            "max_depth",
            u64::try_from(limits.max_depth).map_err(|_| MediaError::ArithmeticOverflow)?,
        ),
        (
            "max_tracks",
            u64::try_from(limits.max_tracks).map_err(|_| MediaError::ArithmeticOverflow)?,
        ),
        ("max_table_entries", limits.max_table_entries),
        ("max_table_bytes", limits.max_table_bytes),
        (
            "max_compatible_brands",
            u64::try_from(limits.max_compatible_brands)
                .map_err(|_| MediaError::ArithmeticOverflow)?,
        ),
        ("max_sample_descriptions", limits.max_sample_descriptions),
    ] {
        if value == 0 {
            return Err(MediaError::InvalidLimit { name });
        }
    }
    Ok(())
}

fn validate_parent(box_type: FourCc, parent: Option<FourCc>) -> Result<(), MediaError> {
    let valid = if box_type == FourCc::FTYP
        || box_type == FourCc::MOOV
        || box_type == FourCc::MDAT
        || box_type == FourCc::MOOF
    {
        parent.is_none()
    } else if box_type == FourCc::MVHD || box_type == FourCc::TRAK {
        parent == Some(FourCc::MOOV)
    } else if box_type == FourCc::TKHD
        || box_type == FourCc::MDIA
        || box_type == FourCc::EDTS
    {
        parent == Some(FourCc::TRAK)
    } else if box_type == FourCc::MDHD || box_type == FourCc::MINF {
        parent == Some(FourCc::MDIA)
    } else if box_type == FourCc::HDLR {
        parent == Some(FourCc::MDIA) || parent == Some(FourCc::META)
    } else if box_type == FourCc::STBL || box_type == FourCc::DINF {
        parent == Some(FourCc::MINF)
    } else if box_type == FourCc::DREF {
        parent == Some(FourCc::DINF)
    } else if matches!(
        box_type,
        FourCc::STSD
            | FourCc::STTS
            | FourCc::CTTS
            | FourCc::STSZ
            | FourCc::STZ2
            | FourCc::STCO
            | FourCc::CO64
            | FourCc::STSC
            | FourCc::STSS
    ) {
        parent == Some(FourCc::STBL)
    } else if box_type == FourCc::TRAF {
        parent == Some(FourCc::MOOF)
    } else if box_type == FourCc::UDTA {
        parent == Some(FourCc::MOOV) || parent == Some(FourCc::TRAK)
    } else if box_type == FourCc::META {
        parent.is_none()
            || parent == Some(FourCc::MOOV)
            || parent == Some(FourCc::TRAK)
            || parent == Some(FourCc::UDTA)
    } else {
        true
    };
    if valid {
        Ok(())
    } else {
        Err(MediaError::InvalidBoxParent { box_type, parent })
    }
}

fn container_prefix(box_type: FourCc) -> Option<u64> {
    if matches!(
        box_type,
        FourCc::MOOV
            | FourCc::TRAK
            | FourCc::MDIA
            | FourCc::MINF
            | FourCc::STBL
            | FourCc::EDTS
            | FourCc::DINF
            | FourCc::UDTA
            | FourCc::MOOF
            | FourCc::TRAF
    ) {
        Some(0)
    } else if box_type == FourCc::META {
        Some(4)
    } else if box_type == FourCc::DREF {
        Some(8)
    } else {
        None
    }
}

fn finish_movie(movie: MovieBuilder, file_length: u64) -> Result<IsoBmffSummary, MediaError> {
    if !movie.saw_moov {
        return Err(MediaError::MissingRequiredBox {
            box_type: FourCc::MOOV,
            track_index: None,
        });
    }
    let movie_timescale = movie
        .movie_timescale
        .ok_or(MediaError::MissingRequiredBox {
            box_type: FourCc::MVHD,
            track_index: None,
        })?;
    let movie_duration = movie
        .movie_duration
        .ok_or(MediaError::MissingRequiredBox {
            box_type: FourCc::MVHD,
            track_index: None,
        })?;
    let mut tracks = Vec::with_capacity(movie.tracks.len());
    let mut track_ids = BTreeSet::new();
    for (track_index, track) in movie.tracks.into_iter().enumerate() {
        let summary = track.finish(track_index)?;
        if !track_ids.insert(summary.track_id) {
            return Err(MediaError::DuplicateTrackId {
                track_id: summary.track_id,
            });
        }
        tracks.push(summary);
    }
    Ok(IsoBmffSummary {
        file_length,
        major_brand: movie.major_brand,
        minor_version: movie.minor_version,
        compatible_brands: movie.compatible_brands,
        movie_timescale,
        movie_duration,
        tracks,
        fragmented: movie.fragmented,
        boxes_visited: movie.boxes_visited,
    })
}

fn require_same_sample_count(
    track_index: usize,
    left_name: &'static str,
    left: u64,
    right_name: &'static str,
    right: u64,
) -> Result<(), MediaError> {
    if left == right {
        Ok(())
    } else {
        Err(MediaError::SampleCountMismatch {
            track_index,
            left_name,
            left,
            right_name,
            right,
        })
    }
}
