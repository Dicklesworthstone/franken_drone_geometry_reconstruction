/// Stable timeline construction, validation, and identity failures.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum TimelineError {
    /// A required numeric value was zero.
    ZeroValue {
        /// Stable field name.
        field: &'static str,
    },
    /// A required identity was the all-zero sentinel.
    ZeroIdentity {
        /// Stable field name.
        field: &'static str,
    },
    /// Basis and sample-window track identities differ.
    TrackMismatch {
        /// Basis track identity.
        expected: u32,
        /// Window track identity.
        observed: u32,
    },
    /// Basis and sample-window timescales differ.
    TimescaleMismatch {
        /// Basis timescale.
        expected: u32,
        /// Window timescale.
        observed: u32,
    },
    /// A request exceeded the hard sample ceiling.
    RequestLimitExceeded {
        /// Requested sample count.
        actual: usize,
        /// Maximum sample count.
        maximum: usize,
    },
    /// One timeline exceeded the hard sample ceiling.
    SampleLimitExceeded {
        /// Observed sample count.
        actual: usize,
        /// Maximum sample count.
        maximum: usize,
    },
    /// A window returned more records than it admitted.
    ReturnedMoreThanRequested {
        /// Observed record count.
        returned: usize,
        /// Requested record ceiling.
        requested: usize,
    },
    /// The requested start lies beyond the track sample count.
    StartBeyondTrack {
        /// Requested first sample.
        start_sample: u64,
        /// Total validated samples.
        total_samples: u64,
    },
    /// The returned window extends beyond the validated track.
    WindowBeyondTrack {
        /// Exclusive returned end.
        end_sample: u64,
        /// Total validated samples.
        total_samples: u64,
    },
    /// The source window's completion bit disagrees with its range.
    CompleteFlagMismatch {
        /// Completion derived from exact range accounting.
        expected: bool,
        /// Completion claimed by the source window.
        observed: bool,
    },
    /// Sample indices were not a canonical contiguous sequence.
    NonContiguousSampleIndex {
        /// Required next sample index.
        expected: u64,
        /// Observed sample index.
        observed: u64,
    },
    /// A sample had no positive decode duration.
    ZeroDuration {
        /// Offending sample.
        sample_index: u64,
    },
    /// A sample had no encoded bytes.
    ZeroByteLength {
        /// Offending sample.
        sample_index: u64,
    },
    /// A sample used the reserved zero sample-description index.
    ZeroSampleDescriptionIndex {
        /// Offending sample.
        sample_index: u64,
    },
    /// A sample byte interval extends beyond the authenticated source object.
    SampleOutsideSource {
        /// Offending sample.
        sample_index: u64,
        /// Exclusive sample byte end.
        byte_end: u64,
        /// Exact authenticated source length.
        source_object_length: u64,
    },
    /// Adjacent decode intervals overlap.
    OverlappingDecodeIntervals {
        /// Previous sample index.
        previous_sample_index: u64,
        /// Exclusive end of the previous sample.
        previous_decode_end: u64,
        /// Current sample index.
        sample_index: u64,
        /// Current sample decode time.
        decode_time: u64,
    },
    /// Encoded byte intervals overlap after sorting by source offset.
    OverlappingByteIntervals {
        /// Previous sample index in source-byte order.
        previous_sample_index: u64,
        /// Exclusive end of the previous byte interval.
        previous_byte_end: u64,
        /// Current sample index in source-byte order.
        sample_index: u64,
        /// Current sample byte offset.
        byte_offset: u64,
    },
    /// A derived arithmetic value overflowed its canonical domain.
    ArithmeticOverflow {
        /// Stable derived field name.
        field: &'static str,
    },
    /// A platform-sized collection length could not fit in `u64`.
    LengthOverflow,
    /// A stored derived field disagreed with a deterministic rebuild.
    DerivedSummaryMismatch {
        /// Stable derived field name.
        field: &'static str,
    },
    /// Canonical encoding or hashing failed.
    Codec(CodecError),
    /// Timeline identity-domain construction failed.
    Domain(DomainError),
    /// Deterministic JSON rendering failed.
    JsonRendering(String),
}

