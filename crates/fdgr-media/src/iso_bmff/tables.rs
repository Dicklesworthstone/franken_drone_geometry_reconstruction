#![forbid(unsafe_code)]
#![allow(
    clippy::indexing_slicing,
    clippy::similar_names,
    clippy::too_many_arguments,
    clippy::too_many_lines
)]
//! Semantic ISO BMFF leaf-box and classic sample-table parsing.

use super::reader::{
    checked_add, enforce_entries, enforce_table_bytes, read_bounded_bytes, read_fourcc,
    read_full_box_version, read_u8, read_u32, read_u64, require_exact_payload, require_payload,
    require_version_zero, table_entry_count,
};
use super::{
    BoxHeader, ChunkOffsetTable, MovieBuilder, ParseLimits, SampleToChunkEntry, TrackBuilder,
};
use crate::{FourCc, MediaError};
use std::io::{Read, Seek};

pub(super) fn parse_leaf<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    parent: Option<FourCc>,
    track_index: Option<usize>,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    match header.box_type {
        FourCc::FTYP => parse_ftyp(reader, header, limits, movie),
        FourCc::MVHD => parse_mvhd(reader, header, movie),
        FourCc::TKHD => parse_tkhd(reader, header, track_index, movie),
        FourCc::MDHD => parse_mdhd(reader, header, track_index, movie),
        FourCc::HDLR => parse_hdlr(reader, header, parent, track_index, movie),
        FourCc::STSD => parse_stsd(reader, header, track_index, limits, movie),
        FourCc::STTS => parse_stts(reader, header, track_index, limits, movie),
        FourCc::CTTS => parse_ctts(reader, header, track_index, limits, movie),
        FourCc::STSZ => parse_stsz(reader, header, track_index, limits, movie),
        FourCc::STZ2 => parse_stz2(reader, header, track_index, limits, movie),
        FourCc::STCO => parse_chunk_offsets(reader, header, track_index, limits, movie, 4),
        FourCc::CO64 => parse_chunk_offsets(reader, header, track_index, limits, movie, 8),
        FourCc::STSC => parse_stsc(reader, header, track_index, limits, movie),
        FourCc::STSS => parse_stss(reader, header, track_index, limits, movie),
        _ => Ok(()),
    }
}

fn parse_ftyp<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    if movie.major_brand.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::FTYP,
            track_index: None,
        });
    }
    let payload_length = header.end.saturating_sub(header.payload_start);
    if payload_length < 8 {
        return Err(MediaError::Truncated {
            offset: header.payload_start,
            needed: 8,
            available: payload_length,
        });
    }
    let remainder = payload_length.saturating_sub(8);
    if remainder % 4 != 0 {
        return Err(MediaError::InvalidTableLength {
            box_type: FourCc::FTYP,
            expected: payload_length.saturating_add(4 - remainder % 4),
            available: payload_length,
        });
    }
    let brand_count_u64 = remainder / 4;
    let brand_count =
        usize::try_from(brand_count_u64).map_err(|_| MediaError::ArithmeticOverflow)?;
    if brand_count > limits.max_compatible_brands {
        return Err(MediaError::BrandLimitExceeded {
            actual: brand_count,
            maximum: limits.max_compatible_brands,
        });
    }
    movie.major_brand = Some(read_fourcc(reader, header.payload_start)?);
    movie.minor_version = Some(read_u32(reader, checked_add(header.payload_start, 4)?)?);
    let mut cursor = checked_add(header.payload_start, 8)?;
    for _ in 0..brand_count {
        movie.compatible_brands.push(read_fourcc(reader, cursor)?);
        cursor = checked_add(cursor, 4)?;
    }
    Ok(())
}

fn parse_mvhd<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    if movie.movie_timescale.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::MVHD,
            track_index: None,
        });
    }
    let version = read_full_box_version(reader, header)?;
    let (timescale_offset, duration_offset, duration_bytes) = match version {
        0 => (12_u64, 16_u64, 4_u64),
        1 => (20_u64, 24_u64, 8_u64),
        other => {
            return Err(MediaError::UnsupportedVersion {
                box_type: FourCc::MVHD,
                version: other,
            });
        }
    };
    require_payload(header, checked_add(duration_offset, duration_bytes)?)?;
    let timescale = read_u32(reader, checked_add(header.payload_start, timescale_offset)?)?;
    if timescale == 0 {
        return Err(MediaError::ZeroTimescale {
            box_type: FourCc::MVHD,
            track_index: None,
        });
    }
    movie.movie_timescale = Some(timescale);
    movie.movie_duration = Some(if duration_bytes == 4 {
        u64::from(read_u32(
            reader,
            checked_add(header.payload_start, duration_offset)?,
        )?)
    } else {
        read_u64(
            reader,
            checked_add(header.payload_start, duration_offset)?,
        )?
    });
    Ok(())
}

