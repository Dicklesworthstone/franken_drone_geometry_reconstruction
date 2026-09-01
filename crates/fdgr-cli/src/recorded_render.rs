#![forbid(unsafe_code)]
#![allow(clippy::too_many_lines)]
//! Stable rendering for root-last recorded-media publication and verification.

use crate::args::OutputFormat;
use crate::render::json_escape;
use fdgr_media::{FourCc, IsoBmffSummary};
use fdgr_recorded_media::{RECORDED_MEDIA_INGEST_SCHEMA, RecordedMediaIngestReceipt};
use fdgr_recorded_media_verify::{VERIFIED_RECORDED_MEDIA_SCHEMA, VerifiedRecordedMedia};
use std::fmt::Write as _;

pub(crate) fn print_recorded_media_ingest(
    receipt: &RecordedMediaIngestReceipt,
    verified: &VerifiedRecordedMedia,
    format: OutputFormat,
) -> Result<(), String> {
    match format {
        OutputFormat::Text => {
            println!("schema\t{RECORDED_MEDIA_INGEST_SCHEMA}");
            println!("publication_complete\ttrue");
            println!("closure_verified\ttrue");
            print_root_identity(
                &receipt.root.manifest_digest.to_string(),
                &receipt.root.object_digest.to_string(),
                receipt.root.object_length,
                receipt.root.chunk_size,
                receipt.root.chunk_count,
            );
            print_child_identity("source", &receipt.source);
            print_child_identity("inspection", &receipt.inspection.artifact);
            print_media_summary_text(&verified.inspection.summary);
            Ok(())
        }
        OutputFormat::Json => {
            println!("{}", recorded_media_ingest_json(receipt, verified)?);
            Ok(())
        }
    }
}

pub(crate) fn print_verified_recorded_media(
    verified: &VerifiedRecordedMedia,
    format: OutputFormat,
) -> Result<(), String> {
    match format {
        OutputFormat::Text => {
            println!("schema\t{VERIFIED_RECORDED_MEDIA_SCHEMA}");
            println!("closure_verified\ttrue");
            println!(
                "root_manifest_digest\t{}",
                verified.root_manifest_digest
            );
            println!("root_object_digest\t{}", verified.root_object_digest);
            print_decoded_child_identity(
                "source",
                &verified.root.source_manifest_digest.to_string(),
                &verified.root.source_object_digest.to_string(),
                verified.root.source_object_length,
                verified.root.source_chunk_size,
                verified.root.source_chunk_count,
            );
            print_decoded_child_identity(
                "inspection",
                &verified.root.inspection_manifest_digest.to_string(),
                &verified.root.inspection_object_digest.to_string(),
                verified.root.inspection_object_length,
                verified.root.inspection_chunk_size,
                verified.root.inspection_chunk_count,
            );
            print_media_summary_text(&verified.inspection.summary);
            Ok(())
        }
        OutputFormat::Json => {
            println!("{}", verified_recorded_media_json(verified)?);
            Ok(())
        }
    }
}

fn print_root_identity(
    manifest_digest: &str,
    object_digest: &str,
    object_length: u64,
    chunk_size: u32,
    chunk_count: u64,
) {
    print_decoded_child_identity(
        "root",
        manifest_digest,
        object_digest,
        object_length,
        chunk_size,
        chunk_count,
    );
}

fn print_child_identity(prefix: &str, receipt: &fdgr_object_store::ImportReceipt) {
    print_decoded_child_identity(
        prefix,
        &receipt.manifest_digest.to_string(),
        &receipt.object_digest.to_string(),
        receipt.object_length,
        receipt.chunk_size,
        receipt.chunk_count,
    );
}

fn print_decoded_child_identity(
    prefix: &str,
    manifest_digest: &str,
    object_digest: &str,
    object_length: u64,
    chunk_size: u32,
    chunk_count: u64,
) {
    println!("{prefix}_manifest_digest\t{manifest_digest}");
    println!("{prefix}_object_digest\t{object_digest}");
    println!("{prefix}_object_length\t{object_length}");
    println!("{prefix}_chunk_size\t{chunk_size}");
    println!("{prefix}_chunk_count\t{chunk_count}");
}

