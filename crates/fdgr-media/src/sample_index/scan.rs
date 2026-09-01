#![forbid(unsafe_code)]
#![allow(clippy::too_many_lines)]
//! Focused second-pass discovery of exact classic sample-table locations.

use super::{
    ByteRange, CompositionTable, FixedTable, FocusedIndex, SampleIndexError, SampleSizeTable,
    SampleToChunkEntry, TrackIndex, inconsistent,
};
use crate::{FourCc, MediaError, ParseLimits};
use std::io::{Read, Seek, SeekFrom};

const MOOV: FourCc = FourCc::new(*b"moov");
const TRAK: FourCc = FourCc::new(*b"trak");
const TKHD: FourCc = FourCc::new(*b"tkhd");
const MDIA: FourCc = FourCc::new(*b"mdia");
const MDHD: FourCc = FourCc::new(*b"mdhd");
const MINF: FourCc = FourCc::new(*b"minf");
const STBL: FourCc = FourCc::new(*b"stbl");
const STTS: FourCc = FourCc::new(*b"stts");
const CTTS: FourCc = FourCc::new(*b"ctts");
const STSZ: FourCc = FourCc::new(*b"stsz");
const STZ2: FourCc = FourCc::new(*b"stz2");
const STCO: FourCc = FourCc::new(*b"stco");
const CO64: FourCc = FourCc::new(*b"co64");
const STSC: FourCc = FourCc::new(*b"stsc");
const STSS: FourCc = FourCc::new(*b"stss");
const EDTS: FourCc = FourCc::new(*b"edts");
const DINF: FourCc = FourCc::new(*b"dinf");
const DREF: FourCc = FourCc::new(*b"dref");
const UDTA: FourCc = FourCc::new(*b"udta");
const META: FourCc = FourCc::new(*b"meta");
const MDAT: FourCc = FourCc::new(*b"mdat");
const MOOF: FourCc = FourCc::new(*b"moof");
const TRAF: FourCc = FourCc::new(*b"traf");
const UUID: FourCc = FourCc::new(*b"uuid");

#[derive(Clone, Copy, Debug)]
struct Header {
    box_type: FourCc,
    payload_start: u64,
    end: u64,
}

#[derive(Clone, Copy, Debug)]
struct Frame {
    cursor: u64,
    end: u64,
    depth: usize,
    track_index: Option<usize>,
}

pub(super) fn scan_focused_index<R: Read + Seek>(
    reader: &mut R,
    file_length: u64,
    limits: ParseLimits,
) -> Result<FocusedIndex, SampleIndexError> {
    let mut index = FocusedIndex::default();
    let mut stack = vec![Frame {
        cursor: 0,
        end: file_length,
        depth: 0,
        track_index: None,
    }];
    let mut boxes_visited = 0_u64;
    while !stack.is_empty() {
        let Some(frame) = stack.last().copied() else {
            break;
        };
        if frame.cursor == frame.end {
            let _ = stack.pop();
            continue;
        }
        let header = read_header(reader, frame.cursor, frame.end, frame.depth == 0)?;
        boxes_visited = boxes_visited
            .checked_add(1)
            .ok_or(MediaError::ArithmeticOverflow)?;
        if boxes_visited > limits.max_boxes {
            return Err(MediaError::BoxLimitExceeded {
                actual: boxes_visited,
                maximum: limits.max_boxes,
            }
            .into());
        }
        if let Some(current) = stack.last_mut() {
            current.cursor = header.end;
        }
        let mut child_track = frame.track_index;
        if header.box_type == TRAK {
            let actual = index.tracks.len().saturating_add(1);
            if actual > limits.max_tracks {
                return Err(MediaError::TrackLimitExceeded {
                    actual,
                    maximum: limits.max_tracks,
                }
                .into());
            }
            index.tracks.push(TrackIndex::default());
            child_track = index.tracks.len().checked_sub(1);
        } else if header.box_type == META || header.box_type == MOOF {
            child_track = None;
        }
        if header.box_type == MDAT {
            index.media_data_ranges.push(ByteRange {
                start: header.payload_start,
                end: header.end,
            });
        }
        if let Some(prefix) = container_prefix(header.box_type) {
            let child_start = add(header.payload_start, prefix)?;
            if child_start > header.end {
                return Err(MediaError::Truncated {
                    offset: header.payload_start,
                    needed: prefix,
                    available: header.end.saturating_sub(header.payload_start),
                }
                .into());
            }
            let depth = frame
                .depth
                .checked_add(1)
                .ok_or(MediaError::ArithmeticOverflow)?;
            if depth > limits.max_depth {
                return Err(MediaError::DepthLimitExceeded {
                    actual: depth,
                    maximum: limits.max_depth,
                }
                .into());
            }
            stack.push(Frame {
                cursor: child_start,
                end: header.end,
                depth,
                track_index: child_track,
            });
            continue;
        }
        parse_focused_leaf(reader, header, frame.track_index, limits, &mut index)?;
    }
    Ok(index)
}

