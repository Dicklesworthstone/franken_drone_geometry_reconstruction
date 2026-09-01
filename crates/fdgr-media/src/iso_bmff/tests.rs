#![allow(clippy::cast_possible_truncation, clippy::indexing_slicing)]

use super::{ParseLimits, inspect_iso_bmff};
use crate::{FourCc, MediaError};
use std::io::Cursor;

fn write_u32(buffer: &mut [u8], offset: usize, value: u32) {
    buffer[offset..offset + 4].copy_from_slice(&value.to_be_bytes());
}

fn make_box(box_type: [u8; 4], payload: &[u8]) -> Vec<u8> {
    let size = 8_u32 + payload.len() as u32;
    let mut output = Vec::with_capacity(size as usize);
    output.extend_from_slice(&size.to_be_bytes());
    output.extend_from_slice(&box_type);
    output.extend_from_slice(payload);
    output
}

fn make_container(box_type: [u8; 4], children: &[Vec<u8>]) -> Vec<u8> {
    let payload_length = children.iter().map(Vec::len).sum();
    let mut payload = Vec::with_capacity(payload_length);
    for child in children {
        payload.extend_from_slice(child);
    }
    make_box(box_type, &payload)
}

fn classic_file(stts_samples: u32, first_chunk_offset: u32) -> Vec<u8> {
    let mut ftyp_payload = Vec::new();
    ftyp_payload.extend_from_slice(b"isom");
    ftyp_payload.extend_from_slice(&0_u32.to_be_bytes());
    ftyp_payload.extend_from_slice(b"isom");
    let ftyp = make_box(*b"ftyp", &ftyp_payload);

    let mdat = make_box(*b"mdat", &[0_u8; 18]);

    let mut mvhd_payload = vec![0_u8; 20];
    write_u32(&mut mvhd_payload, 12, 1_000);
    write_u32(&mut mvhd_payload, 16, 4_000);
    let mvhd = make_box(*b"mvhd", &mvhd_payload);

    let mut tkhd_payload = vec![0_u8; 84];
    write_u32(&mut tkhd_payload, 12, 1);
    write_u32(&mut tkhd_payload, 76, 1_920_u32 << 16);
    write_u32(&mut tkhd_payload, 80, 1_080_u32 << 16);
    let tkhd = make_box(*b"tkhd", &tkhd_payload);

    let mut mdhd_payload = vec![0_u8; 20];
    write_u32(&mut mdhd_payload, 12, 1_000);
    write_u32(&mut mdhd_payload, 16, 4_000);
    let mdhd = make_box(*b"mdhd", &mdhd_payload);

    let mut hdlr_payload = vec![0_u8; 12];
    hdlr_payload[8..12].copy_from_slice(b"vide");
    let hdlr = make_box(*b"hdlr", &hdlr_payload);

    let mut stsd_payload = vec![0_u8; 8];
    write_u32(&mut stsd_payload, 4, 1);
    stsd_payload.extend_from_slice(&8_u32.to_be_bytes());
    stsd_payload.extend_from_slice(b"avc1");
    let stsd = make_box(*b"stsd", &stsd_payload);

    let mut stts_payload = vec![0_u8; 16];
    write_u32(&mut stts_payload, 4, 1);
    write_u32(&mut stts_payload, 8, stts_samples);
    write_u32(&mut stts_payload, 12, 1_000);
    let stts = make_box(*b"stts", &stts_payload);

    let mut stsz_payload = vec![0_u8; 28];
    write_u32(&mut stsz_payload, 8, 4);
    for (index, size) in [3_u32, 4, 5, 6].into_iter().enumerate() {
        write_u32(&mut stsz_payload, 12 + index * 4, size);
    }
    let stsz = make_box(*b"stsz", &stsz_payload);

    let mut stco_payload = vec![0_u8; 16];
    write_u32(&mut stco_payload, 4, 2);
    write_u32(&mut stco_payload, 8, first_chunk_offset);
    write_u32(&mut stco_payload, 12, first_chunk_offset + 7);
    let stco = make_box(*b"stco", &stco_payload);

    let mut stsc_payload = vec![0_u8; 20];
    write_u32(&mut stsc_payload, 4, 1);
    write_u32(&mut stsc_payload, 8, 1);
    write_u32(&mut stsc_payload, 12, 2);
    write_u32(&mut stsc_payload, 16, 1);
    let stsc = make_box(*b"stsc", &stsc_payload);

    let mut stss_payload = vec![0_u8; 16];
    write_u32(&mut stss_payload, 4, 2);
    write_u32(&mut stss_payload, 8, 1);
    write_u32(&mut stss_payload, 12, 3);
    let stss = make_box(*b"stss", &stss_payload);

    let stbl = make_container(*b"stbl", &[stsd, stts, stsz, stco, stsc, stss]);
    let minf = make_container(*b"minf", &[stbl]);
    let mdia = make_container(*b"mdia", &[mdhd, hdlr, minf]);
    let trak = make_container(*b"trak", &[tkhd, mdia]);
    let moov = make_container(*b"moov", &[mvhd, trak]);

    let mut file = Vec::with_capacity(ftyp.len() + mdat.len() + moov.len());
    file.extend_from_slice(&ftyp);
    file.extend_from_slice(&mdat);
    file.extend_from_slice(&moov);
    file
}

