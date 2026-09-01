#![forbid(unsafe_code)]
//! Deterministic JSON rendering for validated decode-worker receipts.

use crate::{
    DecodeReceiptError, DecodeTermination, MediaDecodePlan, MediaDecodeReceipt,
    MEDIA_DECODE_RECEIPT_SCHEMA,
};
use fdgr_types::EvidenceDigest;

/// Renders one receipt only after full validation against its exact decode plan.
///
/// # Errors
///
/// Returns the same semantic validation, canonical encoding, or identity-domain error as
/// [`MediaDecodeReceipt::digest`].
pub fn render_media_decode_receipt_json(
    receipt: &MediaDecodeReceipt,
    plan: &MediaDecodePlan,
) -> Result<String, DecodeReceiptError> {
    let receipt_digest = receipt.digest(plan)?;
    let input = receipt.input();
    let semantic_completion = receipt.semantic_completion(plan);
    let mut output = format!(
        "{{\"schema\":\"{MEDIA_DECODE_RECEIPT_SCHEMA}\",\"receipt_digest\":\"{receipt_digest}\",\"plan_digest\":\"{}\",\"worker_executable_digest\":\"{}\",\"worker_version_digest\":\"{}\",\"profile_digest\":\"{}\",\"termination\":\"{}\",\"exit_code\":{},\"indeterminate\":{},\"semantic_completion\":{},\"framehash_object_digest\":{},\"output_root_manifest_digest\":{},\"output_root_object_digest\":{},\"wall_time_ms\":{},\"peak_memory_bytes\":{},\"stderr_digest\":\"{}\",\"framehash\":",
        input.plan_digest,
        input.worker_executable_digest,
        input.worker_version_digest,
        input.profile_digest,
        input.termination,
        optional_exit_code_json(input.termination),
        input.termination.is_indeterminate(),
        semantic_completion,
        optional_digest_json(input.framehash_object_digest.as_ref()),
        optional_digest_json(input.output_root_manifest_digest.as_ref()),
        optional_digest_json(input.output_root_object_digest.as_ref()),
        input.wall_time_ms,
        input.peak_memory_bytes,
        input.stderr_digest,
    );
    match &input.framehash {
        Some(report) => {
            output.push_str(&format!(
                "{{\"version\":{},\"hash_name\":\"{}\",\"record_count\":{},\"total_frame_bytes\":{},\"records\":[",
                report.version,
                report.hash_name,
                report.records.len(),
                report.total_frame_bytes,
            ));
            for (index, record) in report.records.iter().enumerate() {
                if index > 0 {
                    output.push(',');
                }
                output.push_str(&format!(
                    "{{\"record_index\":{},\"stream_index\":{},\"dts\":{},\"pts\":{},\"duration\":{},\"byte_length\":{},\"digest\":\"{}\"}}",
                    record.record_index,
                    record.stream_index,
                    record.dts,
                    record.pts,
                    record.duration,
                    record.byte_length,
                    record.digest,
                ));
            }
            output.push_str("]}");
        }
        None => output.push_str("null"),
    }
    output.push('}');
    Ok(output)
}

fn optional_exit_code_json(termination: DecodeTermination) -> String {
    match termination {
        DecodeTermination::Failed { exit_code } => exit_code.to_string(),
        _ => "null".to_owned(),
    }
}

fn optional_digest_json(value: Option<&EvidenceDigest>) -> String {
    value.map_or_else(|| "null".to_owned(), |digest| format!("\"{digest}\""))
}
