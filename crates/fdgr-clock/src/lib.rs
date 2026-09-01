#![forbid(unsafe_code)]
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::indexing_slicing,
    clippy::module_name_repetitions,
    clippy::needless_pass_by_value,
    clippy::similar_names,
    clippy::struct_excessive_bools,
    clippy::struct_field_names,
    clippy::too_many_arguments,
    clippy::too_many_lines
)]
//! Epoch-aware deterministic clock fitting and mapping for FDGR.
//!
//! Clock mappings are evidence products, not timestamp guesses. Every model names its source and
//! reference domains, timescales, epochs, exact anchor basis, validity interval, residuals,
//! outliers, drift, and uncertainty. Mapping outside witnessed support is refused.

use fdgr_codec::{CodecError, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};
use std::error::Error;
use std::fmt::{self, Display, Formatter, Write as _};

/// Public schema identity for one immutable fitted clock model.
pub const CLOCK_MODEL_SCHEMA: &str = "fdgr.clock_model/1";
/// Public schema identity for an ordered clock-epoch ledger.
pub const CLOCK_EPOCH_LEDGER_SCHEMA: &str = "fdgr.clock_epoch_ledger/1";
/// Maximum synchronization anchors admitted by one reference fit.
pub const MAX_CLOCK_ANCHORS: usize = 256;
/// Maximum UTF-8 bytes in one clock-domain identifier.
pub const MAX_CLOCK_DOMAIN_BYTES: usize = 64;
/// Maximum absolute source or reference tick accepted by the integer reference fit.
pub const MAX_ABS_CLOCK_TICK: i128 = 1_000_000_000_000_000;
/// Maximum source or reference ticks per second.
pub const MAX_CLOCK_TIMESCALE: u64 = 1_000_000_000_000;
/// Maximum configured or derived tick uncertainty.
pub const MAX_CLOCK_UNCERTAINTY_TICKS: u64 = 1_000_000_000_000;
/// Maximum configured drift gate.
pub const MAX_CLOCK_DRIFT_PPM: u64 = 1_000_000;

/// Canonical lower-case clock-domain identifier.
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct ClockDomain(String);

impl ClockDomain {
    /// Parses one domain such as `media_pts` or `host.monotonic`.
    ///
    /// # Errors
    ///
    /// Returns a stable error for empty, oversized, or noncanonical text.
    pub fn parse(value: impl AsRef<str>) -> Result<Self, ClockError> {
        let value = value.as_ref();
        if value.is_empty() {
            return Err(ClockError::EmptyDomain);
        }
        if value.len() > MAX_CLOCK_DOMAIN_BYTES {
            return Err(ClockError::DomainTooLong {
                actual: value.len(),
                maximum: MAX_CLOCK_DOMAIN_BYTES,
            });
        }
        let mut previous_separator = false;
        for (index, byte) in value.bytes().enumerate() {
            let separator = matches!(byte, b'.' | b'_' | b'-');
            let valid = if index == 0 {
                byte.is_ascii_lowercase()
            } else {
                byte.is_ascii_lowercase() || byte.is_ascii_digit() || separator
            };
            if !valid || (separator && previous_separator) {
                return Err(ClockError::NonCanonicalDomain);
            }
            previous_separator = separator;
        }
        if previous_separator {
            return Err(ClockError::NonCanonicalDomain);
        }
        Ok(Self(value.to_owned()))
    }

    /// Returns canonical domain text.
    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl Display for ClockDomain {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.0)
    }
}

/// One synchronization observation connecting source and reference ticks.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ClockAnchor {
    /// Stable nonzero anchor identity within the basis object.
    pub anchor_id: u64,
    /// Source-domain tick.
    pub source_tick: i128,
    /// Reference-domain tick observed for the same event.
    pub reference_tick: i128,
    /// Declared symmetric uncertainty in reference-domain ticks.
    pub uncertainty_ticks: u64,
    /// Nonzero dependence class. Each class receives one robust vote in fitting.
    pub correlation_group: u32,
}

/// Constitutional fitting gates for one model.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ClockFitOptions {
    /// Residual gate before per-anchor uncertainty is added.
    pub max_residual_ticks: u64,
    /// Maximum absolute drift from the nominal timescale ratio.
    pub max_drift_ppm: u64,
    /// Minimum independent groups retained after robust rejection.
    pub min_independent_groups: u16,
}

impl Default for ClockFitOptions {
    fn default() -> Self {
        Self {
            max_residual_ticks: 1_000_000,
            max_drift_ppm: 5_000,
            min_independent_groups: 3,
        }
    }
}

impl ClockFitOptions {
    fn validate(self) -> Result<(), ClockError> {
        if self.max_residual_ticks == 0 {
            return Err(ClockError::ZeroValue {
                field: "max_residual_ticks",
            });
        }
        if self.max_residual_ticks > MAX_CLOCK_UNCERTAINTY_TICKS {
            return Err(ClockError::BoundExceeded {
                field: "max_residual_ticks",
                actual: u128::from(self.max_residual_ticks),
                maximum: u128::from(MAX_CLOCK_UNCERTAINTY_TICKS),
            });
        }
        if self.max_drift_ppm > MAX_CLOCK_DRIFT_PPM {
            return Err(ClockError::BoundExceeded {
                field: "max_drift_ppm",
                actual: u128::from(self.max_drift_ppm),
                maximum: u128::from(MAX_CLOCK_DRIFT_PPM),
            });
        }
        if self.min_independent_groups < 2 {
            return Err(ClockError::MinimumIndependentGroupsTooSmall {
                actual: self.min_independent_groups,
            });
        }
        Ok(())
    }
}

/// Exact immutable basis for one clock-model generation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ClockModelBasis {
    /// Digest of the exact synchronization-anchor object.
    pub basis_digest: EvidenceDigest,
    /// Source clock domain.
    pub source_domain: ClockDomain,
    /// Reference clock domain.
    pub reference_domain: ClockDomain,
    /// Nonzero source epoch.
    pub source_epoch: u64,
    /// Nonzero reference epoch.
    pub reference_epoch: u64,
    /// Nonzero immutable model generation.
    pub model_generation: u64,
    /// Source ticks per second.
    pub source_timescale: u64,
    /// Reference ticks per second.
    pub reference_timescale: u64,
}

/// One residual classification retained by a model.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ClockAnchorResidual {
    /// Stable anchor identity.
    pub anchor_id: u64,
    /// Correlation group used for robust voting.
    pub correlation_group: u32,
    /// Absolute residual in reference-domain ticks.
    pub residual_ticks: u64,
    /// Whether this exact anchor passed the final gate.
    pub inlier: bool,
}

/// Immutable affine model `reference = (source * rate_numerator + offset_numerator) / rate_denominator`.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ClockModel {
    /// Exact model basis.
    pub basis: ClockModelBasis,
    /// Exact fit gates.
    pub options: ClockFitOptions,
    /// Canonically sorted original anchors.
    pub anchors: Vec<ClockAnchor>,
    /// One source-order median representative anchor for each correlation group.
    pub representative_anchor_ids: Vec<u64>,
    /// Positive reduced affine-rate numerator.
    pub rate_numerator: i128,
    /// Positive reduced affine-rate denominator.
    pub rate_denominator: i128,
    /// Offset numerator sharing `rate_denominator`.
    pub offset_numerator: i128,
    /// Inclusive witnessed source support start.
    pub source_support_start: i128,
    /// Inclusive witnessed source support end.
    pub source_support_end: i128,
    /// Minimum reference tick among retained representatives.
    pub reference_support_start: i128,
    /// Maximum reference tick among retained representatives.
    pub reference_support_end: i128,
    /// Drift from the nominal timescale ratio.
    pub drift_ppm: u64,
    /// Maximum retained pairwise-rate spread around the fitted rate.
    pub rate_spread_ppm: u64,
    /// Median absolute residual among retained anchors.
    pub median_abs_residual_ticks: u64,
    /// Maximum absolute residual among retained anchors.
    pub max_abs_residual_ticks: u64,
    /// Conservative base uncertainty before within-support drift growth.
    pub declared_uncertainty_ticks: u64,
    /// Canonically sorted retained anchor identities.
    pub inlier_anchor_ids: Vec<u64>,
    /// Canonically sorted rejected anchor identities.
    pub outlier_anchor_ids: Vec<u64>,
    /// Canonically sorted retained group identities.
    pub inlier_group_ids: Vec<u32>,
    /// Canonically sorted rejected group identities.
    pub outlier_group_ids: Vec<u32>,
    /// Canonically sorted residual records.
    pub residuals: Vec<ClockAnchorResidual>,
}

