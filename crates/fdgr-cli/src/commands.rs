#![forbid(unsafe_code)]
//! Command dispatch and semantic operations for the current CLI.

use crate::args::{
    parse_format, parse_import, parse_manifest_view, parse_media_inspect, parse_media_samples,
    parse_store_verify, parse_verify,
};
use crate::recorded_args::{parse_recorded_media_ingest, parse_recorded_media_verify};
use crate::recorded_render::{print_recorded_media_ingest, print_verified_recorded_media};
use crate::render::{
    json_escape, print_capabilities, print_doctor, print_file_verification, print_help,
    print_import_receipt, print_manifest, print_media_inspection, print_plan_summary,
    print_store_verification,
};
use crate::sample_render::print_sample_window;
use crate::stored_args::{parse_stored_media_inspect, parse_stored_media_samples};
use crate::stored_render::{print_stored_media_inspection, print_stored_sample_window};
use fdgr_core::{VALIDATE_ID_SCHEMA, VERSION};
use fdgr_evidence::build_file_manifest;
use fdgr_media::{inspect_iso_bmff_file, read_classic_sample_window_file};
use fdgr_media_custody::{inspect_published_media, read_published_sample_window};
use fdgr_object_store::LocalObjectStore;
use fdgr_recorded_media::ingest_recorded_media;
use fdgr_recorded_media_verify::verify_recorded_media_root;
use fdgr_types::EvidenceDigest;

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let Some((command, rest)) = arguments.split_first() else {
        print_complete_help();
        return Ok(());
    };
    match command.as_str() {
        "capabilities" => print_capabilities(parse_format(rest)?),
        "doctor" => print_doctor(parse_format(rest)?),
        "file-manifest" => file_manifest(rest),
        "import-file" => import_file(rest),
        "media-inspect" => media_inspect(rest),
        "media-samples" => media_samples(rest),
        "recorded-media-ingest" => recorded_media_ingest(rest),
        "recorded-media-verify" => recorded_media_verify(rest),
        "stored-media-inspect" => stored_media_inspect(rest),
        "stored-media-samples" => stored_media_samples(rest),
        "verify-file" => verify_file(rest),
        "verify-store" => verify_store(rest),
        "plan-summary" => print_plan_summary(parse_format(rest)?),
        "validate-id" => validate_id(rest),
        "version" | "--version" | "-V" => {
            println!("fdgr {VERSION}");
            Ok(())
        }
        "help" | "--help" | "-h" => {
            print_complete_help();
            Ok(())
        }
        other => Err(format!("unknown command {other:?}; run `fdgr help`")),
    }
}

fn print_complete_help() {
    print_help();
    println!(
        "  fdgr media-samples <path> --track-id <id> [--start-sample n] [--sample-limit n] [--max-window-records n] [--max-index-entries-scanned n] [bounded parser options] [--format text|json]"
    );
    println!(
        "  fdgr recorded-media-ingest <store-root> <source-path> [--source-chunk-size bytes] [--derived-chunk-size bytes] [bounded parser options] [--format text|json]"
    );
    println!(
        "  fdgr recorded-media-verify <store-root> <root-manifest-digest> [--format text|json]"
    );
    println!(
        "  fdgr stored-media-inspect <store-root> <manifest-digest> [bounded parser options] [--format text|json]"
    );
    println!(
        "  fdgr stored-media-samples <store-root> <manifest-digest> --track-id <id> [bounded options] [--format text|json]"
    );
}

fn file_manifest(arguments: &[String]) -> Result<(), String> {
    let options = parse_manifest_view(arguments)?;
    let manifest = build_file_manifest(&options.path, options.chunk_size)
        .map_err(|error| format!("manifest build failed: {error}"))?;
    if options.chunk_offset > manifest.chunks.len() {
        return Err(format!(
            "chunk offset {} exceeds chunk count {}",
            options.chunk_offset,
            manifest.chunks.len()
        ));
    }
    print_manifest(&manifest, &options)
}

