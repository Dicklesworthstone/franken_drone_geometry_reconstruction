#![forbid(unsafe_code)]
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::indexing_slicing,
    clippy::module_name_repetitions,
    clippy::similar_names,
    clippy::struct_excessive_bools,
    clippy::struct_field_names,
    clippy::too_many_lines
)]
//! Correlation-aware metric scale witness resolution for FDGR.
//!
//! Scale candidates remain hypotheses until independent, internally consistent witness groups earn
//! metric authority. Conflicting groups and exact witnesses are retained, and metric mapping fails
//! closed unless witnessed or surveyed authority is established.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest, ScaleStatus};
use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

/// Public schema identity for one scale-resolution model.
pub const SCALE_MODEL_SCHEMA: &str = "fdgr.scale_model/1";
/// Maximum witnesses admitted by one deterministic resolution.
pub const MAX_SCALE_WITNESSES: usize = 256;
/// Maximum metric distance in micrometers.
pub const MAX_METRIC_DISTANCE_MICROMETERS: u64 = 1_000_000_000_000_000;
/// Maximum relative distance in canonical nano-relative-units.
pub const MAX_RELATIVE_DISTANCE_NANOUNITS: u64 = 1_000_000_000_000_000_000;
/// Maximum residual or uncertainty gate.
pub const MAX_SCALE_PPM: u64 = 10_000_000;

/// Evidence class establishing one scale ratio.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ScaleWitnessKind {
    /// Neural, heuristic, or historical prior; never metric authority.
    ModelPrior,
    /// Device motion/position proposal not independently surveyed.
    TelemetryBaseline,
    /// Human- or instrument-measured physical distance.
    MeasuredBaseline,
    /// Calibrated fiducial with known dimensions.
    Fiducial,
    /// Survey-control evidence.
    SurveyControl,
}

impl ScaleWitnessKind {
    /// Canonical wire value.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ModelPrior => "model_prior",
            Self::TelemetryBaseline => "telemetry_baseline",
            Self::MeasuredBaseline => "measured_baseline",
            Self::Fiducial => "fiducial",
            Self::SurveyControl => "survey_control",
        }
    }

    const fn code(self) -> u8 {
        match self {
            Self::ModelPrior => 1,
            Self::TelemetryBaseline => 2,
            Self::MeasuredBaseline => 3,
            Self::Fiducial => 4,
            Self::SurveyControl => 5,
        }
    }

    const fn authority_rank(self) -> u8 {
        match self {
            Self::ModelPrior | Self::TelemetryBaseline => 1,
            Self::MeasuredBaseline | Self::Fiducial => 2,
            Self::SurveyControl => 3,
        }
    }
}

/// Exact basis for one scale-resolution generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ScaleBasis {
    /// Digest of the exact witness collection.
    pub witness_basis_digest: EvidenceDigest,
    /// Geometry generation whose relative units are being resolved.
    pub geometry_root_digest: EvidenceDigest,
    /// Exact calibration generation used by the geometry.
    pub calibration_digest: EvidenceDigest,
    /// Exact spatial domain/scope identity.
    pub scope_digest: EvidenceDigest,
    /// Immutable scale-model generation.
    pub model_generation: u64,
}

/// One observed ratio between metric and relative geometry.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ScaleWitness {
    /// Stable nonzero witness identity.
    pub witness_id: u64,
    /// Exact witness evidence identity.
    pub evidence_digest: EvidenceDigest,
    /// Evidence class.
    pub kind: ScaleWitnessKind,
    /// Physical distance in micrometers.
    pub metric_distance_micrometers: u64,
    /// Corresponding relative-geometry distance in nano-relative-units.
    pub relative_distance_nanounits: u64,
    /// Symmetric physical-distance uncertainty in micrometers.
    pub uncertainty_micrometers: u64,
    /// Nonzero dependence class; each class gets one robust vote.
    pub correlation_group: u32,
}

/// Constitutional robust-resolution gates.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ScaleFitOptions {
    /// Maximum ratio residual before witness uncertainty is added.
    pub max_residual_ppm: u64,
    /// Minimum internally consistent independent groups for a resolved candidate.
    pub min_independent_groups: u16,
    /// Minimum internally consistent witnessed-or-surveyed groups for metric authority.
    pub min_witnessed_groups: u16,
    /// Minimum internally consistent survey-control groups for surveyed authority.
    pub min_surveyed_groups: u16,
}

impl Default for ScaleFitOptions {
    fn default() -> Self {
        Self {
            max_residual_ppm: 20_000,
            min_independent_groups: 2,
            min_witnessed_groups: 2,
            min_surveyed_groups: 2,
        }
    }
}

/// Residual retained for every exact witness.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ScaleWitnessResidual {
    /// Witness identity.
    pub witness_id: u64,
    /// Dependence class.
    pub correlation_group: u32,
    /// Ratio residual relative to the candidate.
    pub residual_ppm: u64,
    /// Whether the exact witness and its whole correlation group were admitted.
    pub inlier: bool,
}

/// Immutable scale resolution. A candidate may be retained without metric authority.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ScaleModel {
    /// Exact resolution basis.
    pub basis: ScaleBasis,
    /// Exact gates.
    pub options: ScaleFitOptions,
    /// Canonically sorted original witnesses.
    pub witnesses: Vec<ScaleWitness>,
    /// Whether enough internally consistent independent groups resolved the candidate.
    pub resolved: bool,
    /// Metric authority. Unresolved candidates are always `RelativeOnly`.
    pub authority: ScaleStatus,
    /// Candidate micrometer numerator.
    pub candidate_scale_numerator_micrometers: i128,
    /// Candidate relative-nanounit denominator.
    pub candidate_scale_denominator_relative_nanounits: i128,
    /// One robust representative witness per correlation group.
    pub representative_witness_ids: Vec<u64>,
    /// Exact witnesses admitted after whole-group consistency checks.
    pub inlier_witness_ids: Vec<u64>,
    /// Exact witnesses rejected by residual or whole-group conflict.
    pub outlier_witness_ids: Vec<u64>,
    /// Internally consistent retained groups.
    pub inlier_group_ids: Vec<u32>,
    /// Rejected or internally conflicting groups.
    pub conflicting_group_ids: Vec<u32>,
    /// Residual evidence for every exact witness.
    pub residuals: Vec<ScaleWitnessResidual>,
    /// Median admitted residual.
    pub median_residual_ppm: u64,
    /// Maximum admitted residual.
    pub maximum_residual_ppm: u64,
    /// Maximum retained representative spread around the candidate.
    pub scale_spread_ppm: u64,
    /// Conservative relative scale uncertainty.
    pub declared_uncertainty_ppm: u64,
}

