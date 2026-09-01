#![forbid(unsafe_code)]
#![allow(clippy::indexing_slicing, clippy::too_many_lines)]

use super::{
    SampleIndexError, SampleWindowLimits, SampleWindowRequest, read_classic_sample_window,
};
use crate::ParseLimits;
use std::io::Cursor;

fn write_u32(buffer: &mut [u8], offset: usize, value: u32) {
    if let Some(target) = buffer.get_mut(offset..offset.saturating_add(4)) {
        target.copy_from_slice(&value.to_be_bytes());
    }
}

fn make_box(box_type: [u8; 4], payload: &[u8]) -> Vec<u8> {
    let payload_length = match u32::try_from(payload.len()) {
        Ok(value) => value,
        Err(_) => return Vec::new(),
    };
    let size = 8_u32.saturating_add(payload_length);
    let capacity = match usize::try_from(size) {
        Ok(value) => value,
        Err(_) => return Vec::new(),
    };
    let mut output = Vec::with_capacity(capacity);
    output.extend_from_slice(&size.to_be_bytes());
    output.extend_from_slice(&box_type);
    output.extend_from_slice(payload);
    output
}

fn make_container(box_type: [u8; 4], children: &[Vec<u8>]) -> Vec<u8> {
    let capacity = children.iter().map(Vec::len).sum();
    let mut payload = Vec::with_capacity(capacity);
    for child in children {
        payload.extend_from_slice(child);
    }
    make_box(box_type, &payload)
}

fn fixture() -> Vec<u8> {
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
    write_u32(&mut tkhd_payload, 12, 7);
    write_u32(&mut tkhd_payload, 76, 1_920_u32 << 16);
    write_u32(&mut tkhd_payload, 80, 1_080_u32 << 16);
    let tkhd = make_box(*b"tkhd", &tkhd_payload);
    let mut mdhd_payload = vec![0_u8; 20];
    write_u32(&mut mdhd_payload, 12, 1_000);
    write_u32(&mut mdhd_payload, 16, 4_000);
    let mdhd = make_box(*b"mdhd", &mdhd_payload);
    let mut hdlr_payload = vec![0_u8; 12];
    if let Some(target) = hdlr_payload.get_mut(8..12) {
        target.copy_from_slice(b"vide");
    }
    let hdlr = make_box(*b"hdlr", &hdlr_payload);
    let mut stsd_payload = vec![0_u8; 8];
    write_u32(&mut stsd_payload, 4, 1);
    stsd_payload.extend_from_slice(&8_u32.to_be_bytes());
    stsd_payload.extend_from_slice(b"avc1");
    let stsd = make_box(*b"stsd", &stsd_payload);
    let mut stts_payload = vec![0_u8; 16];
    write_u32(&mut stts_payload, 4, 1);
    write_u32(&mut stts_payload, 8, 4);
    write_u32(&mut stts_payload, 12, 1_000);
    let stts = make_box(*b"stts", &stts_payload);
    let mut ctts_payload = vec![0_u8; 16];
    if let Some(version) = ctts_payload.first_mut() {
        *version = 1;
    }
    write_u32(&mut ctts_payload, 4, 1);
    write_u32(&mut ctts_payload, 8, 4);
    write_u32(
        &mut ctts_payload,
        12,
        u32::from_be_bytes((-100_i32).to_be_bytes()),
    );
    let ctts = make_box(*b"ctts", &ctts_payload);
    let mut stsz_payload = vec![0_u8; 28];
    write_u32(&mut stsz_payload, 8, 4);
    for (index, size) in [3_u32, 4, 5, 6].into_iter().enumerate() {
        write_u32(&mut stsz_payload, 12 + index * 4, size);
    }
    let stsz = make_box(*b"stsz", &stsz_payload);
    let mut stco_payload = vec![0_u8; 16];
    write_u32(&mut stco_payload, 4, 2);
    write_u32(&mut stco_payload, 8, 28);
    write_u32(&mut stco_payload, 12, 35);
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
    let stbl = make_container(
        *b"stbl",
        &[stsd, stts, ctts, stsz, stco, stsc, stss],
    );
    let minf = make_container(*b"minf", &[stbl]);
    let mdia = make_container(*b"mdia", &[mdhd, hdlr, minf]);
    let trak = make_container(*b"trak", &[tkhd, mdia]);
    let moov = make_container(*b"moov", &[mvhd, trak]);
    let mut file = Vec::new();
    file.extend_from_slice(&ftyp);
    file.extend_from_slice(&mdat);
    file.extend_from_slice(&moov);
    file
}

