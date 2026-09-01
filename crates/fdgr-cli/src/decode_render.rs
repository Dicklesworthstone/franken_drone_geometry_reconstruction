#![forbid(unsafe_code)]
//! Stable rendering for authority-free media decode plans.

use crate::args::OutputFormat;
use fdgr_media_worker::{MEDIA_DECODE_PLAN_SCHEMA, MediaDecodePlan};

pub(crate) fn print_media_decode_plan(
    plan: &MediaDecodePlan,
    format: OutputFormat,
) -> Result<(), String> {
    match format {
        OutputFormat::Json => {
            println!("{}", plan.to_json().map_err(|error| error.to_string())?);
        }
        OutputFormat::Text => {
            let digest = plan.digest().map_err(|error| error.to_string())?;
            let input = plan.input();
            println!("schema\t{MEDIA_DECODE_PLAN_SCHEMA}");
            println!("plan_digest\t{digest}");
            println!(
                "source_root_manifest_digest\t{}",
                input.source_root_manifest_digest
            );
            println!("source_manifest_digest\t{}", input.source_manifest_digest);
            println!("source_object_digest\t{}", input.source_object_digest);
            println!("source_object_length\t{}", input.source_object_length);
            println!("track_id\t{}", input.track_id);
            println!("start_sample\t{}", input.start_sample);
            println!("max_samples\t{}", input.max_samples);
            println!("pixel_format\t{}", input.pixel_format);
            println!("output_width\t{}", input.output_width);
            println!("output_height\t{}", input.output_height);
            println!("max_frames\t{}", input.max_frames);
            println!("max_output_bytes\t{}", input.max_output_bytes);
            println!("max_wall_time_ms\t{}", input.max_wall_time_ms);
            println!("max_memory_bytes\t{}", input.max_memory_bytes);
            println!(
                "worker_executable_digest\t{}",
                input.worker_executable_digest
            );
            println!("worker_version_digest\t{}", input.worker_version_digest);
            println!("profile_digest\t{}", input.profile_digest);
            println!("worker_threads\t{}", input.worker_threads);
            println!("network_allowed\t{}", input.network_allowed);
            println!("deterministic\t{}", input.deterministic);
            println!("dispatch_authority\tfalse");
        }
    }
    Ok(())
}
