#![forbid(unsafe_code)]
#![allow(clippy::too_many_lines)]
//! Bounded expansion of validated classic sample-table descriptors.

use super::scan::{read_u8, read_u16, read_u32, read_u64};
use super::{
    ByteRange, CompositionTable, FixedTable, SampleIndexError, SampleRecord, SampleSizeTable,
    SampleToChunkEntry, SampleWindowRequest, ScanBudget, TrackIndex, inconsistent,
};
use crate::MediaError;
use std::collections::BTreeSet;
use std::io::{Read, Seek};

pub(super) fn expand_window<R: Read + Seek>(
    reader: &mut R,
    request: SampleWindowRequest,
    total_samples: u64,
    track: &TrackIndex,
    media_data_ranges: &[ByteRange],
    returned: u64,
    budget: &mut ScanBudget,
) -> Result<Vec<SampleRecord>, SampleIndexError> {
    let stts = track.stts.ok_or(SampleIndexError::ClassicTableUnavailable {
        track_id: request.track_id,
        table: "stts",
    })?;
    let sizes = track
        .sample_sizes
        .ok_or(SampleIndexError::ClassicTableUnavailable {
            track_id: request.track_id,
            table: "stsz_or_stz2",
        })?;
    if sizes.count() != total_samples {
        return inconsistent(
            Some(request.track_id),
            "sample-size cardinality differs from validated summary",
        );
    }
    let chunk_offsets = track
        .chunk_offsets
        .ok_or(SampleIndexError::ClassicTableUnavailable {
            track_id: request.track_id,
            table: "stco_or_co64",
        })?;
    if track.sample_to_chunk.is_empty() {
        return Err(SampleIndexError::ClassicTableUnavailable {
            track_id: request.track_id,
            table: "stsc",
        });
    }

    let mut decode = DecodeCursor::seek(reader, stts, request.start_sample, budget)?;
    let mut composition = match track.ctts {
        Some(table) => Some(CompositionCursor::seek(
            reader,
            table,
            request.start_sample,
            budget,
        )?),
        None => None,
    };
    let sync_samples = sync_samples_for_window(
        reader,
        track.sync_samples,
        request.start_sample,
        returned,
        budget,
    )?;
    let mut chunk = ChunkCursor::seek(
        reader,
        sizes,
        chunk_offsets,
        &track.sample_to_chunk,
        request.start_sample,
        budget,
    )?;
    let capacity = usize::try_from(returned).map_err(|_| MediaError::ArithmeticOverflow)?;
    let mut samples = Vec::with_capacity(capacity);
    for relative in 0..returned {
        let sample_index = request
            .start_sample
            .checked_add(relative)
            .ok_or(MediaError::ArithmeticOverflow)?;
        let (decode_time, duration) = decode.next(reader, budget)?;
        let composition_offset = match composition.as_mut() {
            Some(cursor) => cursor.next(reader, budget)?,
            None => 0,
        };
        let composition_time = i128::from(decode_time) + i128::from(composition_offset);
        let byte_length = read_sample_size(reader, sizes, sample_index, budget)?;
        let byte_offset = chunk.byte_offset;
        let byte_end = byte_offset
            .checked_add(u64::from(byte_length))
            .ok_or(MediaError::ArithmeticOverflow)?;
        let in_media = media_data_ranges.iter().any(|range| {
            byte_offset >= range.start && byte_offset < range.end && byte_end <= range.end
        });
        if !in_media {
            return Err(SampleIndexError::SampleOutsideMediaData {
                track_id: request.track_id,
                sample_index,
                byte_offset,
                byte_length,
            });
        }
        samples.push(SampleRecord {
            sample_index,
            decode_time,
            composition_time,
            duration,
            byte_offset,
            byte_length,
            is_sync: track.sync_samples.is_none() || sync_samples.contains(&sample_index),
            sample_description_index: chunk.sample_description_index,
        });
        if relative.saturating_add(1) < returned {
            chunk.advance(
                reader,
                chunk_offsets,
                &track.sample_to_chunk,
                byte_length,
                budget,
            )?;
        }
    }
    Ok(samples)
}

#[derive(Clone, Copy, Debug)]
struct DecodeCursor {
    table: FixedTable,
    next_entry: u64,
    remaining: u64,
    delta: u32,
    decode_time: u64,
}

