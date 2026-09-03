#![forbid(unsafe_code)]
//! Deterministic constitutional surfaces for the FDGR scaffold.

use std::process::Command;

/// Stable schema identifier for the current capability document.
pub const CAPABILITIES_SCHEMA: &str = "fdgr.capabilities/1";
/// Stable schema identifier for doctor output.
pub const DOCTOR_SCHEMA: &str = "fdgr.doctor/1";
/// Stable schema identifier for the implementation-plan summary.
pub const PLAN_SUMMARY_SCHEMA: &str = "fdgr.plan_summary/1";
/// Stable schema identifier for digest-validation output.
pub const VALIDATE_ID_SCHEMA: &str = "fdgr.validate_id/1";
/// Stable schema identifier for bounded object-manifest views.
pub const OBJECT_MANIFEST_VIEW_SCHEMA: &str = "fdgr.object_manifest_view/1";
/// Stable schema identifier for exact file-verification receipts.
pub const FILE_VERIFICATION_SCHEMA: &str = "fdgr.file_verification/1";
/// Stable schema identifier for published-store verification receipts.
pub const STORE_VERIFICATION_SCHEMA: &str = "fdgr.store_verification/1";
/// Current package version.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");

/// A named capability whose maturity is explicit.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Capability {
    /// Stable capability identifier.
    pub id: &'static str,
    /// Human-readable purpose.
    pub description: &'static str,
    /// Current maturity state.
    pub status: CapabilityStatus,
}

/// Capability maturity. Source presence never implies qualification.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CapabilityStatus {
    /// Implemented in the minimal executable scaffold.
    Scaffolded,
    /// Deterministic reference source exists but production qualification is separate evidence.
    ReferenceSource,
    /// Normatively designed but not implemented.
    Planned,
    /// Requires external evidence before admission.
    Research,
}

impl CapabilityStatus {
    /// Stable machine-readable text.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Scaffolded => "scaffolded",
            Self::ReferenceSource => "reference_source",
            Self::Planned => "planned",
            Self::Research => "research",
        }
    }
}