/// Result of applying an authoritative scale.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MetricDistance {
    /// Input relative distance.
    pub relative_distance_nanounits: u64,
    /// Rounded metric distance.
    pub metric_distance_micrometers: u64,
    /// Conservative symmetric uncertainty.
    pub uncertainty_micrometers: u64,
    /// Scale authority.
    pub authority: ScaleStatus,
    /// Exact scale generation.
    pub model_generation: u64,
}

/// Stable scale validation, fitting, and mapping failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ScaleError {
    /// A mandatory identity was all zero.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// A mandatory numeric field was zero.
    ZeroValue {
        /// Stable field name.
        field: &'static str,
    },
    /// A bounded numeric field exceeded its limit.
    BoundExceeded {
        /// Stable field name.
        field: &'static str,
        /// Observed value.
        actual: u128,
        /// Maximum value.
        maximum: u128,
    },
    /// A configured group minimum was too small to establish independence.
    MinimumGroupCountTooSmall {
        /// Stable field name.
        field: &'static str,
        /// Observed count.
        actual: u16,
        /// Required minimum.
        minimum: u16,
    },
    /// Too few witnesses were supplied.
    TooFewWitnesses {
        /// Observed count.
        actual: usize,
        /// Required count.
        minimum: usize,
    },
    /// Too many witnesses were supplied.
    TooManyWitnesses {
        /// Observed count.
        actual: usize,
        /// Maximum count.
        maximum: usize,
    },
    /// A witness identity was duplicated.
    DuplicateWitnessId {
        /// Duplicate identity.
        witness_id: u64,
    },
    /// Too few independent groups existed even to form a robust candidate.
    InsufficientIndependentGroups {
        /// Observed groups.
        actual: usize,
        /// Required groups.
        minimum: usize,
    },
    /// Candidate ratio was not positive.
    NonPositiveScale,
    /// Checked arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable operation.
        field: &'static str,
    },
    /// Metric mapping was requested from non-authoritative scale.
    MetricAuthorityRequired {
        /// Current status.
        authority: ScaleStatus,
        /// Whether the candidate resolved.
        resolved: bool,
    },
    /// Deterministic derived fields disagreed with replay.
    DerivedMismatch {
        /// Stable field name.
        field: &'static str,
    },
    /// Canonical encoding or hashing failed.
    Codec(CodecError),
    /// Identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for ScaleError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroIdentity { field } => {
                write!(formatter, "scale identity {field} must not be all zero")
            }
            Self::ZeroValue { field } => write!(formatter, "scale field {field} must be nonzero"),
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(formatter, "scale field {field} is {actual}; maximum is {maximum}"),
            Self::MinimumGroupCountTooSmall {
                field,
                actual,
                minimum,
            } => write!(
                formatter,
                "scale field {field} is {actual}; at least {minimum} independent groups are required"
            ),
            Self::TooFewWitnesses { actual, minimum } => write!(
                formatter,
                "scale resolution received {actual} witnesses; at least {minimum} are required"
            ),
            Self::TooManyWitnesses { actual, maximum } => write!(
                formatter,
                "scale resolution received {actual} witnesses; maximum is {maximum}"
            ),
            Self::DuplicateWitnessId { witness_id } => {
                write!(formatter, "scale witness identity {witness_id} is duplicated")
            }
            Self::InsufficientIndependentGroups { actual, minimum } => write!(
                formatter,
                "scale resolution contains {actual} independent groups; at least {minimum} are required"
            ),
            Self::NonPositiveScale => formatter.write_str("scale candidate must be positive"),
            Self::ArithmeticOverflow { field } => {
                write!(formatter, "scale arithmetic overflowed while computing {field}")
            }
            Self::MetricAuthorityRequired {
                authority,
                resolved,
            } => write!(
                formatter,
                "metric distance requires witnessed or surveyed resolved scale; authority is {} and resolved is {resolved}",
                scale_status_text(*authority)
            ),
            Self::DerivedMismatch { field } => write!(
                formatter,
                "scale derived field {field} disagrees with deterministic replay"
            ),
            Self::Codec(error) => write!(formatter, "scale codec error: {error}"),
            Self::Domain(error) => write!(formatter, "scale identity-domain error: {error}"),
            Self::JsonRendering(error) => write!(formatter, "scale JSON rendering failed: {error}"),
        }
    }
}

