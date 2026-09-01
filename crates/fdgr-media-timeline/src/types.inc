/// Public schema identity for one canonical sample timeline.
pub const MEDIA_TIMELINE_SCHEMA: &str = "fdgr.media_timeline/1";
/// Maximum sample records admitted by one reference timeline.
pub const MAX_TIMELINE_SAMPLES: usize = 1_000_000;

/// Exact immutable basis for a sample timeline.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TimelineBasis {
    /// Published recorded-media root manifest that selected the source representation.
    pub recorded_media_root_manifest_digest: EvidenceDigest,
    /// Published exact source representation manifest.
    pub source_manifest_digest: EvidenceDigest,
    /// Logical digest of the exact encoded source bytes.
    pub source_object_digest: EvidenceDigest,
    /// Exact encoded source length.
    pub source_object_length: u64,
    /// Nonzero ISO BMFF track identity.
    pub track_id: u32,
    /// Nonzero track timescale.
    pub timescale: u32,
}

/// One canonical sample observation in decode/source-table order.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TimelineSample {
    /// Zero-based sample index within the track.
    pub sample_index: u64,
    /// Decode timestamp in track timescale units.
    pub decode_time: u64,
    /// Presentation timestamp in track timescale units.
    pub presentation_time: i128,
    /// Signed presentation-minus-decode offset in track timescale units.
    pub composition_offset: i128,
    /// Decode duration in track timescale units.
    pub duration: u32,
    /// Exclusive decode end in track timescale units.
    pub decode_end: u64,
    /// Exclusive presentation end in track timescale units.
    pub presentation_end: i128,
    /// Exact source-file byte offset.
    pub byte_offset: u64,
    /// Exclusive source-file byte end.
    pub byte_end: u64,
    /// Exact encoded sample length.
    pub byte_length: u32,
    /// Whether the sample is a sync sample.
    pub is_sync: bool,
    /// One-based sample-description index.
    pub sample_description_index: u32,
}

/// Explicit uncovered decode-time interval between adjacent represented samples.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TimelineGap {
    /// Sample immediately before the gap.
    pub after_sample_index: u64,
    /// Sample immediately after the gap.
    pub before_sample_index: u64,
    /// Inclusive gap start in decode-time units.
    pub start_decode_time: u64,
    /// Exclusive gap end in decode-time units.
    pub end_decode_time: u64,
    /// Uncovered duration in decode-time units.
    pub duration: u64,
}

/// Deterministic timeline derived from one exact validated sample window.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CanonicalSampleTimeline {
    /// Exact source and track basis.
    pub basis: TimelineBasis,
    /// Total validated sample count for the track.
    pub total_samples: u64,
    /// First sample represented by this timeline.
    pub start_sample: u64,
    /// Exclusive represented sample end.
    pub end_sample: u64,
    /// Caller-requested maximum records. This is diagnostic request context and is excluded from
    /// the semantic timeline digest.
    pub requested_max_samples: usize,
    /// Whether this window reaches the end of the track's classic sample table.
    pub reaches_track_end: bool,
    /// Whether this window represents the complete track from sample zero through the end.
    pub covers_entire_track: bool,
    /// Samples before this represented window.
    pub prefix_unrepresented_samples: u64,
    /// Samples after this represented window.
    pub suffix_unrepresented_samples: u64,
    /// Table entries charged by the source indexer. This is diagnostic operation cost and is
    /// excluded from the semantic timeline digest.
    pub index_entries_scanned: u64,
    /// Ordered canonical samples.
    pub samples: Vec<TimelineSample>,
    /// Ordered explicit decode-time gaps.
    pub gaps: Vec<TimelineGap>,
    /// Sum of explicit decode-time gap durations.
    pub total_gap_duration: u64,
    /// Number of represented sync samples.
    pub sync_sample_count: u64,
    /// Sorted distinct one-based sample-description indices.
    pub sample_description_indices: Vec<u32>,
    /// Whether source byte offsets move backwards in decode order.
    pub source_byte_order_reordered: bool,
    /// Whether presentation order differs from decode order.
    pub presentation_reordered: bool,
    /// Whether at least one represented presentation timestamp is negative.
    pub has_negative_presentation_time: bool,
    /// Earliest decode timestamp represented by the window.
    pub decode_start: Option<u64>,
    /// Exclusive decode end represented by the window.
    pub decode_end: Option<u64>,
    /// Earliest presentation timestamp represented by the window.
    pub presentation_start: Option<i128>,
    /// Exclusive presentation end represented by the window.
    pub presentation_end: Option<i128>,
}
