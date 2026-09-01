#![forbid(unsafe_code)]
#![allow(clippy::too_many_lines)]
//! Stable rendering for custody-bound media results.

use crate::args::OutputFormat;
use fdgr_media::{FourCc, MEDIA_INSPECTION_SCHEMA, SAMPLE_WINDOW_SCHEMA};
use fdgr_media_custody::{
    STORED_MEDIA_INSPECTION_SCHEMA, STORED_SAMPLE_WINDOW_SCHEMA, StoredMediaInspection,
    StoredSampleWindow,
};
use std::fmt::Write as _;

pub(crate) fn print_stored_media_inspection(
    result: &StoredMediaInspection,
    format: OutputFormat,
) -> Result<(), String> {
    match format {
        OutputFormat::Text => {
            print_custody_text(
                STORED_MEDIA_INSPECTION_SCHEMA,
                &result.manifest.manifest_digest.to_string(),
                &result.manifest.object_digest.to_string(),
                result.manifest.object_length,
            );
            print_inspection_text(&result.summary);
            Ok(())
        }
        OutputFormat::Json => {
            println!("{}", stored_media_inspection_json(result)?);
            Ok(())
        }
    }
}

pub(crate) fn print_stored_sample_window(
    result: &StoredSampleWindow,
    format: OutputFormat,
) -> Result<(), String> {
    match format {
        OutputFormat::Text => {
            print_custody_text(
                STORED_SAMPLE_WINDOW_SCHEMA,
                &result.manifest.manifest_digest.to_string(),
                &result.manifest.object_digest.to_string(),
                result.manifest.object_length,
            );
            println!("window_schema\t{SAMPLE_WINDOW_SCHEMA}");
            println!("scope\tclassic_sample_tables");
            println!("decode_performed\tfalse");
            println!("source_fragmented\t{}", result.summary.fragmented);
            println!("track_id\t{}", result.window.track_id);
            println!("timescale\t{}", result.window.timescale);
            println!("total_samples\t{}", result.window.total_samples);
            println!("start_sample\t{}", result.window.start_sample);
            println!(
                "requested_max_samples\t{}",
                result.window.requested_max_samples
            );
            println!("returned_samples\t{}", result.window.samples.len());
            println!("complete\t{}", result.window.complete);
            println!(
                "index_entries_scanned\t{}",
                result.window.index_entries_scanned
            );
            for sample in &result.window.samples {
                println!(
                    "sample\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
                    sample.sample_index,
                    sample.decode_time,
                    sample.composition_time,
                    sample.duration,
                    sample.byte_offset,
                    sample.byte_length,
                    sample.is_sync,
                    sample.sample_description_index
                );
            }
            Ok(())
        }
        OutputFormat::Json => {
            println!("{}", stored_sample_window_json(result)?);
            Ok(())
        }
    }
}

fn print_custody_text(schema: &str, manifest_digest: &str, object_digest: &str, length: u64) {
    println!("schema\t{schema}");
    println!("custody_verified\ttrue");
    println!("manifest_digest\t{manifest_digest}");
    println!("object_digest\t{object_digest}");
    println!("object_length\t{length}");
}