impl Error for ScaleError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for ScaleError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for ScaleError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct Ratio {
    numerator: i128,
    denominator: i128,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct GroupRepresentative {
    witness_id: u64,
    metric_distance_micrometers: u64,
    relative_distance_nanounits: u64,
    uncertainty_micrometers: u64,
    correlation_group: u32,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct ExactClassification {
    admitted_witness_ids: Vec<u64>,
    rejected_witness_ids: Vec<u64>,
    clean_group_ids: BTreeSet<u32>,
    conflicting_group_ids: BTreeSet<u32>,
    residuals: Vec<ScaleWitnessResidual>,
}

/// Resolves a deterministic scale candidate while retaining conflicts and authority boundaries.
///
/// Correlation groups receive one representative vote during robust fitting. After a candidate is
/// formed, every exact witness is checked. A group is admitted only if its representative passed
/// and every exact witness in that group passed its uncertainty-aware gate. This prevents a
/// correlated high-grade outlier from lending authority to a lower-grade inlier in the same group.
///
/// # Errors
///
/// Returns a stable basis, witness, group, bound, or arithmetic error.
pub fn resolve_scale(
    basis: ScaleBasis,
    options: ScaleFitOptions,
    mut witnesses: Vec<ScaleWitness>,
) -> Result<ScaleModel, ScaleError> {
    validate_basis(&basis)?;
    validate_options(options)?;
    validate_witnesses(&witnesses)?;
    witnesses.sort_by(|left, right| left.witness_id.cmp(&right.witness_id));
    let representatives = group_representatives(&witnesses)?;
    if representatives.len() < 2 {
        return Err(ScaleError::InsufficientIndependentGroups {
            actual: representatives.len(),
            minimum: 2,
        });
    }

    let initial_candidate = median_representative_ratio(&representatives)?;
    ensure_positive(initial_candidate)?;
    let initial_representative_groups = classify_representative_groups(
        &representatives,
        initial_candidate,
        options.max_residual_ppm,
    )?;
    let first_exact = classify_exact_witnesses(
        &witnesses,
        initial_candidate,
        &initial_representative_groups,
        options.max_residual_ppm,
    )?;
    let required_groups = usize::from(options.min_independent_groups);

    let (candidate, representative_groups, exact) = if first_exact.clean_group_ids.len()
        >= required_groups
    {
        let retained = representatives
            .iter()
            .filter(|value| first_exact.clean_group_ids.contains(&value.correlation_group))
            .cloned()
            .collect::<Vec<_>>();
        let candidate = median_representative_ratio(&retained)?;
        ensure_positive(candidate)?;
        let groups = classify_representative_groups(
            &representatives,
            candidate,
            options.max_residual_ppm,
        )?;
        let exact = classify_exact_witnesses(
            &witnesses,
            candidate,
            &groups,
            options.max_residual_ppm,
        )?;
        (candidate, groups, exact)
    } else {
        (
            initial_candidate,
            initial_representative_groups,
            first_exact,
        )
    };

    let resolved = exact.clean_group_ids.len() >= required_groups;
    let inlier_group_ids = exact.clean_group_ids.iter().copied().collect::<Vec<_>>();
    let conflicting_group_ids = exact
        .conflicting_group_ids
        .iter()
        .copied()
        .collect::<Vec<_>>();
    let representative_witness_ids = representatives
        .iter()
        .map(|value| value.witness_id)
        .collect::<Vec<_>>();

    let mut admitted_residuals = exact
        .residuals
        .iter()
        .filter(|value| value.inlier)
        .map(|value| value.residual_ppm)
        .collect::<Vec<_>>();
    admitted_residuals.sort_unstable();
    let median_residual_ppm = admitted_residuals
        .get(admitted_residuals.len().saturating_sub(1) / 2)
        .copied()
        .unwrap_or(0);
    let maximum_residual_ppm = admitted_residuals.iter().copied().max().unwrap_or(0);

    let admitted = exact
        .admitted_witness_ids
        .iter()
        .copied()
        .collect::<BTreeSet<_>>();
    let maximum_uncertainty_ppm = witnesses
        .iter()
        .filter(|value| admitted.contains(&value.witness_id))
        .map(witness_uncertainty_ppm)
        .collect::<Result<Vec<_>, _>>()?
        .into_iter()
        .max()
        .unwrap_or(0);

    let retained_representatives = representatives
        .iter()
        .filter(|value| exact.clean_group_ids.contains(&value.correlation_group))
        .cloned()
        .collect::<Vec<_>>();
    let scale_spread_ppm = if retained_representatives.len() >= 2 {
        scale_spread_ppm(&retained_representatives, candidate)?
    } else {
        0
    };
    let declared_uncertainty_ppm = maximum_residual_ppm
        .checked_add(maximum_uncertainty_ppm)
        .and_then(|value| value.checked_add(scale_spread_ppm))
        .ok_or(ScaleError::ArithmeticOverflow {
            field: "declared_uncertainty_ppm",
        })?;

    let authority = if resolved {
        authority_for_exact_inliers(
            &witnesses,
            &admitted,
            &exact.clean_group_ids,
            options.min_witnessed_groups,
            options.min_surveyed_groups,
        )
    } else {
        ScaleStatus::RelativeOnly
    };

    let all_representative_groups = representatives
        .iter()
        .map(|value| value.correlation_group)
        .collect::<BTreeSet<_>>();
    if !representative_groups.is_subset(&all_representative_groups) {
        return Err(ScaleError::DerivedMismatch {
            field: "representative_groups",
        });
    }

    Ok(ScaleModel {
        basis,
        options,
        witnesses,
        resolved,
        authority,
        candidate_scale_numerator_micrometers: candidate.numerator,
        candidate_scale_denominator_relative_nanounits: candidate.denominator,
        representative_witness_ids,
        inlier_witness_ids: exact.admitted_witness_ids,
        outlier_witness_ids: exact.rejected_witness_ids,
        inlier_group_ids,
        conflicting_group_ids,
        residuals: exact.residuals,
        median_residual_ppm,
        maximum_residual_ppm,
        scale_spread_ppm,
        declared_uncertainty_ppm,
    })
}

impl ScaleModel {
    /// Replays the complete deterministic resolution and compares every field.
    ///
    /// # Errors
    ///
    /// Returns a resolution failure or derived-field mismatch.
    pub fn validate(&self) -> Result<(), ScaleError> {
        let rebuilt = resolve_scale(
            self.basis.clone(),
            self.options,
            self.witnesses.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(ScaleError::DerivedMismatch {
                field: "scale_model",
            })
        }
    }

    /// Maps a relative distance only when metric authority has been earned.
    ///
    /// # Errors
    ///
    /// Returns a validation, authority, bound, or arithmetic error.
    pub fn map_relative_distance(
        &self,
        relative_distance_nanounits: u64,
    ) -> Result<MetricDistance, ScaleError> {
        self.validate()?;
        if !self.resolved || !self.authority.permits_metric_claim() {
            return Err(ScaleError::MetricAuthorityRequired {
                authority: self.authority,
                resolved: self.resolved,
            });
        }
        validate_relative_distance(relative_distance_nanounits)?;
        let numerator = i128::from(relative_distance_nanounits)
            .checked_mul(self.candidate_scale_numerator_micrometers)
            .ok_or(ScaleError::ArithmeticOverflow {
                field: "metric_distance_numerator",
            })?;
        let metric = divide_round_nearest(
            numerator,
            self.candidate_scale_denominator_relative_nanounits,
        )?;
        let metric = u64::try_from(metric).map_err(|_| ScaleError::BoundExceeded {
            field: "metric_distance_micrometers",
            actual: metric.unsigned_abs(),
            maximum: u128::from(u64::MAX),
        })?;
        let uncertainty = ceil_ratio_u128(
            u128::from(metric)
                .checked_mul(u128::from(self.declared_uncertainty_ppm))
                .ok_or(ScaleError::ArithmeticOverflow {
                    field: "metric_uncertainty",
                })?,
            1_000_000,
            "metric_uncertainty",
        )?;
        Ok(MetricDistance {
            relative_distance_nanounits,
            metric_distance_micrometers: metric,
            uncertainty_micrometers: u64::try_from(uncertainty).map_err(|_| {
                ScaleError::BoundExceeded {
                    field: "uncertainty_micrometers",
                    actual: uncertainty,
                    maximum: u128::from(u64::MAX),
                }
            })?,
            authority: self.authority,
            model_generation: self.basis.model_generation,
        })
    }

    /// Returns deterministic canonical model bytes.
    ///
    /// # Errors
    ///
    /// Returns a validation or codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, ScaleError> {
        self.validate()?;
        let capacity = 768_usize.saturating_add(self.witnesses.len().saturating_mul(128));
        let mut encoder = Encoder::with_capacity(capacity);
        encoder.put_str(SCALE_MODEL_SCHEMA)?;
        encoder.put_digest(&self.basis.witness_basis_digest);
        encoder.put_digest(&self.basis.geometry_root_digest);
        encoder.put_digest(&self.basis.calibration_digest);
        encoder.put_digest(&self.basis.scope_digest);
        encoder.put_u64(self.basis.model_generation);
        encoder.put_u64(self.options.max_residual_ppm);
        encoder.put_u16(self.options.min_independent_groups);
        encoder.put_u16(self.options.min_witnessed_groups);
        encoder.put_u16(self.options.min_surveyed_groups);
        encoder.put_bool(self.resolved);
        encoder.put_u8(scale_status_code(self.authority));
        put_i128(&mut encoder, self.candidate_scale_numerator_micrometers)?;
        put_i128(
            &mut encoder,
            self.candidate_scale_denominator_relative_nanounits,
        )?;
        put_u64_sequence(&mut encoder, &self.representative_witness_ids)?;
        put_u64_sequence(&mut encoder, &self.inlier_witness_ids)?;
        put_u64_sequence(&mut encoder, &self.outlier_witness_ids)?;
        put_u32_sequence(&mut encoder, &self.inlier_group_ids)?;
        put_u32_sequence(&mut encoder, &self.conflicting_group_ids)?;
        encoder.put_u64(self.median_residual_ppm);
        encoder.put_u64(self.maximum_residual_ppm);
        encoder.put_u64(self.scale_spread_ppm);
        encoder.put_u64(self.declared_uncertainty_ppm);
        encoder.put_u64(usize_to_u64(self.witnesses.len())?);
        for witness in &self.witnesses {
            encoder.put_u64(witness.witness_id);
            encoder.put_digest(&witness.evidence_digest);
            encoder.put_u8(witness.kind.code());
            encoder.put_u64(witness.metric_distance_micrometers);
            encoder.put_u64(witness.relative_distance_nanounits);
            encoder.put_u64(witness.uncertainty_micrometers);
            encoder.put_u32(witness.correlation_group);
        }
        encoder.put_u64(usize_to_u64(self.residuals.len())?);
        for residual in &self.residuals {
            encoder.put_u64(residual.witness_id);
            encoder.put_u32(residual.correlation_group);
            encoder.put_u64(residual.residual_ppm);
            encoder.put_bool(residual.inlier);
        }
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated scale-model identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, ScaleError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(SCALE_MODEL_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }

    /// Renders deterministic field-ordered JSON.
    ///
    /// Candidate rational values are decimal strings to remain lossless in generic clients.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, ScaleError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{SCALE_MODEL_SCHEMA}\",\"scale_digest\":\"{digest}\",\"witness_basis_digest\":\"{}\",\"geometry_root_digest\":\"{}\",\"calibration_digest\":\"{}\",\"scope_digest\":\"{}\",\"model_generation\":{},\"resolved\":{},\"authority\":\"{}\",\"candidate_scale_numerator_micrometers\":\"{}\",\"candidate_scale_denominator_relative_nanounits\":\"{}\",\"max_residual_ppm\":{},\"min_independent_groups\":{},\"min_witnessed_groups\":{},\"min_surveyed_groups\":{},\"median_residual_ppm\":{},\"maximum_residual_ppm\":{},\"scale_spread_ppm\":{},\"declared_uncertainty_ppm\":{},\"inlier_witness_ids\":[",
            self.basis.witness_basis_digest,
            self.basis.geometry_root_digest,
            self.basis.calibration_digest,
            self.basis.scope_digest,
            self.basis.model_generation,
            self.resolved,
            scale_status_text(self.authority),
            self.candidate_scale_numerator_micrometers,
            self.candidate_scale_denominator_relative_nanounits,
            self.options.max_residual_ppm,
            self.options.min_independent_groups,
            self.options.min_witnessed_groups,
            self.options.min_surveyed_groups,
            self.median_residual_ppm,
            self.maximum_residual_ppm,
            self.scale_spread_ppm,
            self.declared_uncertainty_ppm,
        );
        push_u64_json_array(&mut output, &self.inlier_witness_ids)?;
        output.push_str("],\"outlier_witness_ids\":[");
        push_u64_json_array(&mut output, &self.outlier_witness_ids)?;
        output.push_str("],\"inlier_group_ids\":[");
        push_u32_json_array(&mut output, &self.inlier_group_ids)?;
        output.push_str("],\"conflicting_group_ids\":[");
        push_u32_json_array(&mut output, &self.conflicting_group_ids)?;
        output.push_str("],\"witnesses\":[");
        for (position, witness) in self.witnesses.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"witness_id\":{},\"evidence_digest\":\"{}\",\"kind\":\"{}\",\"metric_distance_micrometers\":{},\"relative_distance_nanounits\":{},\"uncertainty_micrometers\":{},\"correlation_group\":{}}}",
                witness.witness_id,
                witness.evidence_digest,
                witness.kind.as_str(),
                witness.metric_distance_micrometers,
                witness.relative_distance_nanounits,
                witness.uncertainty_micrometers,
                witness.correlation_group,
            )
            .map_err(json_rendering)?;
        }
        output.push_str("],\"residuals\":[");
        for (position, residual) in self.residuals.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(
                output,
                "{{\"witness_id\":{},\"correlation_group\":{},\"residual_ppm\":{},\"inlier\":{}}}",
                residual.witness_id,
                residual.correlation_group,
                residual.residual_ppm,
                residual.inlier,
            )
            .map_err(json_rendering)?;
        }
        output.push_str("]}");
        Ok(output)
    }
}