fn parse_tkhd<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, FourCc::TKHD)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.track_id.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::TKHD,
            track_index: Some(track_index),
        });
    }
    let version = read_full_box_version(reader, header)?;
    let (track_id_offset, width_offset, required) = match version {
        0 => (12_u64, 76_u64, 84_u64),
        1 => (20_u64, 88_u64, 96_u64),
        other => {
            return Err(MediaError::UnsupportedVersion {
                box_type: FourCc::TKHD,
                version: other,
            });
        }
    };
    require_payload(header, required)?;
    track.track_id = Some(read_u32(
        reader,
        checked_add(header.payload_start, track_id_offset)?,
    )?);
    track.width_fixed_16_16 = Some(read_u32(
        reader,
        checked_add(header.payload_start, width_offset)?,
    )?);
    track.height_fixed_16_16 = Some(read_u32(
        reader,
        checked_add(header.payload_start, checked_add(width_offset, 4)?)?,
    )?);
    Ok(())
}

fn parse_mdhd<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, FourCc::MDHD)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.timescale.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::MDHD,
            track_index: Some(track_index),
        });
    }
    let version = read_full_box_version(reader, header)?;
    let (timescale_offset, duration_offset, duration_bytes) = match version {
        0 => (12_u64, 16_u64, 4_u64),
        1 => (20_u64, 24_u64, 8_u64),
        other => {
            return Err(MediaError::UnsupportedVersion {
                box_type: FourCc::MDHD,
                version: other,
            });
        }
    };
    require_payload(header, checked_add(duration_offset, duration_bytes)?)?;
    let timescale = read_u32(reader, checked_add(header.payload_start, timescale_offset)?)?;
    if timescale == 0 {
        return Err(MediaError::ZeroTimescale {
            box_type: FourCc::MDHD,
            track_index: Some(track_index),
        });
    }
    track.timescale = Some(timescale);
    track.duration = Some(if duration_bytes == 4 {
        u64::from(read_u32(
            reader,
            checked_add(header.payload_start, duration_offset)?,
        )?)
    } else {
        read_u64(
            reader,
            checked_add(header.payload_start, duration_offset)?,
        )?
    });
    Ok(())
}

fn parse_hdlr<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    parent: Option<FourCc>,
    track_index: Option<usize>,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    if parent == Some(FourCc::META) {
        return Ok(());
    }
    let track_index = require_track(track_index, FourCc::HDLR)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.handler_type.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::HDLR,
            track_index: Some(track_index),
        });
    }
    let version = read_full_box_version(reader, header)?;
    if version != 0 {
        return Err(MediaError::UnsupportedVersion {
            box_type: FourCc::HDLR,
            version,
        });
    }
    require_payload(header, 12)?;
    track.handler_type = Some(read_fourcc(
        reader,
        checked_add(header.payload_start, 8)?,
    )?);
    Ok(())
}

fn parse_stsd<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, FourCc::STSD)?;
    let payload_length = header.end.saturating_sub(header.payload_start);
    enforce_table_bytes(FourCc::STSD, payload_length, limits)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.sample_description_count.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::STSD,
            track_index: Some(track_index),
        });
    }
    let version = read_full_box_version(reader, header)?;
    if version != 0 {
        return Err(MediaError::UnsupportedVersion {
            box_type: FourCc::STSD,
            version,
        });
    }
    require_payload(header, 8)?;
    let count = u64::from(read_u32(
        reader,
        checked_add(header.payload_start, 4)?,
    )?);
    if count > limits.max_sample_descriptions {
        return Err(MediaError::SampleDescriptionLimitExceeded {
            actual: count,
            maximum: limits.max_sample_descriptions,
        });
    }
    let mut cursor = checked_add(header.payload_start, 8)?;
    let mut first_codec = None;
    for entry_index in 0..count {
        let available = header.end.saturating_sub(cursor);
        if available < 8 {
            return Err(MediaError::Truncated {
                offset: cursor,
                needed: 8,
                available,
            });
        }
        let size = u64::from(read_u32(reader, cursor)?);
        if size < 8 {
            return Err(MediaError::InvalidBoxSize {
                offset: cursor,
                size,
                header_size: 8,
                parent_end: header.end,
            });
        }
        let end = checked_add(cursor, size)?;
        if end > header.end {
            return Err(MediaError::InvalidBoxSize {
                offset: cursor,
                size,
                header_size: 8,
                parent_end: header.end,
            });
        }
        let codec = read_fourcc(reader, checked_add(cursor, 4)?)?;
        if entry_index == 0 {
            first_codec = Some(codec);
        }
        cursor = end;
    }
    if cursor != header.end {
        return Err(MediaError::InvalidTableLength {
            box_type: FourCc::STSD,
            expected: cursor.saturating_sub(header.payload_start),
            available: header.end.saturating_sub(header.payload_start),
        });
    }
    track.sample_description_count = Some(
        u32::try_from(count).map_err(|_| MediaError::ArithmeticOverflow)?,
    );
    track.codec = first_codec;
    Ok(())
}