fn verify_file(arguments: &[String]) -> Result<(), String> {
    let options = parse_verify(arguments)?;
    let manifest = build_file_manifest(&options.path, options.chunk_size)
        .map_err(|error| format!("file verification failed: {error}"))?;
    if manifest.object_digest != options.object_digest {
        return Err(format!(
            "object digest mismatch: expected {}, observed {}",
            options.object_digest, manifest.object_digest
        ));
    }
    if manifest.manifest_digest != options.manifest_digest {
        return Err(format!(
            "manifest digest mismatch: expected {}, observed {}",
            options.manifest_digest, manifest.manifest_digest
        ));
    }
    print_file_verification(&manifest, options.format);
    Ok(())
}

fn import_file(arguments: &[String]) -> Result<(), String> {
    let options = parse_import(arguments)?;
    let mut store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    let receipt = store
        .import_file(&options.path, options.chunk_size)
        .map_err(|error| format!("file import failed: {error}"))?;
    print_import_receipt(receipt, options.format);
    Ok(())
}

fn media_inspect(arguments: &[String]) -> Result<(), String> {
    let options = parse_media_inspect(arguments)?;
    let summary = inspect_iso_bmff_file(&options.path, options.limits)
        .map_err(|error| format!("media inspection failed: {error}"))?;
    print_media_inspection(&summary, options.format)
}

fn media_samples(arguments: &[String]) -> Result<(), String> {
    let options = parse_media_samples(arguments)?;
    let (summary, window) = read_classic_sample_window_file(
        &options.path,
        options.request,
        options.parse_limits,
        options.window_limits,
    )
    .map_err(|error| format!("sample-window inspection failed: {error}"))?;
    print_sample_window(&summary, &window, options.format)
}

fn recorded_media_ingest(arguments: &[String]) -> Result<(), String> {
    let options = parse_recorded_media_ingest(arguments)?;
    let mut store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    let receipt = ingest_recorded_media(&mut store, &options.source_path, options.ingest)
        .map_err(|error| format!("recorded-media ingest failed: {error}"))?;
    let root_manifest_digest = receipt.root_manifest_digest().clone();
    let verified = verify_recorded_media_root(&store, &root_manifest_digest).map_err(|error| {
        format!(
            "recorded-media root {root_manifest_digest} was published, but independent closure verification failed: {error}"
        )
    })?;
    print_recorded_media_ingest(&receipt, &verified, options.format)
}

fn recorded_media_verify(arguments: &[String]) -> Result<(), String> {
    let options = parse_recorded_media_verify(arguments)?;
    let store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    let verified = verify_recorded_media_root(&store, &options.root_manifest_digest)
        .map_err(|error| format!("recorded-media verification failed: {error}"))?;
    print_verified_recorded_media(&verified, options.format)
}

fn stored_media_inspect(arguments: &[String]) -> Result<(), String> {
    let options = parse_stored_media_inspect(arguments)?;
    let store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    let result = inspect_published_media(&store, &options.manifest_digest, options.limits)
        .map_err(|error| format!("stored media inspection failed: {error}"))?;
    print_stored_media_inspection(&result, options.format)
}

fn stored_media_samples(arguments: &[String]) -> Result<(), String> {
    let options = parse_stored_media_samples(arguments)?;
    let store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    let result = read_published_sample_window(
        &store,
        &options.manifest_digest,
        options.request,
        options.parse_limits,
        options.window_limits,
    )
    .map_err(|error| format!("stored media sample-window failed: {error}"))?;
    print_stored_sample_window(&result, options.format)
}

fn verify_store(arguments: &[String]) -> Result<(), String> {
    let options = parse_store_verify(arguments)?;
    let store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    store
        .verify_manifest(&options.manifest_digest)
        .map_err(|error| format!("store verification failed: {error}"))?;
    let manifest = store
        .read_manifest(&options.manifest_digest)
        .map_err(|error| format!("manifest readback failed: {error}"))?;
    print_store_verification(&manifest, options.format);
    Ok(())
}

fn validate_id(arguments: &[String]) -> Result<(), String> {
    let [value] = arguments else {
        return Err("usage: fdgr validate-id <64-lowercase-hex-digest>".to_owned());
    };
    match EvidenceDigest::parse(value) {
        Ok(digest) => {
            println!(
                "{{\"schema\":\"{}\",\"valid\":true,\"digest\":\"{}\"}}",
                json_escape(VALIDATE_ID_SCHEMA),
                digest.as_str()
            );
            Ok(())
        }
        Err(error) => Err(error.to_string()),
    }
}