fn validate_basis(basis: &ScaleBasis) -> Result<(), ScaleError> {
    for (field, digest) in [
        ("witness_basis_digest", &basis.witness_basis_digest),
        ("geometry_root_digest", &basis.geometry_root_digest),
        ("calibration_digest", &basis.calibration_digest),
        ("scope_digest", &basis.scope_digest),
    ] {
        reject_zero_digest(field, digest)?;
    }
    if basis.model_generation == 0 {
        return Err(ScaleError::ZeroValue {
            field: "model_generation",
        });
    }
    Ok(())
}

fn validate_options(options: ScaleFitOptions) -> Result<(), ScaleError> {
    if options.max_residual_ppm == 0 {
        return Err(ScaleError::ZeroValue {
            field: "max_residual_ppm",
        });
    }
    if options.max_residual_ppm > MAX_SCALE_PPM {
        return Err(ScaleError::BoundExceeded {
            field: "max_residual_ppm",
            actual: u128::from(options.max_residual_ppm),
            maximum: u128::from(MAX_SCALE_PPM),
        });
    }
    for (field, value) in [
        ("min_independent_groups", options.min_independent_groups),
        ("min_witnessed_groups", options.min_witnessed_groups),
        ("min_surveyed_groups", options.min_surveyed_groups),
    ] {
        if value < 2 {
            return Err(ScaleError::MinimumGroupCountTooSmall {
                field,
                actual: value,
                minimum: 2,
            });
        }
    }
    Ok(())
}