fn parse_stts<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, FourCc::STTS)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.stts_sample_count.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::STTS,
            track_index: Some(track_index),
        });
    }
    require_version_zero(reader, header)?;
    let count = table_entry_count(reader, header, FourCc::STTS, 8, limits)?;
    let mut cursor = checked_add(header.payload_start, 8)?;
    let mut samples = 0_u64;
    let mut duration = 0_u64;
    for _ in 0..count {
        let sample_count = u64::from(read_u32(reader, cursor)?);
        let delta = u64::from(read_u32(reader, checked_add(cursor, 4)?)?);
        samples = samples
            .checked_add(sample_count)
            .ok_or(MediaError::ArithmeticOverflow)?;
        duration = duration
            .checked_add(
                sample_count
                    .checked_mul(delta)
                    .ok_or(MediaError::ArithmeticOverflow)?,
            )
            .ok_or(MediaError::ArithmeticOverflow)?;
        cursor = checked_add(cursor, 8)?;
    }
    track.stts_sample_count = Some(samples);
    track.decode_duration = Some(duration);
    Ok(())
}

fn parse_ctts<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, FourCc::CTTS)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.composition_sample_count.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::CTTS,
            track_index: Some(track_index),
        });
    }
    let version = read_full_box_version(reader, header)?;
    if version > 1 {
        return Err(MediaError::UnsupportedVersion {
            box_type: FourCc::CTTS,
            version,
        });
    }
    let count = table_entry_count(reader, header, FourCc::CTTS, 8, limits)?;
    let mut cursor = checked_add(header.payload_start, 8)?;
    let mut samples = 0_u64;
    for _ in 0..count {
        samples = samples
            .checked_add(u64::from(read_u32(reader, cursor)?))
            .ok_or(MediaError::ArithmeticOverflow)?;
        cursor = checked_add(cursor, 8)?;
    }
    track.composition_sample_count = Some(samples);
    Ok(())
}

fn parse_stsz<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, FourCc::STSZ)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.sample_count.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::STSZ,
            track_index: Some(track_index),
        });
    }
    require_version_zero(reader, header)?;
    require_payload(header, 12)?;
    let sample_size = read_u32(reader, checked_add(header.payload_start, 4)?)?;
    let sample_count = u64::from(read_u32(
        reader,
        checked_add(header.payload_start, 8)?,
    )?);
    enforce_entries(FourCc::STSZ, sample_count, limits)?;
    let table_bytes = if sample_size == 0 {
        sample_count
            .checked_mul(4)
            .ok_or(MediaError::ArithmeticOverflow)?
    } else {
        0
    };
    enforce_table_bytes(FourCc::STSZ, table_bytes, limits)?;
    require_exact_payload(header, checked_add(12, table_bytes)?)?;
    let total = if sample_size == 0 {
        let mut cursor = checked_add(header.payload_start, 12)?;
        let mut sum = 0_u64;
        for _ in 0..sample_count {
            sum = sum
                .checked_add(u64::from(read_u32(reader, cursor)?))
                .ok_or(MediaError::ArithmeticOverflow)?;
            cursor = checked_add(cursor, 4)?;
        }
        sum
    } else {
        sample_count
            .checked_mul(u64::from(sample_size))
            .ok_or(MediaError::ArithmeticOverflow)?
    };
    track.sample_count = Some(sample_count);
    track.total_sample_bytes = Some(total);
    track.constant_sample_size = (sample_size != 0).then_some(sample_size);
    Ok(())
}