/// One support-bounded source-to-reference mapping result.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MappedClockTick {
    /// Exact source epoch.
    pub source_epoch: u64,
    /// Exact reference epoch.
    pub reference_epoch: u64,
    /// Source tick supplied by the caller.
    pub source_tick: i128,
    /// Rounded reference tick.
    pub reference_tick: i128,
    /// Conservative symmetric uncertainty in reference ticks.
    pub uncertainty_ticks: u64,
    /// Exact model generation used.
    pub model_generation: u64,
}

/// Explicit gap between successive non-overlapping models in one source epoch.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ClockGap {
    /// Source epoch containing the gap.
    pub source_epoch: u64,
    /// Model generation immediately before the gap.
    pub after_generation: u64,
    /// Model generation immediately after the gap.
    pub before_generation: u64,
    /// Inclusive final supported source tick before the gap.
    pub after_source_tick: i128,
    /// Inclusive first supported source tick after the gap.
    pub before_source_tick: i128,
}

/// Explicit source/reference epoch transition between generations.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ClockEpochTransition {
    /// Model generation before the transition.
    pub after_generation: u64,
    /// Model generation after the transition.
    pub before_generation: u64,
    /// Previous source epoch.
    pub previous_source_epoch: u64,
    /// New source epoch.
    pub new_source_epoch: u64,
    /// Previous reference epoch.
    pub previous_reference_epoch: u64,
    /// New reference epoch.
    pub new_reference_epoch: u64,
}

/// Ordered model ledger with explicit same-epoch gaps and epoch transitions.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ClockEpochLedger {
    /// Source domain shared by every model.
    pub source_domain: ClockDomain,
    /// Reference domain shared by every model.
    pub reference_domain: ClockDomain,
    /// Source ticks per second.
    pub source_timescale: u64,
    /// Reference ticks per second.
    pub reference_timescale: u64,
    /// Consecutive immutable model generations.
    pub models: Vec<ClockModel>,
    /// Explicit same-epoch support gaps.
    pub gaps: Vec<ClockGap>,
    /// Explicit epoch transitions.
    pub epoch_transitions: Vec<ClockEpochTransition>,
}

/// Stable failures for clock fitting, validation, mapping, and ledger replay.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ClockError {
    /// A domain identifier was empty.
    EmptyDomain,
    /// A domain identifier exceeded its hard byte bound.
    DomainTooLong {
        /// Observed bytes.
        actual: usize,
        /// Maximum bytes.
        maximum: usize,
    },
    /// A domain identifier was noncanonical.
    NonCanonicalDomain,
    /// Source and reference domains were identical.
    SameDomain,
    /// A mandatory digest was all zero.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// A mandatory numeric field was zero.
    ZeroValue {
        /// Stable field name.
        field: &'static str,
    },
    /// A numeric field exceeded a constitutional bound.
    BoundExceeded {
        /// Stable field name.
        field: &'static str,
        /// Observed value.
        actual: u128,
        /// Maximum value.
        maximum: u128,
    },
    /// A fit requested fewer than two independent groups.
    MinimumIndependentGroupsTooSmall {
        /// Requested minimum.
        actual: u16,
    },
    /// Too few anchors were supplied.
    TooFewAnchors {
        /// Observed anchor count.
        actual: usize,
        /// Required count.
        minimum: usize,
    },
    /// Too many anchors were supplied.
    TooManyAnchors {
        /// Observed anchor count.
        actual: usize,
        /// Maximum count.
        maximum: usize,
    },
    /// Two anchors used the same identity.
    DuplicateAnchorId {
        /// Duplicate identity.
        anchor_id: u64,
    },
    /// Too few independent groups existed.
    InsufficientIndependentGroups {
        /// Observed groups.
        actual: usize,
        /// Required groups.
        minimum: usize,
    },
    /// Independent groups did not contain two distinct source ticks.
    NoDistinctSourceTicks,
    /// The robust rate was zero or negative.
    NonPositiveRate,
    /// Fitted drift exceeded the configured gate.
    DriftExceeded {
        /// Observed drift.
        observed_ppm: u64,
        /// Maximum drift.
        maximum_ppm: u64,
    },
    /// Robust residual classification retained too few groups.
    ResidualGateFailed {
        /// Retained groups.
        inlier_groups: usize,
        /// Required groups.
        minimum_groups: usize,
    },
    /// Checked arithmetic overflowed.
    ArithmeticOverflow {
        /// Stable field or operation.
        field: &'static str,
    },
    /// A mapping request lay outside witnessed support.
    OutsideSupport {
        /// Source epoch.
        source_epoch: u64,
        /// Requested source tick.
        source_tick: i128,
        /// Inclusive support start.
        support_start: i128,
        /// Inclusive support end.
        support_end: i128,
    },
    /// A model did not match the ledger domain.
    DomainMismatch {
        /// Stable field name.
        field: &'static str,
        /// Expected domain.
        expected: String,
        /// Observed domain.
        observed: String,
    },
    /// A model did not match the ledger timescale.
    TimescaleMismatch {
        /// Stable field name.
        field: &'static str,
        /// Expected timescale.
        expected: u64,
        /// Observed timescale.
        observed: u64,
    },
    /// An epoch regressed.
    EpochRegression {
        /// Stable epoch field.
        field: &'static str,
        /// Previous epoch.
        previous: u64,
        /// Observed epoch.
        observed: u64,
    },
    /// Generations were not consecutive.
    GenerationNotConsecutive {
        /// Required next generation.
        expected: u64,
        /// Observed generation.
        observed: u64,
    },
    /// Two models overlapped in one source epoch.
    OverlappingSupport {
        /// Source epoch.
        source_epoch: u64,
        /// Previous support end.
        previous_end: i128,
        /// New support start.
        new_start: i128,
    },
    /// A ledger contained no models.
    EmptyLedger,
    /// No model covered an epoch/tick pair.
    UnmappedTick {
        /// Source epoch.
        source_epoch: u64,
        /// Source tick.
        source_tick: i128,
    },
    /// A deterministic derived field disagreed with a rebuild.
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