fn parse_focused_leaf<R: Read + Seek>(
    reader: &mut R,
    header: Header,
    track_index: Option<usize>,
    limits: ParseLimits,
    index: &mut FocusedIndex,
) -> Result<(), SampleIndexError> {
    let Some(track_index) = track_index else {
        return Ok(());
    };
    let track = index
        .tracks
        .get_mut(track_index)
        .ok_or(SampleIndexError::InconsistentIndex {
            track_id: None,
            reason: "focused traversal lost track scope",
        })?;
    if header.box_type == TKHD {
        if track.track_id.is_some() {
            return inconsistent(track.track_id, "duplicate tkhd in focused index");
        }
        let version = read_u8(reader, header.payload_start)?;
        let offset = match version {
            0 => 12,
            1 => 20,
            _ => return Err(MediaError::UnsupportedVersion { box_type: TKHD, version }.into()),
        };
        require_payload(header, offset + 4)?;
        track.track_id = Some(read_u32(reader, add(header.payload_start, offset)?)?);
    } else if header.box_type == MDHD {
        if track.timescale.is_some() {
            return inconsistent(track.track_id, "duplicate mdhd in focused index");
        }
        let version = read_u8(reader, header.payload_start)?;
        let offset = match version {
            0 => 12,
            1 => 20,
            _ => return Err(MediaError::UnsupportedVersion { box_type: MDHD, version }.into()),
        };
        require_payload(header, offset + 4)?;
        track.timescale = Some(read_u32(reader, add(header.payload_start, offset)?)?);
    } else if header.box_type == STTS {
        let table = fixed_table(reader, header, STTS, 8, limits, &[0])?;
        let track_id = track.track_id;
        set_table(
            &mut track.stts,
            table,
            track_id,
            "duplicate stts in focused index",
        )?;
    } else if header.box_type == CTTS {
        if track.ctts.is_some() {
            return inconsistent(track.track_id, "duplicate ctts in focused index");
        }
        let version = read_u8(reader, header.payload_start)?;
        if version > 1 {
            return Err(MediaError::UnsupportedVersion { box_type: CTTS, version }.into());
        }
        let table = fixed_table(reader, header, CTTS, 8, limits, &[0, 1])?;
        track.ctts = Some(CompositionTable { table, version });
    } else if header.box_type == STSZ {
        parse_stsz(reader, header, limits, track)?;
    } else if header.box_type == STZ2 {
        parse_stz2(reader, header, limits, track)?;
    } else if header.box_type == STCO || header.box_type == CO64 {
        let width = if header.box_type == STCO { 4 } else { 8 };
        let table = fixed_table(reader, header, header.box_type, width, limits, &[0])?;
        let track_id = track.track_id;
        set_table(
            &mut track.chunk_offsets,
            table,
            track_id,
            "duplicate chunk-offset table in focused index",
        )?;
    } else if header.box_type == STSC {
        parse_stsc(reader, header, limits, track)?;
    } else if header.box_type == STSS {
        let table = fixed_table(reader, header, STSS, 4, limits, &[0])?;
        let track_id = track.track_id;
        set_table(
            &mut track.sync_samples,
            table,
            track_id,
            "duplicate stss in focused index",
        )?;
    }
    Ok(())
}

fn parse_stsz<R: Read + Seek>(
    reader: &mut R,
    header: Header,
    limits: ParseLimits,
    track: &mut TrackIndex,
) -> Result<(), SampleIndexError> {
    if track.sample_sizes.is_some() {
        return inconsistent(track.track_id, "duplicate sample-size table in focused index");
    }
    require_version(reader, header, STSZ, &[0])?;
    require_payload(header, 12)?;
    let sample_size = read_u32(reader, add(header.payload_start, 4)?)?;
    let count = u64::from(read_u32(reader, add(header.payload_start, 8)?)?);
    enforce_count(STSZ, count, limits)?;
    let table_bytes = if sample_size == 0 {
        count.checked_mul(4).ok_or(MediaError::ArithmeticOverflow)?
    } else {
        0
    };
    enforce_bytes(STSZ, table_bytes, limits)?;
    require_exact_payload(header, add(12, table_bytes)?, STSZ)?;
    track.sample_sizes = Some(if sample_size == 0 {
        SampleSizeTable::U32 {
            entries_start: add(header.payload_start, 12)?,
            count,
        }
    } else {
        SampleSizeTable::Constant { count, sample_size }
    });
    Ok(())
}

