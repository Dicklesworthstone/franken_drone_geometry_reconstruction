#![forbid(unsafe_code)]
//! Stable rendering for bounded exact classic-sample windows.

use crate::args::OutputFormat;
use fdgr_media::{IsoBmffSummary, SAMPLE_WINDOW_SCHEMA, TrackSampleWindow};
use std::fmt::Write as _;

pub(crate) fn print_sample_window(
    summary: &IsoBmffSummary,
    window: &TrackSampleWindow,
    format: OutputFormat,
) -> Result<(), String> {
    match format {
        OutputFormat::Text => {
            println!("schema\t{SAMPLE_WINDOW_SCHEMA}");
            println!("scope\tclassic_sample_tables");
            println!("decode_performed\tfalse");
            println!("source_file_length\t{}", summary.file_length);
            println!("source_fragmented\t{}", summary.fragmented);
            println!("track_id\t{}", window.track_id);
            println!("timescale\t{}", window.timescale);
            println!("total_samples\t{}", window.total_samples);
            println!("start_sample\t{}", window.start_sample);
            println!(
                "requested_max_samples\t{}",
                window.requested_max_samples
            );
            println!("returned_samples\t{}", window.samples.len());
            println!("complete\t{}", window.complete);
            println!("index_entries_scanned\t{}", window.index_entries_scanned);
            for sample in &window.samples {
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
        }
        OutputFormat::Json => {
            let mut output = format!(
                "{{\"schema\":\"{SAMPLE_WINDOW_SCHEMA}\",\"scope\":\"classic_sample_tables\",\"decode_performed\":false,\"source_file_length\":{},\"source_fragmented\":{},\"track_id\":{},\"timescale\":{},\"total_samples\":{},\"start_sample\":{},\"requested_max_samples\":{},\"returned_samples\":{},\"complete\":{},\"index_entries_scanned\":{},\"samples\":[",
                summary.file_length,
                summary.fragmented,
                window.track_id,
                window.timescale,
                window.total_samples,
                window.start_sample,
                window.requested_max_samples,
                window.samples.len(),
                window.complete,
                window.index_entries_scanned
            );
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
            println!("{output}");
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::print_sample_window;
    use crate::args::OutputFormat;
    use fdgr_media::{FourCc, IsoBmffSummary, SampleRecord, TrackSampleWindow};

    #[test]
    fn text_renderer_accepts_negative_composition_time() {
        let summary = IsoBmffSummary {
            file_length: 100,
            major_brand: Some(FourCc::new(*b"isom")),
            minor_version: Some(0),
            compatible_brands: vec![FourCc::new(*b"isom")],
            movie_timescale: 1_000,
            movie_duration: 1_000,
            fragmented: false,
            boxes_visited: 10,
            tracks: Vec::new(),
        };
        let window = TrackSampleWindow {
            track_id: 1,
            timescale: 1_000,
            total_samples: 1,
            start_sample: 0,
            requested_max_samples: 1,
            complete: true,
            index_entries_scanned: 4,
            samples: vec![SampleRecord {
                sample_index: 0,
                decode_time: 0,
                composition_time: -10,
                duration: 1_000,
                byte_offset: 8,
                byte_length: 4,
                is_sync: true,
                sample_description_index: 1,
            }],
        };
        assert!(print_sample_window(&summary, &window, OutputFormat::Text).is_ok());
    }
}
