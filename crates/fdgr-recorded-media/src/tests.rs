#![forbid(unsafe_code)]
#![allow(
    clippy::cast_lossless,
    clippy::cast_possible_truncation,
    clippy::indexing_slicing,
    clippy::too_many_lines
)]

use super::{
    RecordedMediaIngestError, RecordedMediaIngestOptions, RecordedMediaRoot,
    ingest_recorded_media,
};
use fdgr_object_store::LocalObjectStore;
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

fn test_root(label: &str) -> PathBuf {
    let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
    std::env::temp_dir().join(format!(
        "fdgr-recorded-media-{label}-{}-{id}",
        std::process::id()
    ))
}

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

fn classic_file() -> Vec<u8> {
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

    let stbl = make_container(*b"stbl", &[stsd, stts, stsz, stco, stsc]);
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

fn write_source(path: &Path, bytes: &[u8]) -> bool {
    let Some(parent) = path.parent() else {
        return false;
    };
    fs::create_dir_all(parent).is_ok() && fs::write(path, bytes).is_ok()
}

#[test]
fn root_codec_round_trips_exact_child_identities() {
    let source = fdgr_object_store::ImportReceipt {
        schema: fdgr_object_store::IMPORT_RECEIPT_SCHEMA,
        object_digest: fdgr_types::EvidenceDigest::from_bytes([1_u8; 32]),
        manifest_digest: fdgr_types::EvidenceDigest::from_bytes([2_u8; 32]),
        object_length: 100,
        chunk_size: 64,
        chunk_count: 2,
        object_created: true,
        manifest_created: true,
        staging_cleanup_complete: true,
        staging_entry: None,
        stage: fdgr_types::PublicationStage::Published,
    };
    let inspection = fdgr_object_store::ImportReceipt {
        schema: fdgr_object_store::IMPORT_RECEIPT_SCHEMA,
        object_digest: fdgr_types::EvidenceDigest::from_bytes([3_u8; 32]),
        manifest_digest: fdgr_types::EvidenceDigest::from_bytes([4_u8; 32]),
        object_length: 200,
        chunk_size: 128,
        chunk_count: 2,
        object_created: true,
        manifest_created: true,
        staging_cleanup_complete: true,
        staging_entry: None,
        stage: fdgr_types::PublicationStage::Published,
    };
    let root = RecordedMediaRoot::from_receipts(&source, &inspection);
    let bytes = root.to_canonical_bytes();
    assert!(matches!(
        bytes,
        Ok(ref encoded)
            if matches!(RecordedMediaRoot::from_canonical_bytes(encoded), Ok(ref decoded) if decoded == &root)
    ));
}

#[test]
fn ingest_publishes_a_verifiable_root_last_graph() {
    let root_path = test_root("success");
    let source_path = root_path.join("source.mp4");
    assert!(write_source(&source_path, &classic_file()));
    let mut store = LocalObjectStore::open(root_path.join("store"));
    assert!(store.is_ok());
    if let Ok(ref mut store) = store {
        let receipt = ingest_recorded_media(
            store,
            &source_path,
            RecordedMediaIngestOptions {
                source_chunk_size: 16,
                derived_chunk_size: 64,
                ..RecordedMediaIngestOptions::default()
            },
        );
        assert!(receipt.is_ok());
        if let Ok(receipt) = receipt {
            assert!(store.verify_manifest(&receipt.source.manifest_digest).is_ok());
            assert!(store
                .verify_manifest(&receipt.inspection.artifact.manifest_digest)
                .is_ok());
            assert!(store.verify_manifest(&receipt.root.manifest_digest).is_ok());
            let root_object = store.open_verified_object(&receipt.root.manifest_digest);
            assert!(root_object.is_ok());
            if let Ok(mut root_object) = root_object {
                let mut bytes = Vec::new();
                assert!(root_object.read_to_end(&mut bytes).is_ok());
                assert!(matches!(
                    RecordedMediaRoot::from_canonical_bytes(&bytes),
                    Ok(ref decoded) if decoded == &receipt.root_value()
                ));
            }
        }
    }
    assert!(fs::remove_dir_all(root_path).is_ok());
}

#[test]
fn inspection_failure_retains_the_durable_source_receipt() {
    let root_path = test_root("invalid-media");
    let source_path = root_path.join("source.bin");
    assert!(write_source(&source_path, b"this is not an ISO BMFF object"));
    let mut store = LocalObjectStore::open(root_path.join("store"));
    assert!(store.is_ok());
    if let Ok(ref mut store) = store {
        let result = ingest_recorded_media(
            store,
            &source_path,
            RecordedMediaIngestOptions {
                source_chunk_size: 8,
                derived_chunk_size: 64,
                ..RecordedMediaIngestOptions::default()
            },
        );
        assert!(matches!(
            &result,
            Err(RecordedMediaIngestError::Inspection { .. })
        ));
        if let Err(error) = result {
            assert!(error.source_receipt().is_some());
            if let Some(receipt) = error.source_receipt() {
                assert!(store.verify_manifest(&receipt.manifest_digest).is_ok());
            }
            assert!(error.published_inspection().is_none());
        }
    }
    assert!(fs::remove_dir_all(root_path).is_ok());
}