fn parse_stz2<R: Read + Seek>(
    reader: &mut R,
    header: Header,
    limits: ParseLimits,
    track: &mut TrackIndex,
) -> Result<(), SampleIndexError> {
    if track.sample_sizes.is_some() {
        return inconsistent(track.track_id, "duplicate sample-size table in focused index");
    }
    require_version(reader, header, STZ2, &[0])?;
    require_payload(header, 12)?;
    let field_size = read_u8(reader, add(header.payload_start, 7)?)?;
    if !matches!(field_size, 4 | 8 | 16) {
        return Err(MediaError::InvalidCompactSampleFieldSize { field_size }.into());
    }
    let count = u64::from(read_u32(reader, add(header.payload_start, 8)?)?);
    enforce_count(STZ2, count, limits)?;
    let bits = count
        .checked_mul(u64::from(field_size))
        .ok_or(MediaError::ArithmeticOverflow)?;
    let table_bytes = bits
        .checked_add(7)
        .ok_or(MediaError::ArithmeticOverflow)?
        / 8;
    enforce_bytes(STZ2, table_bytes, limits)?;
    require_exact_payload(header, add(12, table_bytes)?, STZ2)?;
    track.sample_sizes = Some(SampleSizeTable::Compact {
        entries_start: add(header.payload_start, 12)?,
        count,
        field_size,
    });
    Ok(())
}

fn parse_stsc<R: Read + Seek>(
    reader: &mut R,
    header: Header,
    limits: ParseLimits,
    track: &mut TrackIndex,
) -> Result<(), SampleIndexError> {
    if !track.sample_to_chunk.is_empty() {
        return inconsistent(track.track_id, "duplicate stsc in focused index");
    }
    let table = fixed_table(reader, header, STSC, 12, limits, &[0])?;
    let capacity = usize::try_from(table.count).map_err(|_| MediaError::ArithmeticOverflow)?;
    let mut entries = Vec::with_capacity(capacity);
    let mut previous = 0_u32;
    for entry_index in 0..table.count {
        let offset = add(
            table.entries_start,
            entry_index
                .checked_mul(12)
                .ok_or(MediaError::ArithmeticOverflow)?,
        )?;
        let first_chunk = read_u32(reader, offset)?;
        let samples_per_chunk = read_u32(reader, add(offset, 4)?)?;
        let sample_description_index = read_u32(reader, add(offset, 8)?)?;
        if first_chunk == 0
            || first_chunk <= previous
            || samples_per_chunk == 0
            || sample_description_index == 0
        {
            return inconsistent(track.track_id, "invalid stsc entry in focused index");
        }
        entries.push(SampleToChunkEntry {
            first_chunk,
            samples_per_chunk,
            sample_description_index,
        });
        previous = first_chunk;
    }
    track.sample_to_chunk = entries;
    Ok(())
}

fn set_table(
    slot: &mut Option<FixedTable>,
    value: FixedTable,
    track_id: Option<u32>,
    reason: &'static str,
) -> Result<(), SampleIndexError> {
    if slot.replace(value).is_some() {
        inconsistent(track_id, reason)
    } else {
        Ok(())
    }
}

fn fixed_table<R: Read + Seek>(
    reader: &mut R,
    header: Header,
    box_type: FourCc,
    entry_bytes: u64,
    limits: ParseLimits,
    supported_versions: &[u8],
) -> Result<FixedTable, SampleIndexError> {
    require_version(reader, header, box_type, supported_versions)?;
    require_payload(header, 8)?;
    let count = u64::from(read_u32(reader, add(header.payload_start, 4)?)?);
    enforce_count(box_type, count, limits)?;
    let table_bytes = count
        .checked_mul(entry_bytes)
        .ok_or(MediaError::ArithmeticOverflow)?;
    enforce_bytes(box_type, table_bytes, limits)?;
    require_exact_payload(header, add(8, table_bytes)?, box_type)?;
    Ok(FixedTable {
        entries_start: add(header.payload_start, 8)?,
        count,
        entry_bytes,
    })
}

fn require_version<R: Read + Seek>(
    reader: &mut R,
    header: Header,
    box_type: FourCc,
    supported: &[u8],
) -> Result<u8, SampleIndexError> {
    require_payload(header, 4)?;
    let version = read_u8(reader, header.payload_start)?;
    if supported.contains(&version) {
        Ok(version)
    } else {
        Err(MediaError::UnsupportedVersion { box_type, version }.into())
    }
}

fn require_payload(header: Header, required: u64) -> Result<(), SampleIndexError> {
    let available = header.end.saturating_sub(header.payload_start);
    if available < required {
        Err(MediaError::Truncated {
            offset: header.payload_start,
            needed: required,
            available,
        }
        .into())
    } else {
        Ok(())
    }
}