/// Returns the deterministic capability registry exposed by the CLI.
#[must_use]
pub fn capabilities() -> &'static [Capability] {
    &[
        Capability {
            id: "archive.replicate.s3",
            description: "replicate immutable content-addressed evidence to compatible object storage",
            status: CapabilityStatus::Planned,
        },
        Capability {
            id: "canonical.digest.validate",
            description: "validate canonical lowercase SHA-256 text identities",
            status: CapabilityStatus::Scaffolded,
        },
        Capability {
            id: "canonical.sha256.compute",
            description: "compute dependency-free streaming SHA-256 and domain-separated identities",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "capture.dji.flip.live",
            description: "acquire an owner-authorized DJI Flip live-view stream through an admitted adapter",
            status: CapabilityStatus::Research,
        },
        Capability {
            id: "capture.original.import",
            description: "preserve exact original media bytes in immutable local custody before any derived work",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "evidence.ledger.append",
            description: "append authenticated immutable events against an exact optimistic anchor",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "evidence.ledger.replay",
            description: "replay and validate lineage, epoch, sequence, predecessor, and event identities",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "evidence.manifest.build",
            description: "build bounded streaming chunk manifests with separate logical and representation identities",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "evidence.manifest.verify",
            description: "verify exact files, chunk ordering, lengths, and identities without whole-file buffering",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "evidence.store.local",
            description: "publish verified immutable objects locally with object-first manifest-root-last visibility",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "geometry.correspondence.build",
            description: "build bounded deterministic descriptor correspondences and collision-safe multi-view tracks from exact evidence tables",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "geometry.edge_scale.resolve",
            description: "reconcile correlation-aware relative baseline ratios within exact pose components without metric or optimized-pose authority",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "geometry.epipolar.verify",
            description: "adjudicate exact essential-matrix proposals against calibrated correspondences without granting rotation, translation, or pose authority",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "geometry.global_pose.initialize",
            description: "initialize deterministic component-relative camera orientations and centers in explicit arbitrary edge-scale gauges without metric, trajectory-publication, or bundle-adjustment authority",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "geometry.graph.analyze",
            description: "derive deterministic components, forests, bridges, and cycle witnesses from exact graph evidence without geometric authority",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "geometry.keyframe.select",
            description: "select deterministic quality, coverage, and diversity-aware keyframes from exact candidate evidence",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "geometry.pose_graph.build",
            description: "compose component-local orientations and assess rotation cycles while leaving translation baselines underdetermined",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "geometry.reconstruct.metric",
            description: "publish uncertainty-bearing geometry with an explicit metric scale witness",
            status: CapabilityStatus::Planned,
        },
        Capability {
            id: "geometry.relative_pose.verify",
            description: "adjudicate exact two-view motion candidates using fixed-point epipolar, parallax, and cheirality evidence without granting pose-graph or metric authority",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "media.decode.plan",
            description: "compile authority-free bounded decode plans against independently verified recorded-media roots and exact video sample domains",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "media.decode.receipt.validate",
            description: "validate identity-bound framehash evidence, termination, resource use, output-root publication, and semantic completion without treating process exit as success",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "media.index.classic_samples",
            description: "expand bounded exact classic sample windows with timing and byte-range evidence",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "media.index.published_samples",
            description: "expand exact sample windows only after authenticating published immutable media custody",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "media.inspect.iso_bmff",
            description: "inspect bounded ISO BMFF metadata and classic sample-table consistency without decoding",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "media.inspect.published",
            description: "inspect media through the same authenticated published-object handle and retain custody identities",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "media.normalize",
            description: "supervise ffmpeg/ffprobe sidecars and publish deterministic media renditions",
            status: CapabilityStatus::Planned,
        },
        Capability {
            id: "media.recorded.ingest",
            description: "publish original media, native inspection, and a root-last recorded-media graph, then independently verify its complete closure",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "media.recorded.verify",
            description: "reconstruct and authenticate a recorded-media graph from only its published root manifest identity",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "media.timeline.classic",
            description: "derive a canonical custody-bound DTS/PTS timeline with explicit partial coverage, gaps, reordering, and source byte spans",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "repository.doctor",
            description: "inspect local prerequisite executables without mutating the host",
            status: CapabilityStatus::Scaffolded,
        },
        Capability {
            id: "semantics.resolve.assets",
            description: "resolve evidence-linked home assets and utility observations",
            status: CapabilityStatus::Planned,
        },
        Capability {
            id: "sensor.calibration.derive",
            description: "validate exact camera calibration scope and derive crop, resize, distortion, rolling-shutter, and extrinsic evidence without estimating missing parameters",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "sensor.clock.fit",
            description: "fit a robust epoch-aware affine clock model from exact content-addressed synchronization anchors without extrapolation",
            status: CapabilityStatus::ReferenceSource,
        },
        Capability {
            id: "sensor.scale.resolve",
            description: "resolve correlation-aware relative, estimated, witnessed, or surveyed scale while refusing metric mapping without sufficient independent evidence",
            status: CapabilityStatus::ReferenceSource,
        },
    ]
}

/// Doctor verdict for one bounded check.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum DoctorStatus {
    /// The prerequisite was observed and responded successfully.
    Pass,
    /// The prerequisite is absent, optional, or not yet configured.
    Warn,
}

impl DoctorStatus {
    /// Stable machine-readable text.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Pass => "pass",
            Self::Warn => "warn",
        }
    }
}

/// One deterministic doctor finding.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DoctorFinding {
    /// Stable check identifier.
    pub id: &'static str,
    /// Verdict.
    pub status: DoctorStatus,
    /// Bounded explanation.
    pub detail: String,
}

/// Runs read-only prerequisite probes in stable order.
#[must_use]
pub fn doctor() -> Vec<DoctorFinding> {
    vec![
        executable_check(
            "tool.ffmpeg",
            "ffmpeg",
            &["-version"],
            "ffmpeg is available, but FDGR process supervision and profile admission remain separate gates",
            "ffmpeg was not found; external decode and media normalization remain unavailable",
        ),
        executable_check(
            "tool.ffprobe",
            "ffprobe",
            &["-version"],
            "ffprobe is available for differential media inspection",
            "ffprobe was not found; native bounded inspection remains available but oracle comparison is unavailable",
        ),
        executable_check(
            "tool.python3",
            "python3",
            &["--version"],
            "python3 is available for isolated research model workers",
            "python3 was not found; optional research model workers remain unavailable",
        ),
    ]
}

fn executable_check(
    id: &'static str,
    executable: &str,
    arguments: &[&str],
    pass_detail: &str,
    warning_detail: &str,
) -> DoctorFinding {
    match Command::new(executable).args(arguments).output() {
        Ok(output) if output.status.success() => DoctorFinding {
            id,
            status: DoctorStatus::Pass,
            detail: pass_detail.to_owned(),
        },
        Ok(output) => DoctorFinding {
            id,
            status: DoctorStatus::Warn,
            detail: format!("{warning_detail}; process status was {}", output.status),
        },
        Err(error) => DoctorFinding {
            id,
            status: DoctorStatus::Warn,
            detail: format!("{warning_detail}; {error}"),
        },
    }
}