fn print_inspection_text(summary: &fdgr_media::IsoBmffSummary) {
    println!("inspection_schema\t{MEDIA_INSPECTION_SCHEMA}");
    println!("scope\tcontainer_metadata_and_classic_sample_tables");
    println!("decode_performed\tfalse");
    println!("file_length\t{}", summary.file_length);
    println!(
        "major_brand\t{}",
        summary
            .major_brand
            .map_or_else(|| "unknown".to_owned(), |value| value.to_string())
    );
    println!(
        "minor_version\t{}",
        summary
            .minor_version
            .map_or_else(|| "unknown".to_owned(), |value| value.to_string())
    );
    for brand in &summary.compatible_brands {
        println!("compatible_brand\t{brand}");
    }
    println!("movie_timescale\t{}", summary.movie_timescale);
    println!("movie_duration\t{}", summary.movie_duration);
    println!("fragmented\t{}", summary.fragmented);
    println!("boxes_visited\t{}", summary.boxes_visited);
    println!("track_count\t{}", summary.tracks.len());
    for (index, track) in summary.tracks.iter().enumerate() {
        println!("track\t{index}\ttrack_id\t{}", track.track_id);
        println!("track\t{index}\thandler_type\t{}", track.handler_type);
        println!(
            "track\t{index}\tcodec\t{}",
            track
                .codec
                .map_or_else(|| "unknown".to_owned(), |value| value.to_string())
        );
        println!("track\t{index}\ttimescale\t{}", track.timescale);
        println!("track\t{index}\tduration\t{}", track.duration);
        println!("track\t{index}\twidth_pixels\t{}", track.width_pixels());
        println!("track\t{index}\theight_pixels\t{}", track.height_pixels());
        println!(
            "track\t{index}\tsample_count\t{}",
            optional_u64_text(track.sample_count)
        );
        println!(
            "track\t{index}\tdecode_duration\t{}",
            optional_u64_text(track.decode_duration)
        );
        println!(
            "track\t{index}\tcomposition_sample_count\t{}",
            optional_u64_text(track.composition_sample_count)
        );
        println!(
            "track\t{index}\ttotal_sample_bytes\t{}",
            optional_u64_text(track.total_sample_bytes)
        );
        println!(
            "track\t{index}\tconstant_sample_size\t{}",
            optional_u32_text(track.constant_sample_size)
        );
        println!(
            "track\t{index}\tchunk_count\t{}",
            optional_u64_text(track.chunk_count)
        );
        println!(
            "track\t{index}\tsync_sample_count\t{}",
            optional_u64_text(track.sync_sample_count)
        );
        println!(
            "track\t{index}\tsample_description_count\t{}",
            optional_u32_text(track.sample_description_count)
        );
        println!(
            "track\t{index}\tsample_to_chunk_entry_count\t{}",
            optional_u64_text(track.sample_to_chunk_entry_count)
        );
    }
}

fn stored_media_inspection_json(result: &StoredMediaInspection) -> Result<String, String> {
    let mut output = format!(
        "{{\"schema\":\"{STORED_MEDIA_INSPECTION_SCHEMA}\",\"custody_verified\":true,\"manifest_digest\":\"{}\",\"object_digest\":\"{}\",\"object_length\":{},\"inspection\":",
        result.manifest.manifest_digest,
        result.manifest.object_digest,
        result.manifest.object_length
    );
    push_inspection_json(&mut output, &result.summary)?;
    output.push('}');
    Ok(output)
}

fn stored_sample_window_json(result: &StoredSampleWindow) -> Result<String, String> {
    let mut output = format!(
        "{{\"schema\":\"{STORED_SAMPLE_WINDOW_SCHEMA}\",\"custody_verified\":true,\"manifest_digest\":\"{}\",\"object_digest\":\"{}\",\"object_length\":{},\"window\":",
        result.manifest.manifest_digest,
        result.manifest.object_digest,
        result.manifest.object_length
    );
    push_sample_window_json(&mut output, result)?;
    output.push('}');
    Ok(output)
}

fn push_inspection_json(
    output: &mut String,
    summary: &fdgr_media::IsoBmffSummary,
) -> Result<(), String> {
    write!(
        output,
        "{{\"schema\":\"{MEDIA_INSPECTION_SCHEMA}\",\"scope\":\"container_metadata_and_classic_sample_tables\",\"decode_performed\":false,\"file_length\":{},\"major_brand\":{},\"minor_version\":{},\"compatible_brands\":[",
        summary.file_length,
        optional_fourcc_json(summary.major_brand),
        optional_u32_json(summary.minor_version)
    )
    .map_err(|error| error.to_string())?;
    for (index, brand) in summary.compatible_brands.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(output, "\"{}\"", escape_json(&brand.to_string()))
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
            "{{\"track_id\":{},\"handler_type\":\"{}\",\"codec\":{},\"timescale\":{},\"duration\":{},\"width_fixed_16_16\":{},\"height_fixed_16_16\":{},\"width_pixels\":{},\"height_pixels\":{},\"sample_count\":{},\"decode_duration\":{},\"composition_sample_count\":{},\"total_sample_bytes\":{},\"constant_sample_size\":{},\"chunk_count\":{},\"sync_sample_count\":{},\"sample_description_count\":{},\"sample_to_chunk_entry_count\":{}}}",
            track.track_id,
            escape_json(&track.handler_type.to_string()),
            optional_fourcc_json(track.codec),
            track.timescale,
            track.duration,
            track.width_fixed_16_16,
            track.height_fixed_16_16,
            track.width_pixels(),
            track.height_pixels(),
            optional_u64_json(track.sample_count),
            optional_u64_json(track.decode_duration),
            optional_u64_json(track.composition_sample_count),
            optional_u64_json(track.total_sample_bytes),
            optional_u32_json(track.constant_sample_size),
            optional_u64_json(track.chunk_count),
            optional_u64_json(track.sync_sample_count),
            optional_u32_json(track.sample_description_count),
            optional_u64_json(track.sample_to_chunk_entry_count)
        )
        .map_err(|error| error.to_string())?;
    }
    output.push_str("]}");
    Ok(())
}