impl Display for ClockError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::EmptyDomain => formatter.write_str("clock domain must not be empty"),
            Self::DomainTooLong { actual, maximum } => {
                write!(formatter, "clock domain is {actual} bytes; maximum is {maximum}")
            }
            Self::NonCanonicalDomain => formatter.write_str("clock domain is not canonical"),
            Self::SameDomain => formatter.write_str("source and reference clock domains must differ"),
            Self::ZeroIdentity { field } => write!(formatter, "clock identity {field} must not be all zero"),
            Self::ZeroValue { field } => write!(formatter, "clock field {field} must be nonzero"),
            Self::BoundExceeded { field, actual, maximum } => {
                write!(formatter, "clock field {field} is {actual}; maximum is {maximum}")
            }
            Self::MinimumIndependentGroupsTooSmall { actual } => write!(formatter, "clock fit minimum independent groups is {actual}; at least 2 are required"),
            Self::TooFewAnchors { actual, minimum } => write!(formatter, "clock fit received {actual} anchors; at least {minimum} are required"),
            Self::TooManyAnchors { actual, maximum } => write!(formatter, "clock fit received {actual} anchors; maximum is {maximum}"),
            Self::DuplicateAnchorId { anchor_id } => write!(formatter, "clock anchor identity {anchor_id} is duplicated"),
            Self::InsufficientIndependentGroups { actual, minimum } => write!(formatter, "clock fit contains {actual} independent groups; at least {minimum} are required"),
            Self::NoDistinctSourceTicks => formatter.write_str("clock fit needs at least two distinct source ticks"),
            Self::NonPositiveRate => formatter.write_str("clock fit produced a nonpositive rate"),
            Self::DriftExceeded { observed_ppm, maximum_ppm } => write!(formatter, "clock drift is {observed_ppm} ppm; maximum is {maximum_ppm} ppm"),
            Self::ResidualGateFailed { inlier_groups, minimum_groups } => write!(formatter, "clock residual gate retained {inlier_groups} groups; at least {minimum_groups} are required"),
            Self::ArithmeticOverflow { field } => write!(formatter, "clock arithmetic overflowed while computing {field}"),
            Self::OutsideSupport { source_epoch, source_tick, support_start, support_end } => write!(formatter, "clock tick {source_tick} in epoch {source_epoch} is outside support [{support_start}, {support_end}]"),
            Self::DomainMismatch { field, expected, observed } => write!(formatter, "clock ledger domain {field} mismatch: expected {expected:?}, observed {observed:?}"),
            Self::TimescaleMismatch { field, expected, observed } => write!(formatter, "clock ledger timescale {field} mismatch: expected {expected}, observed {observed}"),
            Self::EpochRegression { field, previous, observed } => write!(formatter, "clock epoch {field} regressed from {previous} to {observed}"),
            Self::GenerationNotConsecutive { expected, observed } => write!(formatter, "clock generation is {observed}; expected {expected}"),
            Self::OverlappingSupport { source_epoch, previous_end, new_start } => write!(formatter, "clock support overlaps in epoch {source_epoch}: previous end {previous_end}, new start {new_start}"),
            Self::EmptyLedger => formatter.write_str("clock epoch ledger must contain at least one model"),
            Self::UnmappedTick { source_epoch, source_tick } => write!(formatter, "no clock model covers epoch {source_epoch} tick {source_tick}"),
            Self::DerivedMismatch { field } => write!(formatter, "clock derived field {field} disagrees with deterministic rebuild"),
            Self::Codec(error) => write!(formatter, "clock codec error: {error}"),
            Self::Domain(error) => write!(formatter, "clock identity-domain error: {error}"),
            Self::JsonRendering(error) => write!(formatter, "clock JSON rendering failed: {error}"),
        }
    }
}

impl Error for ClockError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for ClockError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for ClockError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct Rational {
    numerator: i128,
    denominator: i128,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct GroupRepresentative {
    anchor_id: u64,
    source_tick: i128,
    reference_tick: i128,
    uncertainty_ticks: u64,
    correlation_group: u32,
}

/// Fits one deterministic robust affine clock model.
///
/// Correlated observations are collapsed to one source-order median representative per group.
/// Pairwise representative rates define a deterministic median rate; representative offsets define
/// a deterministic median offset. One residual-rejection pass then refits from retained groups.
///
/// # Errors
///
/// Returns a stable basis, anchor, arithmetic, drift, or residual-gate error.
pub fn fit_clock_model(
    basis: ClockModelBasis,
    options: ClockFitOptions,
    mut anchors: Vec<ClockAnchor>,
) -> Result<ClockModel, ClockError> {
    validate_basis(&basis)?;
    options.validate()?;
    validate_anchors(&anchors)?;
    anchors.sort_by(|left, right| left.anchor_id.cmp(&right.anchor_id));
    let representatives = group_representatives(&anchors)?;
    let minimum_groups = usize::from(options.min_independent_groups);
    if representatives.len() < minimum_groups {
        return Err(ClockError::InsufficientIndependentGroups {
            actual: representatives.len(),
            minimum: minimum_groups,
        });
    }
    let initial_rate = median_rate(&representatives)?;
    ensure_positive_rate(initial_rate)?;
    let initial_offset = median_offset(&representatives, initial_rate)?;
    let initial_groups = classify_groups(
        &representatives,
        initial_rate,
        initial_offset,
        options.max_residual_ticks,
    )?;
    if initial_groups.len() < minimum_groups {
        return Err(ClockError::ResidualGateFailed {
            inlier_groups: initial_groups.len(),
            minimum_groups,
        });
    }
    let retained = representatives
        .iter()
        .filter(|representative| initial_groups.contains(&representative.correlation_group))
        .cloned()
        .collect::<Vec<_>>();
    let final_rate = median_rate(&retained)?;
    ensure_positive_rate(final_rate)?;
    let final_offset = median_offset(&retained, final_rate)?;
    let final_groups = classify_groups(
        &representatives,
        final_rate,
        final_offset,
        options.max_residual_ticks,
    )?;
    if final_groups.len() < minimum_groups {
        return Err(ClockError::ResidualGateFailed {
            inlier_groups: final_groups.len(),
            minimum_groups,
        });
    }
    let final_retained = representatives
        .iter()
        .filter(|representative| final_groups.contains(&representative.correlation_group))
        .cloned()
        .collect::<Vec<_>>();
    let drift_ppm = nominal_drift_ppm(
        final_rate,
        basis.source_timescale,
        basis.reference_timescale,
    )?;
    if drift_ppm > options.max_drift_ppm {
        return Err(ClockError::DriftExceeded {
            observed_ppm: drift_ppm,
            maximum_ppm: options.max_drift_ppm,
        });
    }
    let rate_spread_ppm = retained_rate_spread_ppm(&final_retained, final_rate)?;
    let all_groups = representatives
        .iter()
        .map(|representative| representative.correlation_group)
        .collect::<BTreeSet<_>>();
    let inlier_group_ids = final_groups.iter().copied().collect::<Vec<_>>();
    let outlier_group_ids = all_groups.difference(&final_groups).copied().collect::<Vec<_>>();
    let representative_anchor_ids = representatives
        .iter()
        .map(|representative| representative.anchor_id)
        .collect::<Vec<_>>();
    let mut residuals = Vec::with_capacity(anchors.len());
    let mut inlier_anchor_ids = Vec::new();
    let mut outlier_anchor_ids = Vec::new();
    let mut retained_residuals = Vec::new();
    let mut maximum_inlier_uncertainty = 0_u64;
    for anchor in &anchors {
        let predicted = predict(final_rate, final_offset, anchor.source_tick)?;
        let residual_ticks = absolute_difference_u64(predicted, anchor.reference_tick)?;
        let anchor_threshold = options
            .max_residual_ticks
            .checked_add(anchor.uncertainty_ticks)
            .ok_or(ClockError::ArithmeticOverflow {
                field: "anchor_residual_threshold",
            })?;
        let inlier = final_groups.contains(&anchor.correlation_group)
            && residual_ticks <= anchor_threshold;
        if inlier {
            inlier_anchor_ids.push(anchor.anchor_id);
            retained_residuals.push(residual_ticks);
            maximum_inlier_uncertainty = maximum_inlier_uncertainty.max(anchor.uncertainty_ticks);
        } else {
            outlier_anchor_ids.push(anchor.anchor_id);
        }
        residuals.push(ClockAnchorResidual {
            anchor_id: anchor.anchor_id,
            correlation_group: anchor.correlation_group,
            residual_ticks,
            inlier,
        });
    }
    retained_residuals.sort_unstable();
    let median_abs_residual_ticks = lower_median_u64(&retained_residuals)?;
    let max_abs_residual_ticks = retained_residuals
        .iter()
        .copied()
        .max()
        .ok_or(ClockError::ResidualGateFailed {
            inlier_groups: 0,
            minimum_groups,
        })?;
    let declared_uncertainty_ticks = max_abs_residual_ticks
        .checked_add(maximum_inlier_uncertainty)
        .ok_or(ClockError::ArithmeticOverflow {
            field: "declared_uncertainty_ticks",
        })?;
    if declared_uncertainty_ticks > MAX_CLOCK_UNCERTAINTY_TICKS {
        return Err(ClockError::BoundExceeded {
            field: "declared_uncertainty_ticks",
            actual: u128::from(declared_uncertainty_ticks),
            maximum: u128::from(MAX_CLOCK_UNCERTAINTY_TICKS),
        });
    }
    let (source_support_start, source_support_end) = source_support(&final_retained)?;
    let (reference_support_start, reference_support_end) = reference_support(&final_retained)?;
    Ok(ClockModel {
        basis,
        options,
        anchors,
        representative_anchor_ids,
        rate_numerator: final_rate.numerator,
        rate_denominator: final_rate.denominator,
        offset_numerator: final_offset,
        source_support_start,
        source_support_end,
        reference_support_start,
        reference_support_end,
        drift_ppm,
        rate_spread_ppm,
        median_abs_residual_ticks,
        max_abs_residual_ticks,
        declared_uncertainty_ticks,
        inlier_anchor_ids,
        outlier_anchor_ids,
        inlier_group_ids,
        outlier_group_ids,
        residuals,
    })
}

impl ClockModel {
    /// Rebuilds the model and compares every deterministic field.
    ///
    /// # Errors
    ///
    /// Returns a fit failure or derived-field mismatch.
    pub fn validate(&self) -> Result<(), ClockError> {
        let rebuilt = fit_clock_model(
            self.basis.clone(),
            self.options,
            self.anchors.clone(),
        )?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(ClockError::DerivedMismatch {
                field: "clock_model",
            })
        }
    }