#[test]
fn classic_sample_tables_produce_a_consistent_summary() {
    let bytes = classic_file(4, 28);
    let mut reader = Cursor::new(bytes.clone());
    let result = inspect_iso_bmff(&mut reader, bytes.len() as u64, ParseLimits::default());
    assert!(matches!(
        result,
        Ok(ref summary)
            if summary.major_brand == Some(FourCc::new(*b"isom"))
                && summary.movie_timescale == 1_000
                && summary.movie_duration == 4_000
                && summary.tracks.len() == 1
                && summary.tracks[0].track_id == 1
                && summary.tracks[0].handler_type == FourCc::new(*b"vide")
                && summary.tracks[0].codec == Some(FourCc::new(*b"avc1"))
                && summary.tracks[0].sample_count == Some(4)
                && summary.tracks[0].decode_duration == Some(4_000)
                && summary.tracks[0].total_sample_bytes == Some(18)
                && summary.tracks[0].chunk_count == Some(2)
                && summary.tracks[0].sync_sample_count == Some(2)
                && summary.tracks[0].width_pixels() == 1_920
                && summary.tracks[0].height_pixels() == 1_080
    ));
}

#[test]
fn mismatched_sample_tables_fail_closed() {
    let bytes = classic_file(3, 28);
    let mut reader = Cursor::new(bytes.clone());
    assert!(matches!(
        inspect_iso_bmff(&mut reader, bytes.len() as u64, ParseLimits::default()),
        Err(MediaError::SampleCountMismatch {
            left_name: "stts",
            left: 3,
            right_name: "stsz_or_stz2",
            right: 4,
            ..
        })
    ));
}

#[test]
fn chunk_offsets_must_point_inside_media_payload() {
    let bytes = classic_file(4, 20);
    let mut reader = Cursor::new(bytes.clone());
    assert!(matches!(
        inspect_iso_bmff(&mut reader, bytes.len() as u64, ParseLimits::default()),
        Err(MediaError::ChunkOffsetOutsideMediaData {
            track_index: 0,
            entry_index: 0,
            offset: 20,
        })
    ));
}

#[test]
fn zero_limits_are_rejected_before_traversal() {
    let bytes = classic_file(4, 28);
    let mut reader = Cursor::new(bytes.clone());
    let limits = ParseLimits {
        max_boxes: 0,
        ..ParseLimits::default()
    };
    assert!(matches!(
        inspect_iso_bmff(&mut reader, bytes.len() as u64, limits),
        Err(MediaError::InvalidLimit { name: "max_boxes" })
    ));
}