fn require_exact_payload(
    header: Header,
    expected: u64,
    box_type: FourCc,
) -> Result<(), SampleIndexError> {
    let available = header.end.saturating_sub(header.payload_start);
    if available == expected {
        Ok(())
    } else {
        Err(MediaError::InvalidTableLength {
            box_type,
            expected,
            available,
        }
        .into())
    }
}

fn enforce_count(
    box_type: FourCc,
    count: u64,
    limits: ParseLimits,
) -> Result<(), SampleIndexError> {
    if count > limits.max_table_entries {
        Err(MediaError::TableEntryLimitExceeded {
            box_type,
            actual: count,
            maximum: limits.max_table_entries,
        }
        .into())
    } else {
        Ok(())
    }
}

fn enforce_bytes(
    box_type: FourCc,
    bytes: u64,
    limits: ParseLimits,
) -> Result<(), SampleIndexError> {
    if bytes > limits.max_table_bytes {
        Err(MediaError::TableByteLimitExceeded {
            box_type,
            actual: bytes,
            maximum: limits.max_table_bytes,
        }
        .into())
    } else {
        Ok(())
    }
}

fn read_header<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
    parent_end: u64,
    top_level: bool,
) -> Result<Header, SampleIndexError> {
    let available = parent_end.saturating_sub(offset);
    if available < 8 {
        return Err(MediaError::Truncated {
            offset,
            needed: 8,
            available,
        }
        .into());
    }
    let size32 = u64::from(read_u32(reader, offset)?);
    let box_type = read_fourcc(reader, add(offset, 4)?)?;
    let (size, base_header_size) = if size32 == 1 {
        if available < 16 {
            return Err(MediaError::Truncated {
                offset,
                needed: 16,
                available,
            }
            .into());
        }
        (read_u64(reader, add(offset, 8)?)?, 16_u64)
    } else if size32 == 0 {
        if !top_level {
            return Err(MediaError::NestedZeroSizedBox { offset, box_type }.into());
        }
        (parent_end.saturating_sub(offset), 8_u64)
    } else {
        (size32, 8_u64)
    };
    let header_size = if box_type == UUID {
        base_header_size
            .checked_add(16)
            .ok_or(MediaError::ArithmeticOverflow)?
    } else {
        base_header_size
    };
    let end = offset
        .checked_add(size)
        .ok_or(MediaError::ArithmeticOverflow)?;
    if size < header_size || end > parent_end {
        return Err(MediaError::InvalidBoxSize {
            offset,
            size,
            header_size,
            parent_end,
        }
        .into());
    }
    Ok(Header {
        box_type,
        payload_start: add(offset, header_size)?,
        end,
    })
}

fn container_prefix(box_type: FourCc) -> Option<u64> {
    if matches!(
        box_type,
        MOOV | TRAK | MDIA | MINF | STBL | EDTS | DINF | UDTA | MOOF | TRAF
    ) {
        Some(0)
    } else if box_type == META {
        Some(4)
    } else if box_type == DREF {
        Some(8)
    } else {
        None
    }
}

fn add(left: u64, right: u64) -> Result<u64, SampleIndexError> {
    left.checked_add(right)
        .ok_or_else(|| MediaError::ArithmeticOverflow.into())
}

fn read_exact_at<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
    buffer: &mut [u8],
    operation: &'static str,
) -> Result<(), SampleIndexError> {
    reader
        .seek(SeekFrom::Start(offset))
        .and_then(|_| reader.read_exact(buffer))
        .map_err(|source| MediaError::Io {
            operation,
            offset,
            source,
        })?;
    Ok(())
}

pub(super) fn read_u8<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
) -> Result<u8, SampleIndexError> {
    let mut bytes = [0_u8; 1];
    read_exact_at(reader, offset, &mut bytes, "read_sample_index_u8")?;
    Ok(bytes[0])
}

pub(super) fn read_u16<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
) -> Result<u16, SampleIndexError> {
    let mut bytes = [0_u8; 2];
    read_exact_at(reader, offset, &mut bytes, "read_sample_index_u16")?;
    Ok(u16::from_be_bytes(bytes))
}

pub(super) fn read_u32<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
) -> Result<u32, SampleIndexError> {
    let mut bytes = [0_u8; 4];
    read_exact_at(reader, offset, &mut bytes, "read_sample_index_u32")?;
    Ok(u32::from_be_bytes(bytes))
}

pub(super) fn read_u64<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
) -> Result<u64, SampleIndexError> {
    let mut bytes = [0_u8; 8];
    read_exact_at(reader, offset, &mut bytes, "read_sample_index_u64")?;
    Ok(u64::from_be_bytes(bytes))
}

fn read_fourcc<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
) -> Result<FourCc, SampleIndexError> {
    let mut bytes = [0_u8; 4];
    read_exact_at(reader, offset, &mut bytes, "read_sample_index_fourcc")?;
    Ok(FourCc::new(bytes))
}