fn parse_stz2<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, FourCc::STZ2)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.sample_count.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::STZ2,
            track_index: Some(track_index),
        });
    }
    require_version_zero(reader, header)?;
    require_payload(header, 12)?;
    let field_size = read_u8(reader, checked_add(header.payload_start, 7)?)?;
    if !matches!(field_size, 4 | 8 | 16) {
        return Err(MediaError::InvalidCompactSampleFieldSize { field_size });
    }
    let sample_count = u64::from(read_u32(
        reader,
        checked_add(header.payload_start, 8)?,
    )?);
    enforce_entries(FourCc::STZ2, sample_count, limits)?;
    let bits = sample_count
        .checked_mul(u64::from(field_size))
        .ok_or(MediaError::ArithmeticOverflow)?;
    let table_bytes = bits
        .checked_add(7)
        .ok_or(MediaError::ArithmeticOverflow)?
        / 8;
    enforce_table_bytes(FourCc::STZ2, table_bytes, limits)?;
    require_exact_payload(header, checked_add(12, table_bytes)?)?;
    let table_start = checked_add(header.payload_start, 12)?;
    let bytes = read_bounded_bytes(reader, table_start, table_bytes, FourCc::STZ2, limits)?;
    track.sample_count = Some(sample_count);
    track.total_sample_bytes = Some(sum_compact_sizes(&bytes, sample_count, field_size)?);
    track.constant_sample_size = None;
    Ok(())
}

fn parse_chunk_offsets<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
    entry_bytes: u64,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, header.box_type)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.chunk_count.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: header.box_type,
            track_index: Some(track_index),
        });
    }
    require_version_zero(reader, header)?;
    let count = table_entry_count(reader, header, header.box_type, entry_bytes, limits)?;
    track.chunk_count = Some(count);
    track.chunk_offset_table = Some(ChunkOffsetTable {
        box_type: header.box_type,
        entries_start: checked_add(header.payload_start, 8)?,
        count,
        entry_bytes,
    });
    Ok(())
}

fn parse_stsc<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, FourCc::STSC)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.sample_to_chunk_entry_count.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::STSC,
            track_index: Some(track_index),
        });
    }
    require_version_zero(reader, header)?;
    let count = table_entry_count(reader, header, FourCc::STSC, 12, limits)?;
    let capacity = usize::try_from(count).map_err(|_| MediaError::ArithmeticOverflow)?;
    let mut entries = Vec::with_capacity(capacity);
    let mut cursor = checked_add(header.payload_start, 8)?;
    let mut previous_first_chunk = 0_u32;
    for entry_index in 0..count {
        let first_chunk = read_u32(reader, cursor)?;
        let samples_per_chunk = read_u32(reader, checked_add(cursor, 4)?)?;
        let description_index = read_u32(reader, checked_add(cursor, 8)?)?;
        if first_chunk == 0 {
            return Err(MediaError::InvalidSampleToChunk {
                track_index,
                entry_index,
                reason: "first_chunk must be positive",
            });
        }
        if first_chunk <= previous_first_chunk {
            return Err(MediaError::InvalidSampleToChunk {
                track_index,
                entry_index,
                reason: "first_chunk values must be strictly increasing",
            });
        }
        if samples_per_chunk == 0 {
            return Err(MediaError::InvalidSampleToChunk {
                track_index,
                entry_index,
                reason: "samples_per_chunk must be positive",
            });
        }
        if description_index == 0 {
            return Err(MediaError::InvalidSampleToChunk {
                track_index,
                entry_index,
                reason: "sample_description_index must be positive",
            });
        }
        previous_first_chunk = first_chunk;
        entries.push(SampleToChunkEntry {
            first_chunk,
            samples_per_chunk,
            sample_description_index: description_index,
        });
        cursor = checked_add(cursor, 12)?;
    }
    track.sample_to_chunk_entry_count = Some(count);
    track.sample_to_chunk_entries = entries;
    Ok(())
}