fn validate_witnesses(witnesses: &[ScaleWitness]) -> Result<(), ScaleError> {
    if witnesses.len() < 2 {
        return Err(ScaleError::TooFewWitnesses {
            actual: witnesses.len(),
            minimum: 2,
        });
    }
    if witnesses.len() > MAX_SCALE_WITNESSES {
        return Err(ScaleError::TooManyWitnesses {
            actual: witnesses.len(),
            maximum: MAX_SCALE_WITNESSES,
        });
    }
    let mut identities = BTreeSet::new();
    for witness in witnesses {
        if witness.witness_id == 0 {
            return Err(ScaleError::ZeroValue {
                field: "witness_id",
            });
        }
        if !identities.insert(witness.witness_id) {
            return Err(ScaleError::DuplicateWitnessId {
                witness_id: witness.witness_id,
            });
        }
        reject_zero_digest("witness_evidence_digest", &witness.evidence_digest)?;
        validate_metric_distance(witness.metric_distance_micrometers)?;
        validate_relative_distance(witness.relative_distance_nanounits)?;
        if witness.uncertainty_micrometers >= witness.metric_distance_micrometers {
            return Err(ScaleError::BoundExceeded {
                field: "uncertainty_micrometers",
                actual: u128::from(witness.uncertainty_micrometers),
                maximum: u128::from(witness.metric_distance_micrometers.saturating_sub(1)),
            });
        }
        if witness.correlation_group == 0 {
            return Err(ScaleError::ZeroValue {
                field: "correlation_group",
            });
        }
    }
    Ok(())
}

fn validate_metric_distance(value: u64) -> Result<(), ScaleError> {
    if value == 0 {
        return Err(ScaleError::ZeroValue {
            field: "metric_distance_micrometers",
        });
    }
    if value > MAX_METRIC_DISTANCE_MICROMETERS {
        return Err(ScaleError::BoundExceeded {
            field: "metric_distance_micrometers",
            actual: u128::from(value),
            maximum: u128::from(MAX_METRIC_DISTANCE_MICROMETERS),
        });
    }
    Ok(())
}

fn validate_relative_distance(value: u64) -> Result<(), ScaleError> {
    if value == 0 {
        return Err(ScaleError::ZeroValue {
            field: "relative_distance_nanounits",
        });
    }
    if value > MAX_RELATIVE_DISTANCE_NANOUNITS {
        return Err(ScaleError::BoundExceeded {
            field: "relative_distance_nanounits",
            actual: u128::from(value),
            maximum: u128::from(MAX_RELATIVE_DISTANCE_NANOUNITS),
        });
    }
    Ok(())
}

fn group_representatives(
    witnesses: &[ScaleWitness],
) -> Result<Vec<GroupRepresentative>, ScaleError> {
    let mut groups: BTreeMap<u32, Vec<ScaleWitness>> = BTreeMap::new();
    for witness in witnesses {
        groups
            .entry(witness.correlation_group)
            .or_default()
            .push(witness.clone());
    }
    let mut output = Vec::with_capacity(groups.len());
    for (group, mut members) in groups {
        members.sort_by(|left, right| {
            compare_ratio(
                &ratio_from_witness(left),
                &ratio_from_witness(right),
            )
            .then(left.witness_id.cmp(&right.witness_id))
        });
        let index = members.len().saturating_sub(1) / 2;
        let selected = members.get(index).ok_or(ScaleError::InsufficientIndependentGroups {
            actual: output.len(),
            minimum: 2,
        })?;
        let uncertainty_micrometers = members
            .iter()
            .map(|value| value.uncertainty_micrometers)
            .max()
            .unwrap_or(0);
        output.push(GroupRepresentative {
            witness_id: selected.witness_id,
            metric_distance_micrometers: selected.metric_distance_micrometers,
            relative_distance_nanounits: selected.relative_distance_nanounits,
            uncertainty_micrometers,
            correlation_group: group,
        });
    }
    output.sort_by(|left, right| {
        compare_ratio(
            &ratio_from_representative(left),
            &ratio_from_representative(right),
        )
        .then(left.correlation_group.cmp(&right.correlation_group))
    });
    Ok(output)
}

fn median_representative_ratio(
    representatives: &[GroupRepresentative],
) -> Result<Ratio, ScaleError> {
    if representatives.is_empty() {
        return Err(ScaleError::InsufficientIndependentGroups {
            actual: 0,
            minimum: 1,
        });
    }
    let mut ratios = representatives
        .iter()
        .map(ratio_from_representative)
        .collect::<Vec<_>>();
    ratios.sort_by(compare_ratio);
    ratios
        .get(ratios.len().saturating_sub(1) / 2)
        .copied()
        .ok_or(ScaleError::NonPositiveScale)
}