impl DecodeCursor {
    fn seek<R: Read + Seek>(
        reader: &mut R,
        table: FixedTable,
        target: u64,
        budget: &mut ScanBudget,
    ) -> Result<Self, SampleIndexError> {
        let mut samples_before = 0_u64;
        let mut time_before = 0_u64;
        for entry_index in 0..table.count {
            budget.charge(1)?;
            let offset = table_entry_offset(table, entry_index)?;
            let count = u64::from(read_u32(reader, offset)?);
            let delta = read_u32(reader, add(offset, 4)?)?;
            let end = samples_before
                .checked_add(count)
                .ok_or(MediaError::ArithmeticOverflow)?;
            if target < end {
                let in_run = target.saturating_sub(samples_before);
                let partial = in_run
                    .checked_mul(u64::from(delta))
                    .ok_or(MediaError::ArithmeticOverflow)?;
                return Ok(Self {
                    table,
                    next_entry: entry_index.saturating_add(1),
                    remaining: count.saturating_sub(in_run),
                    delta,
                    decode_time: time_before
                        .checked_add(partial)
                        .ok_or(MediaError::ArithmeticOverflow)?,
                });
            }
            samples_before = end;
            time_before = time_before
                .checked_add(
                    count
                        .checked_mul(u64::from(delta))
                        .ok_or(MediaError::ArithmeticOverflow)?,
                )
                .ok_or(MediaError::ArithmeticOverflow)?;
        }
        inconsistent(None, "stts does not cover requested sample")
    }

    fn next<R: Read + Seek>(
        &mut self,
        reader: &mut R,
        budget: &mut ScanBudget,
    ) -> Result<(u64, u32), SampleIndexError> {
        while self.remaining == 0 {
            if self.next_entry >= self.table.count {
                return inconsistent(None, "stts ended before requested window");
            }
            budget.charge(1)?;
            let offset = table_entry_offset(self.table, self.next_entry)?;
            self.remaining = u64::from(read_u32(reader, offset)?);
            self.delta = read_u32(reader, add(offset, 4)?)?;
            self.next_entry = self.next_entry.saturating_add(1);
        }
        let current = self.decode_time;
        self.decode_time = self
            .decode_time
            .checked_add(u64::from(self.delta))
            .ok_or(MediaError::ArithmeticOverflow)?;
        self.remaining = self.remaining.saturating_sub(1);
        Ok((current, self.delta))
    }
}

#[derive(Clone, Copy, Debug)]
struct CompositionCursor {
    table: CompositionTable,
    next_entry: u64,
    remaining: u64,
    offset: i64,
}

impl CompositionCursor {
    fn seek<R: Read + Seek>(
        reader: &mut R,
        table: CompositionTable,
        target: u64,
        budget: &mut ScanBudget,
    ) -> Result<Self, SampleIndexError> {
        let mut samples_before = 0_u64;
        for entry_index in 0..table.table.count {
            budget.charge(1)?;
            let entry_offset = table_entry_offset(table.table, entry_index)?;
            let count = u64::from(read_u32(reader, entry_offset)?);
            let end = samples_before
                .checked_add(count)
                .ok_or(MediaError::ArithmeticOverflow)?;
            if target < end {
                return Ok(Self {
                    table,
                    next_entry: entry_index.saturating_add(1),
                    remaining: count.saturating_sub(target.saturating_sub(samples_before)),
                    offset: read_composition_offset(reader, entry_offset, table.version)?,
                });
            }
            samples_before = end;
        }
        inconsistent(None, "ctts does not cover requested sample")
    }

    fn next<R: Read + Seek>(
        &mut self,
        reader: &mut R,
        budget: &mut ScanBudget,
    ) -> Result<i64, SampleIndexError> {
        while self.remaining == 0 {
            if self.next_entry >= self.table.table.count {
                return inconsistent(None, "ctts ended before requested window");
            }
            budget.charge(1)?;
            let entry_offset = table_entry_offset(self.table.table, self.next_entry)?;
            self.remaining = u64::from(read_u32(reader, entry_offset)?);
            self.offset = read_composition_offset(reader, entry_offset, self.table.version)?;
            self.next_entry = self.next_entry.saturating_add(1);
        }
        self.remaining = self.remaining.saturating_sub(1);
        Ok(self.offset)
    }
}

fn read_composition_offset<R: Read + Seek>(
    reader: &mut R,
    entry_offset: u64,
    version: u8,
) -> Result<i64, SampleIndexError> {
    let raw = read_u32(reader, add(entry_offset, 4)?)?;
    if version == 0 {
        Ok(i64::from(raw))
    } else {
        Ok(i64::from(i32::from_be_bytes(raw.to_be_bytes())))
    }
}

