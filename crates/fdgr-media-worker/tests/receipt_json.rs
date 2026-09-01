#![forbid(unsafe_code)]

use fdgr_media_worker::{
    DecodePixelFormat, DecodeTermination, FrameHashLimits, MediaDecodePlan, MediaDecodePlanInput,
    MediaDecodeReceipt, MediaDecodeReceiptInput, parse_framehash_v2,
    render_media_decode_receipt_json,
};
use fdgr_types::EvidenceDigest;

fn digest(byte: u8) -> EvidenceDigest {
    EvidenceDigest::from_bytes([byte; 32])
}

fn plan() -> Option<MediaDecodePlan> {
    MediaDecodePlan::new(MediaDecodePlanInput {
        source_root_manifest_digest: digest(1),
        source_manifest_digest: digest(2),
        source_object_digest: digest(3),
        source_object_length: 1_000,
        track_id: 1,
        start_sample: 0,
        max_samples: 2,
        pixel_format: DecodePixelFormat::Gray8,
        output_width: 2,
        output_height: 2,
        max_frames: 2,
        max_output_bytes: 8,
        max_wall_time_ms: 1_000,
        max_memory_bytes: 1_000_000,
        worker_executable_digest: digest(4),
        worker_version_digest: digest(5),
        profile_digest: digest(6),
        worker_threads: 1,
        network_allowed: false,
        deterministic: true,
    })
    .ok()
}

fn framehash() -> Option<fdgr_media_worker::FrameHashReport> {
    let first = "ab".repeat(32);
    let second = "cd".repeat(32);
    parse_framehash_v2(
        &format!(
            "#version: 2\n#hash: SHA256\n0, 0, 0, 1, 4, {first}\n0, 1, 1, 1, 4, {second}\n"
        ),
        FrameHashLimits::default(),
    )
    .ok()
}

#[test]
fn successful_receipt_json_is_complete_and_deterministic() {
    let plan = plan();
    assert!(plan.is_some());
    if let Some(plan) = plan {
        let plan_digest = plan.digest();
        assert!(plan_digest.is_ok());
        if let Ok(plan_digest) = plan_digest {
            let receipt = MediaDecodeReceipt::new(MediaDecodeReceiptInput {
                plan_digest,
                worker_executable_digest: plan.input().worker_executable_digest.clone(),
                worker_version_digest: plan.input().worker_version_digest.clone(),
                profile_digest: plan.input().profile_digest.clone(),
                termination: DecodeTermination::Succeeded,
                framehash_object_digest: Some(digest(7)),
                framehash: framehash(),
                output_root_manifest_digest: Some(digest(8)),
                output_root_object_digest: Some(digest(9)),
                wall_time_ms: 500,
                peak_memory_bytes: 500_000,
                stderr_digest: digest(10),
            });
            let first = render_media_decode_receipt_json(&receipt, &plan);
            let second = render_media_decode_receipt_json(&receipt, &plan);
            assert!(matches!(
                (&first, &second),
                (Ok(left), Ok(right))
                    if left == right
                        && left.contains("\"schema\":\"fdgr.media_decode_receipt/1\"")
                        && left.contains("\"termination\":\"succeeded\"")
                        && left.contains("\"indeterminate\":false")
                        && left.contains("\"semantic_completion\":true")
                        && left.contains("\"record_count\":2")
                        && left.contains("\"total_frame_bytes\":8")
            ));
        }
    }
}

#[test]
fn forced_termination_json_stays_indeterminate_and_unpublished() {
    let plan = plan();
    assert!(plan.is_some());
    if let Some(plan) = plan {
        let plan_digest = plan.digest();
        assert!(plan_digest.is_ok());
        if let Ok(plan_digest) = plan_digest {
            let receipt = MediaDecodeReceipt::new(MediaDecodeReceiptInput {
                plan_digest,
                worker_executable_digest: plan.input().worker_executable_digest.clone(),
                worker_version_digest: plan.input().worker_version_digest.clone(),
                profile_digest: plan.input().profile_digest.clone(),
                termination: DecodeTermination::KilledAfterGrace,
                framehash_object_digest: None,
                framehash: None,
                output_root_manifest_digest: None,
                output_root_object_digest: None,
                wall_time_ms: 1_001,
                peak_memory_bytes: 750_000,
                stderr_digest: digest(10),
            });
            let rendered = render_media_decode_receipt_json(&receipt, &plan);
            assert!(matches!(
                rendered,
                Ok(ref json)
                    if json.contains("\"termination\":\"killed_after_grace\"")
                        && json.contains("\"indeterminate\":true")
                        && json.contains("\"semantic_completion\":false")
                        && json.contains("\"output_root_manifest_digest\":null")
                        && json.contains("\"output_root_object_digest\":null")
                        && json.contains("\"framehash\":null")
            ));
        }
    }
}
