#![forbid(unsafe_code)]
#![allow(clippy::indexing_slicing, clippy::too_many_lines)]
//! Exact-byte CLI adapter for deterministic keyframe selection.

use crate::args::OutputFormat;
use fdgr_codec::hash_bytes;
use fdgr_keyframe::{
    KeyframeBasis, KeyframeCandidate, KeyframePolicy, MAX_KEYFRAME_CANDIDATES,
    MAX_TOTAL_COVERAGE_REFERENCES, PPM_SCALE, select_keyframes,
};
use fdgr_types::EvidenceDigest;
use std::fs::{self, File};
use std::io::Read;
use std::path::{Path, PathBuf};

const CANDIDATE_HEADER: &str =
    "sample_index\tframe_digest\tpresentation_tick\tsharpness_ppm\ttexture_ppm\tdark_clipped_ppm\tbright_clipped_ppm\tdynamic_content_ppm\toverlap_ppm\tview_sector\tbaseline_bin\tcoverage_cells";
const MAX_CANDIDATE_TABLE_BYTES: u64 = 64 * 1024 * 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
struct KeyframeCliOptions {
    candidate_path: PathBuf,
    candidate_basis_digest: EvidenceDigest,
    timeline_digest: EvidenceDigest,
    decoded_frame_generation_digest: EvidenceDigest,
    calibration_digest: EvidenceDigest,
    policy_digest: EvidenceDigest,
    selection_generation: u64,
    policy: KeyframePolicy,
    format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct CandidateTable {
    digest: EvidenceDigest,
    candidates: Vec<KeyframeCandidate>,
}

pub(crate) fn is_command(arguments: &[String]) -> bool {
    arguments.first().is_some_and(|value| value == "keyframe-select")
}

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let options = parse(arguments)?;
    let table = read_candidate_table(&options.candidate_path, &options.candidate_basis_digest)?;
    let selection = select_keyframes(
        KeyframeBasis {
            timeline_digest: options.timeline_digest,
            decoded_frame_generation_digest: options.decoded_frame_generation_digest,
            calibration_digest: options.calibration_digest,
            candidate_basis_digest: table.digest,
            policy_digest: options.policy_digest,
            selection_generation: options.selection_generation,
        },
        options.policy,
        table.candidates,
    )
    .map_err(|error| format!("keyframe selection rejected: {error}"))?;
    match options.format {
        OutputFormat::Json => {
            println!(
                "{}",
                selection
                    .to_json()
                    .map_err(|error| format!("keyframe JSON failed: {error}"))?
            );
        }
        OutputFormat::Text => {
            println!("schema\tfdgr.keyframe_selection/1");
            println!(
                "selection_digest\t{}",
                selection
                    .digest()
                    .map_err(|error| format!("keyframe identity failed: {error}"))?
            );
            println!("candidate_count\t{}", selection.candidates.len());
            println!("selected_count\t{}", selection.selected.len());
            println!("rejected_count\t{}", selection.rejected.len());
            println!("covered_cell_count\t{}", selection.covered_cells.len());
            for selected in &selection.selected {
                println!(
                    "selected\t{}\t{}\t{}\t{}\t{}",
                    selected.rank,
                    selected.sample_index,
                    selected.frame_digest,
                    selected.marginal_coverage_cells,
                    selected.total_score
                );
            }
            for rejected in &selection.rejected {
                println!(
                    "rejected\t{}\t{}\t{}\t{}",
                    rejected.sample_index,
                    rejected.frame_digest,
                    rejected.reason.as_str(),
                    rejected.marginal_coverage_cells
                );
            }
        }
    }
    Ok(())
}

pub(crate) fn print_help_line() {
    println!(
        "  fdgr keyframe-select <candidates.tsv> --candidate-basis-digest <digest> --timeline-digest <digest> --decoded-frame-generation-digest <digest> --calibration-digest <digest> --policy-digest <digest> --generation <n> [quality/selection policy] [--format text|json]"
    );
}

include!("keyframe_args.inc");
include!("keyframe_tests.inc");