    /// Maps a source tick only inside witnessed support.
    ///
    /// # Errors
    ///
    /// Returns a validation, support, or arithmetic error.
    pub fn map_tick(&self, source_tick: i128) -> Result<MappedClockTick, ClockError> {
        self.validate()?;
        if source_tick < self.source_support_start || source_tick > self.source_support_end {
            return Err(ClockError::OutsideSupport {
                source_epoch: self.basis.source_epoch,
                source_tick,
                support_start: self.source_support_start,
                support_end: self.source_support_end,
            });
        }
        let reference_tick = predict(
            Rational {
                numerator: self.rate_numerator,
                denominator: self.rate_denominator,
            },
            self.offset_numerator,
            source_tick,
        )?;
        Ok(MappedClockTick {
            source_epoch: self.basis.source_epoch,
            reference_epoch: self.basis.reference_epoch,
            source_tick,
            reference_tick,
            uncertainty_ticks: mapping_uncertainty(self, source_tick)?,
            model_generation: self.basis.model_generation,
        })
    }

    /// Returns deterministic canonical model bytes.
    ///
    /// # Errors
    ///
    /// Returns a validation or codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, ClockError> {
        self.validate()?;
        let capacity = 1024_usize.saturating_add(self.anchors.len().saturating_mul(128));
        let mut encoder = Encoder::with_capacity(capacity);
        encoder.put_str(CLOCK_MODEL_SCHEMA)?;
        encoder.put_digest(&self.basis.basis_digest);
        encoder.put_str(self.basis.source_domain.as_str())?;
        encoder.put_str(self.basis.reference_domain.as_str())?;
        encoder.put_u64(self.basis.source_epoch);
        encoder.put_u64(self.basis.reference_epoch);
        encoder.put_u64(self.basis.model_generation);
        encoder.put_u64(self.basis.source_timescale);
        encoder.put_u64(self.basis.reference_timescale);
        encoder.put_u64(self.options.max_residual_ticks);
        encoder.put_u64(self.options.max_drift_ppm);
        encoder.put_u16(self.options.min_independent_groups);
        put_i128(&mut encoder, self.rate_numerator)?;
        put_i128(&mut encoder, self.rate_denominator)?;
        put_i128(&mut encoder, self.offset_numerator)?;
        put_i128(&mut encoder, self.source_support_start)?;
        put_i128(&mut encoder, self.source_support_end)?;
        put_i128(&mut encoder, self.reference_support_start)?;
        put_i128(&mut encoder, self.reference_support_end)?;
        encoder.put_u64(self.drift_ppm);
        encoder.put_u64(self.rate_spread_ppm);
        encoder.put_u64(self.median_abs_residual_ticks);
        encoder.put_u64(self.max_abs_residual_ticks);
        encoder.put_u64(self.declared_uncertainty_ticks);
        put_u64_sequence(&mut encoder, &self.representative_anchor_ids)?;
        put_u64_sequence(&mut encoder, &self.inlier_anchor_ids)?;
        put_u64_sequence(&mut encoder, &self.outlier_anchor_ids)?;
        put_u32_sequence(&mut encoder, &self.inlier_group_ids)?;
        put_u32_sequence(&mut encoder, &self.outlier_group_ids)?;
        encoder.put_u64(usize_to_u64(self.anchors.len())?);
        for anchor in &self.anchors {
            encoder.put_u64(anchor.anchor_id);
            put_i128(&mut encoder, anchor.source_tick)?;
            put_i128(&mut encoder, anchor.reference_tick)?;
            encoder.put_u64(anchor.uncertainty_ticks);
            encoder.put_u32(anchor.correlation_group);
        }
        encoder.put_u64(usize_to_u64(self.residuals.len())?);
        for residual in &self.residuals {
            encoder.put_u64(residual.anchor_id);
            encoder.put_u32(residual.correlation_group);
            encoder.put_u64(residual.residual_ticks);
            encoder.put_bool(residual.inlier);
        }
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated model identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, ClockError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(CLOCK_MODEL_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }

    /// Renders deterministic field-ordered JSON.
    ///
    /// Signed 128-bit values are decimal strings to preserve exactness in generic JSON clients.
    ///
    /// # Errors
    ///
    /// Returns a validation, identity, or formatting error.
    pub fn to_json(&self) -> Result<String, ClockError> {
        let digest = self.digest()?;
        let mut output = format!(
            "{{\"schema\":\"{CLOCK_MODEL_SCHEMA}\",\"model_digest\":\"{digest}\",\"basis_digest\":\"{}\",\"source_domain\":\"{}\",\"reference_domain\":\"{}\",\"source_epoch\":{},\"reference_epoch\":{},\"model_generation\":{},\"source_timescale\":{},\"reference_timescale\":{},\"max_residual_ticks\":{},\"max_drift_ppm\":{},\"min_independent_groups\":{},\"rate_numerator\":\"{}\",\"rate_denominator\":\"{}\",\"offset_numerator\":\"{}\",\"source_support_start_ticks\":\"{}\",\"source_support_end_ticks\":\"{}\",\"reference_support_start_ticks\":\"{}\",\"reference_support_end_ticks\":\"{}\",\"drift_ppm\":{},\"rate_spread_ppm\":{},\"median_abs_residual_ticks\":{},\"max_abs_residual_ticks\":{},\"declared_uncertainty_ticks\":{},\"inlier_anchor_ids\":[",
            self.basis.basis_digest,
            self.basis.source_domain,
            self.basis.reference_domain,
            self.basis.source_epoch,
            self.basis.reference_epoch,
            self.basis.model_generation,
            self.basis.source_timescale,
            self.basis.reference_timescale,
            self.options.max_residual_ticks,
            self.options.max_drift_ppm,
            self.options.min_independent_groups,
            self.rate_numerator,
            self.rate_denominator,
            self.offset_numerator,
            self.source_support_start,
            self.source_support_end,
            self.reference_support_start,
            self.reference_support_end,
            self.drift_ppm,
            self.rate_spread_ppm,
            self.median_abs_residual_ticks,
            self.max_abs_residual_ticks,
            self.declared_uncertainty_ticks,
        );
        push_u64_json_array(&mut output, &self.inlier_anchor_ids)?;
        output.push_str("],\"outlier_anchor_ids\":[");
        push_u64_json_array(&mut output, &self.outlier_anchor_ids)?;
        output.push_str("],\"inlier_group_ids\":[");
        push_u32_json_array(&mut output, &self.inlier_group_ids)?;
        output.push_str("],\"outlier_group_ids\":[");
        push_u32_json_array(&mut output, &self.outlier_group_ids)?;
        output.push_str("],\"residuals\":[");
        for (position, residual) in self.residuals.iter().enumerate() {
            if position > 0 {
                output.push(',');
            }
            write!(output, "{{\"anchor_id\":{},\"correlation_group\":{},\"residual_ticks\":{},\"inlier\":{}}}", residual.anchor_id, residual.correlation_group, residual.residual_ticks, residual.inlier).map_err(json_rendering)?;
        }
        output.push_str("]}");
        Ok(output)
    }
}