#[derive(Clone, Copy, Debug)]
struct ChunkCursor {
    chunk_number: u64,
    chunk_count: u64,
    stsc_index: usize,
    samples_per_chunk: u64,
    sample_in_chunk: u64,
    byte_offset: u64,
    sample_description_index: u32,
}

impl ChunkCursor {
    fn seek<R: Read + Seek>(
        reader: &mut R,
        sizes: SampleSizeTable,
        chunks: FixedTable,
        stsc: &[SampleToChunkEntry],
        target: u64,
        budget: &mut ScanBudget,
    ) -> Result<Self, SampleIndexError> {
        let mut samples_before = 0_u64;
        for (index, entry) in stsc.iter().enumerate() {
            budget.charge(1)?;
            let run_start = u64::from(entry.first_chunk);
            let run_end = match stsc.get(index.saturating_add(1)) {
                Some(next) => u64::from(next.first_chunk),
                None => chunks
                    .count
                    .checked_add(1)
                    .ok_or(MediaError::ArithmeticOverflow)?,
            };
            let run_chunks = run_end.saturating_sub(run_start);
            let run_samples = run_chunks
                .checked_mul(u64::from(entry.samples_per_chunk))
                .ok_or(MediaError::ArithmeticOverflow)?;
            let samples_end = samples_before
                .checked_add(run_samples)
                .ok_or(MediaError::ArithmeticOverflow)?;
            if target < samples_end {
                let inside = target.saturating_sub(samples_before);
                let samples_per_chunk = u64::from(entry.samples_per_chunk);
                let chunk_delta = inside / samples_per_chunk;
                let sample_in_chunk = inside % samples_per_chunk;
                let chunk_number = run_start
                    .checked_add(chunk_delta)
                    .ok_or(MediaError::ArithmeticOverflow)?;
                let chunk_base = read_chunk_offset(reader, chunks, chunk_number, budget)?;
                let chunk_first_sample = target.saturating_sub(sample_in_chunk);
                let mut byte_offset = chunk_base;
                for sample_index in chunk_first_sample..target {
                    let size = read_sample_size(reader, sizes, sample_index, budget)?;
                    byte_offset = byte_offset
                        .checked_add(u64::from(size))
                        .ok_or(MediaError::ArithmeticOverflow)?;
                }
                return Ok(Self {
                    chunk_number,
                    chunk_count: chunks.count,
                    stsc_index: index,
                    samples_per_chunk,
                    sample_in_chunk,
                    byte_offset,
                    sample_description_index: entry.sample_description_index,
                });
            }
            samples_before = samples_end;
        }
        inconsistent(None, "stsc does not cover requested sample")
    }

    fn advance<R: Read + Seek>(
        &mut self,
        reader: &mut R,
        chunks: FixedTable,
        stsc: &[SampleToChunkEntry],
        byte_length: u32,
        budget: &mut ScanBudget,
    ) -> Result<(), SampleIndexError> {
        self.byte_offset = self
            .byte_offset
            .checked_add(u64::from(byte_length))
            .ok_or(MediaError::ArithmeticOverflow)?;
        self.sample_in_chunk = self.sample_in_chunk.saturating_add(1);
        if self.sample_in_chunk < self.samples_per_chunk {
            return Ok(());
        }
        if self.chunk_number >= self.chunk_count {
            return Ok(());
        }
        self.chunk_number = self
            .chunk_number
            .checked_add(1)
            .ok_or(MediaError::ArithmeticOverflow)?;
        if let Some(next) = stsc.get(self.stsc_index.saturating_add(1)) {
            if self.chunk_number == u64::from(next.first_chunk) {
                self.stsc_index = self.stsc_index.saturating_add(1);
            }
        }
        let entry = stsc
            .get(self.stsc_index)
            .ok_or(SampleIndexError::InconsistentIndex {
                track_id: None,
                reason: "stsc cursor moved beyond retained entries",
            })?;
        self.samples_per_chunk = u64::from(entry.samples_per_chunk);
        self.sample_description_index = entry.sample_description_index;
        self.sample_in_chunk = 0;
        self.byte_offset = read_chunk_offset(reader, chunks, self.chunk_number, budget)?;
        Ok(())
    }
}