fn print_media_summary_text(summary: &IsoBmffSummary) {
    println!("media_scope\tcontainer_metadata_and_classic_sample_tables");
    println!("media_decode_performed\tfalse");
    println!("media_file_length\t{}", summary.file_length);
    println!(
        "media_major_brand\t{}",
        summary
            .major_brand
            .map_or_else(|| "unknown".to_owned(), |value| value.to_string())
    );
    println!("media_fragmented\t{}", summary.fragmented);
    println!("media_boxes_visited\t{}", summary.boxes_visited);
    println!("media_track_count\t{}", summary.tracks.len());
    for (index, track) in summary.tracks.iter().enumerate() {
        println!("media_track\t{index}\ttrack_id\t{}", track.track_id);
        println!(
            "media_track\t{index}\thandler_type\t{}",
            track.handler_type
        );
        println!(
            "media_track\t{index}\tcodec\t{}",
            track
                .codec
                .map_or_else(|| "unknown".to_owned(), |value| value.to_string())
        );
        println!("media_track\t{index}\ttimescale\t{}", track.timescale);
        println!("media_track\t{index}\tduration\t{}", track.duration);
        println!(
            "media_track\t{index}\tsample_count\t{}",
            optional_u64_text(track.sample_count)
        );
    }
}

fn recorded_media_ingest_json(
    receipt: &RecordedMediaIngestReceipt,
    verified: &VerifiedRecordedMedia,
) -> Result<String, String> {
    let mut output = format!(
        "{{\"schema\":\"{}\",\"publication_complete\":true,\"closure_verified\":true,\"root\":",
        json_escape(receipt.schema)
    );
    push_publication_json(&mut output, &receipt.root)?;
    output.push_str(",\"source\":");
    push_publication_json(&mut output, &receipt.source)?;
    output.push_str(",\"inspection\":");
    push_publication_json(&mut output, &receipt.inspection.artifact)?;
    output.push_str(",\"media\":");
    push_media_summary_json(&mut output, &verified.inspection.summary)?;
    output.push('}');
    Ok(output)
}

fn verified_recorded_media_json(verified: &VerifiedRecordedMedia) -> Result<String, String> {
    let mut output = format!(
        "{{\"schema\":\"{}\",\"closure_verified\":true,\"root_manifest_digest\":\"{}\",\"root_object_digest\":\"{}\",\"source\":",
        json_escape(VERIFIED_RECORDED_MEDIA_SCHEMA),
        verified.root_manifest_digest,
        verified.root_object_digest
    );
    push_decoded_publication_json(
        &mut output,
        &verified.root.source_manifest_digest.to_string(),
        &verified.root.source_object_digest.to_string(),
        verified.root.source_object_length,
        verified.root.source_chunk_size,
        verified.root.source_chunk_count,
    )?;
    output.push_str(",\"inspection\":");
    push_decoded_publication_json(
        &mut output,
        &verified.root.inspection_manifest_digest.to_string(),
        &verified.root.inspection_object_digest.to_string(),
        verified.root.inspection_object_length,
        verified.root.inspection_chunk_size,
        verified.root.inspection_chunk_count,
    )?;
    output.push_str(",\"media\":");
    push_media_summary_json(&mut output, &verified.inspection.summary)?;
    output.push('}');
    Ok(output)
}

fn push_publication_json(
    output: &mut String,
    receipt: &fdgr_object_store::ImportReceipt,
) -> Result<(), String> {
    push_decoded_publication_json(
        output,
        &receipt.manifest_digest.to_string(),
        &receipt.object_digest.to_string(),
        receipt.object_length,
        receipt.chunk_size,
        receipt.chunk_count,
    )
}