impl Display for TimelineError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::ZeroValue { field } => write!(formatter, "timeline field {field} must be nonzero"),
            Self::ZeroIdentity { field } => {
                write!(formatter, "timeline identity {field} must not be all zero")
            }
            Self::TrackMismatch { expected, observed } => write!(
                formatter,
                "timeline track mismatch: expected {expected}, observed {observed}"
            ),
            Self::TimescaleMismatch { expected, observed } => write!(
                formatter,
                "timeline timescale mismatch: expected {expected}, observed {observed}"
            ),
            Self::RequestLimitExceeded { actual, maximum } => write!(
                formatter,
                "timeline request admits {actual} samples; maximum is {maximum}"
            ),
            Self::SampleLimitExceeded { actual, maximum } => write!(
                formatter,
                "timeline contains {actual} samples; maximum is {maximum}"
            ),
            Self::ReturnedMoreThanRequested { returned, requested } => write!(
                formatter,
                "timeline returned {returned} samples for request ceiling {requested}"
            ),
            Self::StartBeyondTrack {
                start_sample,
                total_samples,
            } => write!(
                formatter,
                "timeline starts at sample {start_sample} beyond track count {total_samples}"
            ),
            Self::WindowBeyondTrack {
                end_sample,
                total_samples,
            } => write!(
                formatter,
                "timeline ends at sample {end_sample} beyond track count {total_samples}"
            ),
            Self::CompleteFlagMismatch { expected, observed } => write!(
                formatter,
                "timeline completion mismatch: expected {expected}, observed {observed}"
            ),
            Self::NonContiguousSampleIndex { expected, observed } => write!(
                formatter,
                "timeline sample index is {observed}; expected {expected}"
            ),
            Self::ZeroDuration { sample_index } => {
                write!(formatter, "timeline sample {sample_index} has zero duration")
            }
            Self::ZeroByteLength { sample_index } => {
                write!(formatter, "timeline sample {sample_index} has zero encoded bytes")
            }
            Self::ZeroSampleDescriptionIndex { sample_index } => write!(
                formatter,
                "timeline sample {sample_index} has zero sample-description index"
            ),
            Self::SampleOutsideSource {
                sample_index,
                byte_end,
                source_object_length,
            } => write!(
                formatter,
                "timeline sample {sample_index} ends at byte {byte_end} beyond source length {source_object_length}"
            ),
            Self::OverlappingDecodeIntervals {
                previous_sample_index,
                previous_decode_end,
                sample_index,
                decode_time,
            } => write!(
                formatter,
                "timeline sample {sample_index} begins at {decode_time} before sample {previous_sample_index} ends at {previous_decode_end}"
            ),
            Self::OverlappingByteIntervals {
                previous_sample_index,
                previous_byte_end,
                sample_index,
                byte_offset,
            } => write!(
                formatter,
                "timeline sample {sample_index} begins at byte {byte_offset} before sample {previous_sample_index} ends at byte {previous_byte_end}"
            ),
            Self::ArithmeticOverflow { field } => {
                write!(formatter, "timeline arithmetic overflow at {field}")
            }
            Self::LengthOverflow => formatter.write_str("timeline collection length overflows u64"),
            Self::DerivedSummaryMismatch { field } => {
                write!(formatter, "timeline derived summary mismatch at {field}")
            }
            Self::Codec(error) => write!(formatter, "timeline codec error: {error}"),
            Self::Domain(error) => write!(formatter, "timeline identity-domain error: {error}"),
            Self::JsonRendering(error) => write!(formatter, "timeline JSON rendering failed: {error}"),
        }
    }
}

impl Error for TimelineError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for TimelineError {
    fn from(value: CodecError) -> Self {
        Self::Codec(value)
    }
}

impl From<DomainError> for TimelineError {
    fn from(value: DomainError) -> Self {
        Self::Domain(value)
    }
}