impl ClockEpochLedger {
    /// Creates a ledger from its first validated model.
    ///
    /// # Errors
    ///
    /// Returns a model-validation error.
    pub fn new(first: ClockModel) -> Result<Self, ClockError> {
        first.validate()?;
        Ok(Self {
            source_domain: first.basis.source_domain.clone(),
            reference_domain: first.basis.reference_domain.clone(),
            source_timescale: first.basis.source_timescale,
            reference_timescale: first.basis.reference_timescale,
            models: vec![first],
            gaps: Vec::new(),
            epoch_transitions: Vec::new(),
        })
    }

    /// Builds a ledger from model generations in append order.
    ///
    /// # Errors
    ///
    /// Returns an empty-ledger, model, ordering, overlap, or epoch error.
    pub fn from_models(models: Vec<ClockModel>) -> Result<Self, ClockError> {
        let mut iterator = models.into_iter();
        let first = iterator.next().ok_or(ClockError::EmptyLedger)?;
        let mut ledger = Self::new(first)?;
        for model in iterator {
            ledger.append(model)?;
        }
        Ok(ledger)
    }

    /// Appends one exact consecutive generation.
    ///
    /// # Errors
    ///
    /// Returns a model, domain, timescale, generation, epoch, or support-overlap error.
    pub fn append(&mut self, model: ClockModel) -> Result<(), ClockError> {
        model.validate()?;
        compare_domain("source_domain", self.source_domain.as_str(), model.basis.source_domain.as_str())?;
        compare_domain("reference_domain", self.reference_domain.as_str(), model.basis.reference_domain.as_str())?;
        compare_timescale("source_timescale", self.source_timescale, model.basis.source_timescale)?;
        compare_timescale("reference_timescale", self.reference_timescale, model.basis.reference_timescale)?;
        let previous = self.models.last().ok_or(ClockError::EmptyLedger)?;
        let expected_generation = previous
            .basis
            .model_generation
            .checked_add(1)
            .ok_or(ClockError::ArithmeticOverflow {
                field: "model_generation",
            })?;
        if model.basis.model_generation != expected_generation {
            return Err(ClockError::GenerationNotConsecutive {
                expected: expected_generation,
                observed: model.basis.model_generation,
            });
        }
        if model.basis.source_epoch < previous.basis.source_epoch {
            return Err(ClockError::EpochRegression {
                field: "source_epoch",
                previous: previous.basis.source_epoch,
                observed: model.basis.source_epoch,
            });
        }
        if model.basis.reference_epoch < previous.basis.reference_epoch {
            return Err(ClockError::EpochRegression {
                field: "reference_epoch",
                previous: previous.basis.reference_epoch,
                observed: model.basis.reference_epoch,
            });
        }
        if model.basis.source_epoch == previous.basis.source_epoch {
            if model.source_support_start <= previous.source_support_end {
                return Err(ClockError::OverlappingSupport {
                    source_epoch: model.basis.source_epoch,
                    previous_end: previous.source_support_end,
                    new_start: model.source_support_start,
                });
            }
            self.gaps.push(ClockGap {
                source_epoch: model.basis.source_epoch,
                after_generation: previous.basis.model_generation,
                before_generation: model.basis.model_generation,
                after_source_tick: previous.source_support_end,
                before_source_tick: model.source_support_start,
            });
        }
        if model.basis.source_epoch != previous.basis.source_epoch
            || model.basis.reference_epoch != previous.basis.reference_epoch
        {
            self.epoch_transitions.push(ClockEpochTransition {
                after_generation: previous.basis.model_generation,
                before_generation: model.basis.model_generation,
                previous_source_epoch: previous.basis.source_epoch,
                new_source_epoch: model.basis.source_epoch,
                previous_reference_epoch: previous.basis.reference_epoch,
                new_reference_epoch: model.basis.reference_epoch,
            });
        }
        self.models.push(model);
        Ok(())
    }

    /// Replays and validates all derived ledger state.
    ///
    /// # Errors
    ///
    /// Returns a model, ordering, epoch, or derived-field error.
    pub fn validate(&self) -> Result<(), ClockError> {
        let rebuilt = Self::from_models(self.models.clone())?;
        if self == &rebuilt {
            Ok(())
        } else {
            Err(ClockError::DerivedMismatch {
                field: "clock_epoch_ledger",
            })
        }
    }

    /// Maps through the unique model covering an exact epoch/tick pair.
    ///
    /// # Errors
    ///
    /// Returns a ledger/model validation error or [`ClockError::UnmappedTick`].
    pub fn map_tick(&self, source_epoch: u64, source_tick: i128) -> Result<MappedClockTick, ClockError> {
        self.validate()?;
        for model in &self.models {
            if model.basis.source_epoch == source_epoch
                && source_tick >= model.source_support_start
                && source_tick <= model.source_support_end
            {
                return model.map_tick(source_tick);
            }
        }
        Err(ClockError::UnmappedTick {
            source_epoch,
            source_tick,
        })
    }

    /// Returns deterministic canonical ledger bytes.
    ///
    /// # Errors
    ///
    /// Returns a validation, model-identity, or codec error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, ClockError> {
        self.validate()?;
        let mut encoder = Encoder::with_capacity(512_usize.saturating_add(self.models.len().saturating_mul(64)));
        encoder.put_str(CLOCK_EPOCH_LEDGER_SCHEMA)?;
        encoder.put_str(self.source_domain.as_str())?;
        encoder.put_str(self.reference_domain.as_str())?;
        encoder.put_u64(self.source_timescale);
        encoder.put_u64(self.reference_timescale);
        encoder.put_u64(usize_to_u64(self.models.len())?);
        for model in &self.models {
            encoder.put_digest(&model.digest()?);
        }
        encoder.put_u64(usize_to_u64(self.gaps.len())?);
        for gap in &self.gaps {
            encoder.put_u64(gap.source_epoch);
            encoder.put_u64(gap.after_generation);
            encoder.put_u64(gap.before_generation);
            put_i128(&mut encoder, gap.after_source_tick)?;
            put_i128(&mut encoder, gap.before_source_tick)?;
        }
        encoder.put_u64(usize_to_u64(self.epoch_transitions.len())?);
        for transition in &self.epoch_transitions {
            encoder.put_u64(transition.after_generation);
            encoder.put_u64(transition.before_generation);
            encoder.put_u64(transition.previous_source_epoch);
            encoder.put_u64(transition.new_source_epoch);
            encoder.put_u64(transition.previous_reference_epoch);
            encoder.put_u64(transition.new_reference_epoch);
        }
        Ok(encoder.into_bytes())
    }

    /// Computes the domain-separated ledger identity.
    ///
    /// # Errors
    ///
    /// Returns a validation, model-identity, domain, codec, or hashing error.
    pub fn digest(&self) -> Result<EvidenceDigest, ClockError> {
        let bytes = self.to_canonical_bytes()?;
        let domain = DigestDomain::parse(CLOCK_EPOCH_LEDGER_SCHEMA)?;
        Ok(hash_domain(&domain, &bytes)?)
    }
}