fn push_sample_window_json(
    output: &mut String,
    result: &StoredSampleWindow,
) -> Result<(), String> {
    let window = &result.window;
    write!(
        output,
        "{{\"schema\":\"{SAMPLE_WINDOW_SCHEMA}\",\"scope\":\"classic_sample_tables\",\"decode_performed\":false,\"source_file_length\":{},\"source_fragmented\":{},\"track_id\":{},\"timescale\":{},\"total_samples\":{},\"start_sample\":{},\"requested_max_samples\":{},\"returned_samples\":{},\"complete\":{},\"index_entries_scanned\":{},\"samples\":[",
        result.summary.file_length,
        result.summary.fragmented,
        window.track_id,
        window.timescale,
        window.total_samples,
        window.start_sample,
        window.requested_max_samples,
        window.samples.len(),
        window.complete,
        window.index_entries_scanned
    )
    .map_err(|error| error.to_string())?;
    for (index, sample) in window.samples.iter().enumerate() {
        if index > 0 {
            output.push(',');
        }
        write!(
            output,
            "{{\"sample_index\":{},\"decode_time\":{},\"composition_time\":{},\"duration\":{},\"byte_offset\":{},\"byte_length\":{},\"is_sync\":{},\"sample_description_index\":{}}}",
            sample.sample_index,
            sample.decode_time,
            sample.composition_time,
            sample.duration,
            sample.byte_offset,
            sample.byte_length,
            sample.is_sync,
            sample.sample_description_index
        )
        .map_err(|error| error.to_string())?;
    }
    output.push_str("]}");
    Ok(())
}

fn optional_fourcc_json(value: Option<FourCc>) -> String {
    value.map_or_else(
        || "null".to_owned(),
        |code| format!("\"{}\"", escape_json(&code.to_string())),
    )
}

fn optional_u32_json(value: Option<u32>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn optional_u64_json(value: Option<u64>) -> String {
    value.map_or_else(|| "null".to_owned(), |number| number.to_string())
}

fn optional_u32_text(value: Option<u32>) -> String {
    value.map_or_else(|| "unknown".to_owned(), |number| number.to_string())
}

fn optional_u64_text(value: Option<u64>) -> String {
    value.map_or_else(|| "unknown".to_owned(), |number| number.to_string())
}

fn escape_json(value: &str) -> String {
    let mut escaped = String::with_capacity(value.len());
    for character in value.chars() {
        match character {
            '"' => escaped.push_str("\\\""),
            '\\' => escaped.push_str("\\\\"),
            '\n' => escaped.push_str("\\n"),
            '\r' => escaped.push_str("\\r"),
            '\t' => escaped.push_str("\\t"),
            character if character.is_control() => {
                let value = u32::from(character);
                let _ = write!(escaped, "\\u{value:04x}");
            }
            character => escaped.push(character),
        }
    }
    escaped
}

#[cfg(test)]
mod tests {
    use super::stored_media_inspection_json;
    use fdgr_evidence::ObjectManifest;
    use fdgr_media::{FourCc, IsoBmffSummary};
    use fdgr_media_custody::StoredMediaInspection;

    #[test]
    fn custody_json_keeps_both_identities() {
        let manifest = ObjectManifest::build(b"abc", 3);
        assert!(manifest.is_ok());
        if let Ok(manifest) = manifest {
            let result = StoredMediaInspection {
                manifest,
                summary: IsoBmffSummary {
                    file_length: 3,
                    major_brand: Some(FourCc::new(*b"isom")),
                    minor_version: Some(0),
                    compatible_brands: vec![FourCc::new(*b"isom")],
                    movie_timescale: 1,
                    movie_duration: 0,
                    fragmented: false,
                    boxes_visited: 1,
                    tracks: Vec::new(),
                },
            };
            let json = stored_media_inspection_json(&result);
            assert!(matches!(
                json,
                Ok(ref value)
                    if value.contains(result.manifest.manifest_digest.as_str())
                        && value.contains(result.manifest.object_digest.as_str())
                        && value.contains("\"custody_verified\":true")
            ));
        }
    }
}