fn read_sample_size<R: Read + Seek>(
    reader: &mut R,
    table: SampleSizeTable,
    sample_index: u64,
    budget: &mut ScanBudget,
) -> Result<u32, SampleIndexError> {
    if sample_index >= table.count() {
        return inconsistent(None, "sample-size index is out of range");
    }
    budget.charge(1)?;
    match table {
        SampleSizeTable::Constant { sample_size, .. } => Ok(sample_size),
        SampleSizeTable::U32 { entries_start, .. } => read_u32(
            reader,
            add(
                entries_start,
                sample_index
                    .checked_mul(4)
                    .ok_or(MediaError::ArithmeticOverflow)?,
            )?,
        ),
        SampleSizeTable::Compact {
            entries_start,
            field_size,
            ..
        } => read_compact_size(reader, entries_start, sample_index, field_size),
    }
}

fn read_compact_size<R: Read + Seek>(
    reader: &mut R,
    entries_start: u64,
    sample_index: u64,
    field_size: u8,
) -> Result<u32, SampleIndexError> {
    match field_size {
        4 => {
            let byte = read_u8(reader, add(entries_start, sample_index / 2)?)?;
            Ok(if sample_index % 2 == 0 {
                u32::from(byte >> 4)
            } else {
                u32::from(byte & 0x0f)
            })
        }
        8 => Ok(u32::from(read_u8(
            reader,
            add(entries_start, sample_index)?,
        )?)),
        16 => {
            let offset = add(
                entries_start,
                sample_index
                    .checked_mul(2)
                    .ok_or(MediaError::ArithmeticOverflow)?,
            )?;
            Ok(u32::from(read_u16(reader, offset)?))
        }
        _ => Err(MediaError::InvalidCompactSampleFieldSize { field_size }.into()),
    }
}

fn read_chunk_offset<R: Read + Seek>(
    reader: &mut R,
    table: FixedTable,
    chunk_number: u64,
    budget: &mut ScanBudget,
) -> Result<u64, SampleIndexError> {
    if chunk_number == 0 || chunk_number > table.count {
        return inconsistent(None, "chunk number is outside chunk-offset table");
    }
    budget.charge(1)?;
    let offset = table_entry_offset(table, chunk_number.saturating_sub(1))?;
    if table.entry_bytes == 4 {
        Ok(u64::from(read_u32(reader, offset)?))
    } else if table.entry_bytes == 8 {
        read_u64(reader, offset)
    } else {
        inconsistent(None, "chunk-offset width is neither 32 nor 64 bits")
    }
}

fn sync_samples_for_window<R: Read + Seek>(
    reader: &mut R,
    table: Option<FixedTable>,
    start_sample: u64,
    returned: u64,
    budget: &mut ScanBudget,
) -> Result<BTreeSet<u64>, SampleIndexError> {
    let Some(table) = table else {
        return Ok(BTreeSet::new());
    };
    let first_one_based = start_sample
        .checked_add(1)
        .ok_or(MediaError::ArithmeticOverflow)?;
    let end_exclusive = start_sample
        .checked_add(returned)
        .ok_or(MediaError::ArithmeticOverflow)?;
    let mut low = 0_u64;
    let mut high = table.count;
    while low < high {
        let middle = low + (high - low) / 2;
        budget.charge(1)?;
        let value = u64::from(read_u32(reader, table_entry_offset(table, middle)?)?);
        if value < first_one_based {
            low = middle.saturating_add(1);
        } else {
            high = middle;
        }
    }
    let mut sync = BTreeSet::new();
    let mut index = low;
    while index < table.count {
        budget.charge(1)?;
        let one_based = u64::from(read_u32(reader, table_entry_offset(table, index)?)?);
        let zero_based = one_based.saturating_sub(1);
        if zero_based >= end_exclusive {
            break;
        }
        if zero_based >= start_sample {
            sync.insert(zero_based);
        }
        index = index.saturating_add(1);
    }
    Ok(sync)
}

fn table_entry_offset(table: FixedTable, entry_index: u64) -> Result<u64, SampleIndexError> {
    add(
        table.entries_start,
        entry_index
            .checked_mul(table.entry_bytes)
            .ok_or(MediaError::ArithmeticOverflow)?,
    )
}

fn add(left: u64, right: u64) -> Result<u64, SampleIndexError> {
    left.checked_add(right)
        .ok_or_else(|| MediaError::ArithmeticOverflow.into())
}