fn validate_basis(basis: &ClockModelBasis) -> Result<(), ClockError> {
    if basis.basis_digest.to_bytes() == [0_u8; 32] {
        return Err(ClockError::ZeroIdentity {
            field: "basis_digest",
        });
    }
    if basis.source_domain == basis.reference_domain {
        return Err(ClockError::SameDomain);
    }
    for (field, value) in [
        ("source_epoch", basis.source_epoch),
        ("reference_epoch", basis.reference_epoch),
        ("model_generation", basis.model_generation),
        ("source_timescale", basis.source_timescale),
        ("reference_timescale", basis.reference_timescale),
    ] {
        if value == 0 {
            return Err(ClockError::ZeroValue { field });
        }
    }
    for (field, value) in [
        ("source_timescale", basis.source_timescale),
        ("reference_timescale", basis.reference_timescale),
    ] {
        if value > MAX_CLOCK_TIMESCALE {
            return Err(ClockError::BoundExceeded {
                field,
                actual: u128::from(value),
                maximum: u128::from(MAX_CLOCK_TIMESCALE),
            });
        }
    }
    Ok(())
}

fn validate_anchors(anchors: &[ClockAnchor]) -> Result<(), ClockError> {
    if anchors.len() < 2 {
        return Err(ClockError::TooFewAnchors {
            actual: anchors.len(),
            minimum: 2,
        });
    }
    if anchors.len() > MAX_CLOCK_ANCHORS {
        return Err(ClockError::TooManyAnchors {
            actual: anchors.len(),
            maximum: MAX_CLOCK_ANCHORS,
        });
    }
    let mut identities = BTreeSet::new();
    for anchor in anchors {
        if anchor.anchor_id == 0 {
            return Err(ClockError::ZeroValue { field: "anchor_id" });
        }
        if !identities.insert(anchor.anchor_id) {
            return Err(ClockError::DuplicateAnchorId {
                anchor_id: anchor.anchor_id,
            });
        }
        if anchor.correlation_group == 0 {
            return Err(ClockError::ZeroValue {
                field: "correlation_group",
            });
        }
        validate_tick("source_tick", anchor.source_tick)?;
        validate_tick("reference_tick", anchor.reference_tick)?;
        if anchor.uncertainty_ticks > MAX_CLOCK_UNCERTAINTY_TICKS {
            return Err(ClockError::BoundExceeded {
                field: "anchor_uncertainty_ticks",
                actual: u128::from(anchor.uncertainty_ticks),
                maximum: u128::from(MAX_CLOCK_UNCERTAINTY_TICKS),
            });
        }
    }
    Ok(())
}

fn validate_tick(field: &'static str, tick: i128) -> Result<(), ClockError> {
    let magnitude = tick.unsigned_abs();
    let maximum = MAX_ABS_CLOCK_TICK.unsigned_abs();
    if magnitude > maximum {
        Err(ClockError::BoundExceeded {
            field,
            actual: magnitude,
            maximum,
        })
    } else {
        Ok(())
    }
}

fn group_representatives(anchors: &[ClockAnchor]) -> Result<Vec<GroupRepresentative>, ClockError> {
    let mut groups: BTreeMap<u32, Vec<ClockAnchor>> = BTreeMap::new();
    for anchor in anchors {
        groups.entry(anchor.correlation_group).or_default().push(anchor.clone());
    }
    let mut output = Vec::with_capacity(groups.len());
    for (group, mut members) in groups {
        members.sort_by(|left, right| {
            left.source_tick
                .cmp(&right.source_tick)
                .then(left.reference_tick.cmp(&right.reference_tick))
                .then(left.anchor_id.cmp(&right.anchor_id))
        });
        let median_index = members.len().saturating_sub(1) / 2;
        let selected = members.get(median_index).ok_or(ClockError::InsufficientIndependentGroups {
            actual: output.len(),
            minimum: 2,
        })?;
        let uncertainty_ticks = members
            .iter()
            .map(|anchor| anchor.uncertainty_ticks)
            .max()
            .unwrap_or(0);
        output.push(GroupRepresentative {
            anchor_id: selected.anchor_id,
            source_tick: selected.source_tick,
            reference_tick: selected.reference_tick,
            uncertainty_ticks,
            correlation_group: group,
        });
    }
    output.sort_by(|left, right| {
        left.source_tick
            .cmp(&right.source_tick)
            .then(left.reference_tick.cmp(&right.reference_tick))
            .then(left.anchor_id.cmp(&right.anchor_id))
    });
    Ok(output)
}

fn median_rate(representatives: &[GroupRepresentative]) -> Result<Rational, ClockError> {
    let mut rates = Vec::new();
    for (position, left) in representatives.iter().enumerate() {
        for right in representatives.iter().skip(position.saturating_add(1)) {
            let source_delta = right.source_tick.checked_sub(left.source_tick).ok_or(ClockError::ArithmeticOverflow {
                field: "source_delta",
            })?;
            if source_delta == 0 {
                continue;
            }
            let reference_delta = right.reference_tick.checked_sub(left.reference_tick).ok_or(ClockError::ArithmeticOverflow {
                field: "reference_delta",
            })?;
            rates.push(reduce_ratio(reference_delta, source_delta)?);
        }
    }
    if rates.is_empty() {
        return Err(ClockError::NoDistinctSourceTicks);
    }
    rates.sort_by(compare_rational);
    rates
        .get(rates.len().saturating_sub(1) / 2)
        .copied()
        .ok_or(ClockError::NoDistinctSourceTicks)
}

fn median_offset(representatives: &[GroupRepresentative], rate: Rational) -> Result<i128, ClockError> {
    let mut offsets = Vec::with_capacity(representatives.len());
    for representative in representatives {
        let reference_scaled = representative.reference_tick.checked_mul(rate.denominator).ok_or(ClockError::ArithmeticOverflow {
            field: "reference_scaled",
        })?;
        let source_scaled = representative.source_tick.checked_mul(rate.numerator).ok_or(ClockError::ArithmeticOverflow {
            field: "source_scaled",
        })?;
        offsets.push(reference_scaled.checked_sub(source_scaled).ok_or(ClockError::ArithmeticOverflow {
            field: "offset_numerator",
        })?);
    }
    offsets.sort_unstable();
    offsets
        .get(offsets.len().saturating_sub(1) / 2)
        .copied()
        .ok_or(ClockError::TooFewAnchors {
            actual: 0,
            minimum: 1,
        })
}

fn classify_groups(
    representatives: &[GroupRepresentative],
    rate: Rational,
    offset: i128,
    max_residual_ticks: u64,
) -> Result<BTreeSet<u32>, ClockError> {
    let mut output = BTreeSet::new();
    for representative in representatives {
        let residual = absolute_difference_u64(
            predict(rate, offset, representative.source_tick)?,
            representative.reference_tick,
        )?;
        let threshold = max_residual_ticks.checked_add(representative.uncertainty_ticks).ok_or(ClockError::ArithmeticOverflow {
            field: "residual_threshold",
        })?;
        if residual <= threshold {
            output.insert(representative.correlation_group);
        }
    }
    Ok(output)
}

fn ensure_positive_rate(rate: Rational) -> Result<(), ClockError> {
    if rate.numerator <= 0 || rate.denominator <= 0 {
        Err(ClockError::NonPositiveRate)
    } else {
        Ok(())
    }
}

fn reduce_ratio(numerator: i128, denominator: i128) -> Result<Rational, ClockError> {
    if denominator <= 0 {
        return Err(ClockError::ArithmeticOverflow {
            field: "rate_denominator",
        });
    }
    let divisor = gcd(numerator.unsigned_abs(), denominator.unsigned_abs());
    let divisor = i128::try_from(divisor).map_err(|_| ClockError::ArithmeticOverflow {
        field: "rate_gcd",
    })?;
    Ok(Rational {
        numerator: numerator / divisor,
        denominator: denominator / divisor,
    })
}

fn gcd(mut left: u128, mut right: u128) -> u128 {
    while right != 0 {
        let remainder = left % right;
        left = right;
        right = remainder;
    }
    left.max(1)
}

fn compare_rational(left: &Rational, right: &Rational) -> Ordering {
    let left_scaled = left.numerator * right.denominator;
    let right_scaled = right.numerator * left.denominator;
    left_scaled
        .cmp(&right_scaled)
        .then(left.numerator.cmp(&right.numerator))
        .then(left.denominator.cmp(&right.denominator))
}