fn classify_representative_groups(
    representatives: &[GroupRepresentative],
    candidate: Ratio,
    max_residual_ppm: u64,
) -> Result<BTreeSet<u32>, ScaleError> {
    let mut groups = BTreeSet::new();
    for representative in representatives {
        let residual = ratio_residual_ppm(
            representative.metric_distance_micrometers,
            representative.relative_distance_nanounits,
            candidate,
        )?;
        let uncertainty = representative_uncertainty_ppm(representative)?;
        let threshold = max_residual_ppm
            .checked_add(uncertainty)
            .ok_or(ScaleError::ArithmeticOverflow {
                field: "group_residual_threshold",
            })?;
        if residual <= threshold {
            groups.insert(representative.correlation_group);
        }
    }
    Ok(groups)
}

fn classify_exact_witnesses(
    witnesses: &[ScaleWitness],
    candidate: Ratio,
    representative_groups: &BTreeSet<u32>,
    max_residual_ppm: u64,
) -> Result<ExactClassification, ScaleError> {
    let mut provisional = Vec::with_capacity(witnesses.len());
    let mut group_pass: BTreeMap<u32, (bool, bool)> = BTreeMap::new();
    let all_groups = witnesses
        .iter()
        .map(|value| value.correlation_group)
        .collect::<BTreeSet<_>>();

    for witness in witnesses {
        let residual_ppm = ratio_residual_ppm(
            witness.metric_distance_micrometers,
            witness.relative_distance_nanounits,
            candidate,
        )?;
        let uncertainty_ppm = witness_uncertainty_ppm(witness)?;
        let threshold = max_residual_ppm
            .checked_add(uncertainty_ppm)
            .ok_or(ScaleError::ArithmeticOverflow {
                field: "witness_residual_threshold",
            })?;
        let passes = representative_groups.contains(&witness.correlation_group)
            && residual_ppm <= threshold;
        let state = group_pass
            .entry(witness.correlation_group)
            .or_insert((false, false));
        if passes {
            state.0 = true;
        } else {
            state.1 = true;
        }
        provisional.push((witness, residual_ppm, passes));
    }

    let clean_group_ids = group_pass
        .iter()
        .filter_map(|(group, (pass, fail))| (*pass && !*fail).then_some(*group))
        .collect::<BTreeSet<_>>();
    let conflicting_group_ids = all_groups
        .difference(&clean_group_ids)
        .copied()
        .collect::<BTreeSet<_>>();
    let mut admitted_witness_ids = Vec::new();
    let mut rejected_witness_ids = Vec::new();
    let mut residuals = Vec::with_capacity(provisional.len());
    for (witness, residual_ppm, passes) in provisional {
        let inlier = passes && clean_group_ids.contains(&witness.correlation_group);
        if inlier {
            admitted_witness_ids.push(witness.witness_id);
        } else {
            rejected_witness_ids.push(witness.witness_id);
        }
        residuals.push(ScaleWitnessResidual {
            witness_id: witness.witness_id,
            correlation_group: witness.correlation_group,
            residual_ppm,
            inlier,
        });
    }
    Ok(ExactClassification {
        admitted_witness_ids,
        rejected_witness_ids,
        clean_group_ids,
        conflicting_group_ids,
        residuals,
    })
}

fn authority_for_exact_inliers(
    witnesses: &[ScaleWitness],
    admitted_witness_ids: &BTreeSet<u64>,
    clean_group_ids: &BTreeSet<u32>,
    min_witnessed_groups: u16,
    min_surveyed_groups: u16,
) -> ScaleStatus {
    let mut witnessed_groups = BTreeSet::new();
    let mut surveyed_groups = BTreeSet::new();
    for witness in witnesses {
        if !admitted_witness_ids.contains(&witness.witness_id)
            || !clean_group_ids.contains(&witness.correlation_group)
        {
            continue;
        }
        if witness.kind.authority_rank() >= 2 {
            witnessed_groups.insert(witness.correlation_group);
        }
        if witness.kind.authority_rank() >= 3 {
            surveyed_groups.insert(witness.correlation_group);
        }
    }
    if surveyed_groups.len() >= usize::from(min_surveyed_groups) {
        ScaleStatus::Surveyed
    } else if witnessed_groups.len() >= usize::from(min_witnessed_groups) {
        ScaleStatus::Witnessed
    } else {
        ScaleStatus::Estimated
    }
}

fn scale_spread_ppm(
    representatives: &[GroupRepresentative],
    candidate: Ratio,
) -> Result<u64, ScaleError> {
    let mut maximum = 0_u64;
    for representative in representatives {
        maximum = maximum.max(ratio_residual_ppm(
            representative.metric_distance_micrometers,
            representative.relative_distance_nanounits,
            candidate,
        )?);
    }
    Ok(maximum)
}

fn ratio_from_witness(witness: &ScaleWitness) -> Ratio {
    reduce_ratio(
        i128::from(witness.metric_distance_micrometers),
        i128::from(witness.relative_distance_nanounits),
    )
}

fn ratio_from_representative(representative: &GroupRepresentative) -> Ratio {
    reduce_ratio(
        i128::from(representative.metric_distance_micrometers),
        i128::from(representative.relative_distance_nanounits),
    )
}

fn reduce_ratio(numerator: i128, denominator: i128) -> Ratio {
    let divisor = gcd(numerator.unsigned_abs(), denominator.unsigned_abs());
    let divisor = i128::try_from(divisor).unwrap_or(1);
    Ratio {
        numerator: numerator / divisor,
        denominator: denominator / divisor,
    }
}

fn ensure_positive(ratio: Ratio) -> Result<(), ScaleError> {
    if ratio.numerator <= 0 || ratio.denominator <= 0 {
        Err(ScaleError::NonPositiveScale)
    } else {
        Ok(())
    }
}

fn compare_ratio(left: &Ratio, right: &Ratio) -> Ordering {
    let left_scaled = left.numerator * right.denominator;
    let right_scaled = right.numerator * left.denominator;
    left_scaled
        .cmp(&right_scaled)
        .then(left.numerator.cmp(&right.numerator))
        .then(left.denominator.cmp(&right.denominator))
}