/// The dependency-ordered implementation sequence summarized for humans and agents.
#[must_use]
pub fn implementation_sequence() -> &'static [&'static str] {
    &[
        "freeze identities, claims, clocks, coordinates, scale witnesses, and publication contracts",
        "build a deterministic reference evidence ledger and content-addressed publication oracle",
        "import original recorded media with exact-byte preservation and timestamp accounting",
        "derive canonical media timelines and explicit clock epochs before joining telemetry",
        "supervise ffmpeg and model workers through bounded process-sidecar protocols",
        "derive explicit calibration and independent scale evidence before metric claims",
        "select evidence-aware keyframes and build deterministic descriptor tracks",
        "adjudicate two-view motion candidates before pose-graph admission",
        "compose orientation topology, reconcile relative edge baselines, and initialize component-relative camera poses before global optimization",
        "add robust pose refinement, fusion, uncertainty, coverage, semantics, archive recovery, and agent surfaces in dependency order",
    ]
}

#[cfg(test)]
mod tests {
    use super::{
        CAPABILITIES_SCHEMA, CapabilityStatus, DOCTOR_SCHEMA, FILE_VERIFICATION_SCHEMA,
        OBJECT_MANIFEST_VIEW_SCHEMA, PLAN_SUMMARY_SCHEMA, STORE_VERIFICATION_SCHEMA,
        VALIDATE_ID_SCHEMA, capabilities, implementation_sequence,
    };
    use std::collections::BTreeSet;

    #[test]
    fn public_schema_ids_use_canonical_slash_versions() {
        for schema in [
            CAPABILITIES_SCHEMA,
            DOCTOR_SCHEMA,
            PLAN_SUMMARY_SCHEMA,
            VALIDATE_ID_SCHEMA,
            OBJECT_MANIFEST_VIEW_SCHEMA,
            FILE_VERIFICATION_SCHEMA,
            STORE_VERIFICATION_SCHEMA,
        ] {
            assert!(schema.starts_with("fdgr."));
            assert!(schema.ends_with("/1"));
            assert!(!schema.contains(".v1"));
        }
    }

    #[test]
    fn capability_ids_are_unique_and_ordered() {
        let ids: Vec<_> = capabilities().iter().map(|capability| capability.id).collect();
        let unique: BTreeSet<_> = ids.iter().copied().collect();
        assert_eq!(ids.len(), unique.len());
        assert!(
            ids.windows(2)
                .all(|pair| matches!(pair, [left, right] if left < right))
        );
    }

    #[test]
    fn live_dji_capture_is_not_claimed_as_implemented() {
        let status = capabilities().iter().find_map(|capability| {
            (capability.id == "capture.dji.flip.live").then_some(capability.status)
        });
        assert_eq!(status, Some(CapabilityStatus::Research));
    }

    #[test]
    fn reference_capabilities_remain_explicit() {
        for id in [
            "capture.original.import",
            "evidence.ledger.append",
            "evidence.ledger.replay",
            "evidence.manifest.build",
            "evidence.manifest.verify",
            "evidence.store.local",
            "geometry.correspondence.build",
            "geometry.edge_scale.resolve",
            "geometry.epipolar.verify",
            "geometry.global_pose.initialize",
            "geometry.graph.analyze",
            "geometry.keyframe.select",
            "geometry.pose_graph.build",
            "geometry.relative_pose.verify",
            "media.decode.plan",
            "media.decode.receipt.validate",
            "media.index.classic_samples",
            "media.index.published_samples",
            "media.inspect.iso_bmff",
            "media.inspect.published",
            "media.recorded.ingest",
            "media.recorded.verify",
            "media.timeline.classic",
            "sensor.calibration.derive",
            "sensor.clock.fit",
            "sensor.scale.resolve",
        ] {
            let status = capabilities()
                .iter()
                .find_map(|capability| (capability.id == id).then_some(capability.status));
            assert_eq!(status, Some(CapabilityStatus::ReferenceSource));
        }
    }

    #[test]
    fn implementation_starts_with_contracts() {
        assert!(
            implementation_sequence()
                .first()
                .is_some_and(|step| step.contains("freeze"))
        );
    }
}
