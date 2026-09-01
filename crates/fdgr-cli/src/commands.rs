#![forbid(unsafe_code)]
//! Command dispatch and semantic operations for the current CLI.

use crate::args::{
    parse_format, parse_import, parse_manifest_view, parse_media_inspect, parse_media_samples,
    parse_store_verify, parse_verify,
};
use crate::decode_args::parse_media_decode_plan;
use crate::decode_render::print_media_decode_plan;
use crate::recorded_args::{
    parse_recorded_media_ingest, parse_recorded_media_timeline, parse_recorded_media_verify,
};
use crate::recorded_render::{
    print_recorded_media_ingest, print_recorded_media_timeline, print_verified_recorded_media,
};
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
use fdgr_media::{FourCc, inspect_iso_bmff_file, read_classic_sample_window_file};
use fdgr_media_custody::{inspect_published_media, read_published_sample_window};
use fdgr_media_timeline::{TimelineBasis, build_sample_timeline};
use fdgr_media_worker::{MediaDecodePlan, MediaDecodePlanInput};
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
        "media-decode-plan" => media_decode_plan(rest),
        "media-inspect" => media_inspect(rest),
        "media-samples" => media_samples(rest),
        "recorded-media-ingest" => recorded_media_ingest(rest),
        "recorded-media-timeline" => recorded_media_timeline(rest),
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
        "  fdgr media-decode-plan <store-root> <recorded-media-root-manifest-digest> --track-id id --max-samples n --pixel-format gray8|rgb24|rgba|yuv420p --width pixels --height pixels --worker-executable-digest digest --worker-version-digest digest --profile-digest digest [bounded options] [--format text|json]"
    );
    println!(
        "  fdgr media-samples <path> --track-id <id> [--start-sample n] [--sample-limit n] [--max-window-records n] [--max-index-entries-scanned n] [bounded parser options] [--format text|json]"
    );
    println!(
        "  fdgr recorded-media-ingest <store-root> <source-path> [--source-chunk-size bytes] [--derived-chunk-size bytes] [bounded parser options] [--format text|json]"
    );
    println!(
        "  fdgr recorded-media-timeline <store-root> <root-manifest-digest> --track-id <id> [--start-sample n] [--sample-limit n] [--max-window-records n] [--max-index-entries-scanned n] [bounded parser options] [--format text|json]"
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

fn media_decode_plan(arguments: &[String]) -> Result<(), String> {
    let options = parse_media_decode_plan(arguments)?;
    let store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    let verified = verify_recorded_media_root(&store, &options.root_manifest_digest)
        .map_err(|error| format!("recorded-media verification failed: {error}"))?;
    let track = verified
        .inspection
        .summary
        .tracks
        .iter()
        .find(|track| track.track_id == options.track_id)
        .ok_or_else(|| {
            format!(
                "recorded-media root contains no track with id {}",
                options.track_id
            )
        })?;
    if track.handler_type != FourCc::new(*b"vide") {
        return Err(format!(
            "track {} has handler {}; a decode plan requires a video track",
            options.track_id, track.handler_type
        ));
    }
    let sample_count = track.sample_count.ok_or_else(|| {
        format!(
            "track {} has no complete classic sample count; deterministic sample-range planning is unavailable",
            options.track_id
        )
    })?;
    let end_sample = options
        .start_sample
        .checked_add(options.max_samples)
        .ok_or_else(|| "requested sample range overflows u64".to_owned())?;
    if end_sample > sample_count {
        return Err(format!(
            "requested sample range [{}..{}) exceeds track {} sample count {}",
            options.start_sample, end_sample, options.track_id, sample_count
        ));
    }
    let plan = MediaDecodePlan::new(MediaDecodePlanInput {
        source_root_manifest_digest: options.root_manifest_digest.clone(),
        source_manifest_digest: verified.root.source_manifest_digest.clone(),
        source_object_digest: verified.root.source_object_digest.clone(),
        source_object_length: verified.root.source_object_length,
        track_id: options.track_id,
        start_sample: options.start_sample,
        max_samples: options.max_samples,
        pixel_format: options.pixel_format,
        output_width: options.output_width,
        output_height: options.output_height,
        max_frames: options.max_frames,
        max_output_bytes: options.max_output_bytes,
        max_wall_time_ms: options.max_wall_time_ms,
        max_memory_bytes: options.max_memory_bytes,
        worker_executable_digest: options.worker_executable_digest,
        worker_version_digest: options.worker_version_digest,
        profile_digest: options.profile_digest,
        worker_threads: options.worker_threads,
        network_allowed: false,
        deterministic: true,
    })
    .map_err(|error| format!("media decode plan rejected: {error}"))?;
    print_media_decode_plan(&plan, options.format)
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

fn recorded_media_timeline(arguments: &[String]) -> Result<(), String> {
    let options = parse_recorded_media_timeline(arguments)?;
    let store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    let verified = verify_recorded_media_root(&store, &options.root_manifest_digest)
        .map_err(|error| format!("recorded-media verification failed: {error}"))?;
    let source = read_published_sample_window(
        &store,
        &verified.root.source_manifest_digest,
        options.request,
        options.parse_limits,
        options.window_limits,
    )
    .map_err(|error| format!("recorded-media timeline source refused: {error}"))?;
    if source.manifest.manifest_digest != verified.root.source_manifest_digest
        || source.manifest.object_digest != verified.root.source_object_digest
        || source.manifest.object_length != verified.root.source_object_length
    {
        return Err("recorded-media timeline source identity drifted after root verification".to_owned());
    }
    if source.summary != verified.inspection.summary {
        return Err(
            "recorded-media timeline reinspection disagrees with the published inspection artifact"
                .to_owned(),
        );
    }
    let timeline = build_sample_timeline(
        TimelineBasis {
            recorded_media_root_manifest_digest: options.root_manifest_digest,
            source_manifest_digest: source.manifest.manifest_digest.clone(),
            source_object_digest: source.manifest.object_digest.clone(),
            source_object_length: source.manifest.object_length,
            track_id: source.window.track_id,
            timescale: source.window.timescale,
        },
        &source.window,
    )
    .map_err(|error| format!("recorded-media timeline rejected: {error}"))?;
    print_recorded_media_timeline(&timeline, options.format)
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