fn ratio_residual_ppm(
    metric_distance_micrometers: u64,
    relative_distance_nanounits: u64,
    candidate: Ratio,
) -> Result<u64, ScaleError> {
    let observed_scaled = i128::from(metric_distance_micrometers)
        .checked_mul(candidate.denominator)
        .ok_or(ScaleError::ArithmeticOverflow {
            field: "residual_observed_scaled",
        })?;
    let predicted_scaled = candidate
        .numerator
        .checked_mul(i128::from(relative_distance_nanounits))
        .ok_or(ScaleError::ArithmeticOverflow {
            field: "residual_predicted_scaled",
        })?;
    let difference = observed_scaled
        .checked_sub(predicted_scaled)
        .ok_or(ScaleError::ArithmeticOverflow {
            field: "scale_residual",
        })?
        .unsigned_abs();
    let denominator = predicted_scaled.unsigned_abs();
    let scaled = difference
        .checked_mul(1_000_000)
        .ok_or(ScaleError::ArithmeticOverflow {
            field: "scale_residual_ppm",
        })?;
    let value = ceil_ratio_u128(scaled, denominator, "scale_residual_ppm")?;
    u64::try_from(value).map_err(|_| ScaleError::BoundExceeded {
        field: "scale_residual_ppm",
        actual: value,
        maximum: u128::from(u64::MAX),
    })
}

fn witness_uncertainty_ppm(witness: &ScaleWitness) -> Result<u64, ScaleError> {
    uncertainty_ppm(
        witness.uncertainty_micrometers,
        witness.metric_distance_micrometers,
    )
}

fn representative_uncertainty_ppm(
    representative: &GroupRepresentative,
) -> Result<u64, ScaleError> {
    uncertainty_ppm(
        representative.uncertainty_micrometers,
        representative.metric_distance_micrometers,
    )
}

fn uncertainty_ppm(uncertainty: u64, metric: u64) -> Result<u64, ScaleError> {
    let scaled = u128::from(uncertainty)
        .checked_mul(1_000_000)
        .ok_or(ScaleError::ArithmeticOverflow {
            field: "witness_uncertainty_ppm",
        })?;
    let value = ceil_ratio_u128(
        scaled,
        u128::from(metric),
        "witness_uncertainty_ppm",
    )?;
    u64::try_from(value).map_err(|_| ScaleError::BoundExceeded {
        field: "witness_uncertainty_ppm",
        actual: value,
        maximum: u128::from(u64::MAX),
    })
}

fn divide_round_nearest(numerator: i128, denominator: i128) -> Result<i128, ScaleError> {
    if denominator <= 0 {
        return Err(ScaleError::NonPositiveScale);
    }
    let quotient = numerator / denominator;
    let remainder = numerator % denominator;
    let doubled = remainder
        .unsigned_abs()
        .checked_mul(2)
        .ok_or(ScaleError::ArithmeticOverflow {
            field: "metric_rounding",
        })?;
    if doubled < denominator.unsigned_abs() {
        return Ok(quotient);
    }
    quotient
        .checked_add(1)
        .ok_or(ScaleError::ArithmeticOverflow {
            field: "metric_rounding",
        })
}

fn ceil_ratio_u128(
    numerator: u128,
    denominator: u128,
    field: &'static str,
) -> Result<u128, ScaleError> {
    if denominator == 0 {
        return Err(ScaleError::ArithmeticOverflow { field });
    }
    numerator
        .checked_add(denominator.saturating_sub(1))
        .ok_or(ScaleError::ArithmeticOverflow { field })
        .map(|value| value / denominator)
}

fn gcd(mut left: u128, mut right: u128) -> u128 {
    while right != 0 {
        let remainder = left % right;
        left = right;
        right = remainder;
    }
    left.max(1)
}

fn reject_zero_digest(field: &'static str, digest: &EvidenceDigest) -> Result<(), ScaleError> {
    if digest.to_bytes() == [0_u8; 32] {
        Err(ScaleError::ZeroIdentity { field })
    } else {
        Ok(())
    }
}

fn scale_status_text(status: ScaleStatus) -> &'static str {
    match status {
        ScaleStatus::RelativeOnly => "relative_only",
        ScaleStatus::Estimated => "estimated",
        ScaleStatus::Witnessed => "witnessed",
        ScaleStatus::Surveyed => "surveyed",
    }
}

const fn scale_status_code(status: ScaleStatus) -> u8 {
    match status {
        ScaleStatus::RelativeOnly => 0,
        ScaleStatus::Estimated => 1,
        ScaleStatus::Witnessed => 2,
        ScaleStatus::Surveyed => 3,
    }
}

fn put_i128(encoder: &mut Encoder, value: i128) -> Result<(), ScaleError> {
    encoder.put_bytes(&value.to_be_bytes())?;
    Ok(())
}

fn put_u64_sequence(encoder: &mut Encoder, values: &[u64]) -> Result<(), ScaleError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn put_u32_sequence(encoder: &mut Encoder, values: &[u32]) -> Result<(), ScaleError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u32(*value);
    }
    Ok(())
}

fn usize_to_u64(value: usize) -> Result<u64, ScaleError> {
    u64::try_from(value).map_err(|_| ScaleError::ArithmeticOverflow {
        field: "collection_length",
    })
}

fn push_u64_json_array(output: &mut String, values: &[u64]) -> Result<(), ScaleError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}

fn push_u32_json_array(output: &mut String, values: &[u32]) -> Result<(), ScaleError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}

fn json_rendering(error: std::fmt::Error) -> ScaleError {
    ScaleError::JsonRendering(error.to_string())
}

#[cfg(test)]
mod tests {
    use super::{
        ScaleBasis, ScaleError, ScaleFitOptions, ScaleWitness, ScaleWitnessKind, resolve_scale,
    };
    use fdgr_types::{EvidenceDigest, ScaleStatus};

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn basis(byte: u8) -> ScaleBasis {
        ScaleBasis {
            witness_basis_digest: digest(byte),
            geometry_root_digest: digest(20),
            calibration_digest: digest(21),
            scope_digest: digest(22),
            model_generation: 1,
        }
    }

    fn options() -> ScaleFitOptions {
        ScaleFitOptions {
            max_residual_ppm: 5_000,
            min_independent_groups: 2,
            min_witnessed_groups: 2,
            min_surveyed_groups: 2,
        }
    }

