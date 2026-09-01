#![forbid(unsafe_code)]
#![allow(clippy::indexing_slicing, clippy::too_many_lines)]

use super::{inspect_published_media, read_published_sample_window};
use fdgr_media::{ParseLimits, SampleWindowLimits, SampleWindowRequest};
use fdgr_object_store::{ImportReceipt, LocalObjectStore};
use std::fs;
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};

static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

struct Fixture {
    root: PathBuf,
    source: PathBuf,
    store: LocalObjectStore,
    receipt: ImportReceipt,
}

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

fn classic_fixture_bytes() -> Vec<u8> {
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
    let mut stss_payload = vec![0_u8; 12];
    write_u32(&mut stss_payload, 4, 1);
    write_u32(&mut stss_payload, 8, 1);
    let stss = make_box(*b"stss", &stss_payload);
    let stbl = make_container(*b"stbl", &[stsd, stts, stsz, stco, stsc, stss]);
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

fn prepare(label: &str) -> Option<Fixture> {
    let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
    let root = std::env::temp_dir().join(format!(
        "fdgr-media-custody-{label}-{}-{id}",
        std::process::id()
    ));
    if fs::create_dir_all(&root).is_err() {
        return None;
    }
    let source = root.join("source.mp4");
    if fs::write(&source, classic_fixture_bytes()).is_err() {
        let _ = fs::remove_dir_all(&root);
        return None;
    }
    let mut store = match LocalObjectStore::open(root.join("store")) {
        Ok(value) => value,
        Err(_) => {
            let _ = fs::remove_dir_all(&root);
            return None;
        }
    };
    let receipt = match store.import_file(&source, 64) {
        Ok(value) => value,
        Err(_) => {
            let _ = fs::remove_dir_all(&root);
            return None;
        }
    };
    Some(Fixture {
        root,
        source,
        store,
        receipt,
    })
}

#[test]
fn published_inspection_is_bound_to_manifest_and_object() {
    let prepared = prepare("inspect");
    assert!(prepared.is_some());
    if let Some(fixture) = prepared {
        let result = inspect_published_media(
            &fixture.store,
            &fixture.receipt.manifest_digest,
            ParseLimits::default(),
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.manifest.manifest_digest == fixture.receipt.manifest_digest
                    && value.manifest.object_digest == fixture.receipt.object_digest
                    && value.summary.tracks.len() == 1
                    && value.summary.tracks[0].sample_count == Some(4)
        ));
        assert!(fs::remove_dir_all(fixture.root).is_ok());
    }
}

#[test]
fn changing_original_path_cannot_change_published_inspection() {
    let prepared = prepare("source-change");
    assert!(prepared.is_some());
    if let Some(fixture) = prepared {
        assert!(fs::write(&fixture.source, b"mutated source path").is_ok());
        let result = inspect_published_media(
            &fixture.store,
            &fixture.receipt.manifest_digest,
            ParseLimits::default(),
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.manifest.object_digest == fixture.receipt.object_digest
                    && value.summary.movie_duration == 4_000
        ));
        assert!(fs::remove_dir_all(fixture.root).is_ok());
    }
}

#[test]
fn published_sample_window_retains_custody_identity() {
    let prepared = prepare("samples");
    assert!(prepared.is_some());
    if let Some(fixture) = prepared {
        let result = read_published_sample_window(
            &fixture.store,
            &fixture.receipt.manifest_digest,
            SampleWindowRequest {
                track_id: 7,
                start_sample: 2,
                max_samples: 2,
            },
            ParseLimits::default(),
            SampleWindowLimits::default(),
        );
        assert!(matches!(
            result,
            Ok(ref value)
                if value.manifest.manifest_digest == fixture.receipt.manifest_digest
                    && value.window.samples.len() == 2
                    && value.window.samples[0].sample_index == 2
                    && value.window.samples[0].byte_offset == 35
                    && value.window.complete
        ));
        assert!(fs::remove_dir_all(fixture.root).is_ok());
    }
}