fn push_decoded_publication_json(
    output: &mut String,
    manifest_digest: &str,
    object_digest: &str,
    object_length: u64,
    chunk_size: u32,
    chunk_count: u64,
) -> Result<(), String> {
    write!(
        output,
        "{{\"manifest_digest\":\"{}\",\"object_digest\":\"{}\",\"object_length\":{},\"chunk_size\":{},\"chunk_count\":{}}}",
        json_escape(manifest_digest),
        json_escape(object_digest),
        object_length,
        chunk_size,
        chunk_count
    )
    .map_err(|error| error.to_string())
}

fn push_media_summary_json(
    output: &mut String,
    summary: &IsoBmffSummary,
) -> Result<(), String> {
    write!(
        output,
        "{{\"scope\":\"container_metadata_and_classic_sample_tables\",\"decode_performed\":false,\"file_length\":{},\"major_brand\":{},\"minor_version\":{},\"compatible_brands\":[",
        summary.file_length,
        optional_fourcc_json(summary.major_brand),
        optional_u32_json(summary.minor_version)
    )
    .map_err(|error| error.to_string())?;
    for (index, brand) in summary.compatible_brands.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "\"{}\"", json_escape(&brand.to_string()))
            .map_err(|error| error.to_string())?;
    }
    write!(
        output,
        "],\"movie_timescale\":{},\"movie_duration\":{},\"fragmented\":{},\"boxes_visited\":{},\"track_count\":{},\"tracks\":[",
        summary.movie_timescale,
        summary.movie_duration,
        summary.fragmented,
        summary.boxes_visited,
        summary.tracks.len()
    )
    .map_err(|error| error.to_string())?;
    for (index, track) in summary.tracks.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(
            output,
            "{{\"track_id\":{},\"handler_type\":\"{}\",\"codec\":{},\"timescale\":{},\"duration\":{},\"width_pixels\":{},\"height_pixels\":{},\"sample_count\":{},\"total_sample_bytes\":{},\"chunk_count\":{},\"sync_sample_count\":{}}}",
            track.track_id,
            json_escape(&track.handler_type.to_string()),
            optional_fourcc_json(track.codec),
            track.timescale,
            track.duration,
            track.width_pixels(),
            track.height_pixels(),
            optional_u64_json(track.sample_count),
            optional_u64_json(track.total_sample_bytes),
            optional_u64_json(track.chunk_count),
            optional_u64_json(track.sync_sample_count)
        )
        .map_err(|error| error.to_string())?;
    }
    output.push_str("]}");
    Ok(())
}

fn optional_fourcc_json(value: Option<FourCc>) -> String {
    value.map_or_else(
        || "null".to_owned(),
        |code| format!("\"{}\"", json_escape(&code.to_string())),
    )
}

fn optional_u32_json(value: Option<u32>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn optional_u64_text(value: Option<u64>) -> String {
    value.map_or_else(|| "unknown".to_owned(), |number| number.to_string())
}

#[cfg(test)]
mod tests {
    use super::push_media_summary_json;
    use fdgr_media::{FourCc, IsoBmffSummary};

    #[test]
    fn media_summary_json_is_deterministic_and_explicit_about_decode_scope() {
        let summary = IsoBmffSummary {
            file_length: 10,
            major_brand: Some(FourCc::new(*b"isom")),
            minor_version: Some(1),
            compatible_brands: vec![FourCc::new(*b"isom")],
            movie_timescale: 1_000,
            movie_duration: 5_000,
            fragmented: false,
            boxes_visited: 3,
            tracks: Vec::new(),
        };
        let mut first = String::new();
        let mut second = String::new();
        assert!(push_media_summary_json(&mut first, &summary).is_ok());
        assert!(push_media_summary_json(&mut second, &summary).is_ok());
        assert_eq!(first, second);
        assert!(first.contains("\"decode_performed\":false"));
        assert!(first.contains("\"track_count\":0"));
    }
}