fn predict(rate: Rational, offset: i128, source_tick: i128) -> Result<i128, ClockError> {
    let scaled = source_tick
        .checked_mul(rate.numerator)
        .and_then(|value| value.checked_add(offset))
        .ok_or(ClockError::ArithmeticOverflow {
            field: "mapped_reference_tick",
        })?;
    divide_round_nearest(scaled, rate.denominator)
}

fn divide_round_nearest(numerator: i128, denominator: i128) -> Result<i128, ClockError> {
    if denominator <= 0 {
        return Err(ClockError::ArithmeticOverflow {
            field: "rounding_denominator",
        });
    }
    let quotient = numerator / denominator;
    let remainder = numerator % denominator;
    let doubled = remainder.unsigned_abs().checked_mul(2).ok_or(ClockError::ArithmeticOverflow {
        field: "rounding_remainder",
    })?;
    if doubled < denominator.unsigned_abs() {
        return Ok(quotient);
    }
    if numerator >= 0 {
        quotient.checked_add(1).ok_or(ClockError::ArithmeticOverflow {
            field: "rounded_reference_tick",
        })
    } else {
        quotient.checked_sub(1).ok_or(ClockError::ArithmeticOverflow {
            field: "rounded_reference_tick",
        })
    }
}

fn absolute_difference_u64(left: i128, right: i128) -> Result<u64, ClockError> {
    let difference = left.checked_sub(right).ok_or(ClockError::ArithmeticOverflow {
        field: "absolute_residual",
    })?;
    u64::try_from(difference.unsigned_abs()).map_err(|_| ClockError::BoundExceeded {
        field: "absolute_residual",
        actual: difference.unsigned_abs(),
        maximum: u128::from(u64::MAX),
    })
}

fn nominal_drift_ppm(rate: Rational, source_timescale: u64, reference_timescale: u64) -> Result<u64, ClockError> {
    let observed = rate.numerator.checked_mul(i128::from(source_timescale)).ok_or(ClockError::ArithmeticOverflow {
        field: "drift_observed",
    })?;
    let expected = rate.denominator.checked_mul(i128::from(reference_timescale)).ok_or(ClockError::ArithmeticOverflow {
        field: "drift_expected",
    })?;
    difference_ppm(observed, expected, expected)
}

fn retained_rate_spread_ppm(representatives: &[GroupRepresentative], fitted: Rational) -> Result<u64, ClockError> {
    let mut maximum = 0_u64;
    for (position, left) in representatives.iter().enumerate() {
        for right in representatives.iter().skip(position.saturating_add(1)) {
            let source_delta = right.source_tick.checked_sub(left.source_tick).ok_or(ClockError::ArithmeticOverflow {
                field: "spread_source_delta",
            })?;
            if source_delta == 0 {
                continue;
            }
            let reference_delta = right.reference_tick.checked_sub(left.reference_tick).ok_or(ClockError::ArithmeticOverflow {
                field: "spread_reference_delta",
            })?;
            let candidate = reduce_ratio(reference_delta, source_delta)?;
            let observed = candidate.numerator.checked_mul(fitted.denominator).ok_or(ClockError::ArithmeticOverflow {
                field: "spread_observed",
            })?;
            let expected = fitted.numerator.checked_mul(candidate.denominator).ok_or(ClockError::ArithmeticOverflow {
                field: "spread_expected",
            })?;
            maximum = maximum.max(difference_ppm(observed, expected, expected)?);
        }
    }
    Ok(maximum)
}

fn difference_ppm(observed: i128, expected: i128, denominator: i128) -> Result<u64, ClockError> {
    if denominator == 0 {
        return Err(ClockError::ArithmeticOverflow {
            field: "ppm_denominator",
        });
    }
    let difference = observed.checked_sub(expected).ok_or(ClockError::ArithmeticOverflow {
        field: "ppm_difference",
    })?.unsigned_abs();
    let scaled = difference.checked_mul(1_000_000).ok_or(ClockError::ArithmeticOverflow {
        field: "ppm_scaled_difference",
    })?;
    let ppm = scaled / denominator.unsigned_abs();
    u64::try_from(ppm).map_err(|_| ClockError::BoundExceeded {
        field: "ppm",
        actual: ppm,
        maximum: u128::from(u64::MAX),
    })
}

fn source_support(representatives: &[GroupRepresentative]) -> Result<(i128, i128), ClockError> {
    let mut values = representatives.iter().map(|value| value.source_tick);
    let first = values.next().ok_or(ClockError::NoDistinctSourceTicks)?;
    let mut minimum = first;
    let mut maximum = first;
    for value in values {
        minimum = minimum.min(value);
        maximum = maximum.max(value);
    }
    if minimum == maximum {
        Err(ClockError::NoDistinctSourceTicks)
    } else {
        Ok((minimum, maximum))
    }
}

fn reference_support(representatives: &[GroupRepresentative]) -> Result<(i128, i128), ClockError> {
    let mut values = representatives.iter().map(|value| value.reference_tick);
    let first = values.next().ok_or(ClockError::NoDistinctSourceTicks)?;
    let mut minimum = first;
    let mut maximum = first;
    for value in values {
        minimum = minimum.min(value);
        maximum = maximum.max(value);
    }
    Ok((minimum, maximum))
}

fn mapping_uncertainty(model: &ClockModel, source_tick: i128) -> Result<u64, ClockError> {
    let midpoint = model.source_support_start.checked_add(model.source_support_end).ok_or(ClockError::ArithmeticOverflow {
        field: "support_midpoint",
    })? / 2;
    let distance = source_tick.checked_sub(midpoint).ok_or(ClockError::ArithmeticOverflow {
        field: "support_distance",
    })?.unsigned_abs();
    let reference_distance = distance.checked_mul(model.rate_numerator.unsigned_abs()).ok_or(ClockError::ArithmeticOverflow {
        field: "reference_support_distance",
    })? / model.rate_denominator.unsigned_abs();
    let growth = reference_distance.checked_mul(u128::from(model.rate_spread_ppm)).ok_or(ClockError::ArithmeticOverflow {
        field: "drift_uncertainty",
    })? / 1_000_000;
    let growth = u64::try_from(growth).map_err(|_| ClockError::BoundExceeded {
        field: "drift_uncertainty",
        actual: growth,
        maximum: u128::from(u64::MAX),
    })?;
    model.declared_uncertainty_ticks.checked_add(growth).ok_or(ClockError::ArithmeticOverflow {
        field: "mapping_uncertainty",
    })
}

fn compare_domain(field: &'static str, expected: &str, observed: &str) -> Result<(), ClockError> {
    if expected == observed {
        Ok(())
    } else {
        Err(ClockError::DomainMismatch {
            field,
            expected: expected.to_owned(),
            observed: observed.to_owned(),
        })
    }
}

fn compare_timescale(field: &'static str, expected: u64, observed: u64) -> Result<(), ClockError> {
    if expected == observed {
        Ok(())
    } else {
        Err(ClockError::TimescaleMismatch {
            field,
            expected,
            observed,
        })
    }
}

fn lower_median_u64(values: &[u64]) -> Result<u64, ClockError> {
    values
        .get(values.len().saturating_sub(1) / 2)
        .copied()
        .ok_or(ClockError::TooFewAnchors {
            actual: 0,
            minimum: 1,
        })
}

fn put_i128(encoder: &mut Encoder, value: i128) -> Result<(), ClockError> {
    encoder.put_bytes(&value.to_be_bytes())?;
    Ok(())
}

fn put_u64_sequence(encoder: &mut Encoder, values: &[u64]) -> Result<(), ClockError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u64(*value);
    }
    Ok(())
}

fn put_u32_sequence(encoder: &mut Encoder, values: &[u32]) -> Result<(), ClockError> {
    encoder.put_u64(usize_to_u64(values.len())?);
    for value in values {
        encoder.put_u32(*value);
    }
    Ok(())
}