fn fixture_length(bytes: &[u8]) -> Option<u64> {
    u64::try_from(bytes.len()).ok()
}

#[test]
fn exact_window_carries_timestamps_offsets_and_sync_state() {
    let bytes = fixture();
    let Some(length) = fixture_length(&bytes) else {
        return;
    };
    let mut reader = Cursor::new(bytes);
    let result = read_classic_sample_window(
        &mut reader,
        length,
        SampleWindowRequest {
            track_id: 7,
            start_sample: 1,
            max_samples: 2,
        },
        ParseLimits::default(),
        SampleWindowLimits::default(),
    );
    assert!(matches!(
        result,
        Ok((_, ref window))
            if window.total_samples == 4
                && window.samples.len() == 2
                && window.samples[0].sample_index == 1
                && window.samples[0].decode_time == 1_000
                && window.samples[0].composition_time == 900
                && window.samples[0].byte_offset == 31
                && window.samples[0].byte_length == 4
                && !window.samples[0].is_sync
                && window.samples[1].sample_index == 2
                && window.samples[1].decode_time == 2_000
                && window.samples[1].composition_time == 1_900
                && window.samples[1].byte_offset == 35
                && window.samples[1].byte_length == 5
                && window.samples[1].is_sync
    ));
}

#[test]
fn unknown_track_and_out_of_range_start_are_typed() {
    let bytes = fixture();
    let Some(length) = fixture_length(&bytes) else {
        return;
    };
    let mut missing_reader = Cursor::new(bytes.clone());
    assert!(matches!(
        read_classic_sample_window(
            &mut missing_reader,
            length,
            SampleWindowRequest {
                track_id: 99,
                start_sample: 0,
                max_samples: 1,
            },
            ParseLimits::default(),
            SampleWindowLimits::default(),
        ),
        Err(SampleIndexError::TrackNotFound { track_id: 99 })
    ));
    let mut range_reader = Cursor::new(bytes);
    assert!(matches!(
        read_classic_sample_window(
            &mut range_reader,
            length,
            SampleWindowRequest {
                track_id: 7,
                start_sample: 5,
                max_samples: 1,
            },
            ParseLimits::default(),
            SampleWindowLimits::default(),
        ),
        Err(SampleIndexError::SampleStartOutOfRange {
            track_id: 7,
            start_sample: 5,
            total_samples: 4,
        })
    ));
}

#[test]
fn table_scan_budget_fails_closed() {
    let bytes = fixture();
    let Some(length) = fixture_length(&bytes) else {
        return;
    };
    let mut reader = Cursor::new(bytes);
    assert!(matches!(
        read_classic_sample_window(
            &mut reader,
            length,
            SampleWindowRequest {
                track_id: 7,
                start_sample: 1,
                max_samples: 2,
            },
            ParseLimits::default(),
            SampleWindowLimits {
                max_records: 2,
                max_index_entries_scanned: 1,
            },
        ),
        Err(SampleIndexError::IndexScanBudgetExceeded { .. })
    ));
}

#[test]
fn zero_record_request_is_rejected() {
    let bytes = fixture();
    let Some(length) = fixture_length(&bytes) else {
        return;
    };
    let mut reader = Cursor::new(bytes);
    assert!(matches!(
        read_classic_sample_window(
            &mut reader,
            length,
            SampleWindowRequest {
                track_id: 7,
                start_sample: 0,
                max_samples: 0,
            },
            ParseLimits::default(),
            SampleWindowLimits::default(),
        ),
        Err(SampleIndexError::InvalidWindowLimit {
            requested: 0,
            ..
        })
    ));
}