fn parse_stss<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    track_index: Option<usize>,
    limits: ParseLimits,
    movie: &mut MovieBuilder,
) -> Result<(), MediaError> {
    let track_index = require_track(track_index, FourCc::STSS)?;
    let track = track_builder_mut(movie, track_index)?;
    if track.sync_sample_count.is_some() {
        return Err(MediaError::DuplicateBox {
            box_type: FourCc::STSS,
            track_index: Some(track_index),
        });
    }
    require_version_zero(reader, header)?;
    let count = table_entry_count(reader, header, FourCc::STSS, 4, limits)?;
    let mut cursor = checked_add(header.payload_start, 8)?;
    let mut previous = 0_u32;
    for entry_index in 0..count {
        let sample_number = read_u32(reader, cursor)?;
        if sample_number == 0 || sample_number <= previous {
            return Err(MediaError::InvalidSyncSample {
                track_index,
                entry_index,
                sample_number,
                sample_count: track.sample_count,
            });
        }
        previous = sample_number;
        cursor = checked_add(cursor, 4)?;
    }
    track.sync_sample_count = Some(count);
    track.max_sync_sample = (count > 0).then_some(previous);
    Ok(())
}

pub(super) fn validate_chunk_offset_tables<R: Read + Seek>(
    reader: &mut R,
    movie: &MovieBuilder,
    file_length: u64,
) -> Result<(), MediaError> {
    for (track_index, track) in movie.tracks.iter().enumerate() {
        let Some(table) = track.chunk_offset_table else {
            continue;
        };
        let mut cursor = table.entries_start;
        for entry_index in 0..table.count {
            let offset = if table.entry_bytes == 4 {
                u64::from(read_u32(reader, cursor)?)
            } else {
                read_u64(reader, cursor)?
            };
            if offset >= file_length {
                return Err(MediaError::ChunkOffsetOutOfRange {
                    track_index,
                    entry_index,
                    offset,
                    file_length,
                });
            }
            if !movie
                .media_data_ranges
                .iter()
                .any(|range| offset >= range.start && offset < range.end)
            {
                return Err(MediaError::ChunkOffsetOutsideMediaData {
                    track_index,
                    entry_index,
                    offset,
                });
            }
            cursor = checked_add(cursor, table.entry_bytes)?;
        }
        if table.box_type != FourCc::STCO && table.box_type != FourCc::CO64 {
            return Err(MediaError::ArithmeticOverflow);
        }
    }
    Ok(())
}

fn require_track(
    track_index: Option<usize>,
    box_type: FourCc,
) -> Result<usize, MediaError> {
    track_index.ok_or(MediaError::MissingRequiredBox {
        box_type,
        track_index: None,
    })
}

fn track_builder_mut(
    movie: &mut MovieBuilder,
    track_index: usize,
) -> Result<&mut TrackBuilder, MediaError> {
    movie
        .tracks
        .get_mut(track_index)
        .ok_or(MediaError::ArithmeticOverflow)
}

fn sum_compact_sizes(
    bytes: &[u8],
    sample_count: u64,
    field_size: u8,
) -> Result<u64, MediaError> {
    let mut total = 0_u64;
    for index in 0..sample_count {
        let value = match field_size {
            4 => {
                let byte_index = usize::try_from(index / 2)
                    .map_err(|_| MediaError::ArithmeticOverflow)?;
                let byte = *bytes
                    .get(byte_index)
                    .ok_or(MediaError::ArithmeticOverflow)?;
                if index % 2 == 0 {
                    u64::from(byte >> 4)
                } else {
                    u64::from(byte & 0x0f)
                }
            }
            8 => {
                let byte_index =
                    usize::try_from(index).map_err(|_| MediaError::ArithmeticOverflow)?;
                u64::from(
                    *bytes
                        .get(byte_index)
                        .ok_or(MediaError::ArithmeticOverflow)?,
                )
            }
            16 => {
                let byte_index = usize::try_from(
                    index
                        .checked_mul(2)
                        .ok_or(MediaError::ArithmeticOverflow)?,
                )
                .map_err(|_| MediaError::ArithmeticOverflow)?;
                let high = *bytes
                    .get(byte_index)
                    .ok_or(MediaError::ArithmeticOverflow)?;
                let low_index = byte_index
                    .checked_add(1)
                    .ok_or(MediaError::ArithmeticOverflow)?;
                let low = *bytes
                    .get(low_index)
                    .ok_or(MediaError::ArithmeticOverflow)?;
                u64::from(u16::from_be_bytes([high, low]))
            }
            other => {
                return Err(MediaError::InvalidCompactSampleFieldSize {
                    field_size: other,
                });
            }
        };
        total = total
            .checked_add(value)
            .ok_or(MediaError::ArithmeticOverflow)?;
    }
    Ok(total)
}