fn usize_to_u64(value: usize) -> Result<u64, ClockError> {
    u64::try_from(value).map_err(|_| ClockError::ArithmeticOverflow {
        field: "collection_length",
    })
}

fn push_u64_json_array(output: &mut String, values: &[u64]) -> Result<(), ClockError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}

fn push_u32_json_array(output: &mut String, values: &[u32]) -> Result<(), ClockError> {
    for (position, value) in values.iter().enumerate() {
        if position > 0 {
            output.push(',');
        }
        write!(output, "{value}").map_err(json_rendering)?;
    }
    Ok(())
}

fn json_rendering(error: std::fmt::Error) -> ClockError {
    ClockError::JsonRendering(error.to_string())
}

#[cfg(test)]
mod tests {
    use super::{
        CLOCK_EPOCH_LEDGER_SCHEMA, CLOCK_MODEL_SCHEMA, ClockAnchor, ClockDomain,
        ClockEpochLedger, ClockError, ClockFitOptions, ClockModel, ClockModelBasis,
        fit_clock_model,
    };
    use fdgr_types::EvidenceDigest;

    fn digest(byte: u8) -> EvidenceDigest {
        EvidenceDigest::from_bytes([byte; 32])
    }

    fn domain(value: &str) -> ClockDomain {
        let parsed = ClockDomain::parse(value);
        assert!(parsed.is_ok());
        match parsed {
            Ok(value) => value,
            Err(_) => std::process::abort(),
        }
    }

    fn basis(generation: u64, source_epoch: u64, reference_epoch: u64, byte: u8) -> ClockModelBasis {
        ClockModelBasis {
            basis_digest: digest(byte),
            source_domain: domain("media_pts"),
            reference_domain: domain("host_monotonic"),
            source_epoch,
            reference_epoch,
            model_generation: generation,
            source_timescale: 1_000,
            reference_timescale: 1_000_000_000,
        }
    }

    fn options() -> ClockFitOptions {
        ClockFitOptions {
            max_residual_ticks: 1_000,
            max_drift_ppm: 1_000,
            min_independent_groups: 3,
        }
    }

    fn anchors(start: i128, reference_start: i128) -> Vec<ClockAnchor> {
        vec![
            ClockAnchor { anchor_id: 1, source_tick: start, reference_tick: reference_start, uncertainty_ticks: 100, correlation_group: 1 },
            ClockAnchor { anchor_id: 2, source_tick: start + 1_000, reference_tick: reference_start + 1_000_000_000, uncertainty_ticks: 100, correlation_group: 2 },
            ClockAnchor { anchor_id: 3, source_tick: start + 2_000, reference_tick: reference_start + 2_000_000_100, uncertainty_ticks: 100, correlation_group: 3 },
            ClockAnchor { anchor_id: 4, source_tick: start + 3_000, reference_tick: reference_start + 8_999_500_000, uncertainty_ticks: 100, correlation_group: 4 },
            ClockAnchor { anchor_id: 5, source_tick: start + 4_000, reference_tick: reference_start + 4_000_000_000, uncertainty_ticks: 100, correlation_group: 5 },
        ]
    }

    fn model(generation: u64, source_epoch: u64, reference_epoch: u64, source_start: i128, reference_start: i128, byte: u8) -> ClockModel {
        let fitted = fit_clock_model(
            basis(generation, source_epoch, reference_epoch, byte),
            options(),
            anchors(source_start, reference_start),
        );
        assert!(fitted.is_ok());
        match fitted {
            Ok(value) => value,
            Err(_) => std::process::abort(),
        }
    }

    #[test]
    fn robust_fit_rejects_one_independent_outlier() {
        let value = model(1, 1, 1, 0, 500_000, 1);
        assert_eq!(value.rate_numerator, 1_000_000);
        assert_eq!(value.rate_denominator, 1);
        assert_eq!(value.offset_numerator, 500_000);
        assert_eq!(value.inlier_group_ids, vec![1, 2, 3, 5]);
        assert_eq!(value.outlier_group_ids, vec![4]);
        assert_eq!(value.outlier_anchor_ids, vec![4]);
        assert_eq!(value.drift_ppm, 0);
        assert_eq!(value.max_abs_residual_ticks, 100);
        assert_eq!(value.declared_uncertainty_ticks, 200);
        assert!(value.validate().is_ok());
        assert!(matches!(value.to_json(), Ok(ref json) if json.contains(CLOCK_MODEL_SCHEMA)));
    }

    #[test]
    fn input_order_does_not_change_identity() {
        let input = anchors(0, 500_000);
        let mut reversed = input.clone();
        reversed.reverse();
        let left = fit_clock_model(basis(1, 1, 1, 2), options(), input);
        let right = fit_clock_model(basis(1, 1, 1, 2), options(), reversed);
        assert!(matches!((left, right), (Ok(left), Ok(right)) if left == right && left.digest() == right.digest()));
    }

    #[test]
    fn mapping_refuses_extrapolation() {
        let value = model(1, 1, 1, 0, 500_000, 3);
        assert!(matches!(value.map_tick(2_500), Ok(ref mapped) if mapped.reference_tick == 2_500_500_000 && mapped.uncertainty_ticks >= 200));
        assert!(matches!(value.map_tick(4_001), Err(ClockError::OutsideSupport { .. })));
    }

    #[test]
    fn correlated_outliers_receive_one_vote() {
        let mut input = anchors(0, 500_000);
        input.push(ClockAnchor { anchor_id: 6, source_tick: 3_100, reference_tick: 12_000_000_000, uncertainty_ticks: 100, correlation_group: 4 });
        input.push(ClockAnchor { anchor_id: 7, source_tick: 3_200, reference_tick: 13_000_000_000, uncertainty_ticks: 100, correlation_group: 4 });
        assert!(matches!(fit_clock_model(basis(1, 1, 1, 4), options(), input), Ok(ref value) if value.outlier_group_ids == vec![4] && value.outlier_anchor_ids == vec![4, 6, 7]));
    }

    #[test]
    fn ledger_records_gap_and_reset() {
        let first = model(1, 1, 1, 0, 500_000, 5);
        let second = model(2, 1, 1, 5_000, 5_000_500_000, 6);
        let reset = model(3, 2, 1, 0, 10_000_500_000, 7);
        let ledger = ClockEpochLedger::from_models(vec![first, second, reset]);
        assert!(matches!(ledger, Ok(ref value) if value.gaps.len() == 1 && value.epoch_transitions.len() == 1 && value.map_tick(1, 5_500).is_ok() && value.map_tick(2, 500).is_ok() && matches!(value.map_tick(1, 4_500), Err(ClockError::UnmappedTick { .. })) && matches!(value.digest(), Ok(_))));
        assert!(CLOCK_EPOCH_LEDGER_SCHEMA.ends_with("/1"));
    }

    #[test]
    fn ledger_refuses_overlap_and_generation_skip() {
        let first = model(1, 1, 1, 0, 500_000, 8);
        let overlapping = model(2, 1, 1, 4_000, 4_000_500_000, 9);
        let skipped = model(3, 1, 1, 5_000, 5_000_500_000, 10);
        let ledger = ClockEpochLedger::new(first);
        assert!(ledger.is_ok());
        if let Ok(mut ledger) = ledger {
            assert!(matches!(ledger.append(overlapping), Err(ClockError::OverlappingSupport { .. })));
            assert!(matches!(ledger.append(skipped), Err(ClockError::GenerationNotConsecutive { .. })));
        }
    }

    #[test]
    fn domains_and_drift_fail_closed() {
        assert!(ClockDomain::parse("Host.Clock").is_err());
        assert!(ClockDomain::parse("host__clock").is_err());
        let mut strict = options();
        strict.max_drift_ppm = 1;
        let mut fast = anchors(0, 500_000);
        for anchor in &mut fast {
            anchor.reference_tick = anchor.reference_tick.saturating_add(anchor.source_tick * 10_000);
        }
        assert!(matches!(fit_clock_model(basis(1, 1, 1, 11), strict, fast), Err(ClockError::DriftExceeded { .. })));
    }
}