    fn witness(
        id: u64,
        kind: ScaleWitnessKind,
        metric: u64,
        relative: u64,
        group: u32,
    ) -> ScaleWitness {
        ScaleWitness {
            witness_id: id,
            evidence_digest: digest(u8::try_from(id).unwrap_or(1)),
            kind,
            metric_distance_micrometers: metric,
            relative_distance_nanounits: relative,
            uncertainty_micrometers: 1_000,
            correlation_group: group,
        }
    }

    #[test]
    fn independent_measured_witnesses_earn_metric_authority() {
        let model = resolve_scale(
            basis(1),
            options(),
            vec![
                witness(1, ScaleWitnessKind::MeasuredBaseline, 1_000_000, 1_000_000_000, 1),
                witness(2, ScaleWitnessKind::Fiducial, 2_000_500, 2_000_000_000, 2),
                witness(3, ScaleWitnessKind::ModelPrior, 2_999_000, 3_000_000_000, 3),
                witness(4, ScaleWitnessKind::MeasuredBaseline, 9_000_000, 4_000_000_000, 4),
            ],
        );
        assert!(matches!(model, Ok(ref value) if value.resolved && value.authority == ScaleStatus::Witnessed && value.conflicting_group_ids == vec![4] && matches!(value.map_relative_distance(1_500_000_000), Ok(ref mapped) if mapped.metric_distance_micrometers > 1_499_000 && mapped.metric_distance_micrometers < 1_502_000) && value.validate().is_ok()));
    }

    #[test]
    fn priors_resolve_estimate_but_cannot_emit_metric_distance() {
        let model = resolve_scale(
            basis(2),
            options(),
            vec![
                witness(1, ScaleWitnessKind::ModelPrior, 1_000_000, 1_000_000_000, 1),
                witness(2, ScaleWitnessKind::TelemetryBaseline, 2_000_000, 2_000_000_000, 2),
            ],
        );
        assert!(matches!(model, Ok(ref value) if value.resolved && value.authority == ScaleStatus::Estimated && matches!(value.map_relative_distance(1_000_000_000), Err(ScaleError::MetricAuthorityRequired { .. }))));
    }

    #[test]
    fn survey_control_requires_independent_clean_groups() {
        let model = resolve_scale(
            basis(3),
            options(),
            vec![
                witness(1, ScaleWitnessKind::SurveyControl, 1_000_000, 1_000_000_000, 1),
                witness(2, ScaleWitnessKind::SurveyControl, 2_000_000, 2_000_000_000, 2),
            ],
        );
        assert!(matches!(model, Ok(ref value) if value.authority == ScaleStatus::Surveyed));
    }

    #[test]
    fn internally_conflicting_group_is_excluded_whole() {
        let model = resolve_scale(
            basis(4),
            options(),
            vec![
                witness(1, ScaleWitnessKind::MeasuredBaseline, 1_000_000, 1_000_000_000, 1),
                witness(2, ScaleWitnessKind::MeasuredBaseline, 10_000_000, 1_000_000_000, 1),
                witness(3, ScaleWitnessKind::Fiducial, 2_000_000, 2_000_000_000, 2),
                witness(4, ScaleWitnessKind::Fiducial, 3_000_000, 3_000_000_000, 3),
            ],
        );
        assert!(matches!(model, Ok(ref value) if value.resolved && value.authority == ScaleStatus::Witnessed && value.inlier_group_ids == vec![2, 3] && value.conflicting_group_ids == vec![1] && value.outlier_witness_ids == vec![1, 2]));
    }

    #[test]
    fn rejected_high_grade_correlated_witness_cannot_elevate_authority() {
        let model = resolve_scale(
            basis(5),
            options(),
            vec![
                witness(1, ScaleWitnessKind::ModelPrior, 1_000_000, 1_000_000_000, 1),
                witness(2, ScaleWitnessKind::SurveyControl, 10_000_000, 1_000_000_000, 1),
                witness(3, ScaleWitnessKind::ModelPrior, 2_000_000, 2_000_000_000, 2),
                witness(4, ScaleWitnessKind::ModelPrior, 3_000_000, 3_000_000_000, 3),
            ],
        );
        assert!(matches!(model, Ok(ref value) if value.resolved && value.authority == ScaleStatus::Estimated && value.conflicting_group_ids == vec![1] && matches!(value.map_relative_distance(1_000_000_000), Err(ScaleError::MetricAuthorityRequired { .. }))));
    }

    #[test]
    fn mutually_conflicting_groups_preserve_relative_only_state() {
        let mut strict = options();
        strict.min_independent_groups = 3;
        let model = resolve_scale(
            basis(6),
            strict,
            vec![
                witness(1, ScaleWitnessKind::MeasuredBaseline, 1_000_000, 1_000_000_000, 1),
                witness(2, ScaleWitnessKind::MeasuredBaseline, 2_000_000, 1_000_000_000, 2),
                witness(3, ScaleWitnessKind::MeasuredBaseline, 3_000_000, 1_000_000_000, 3),
            ],
        );
        assert!(matches!(model, Ok(ref value) if !value.resolved && value.authority == ScaleStatus::RelativeOnly && matches!(value.map_relative_distance(1_000_000_000), Err(ScaleError::MetricAuthorityRequired { .. }))));
    }

    #[test]
    fn input_order_and_basis_control_identity() {
        let witnesses = vec![
            witness(1, ScaleWitnessKind::MeasuredBaseline, 1_000_000, 1_000_000_000, 1),
            witness(2, ScaleWitnessKind::Fiducial, 2_000_000, 2_000_000_000, 2),
        ];
        let mut reversed = witnesses.clone();
        reversed.reverse();
        let left = resolve_scale(basis(7), options(), witnesses);
        let right = resolve_scale(basis(7), options(), reversed);
        let other_basis = resolve_scale(
            basis(8),
            options(),
            vec![
                witness(1, ScaleWitnessKind::MeasuredBaseline, 1_000_000, 1_000_000_000, 1),
                witness(2, ScaleWitnessKind::Fiducial, 2_000_000, 2_000_000_000, 2),
            ],
        );
        assert!(matches!((left, right, other_basis), (Ok(left), Ok(right), Ok(other)) if left == right && left.digest() == right.digest() && left.digest() != other.digest() && matches!(left.to_json(), Ok(ref json) if json.contains("fdgr.scale_model/1"))));
    }
}
