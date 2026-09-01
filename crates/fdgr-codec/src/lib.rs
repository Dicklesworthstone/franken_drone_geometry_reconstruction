#![forbid(unsafe_code)]
//! Deterministic, dependency-free canonical encoding and SHA-256 for FDGR.

use fdgr_types::{DigestDomain, EvidenceDigest};
use std::error::Error;
use std::fmt::{self, Display, Formatter};
use std::str::Utf8Error;

const SHA256_BLOCK_BYTES: usize = 64;
const SHA256_LENGTH_OFFSET: usize = 56;
const SHA256_DIGEST_BYTES: usize = 32;

const INITIAL_STATE: [u32; 8] = [
    0x6a09_e667,
    0xbb67_ae85,
    0x3c6e_f372,
    0xa54f_f53a,
    0x510e_527f,
    0x9b05_688c,
    0x1f83_d9ab,
    0x5be0_cd19,
];

const ROUND_CONSTANTS: [u32; 64] = [
    0x428a_2f98, 0x7137_4491, 0xb5c0_fbcf, 0xe9b5_dba5, 0x3956_c25b, 0x59f1_11f1,
    0x923f_82a4, 0xab1c_5ed5, 0xd807_aa98, 0x1283_5b01, 0x2431_85be, 0x550c_7dc3,
    0x72be_5d74, 0x80de_b1fe, 0x9bdc_06a7, 0xc19b_f174, 0xe49b_69c1, 0xefbe_4786,
    0x0fc1_9dc6, 0x240c_a1cc, 0x2de9_2c6f, 0x4a74_84aa, 0x5cb0_a9dc, 0x76f9_88da,
    0x983e_5152, 0xa831_c66d, 0xb003_27c8, 0xbf59_7fc7, 0xc6e0_0bf3, 0xd5a7_9147,
    0x06ca_6351, 0x1429_2967, 0x27b7_0a85, 0x2e1b_2138, 0x4d2c_6dfc, 0x5338_0d13,
    0x650a_7354, 0x766a_0abb, 0x81c2_c92e, 0x9272_2c85, 0xa2bf_e8a1, 0xa81a_664b,
    0xc24b_8b70, 0xc76c_51a3, 0xd192_e819, 0xd699_0624, 0xf40e_3585, 0x106a_a070,
    0x19a4_c116, 0x1e37_6c08, 0x2748_774c, 0x34b0_bcb5, 0x391c_0cb3, 0x4ed8_aa4a,
    0x5b9c_ca4f, 0x682e_6ff3, 0x748f_82ee, 0x78a5_636f, 0x84c8_7814, 0x8cc7_0208,
    0x90be_fffa, 0xa450_6ceb, 0xbef9_a3f7, 0xc671_78f2,
];

/// Streaming SHA-256 state with no external dependency and only memory-safe code.
#[derive(Clone, Debug)]
pub struct Sha256 {
    state: [u32; 8],
    buffer: [u8; SHA256_BLOCK_BYTES],
    buffer_len: usize,
    message_len_bytes: u64,
}

impl Default for Sha256 {
    fn default() -> Self {
        Self::new()
    }
}

impl Sha256 {
    /// Creates a fresh SHA-256 state.
    #[must_use]
    pub const fn new() -> Self {
        Self {
            state: INITIAL_STATE,
            buffer: [0_u8; SHA256_BLOCK_BYTES],
            buffer_len: 0,
            message_len_bytes: 0,
        }
    }

    /// Adds bytes to the digest state.
    pub fn update(&mut self, mut input: &[u8]) {
        self.message_len_bytes = self
            .message_len_bytes
            .wrapping_add(u64::try_from(input.len()).unwrap_or(u64::MAX));

        if self.buffer_len > 0 {
            let available = SHA256_BLOCK_BYTES.saturating_sub(self.buffer_len);
            let take = available.min(input.len());
            if let Some(destination) = self
                .buffer
                .get_mut(self.buffer_len..self.buffer_len.saturating_add(take))
            {
                if let Some(source) = input.get(..take) {
                    destination.copy_from_slice(source);
                }
            }
            self.buffer_len = self.buffer_len.saturating_add(take);
            input = input.get(take..).unwrap_or_default();
            if self.buffer_len == SHA256_BLOCK_BYTES {
                let block = self.buffer;
                self.compress(&block);
                self.buffer = [0_u8; SHA256_BLOCK_BYTES];
                self.buffer_len = 0;
            }
        }

        while input.len() >= SHA256_BLOCK_BYTES {
            let (block_bytes, remainder) = input.split_at(SHA256_BLOCK_BYTES);
            if let Ok(block) = <&[u8; SHA256_BLOCK_BYTES]>::try_from(block_bytes) {
                self.compress(block);
            }
            input = remainder;
        }

        if !input.is_empty() {
            if let Some(destination) = self.buffer.get_mut(..input.len()) {
                destination.copy_from_slice(input);
                self.buffer_len = input.len();
            }
        }
    }

    /// Finalizes the state and returns the canonical digest.
    #[must_use]
    pub fn finalize(mut self) -> EvidenceDigest {
        let bit_length = self.message_len_bytes.wrapping_mul(8);
        if let Some(marker) = self.buffer.get_mut(self.buffer_len) {
            *marker = 0x80;
        }
        self.buffer_len = self.buffer_len.saturating_add(1);

        if self.buffer_len > SHA256_LENGTH_OFFSET {
            for byte in self.buffer.iter_mut().skip(self.buffer_len) {
                *byte = 0;
            }
            let block = self.buffer;
            self.compress(&block);
            self.buffer = [0_u8; SHA256_BLOCK_BYTES];
            self.buffer_len = 0;
        }

        for byte in self
            .buffer
            .iter_mut()
            .take(SHA256_LENGTH_OFFSET)
            .skip(self.buffer_len)
        {
            *byte = 0;
        }
        if let Some(destination) = self.buffer.get_mut(SHA256_LENGTH_OFFSET..SHA256_BLOCK_BYTES) {
            destination.copy_from_slice(&bit_length.to_be_bytes());
        }
        let block = self.buffer;
        self.compress(&block);

        let mut digest = [0_u8; SHA256_DIGEST_BYTES];
        for (word, output) in self.state.iter().zip(digest.chunks_exact_mut(4)) {
            output.copy_from_slice(&word.to_be_bytes());
        }
        EvidenceDigest::from_bytes(digest)
    }

    fn compress(&mut self, block: &[u8; SHA256_BLOCK_BYTES]) {
        let mut schedule = [0_u32; 64];
        for (slot, bytes) in schedule.iter_mut().take(16).zip(block.chunks_exact(4)) {
            let [a, b, c, d] = bytes else {
                continue;
            };
            *slot = u32::from_be_bytes([*a, *b, *c, *d]);
        }

        for index in 16..64 {
            let Some(w_15) = schedule.get(index - 15).copied() else {
                return;
            };
            let Some(w_2) = schedule.get(index - 2).copied() else {
                return;
            };
            let Some(w_16) = schedule.get(index - 16).copied() else {
                return;
            };
            let Some(w_7) = schedule.get(index - 7).copied() else {
                return;
            };
            let value = small_sigma0(w_15)
                .wrapping_add(w_16)
                .wrapping_add(small_sigma1(w_2))
                .wrapping_add(w_7);
            if let Some(slot) = schedule.get_mut(index) {
                *slot = value;
            }
        }

        let [mut a, mut b, mut c, mut d, mut e, mut f, mut g, mut h] = self.state;
        for (constant, word) in ROUND_CONSTANTS.iter().zip(schedule.iter()) {
            let temporary_1 = h
                .wrapping_add(big_sigma1(e))
                .wrapping_add(choose(e, f, g))
                .wrapping_add(*constant)
                .wrapping_add(*word);
            let temporary_2 = big_sigma0(a).wrapping_add(majority(a, b, c));
            h = g;
            g = f;
            f = e;
            e = d.wrapping_add(temporary_1);
            d = c;
            c = b;
            b = a;
            a = temporary_1.wrapping_add(temporary_2);
        }

        let [s0, s1, s2, s3, s4, s5, s6, s7] = self.state;
        self.state = [
            s0.wrapping_add(a),
            s1.wrapping_add(b),
            s2.wrapping_add(c),
            s3.wrapping_add(d),
            s4.wrapping_add(e),
            s5.wrapping_add(f),
            s6.wrapping_add(g),
            s7.wrapping_add(h),
        ];
    }
}

/// Computes ordinary SHA-256 over the supplied bytes.
#[must_use]
pub fn hash_bytes(bytes: &[u8]) -> EvidenceDigest {
    let mut hash = Sha256::new();
    hash.update(bytes);
    hash.finalize()
}

/// Streaming length-framed domain-separated identity state.
#[derive(Clone, Debug)]
pub struct DomainHasher {
    hash: Sha256,
    expected_payload_bytes: u64,
    observed_payload_bytes: u64,
}

impl DomainHasher {
    /// Creates a streaming domain hash for an exact payload length.
    ///
    /// # Errors
    ///
    /// Returns [`CodecError::LengthOverflow`] if the domain length cannot fit in the
    /// canonical `u64` field.
    pub fn new(
        domain: &DigestDomain,
        expected_payload_bytes: u64,
    ) -> Result<Self, CodecError> {
        let domain_length = u64::try_from(domain.as_str().len())
            .map_err(|_| CodecError::LengthOverflow)?;
        let mut hash = Sha256::new();
        hash.update(b"FDGR\0");
        hash.update(&domain_length.to_be_bytes());
        hash.update(domain.as_str().as_bytes());
        hash.update(&expected_payload_bytes.to_be_bytes());
        Ok(Self {
            hash,
            expected_payload_bytes,
            observed_payload_bytes: 0,
        })
    }

    /// Adds the next exact payload segment.
    ///
    /// # Errors
    ///
    /// Returns [`CodecError::LengthOverflow`] if the observed length cannot be represented,
    /// or [`CodecError::DomainLengthMismatch`] before hashing bytes beyond the declared length.
    pub fn update(&mut self, bytes: &[u8]) -> Result<(), CodecError> {
        let length = u64::try_from(bytes.len()).map_err(|_| CodecError::LengthOverflow)?;
        let observed = self
            .observed_payload_bytes
            .checked_add(length)
            .ok_or(CodecError::LengthOverflow)?;
        if observed > self.expected_payload_bytes {
            return Err(CodecError::DomainLengthMismatch {
                expected: self.expected_payload_bytes,
                observed,
            });
        }
        self.hash.update(bytes);
        self.observed_payload_bytes = observed;
        Ok(())
    }

    /// Finalizes only if the exact declared payload length was observed.
    ///
    /// # Errors
    ///
    /// Returns [`CodecError::DomainLengthMismatch`] when the stream ended early.
    pub fn finalize(self) -> Result<EvidenceDigest, CodecError> {
        if self.observed_payload_bytes != self.expected_payload_bytes {
            return Err(CodecError::DomainLengthMismatch {
                expected: self.expected_payload_bytes,
                observed: self.observed_payload_bytes,
            });
        }
        Ok(self.hash.finalize())
    }
}

/// Computes a length-framed, domain-separated SHA-256 identity.
///
/// # Errors
///
/// Returns a typed length error if the payload cannot be represented by the canonical framing.
pub fn hash_domain(
    domain: &DigestDomain,
    payload: &[u8],
) -> Result<EvidenceDigest, CodecError> {
    let payload_length = u64::try_from(payload.len()).map_err(|_| CodecError::LengthOverflow)?;
    let mut hash = DomainHasher::new(domain, payload_length)?;
    hash.update(payload)?;
    hash.finalize()
}

/// Deterministic fixed-width encoder used by canonical FDGR reference formats.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Encoder {
    bytes: Vec<u8>,
}

impl Encoder {
    /// Creates an empty encoder.
    #[must_use]
    pub const fn new() -> Self {
        Self { bytes: Vec::new() }
    }

    /// Creates an encoder with a bounded capacity hint.
    #[must_use]
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            bytes: Vec::with_capacity(capacity),
        }
    }

    /// Appends one byte.
    pub fn put_u8(&mut self, value: u8) {
        self.bytes.push(value);
    }

    /// Appends one canonical boolean byte.
    pub fn put_bool(&mut self, value: bool) {
        self.put_u8(u8::from(value));
    }

    /// Appends a big-endian unsigned 16-bit integer.
    pub fn put_u16(&mut self, value: u16) {
        self.bytes.extend_from_slice(&value.to_be_bytes());
    }

    /// Appends a big-endian unsigned 32-bit integer.
    pub fn put_u32(&mut self, value: u32) {
        self.bytes.extend_from_slice(&value.to_be_bytes());
    }

    /// Appends a big-endian unsigned 64-bit integer.
    pub fn put_u64(&mut self, value: u64) {
        self.bytes.extend_from_slice(&value.to_be_bytes());
    }

    /// Appends a big-endian signed 64-bit integer.
    pub fn put_i64(&mut self, value: i64) {
        self.bytes.extend_from_slice(&value.to_be_bytes());
    }

    /// Appends a canonical digest as exactly 32 bytes.
    pub fn put_digest(&mut self, value: &EvidenceDigest) {
        self.bytes.extend_from_slice(&value.to_bytes());
    }

    /// Appends a `u64` length followed by raw bytes.
    ///
    /// # Errors
    ///
    /// Returns [`CodecError::LengthOverflow`] if the slice length cannot fit in `u64`.
    pub fn put_bytes(&mut self, value: &[u8]) -> Result<(), CodecError> {
        let length = u64::try_from(value.len()).map_err(|_| CodecError::LengthOverflow)?;
        self.put_u64(length);
        self.bytes.extend_from_slice(value);
        Ok(())
    }

    /// Appends a `u64` UTF-8 byte length followed by the exact string bytes.
    ///
    /// # Errors
    ///
    /// Returns [`CodecError::LengthOverflow`] if the string length cannot fit in `u64`.
    pub fn put_str(&mut self, value: &str) -> Result<(), CodecError> {
        self.put_bytes(value.as_bytes())
    }

    /// Borrows the encoded bytes.
    #[must_use]
    pub fn as_bytes(&self) -> &[u8] {
        &self.bytes
    }

    /// Consumes the encoder and returns its bytes.
    #[must_use]
    pub fn into_bytes(self) -> Vec<u8> {
        self.bytes
    }
}

/// Hard bounds applied while decoding attacker-controlled canonical payloads.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct DecodeLimits {
    /// Maximum total input bytes.
    pub max_total_bytes: usize,
    /// Maximum one length-prefixed byte string.
    pub max_blob_bytes: usize,
    /// Maximum one UTF-8 string.
    pub max_string_bytes: usize,
}

impl DecodeLimits {
    /// Conservative default limits for control-plane payloads.
    pub const CONTROL_PLANE: Self = Self {
        max_total_bytes: 16 * 1024 * 1024,
        max_blob_bytes: 8 * 1024 * 1024,
        max_string_bytes: 1024 * 1024,
    };
}

/// Bounded decoder for the canonical fixed-width format.
#[derive(Clone, Debug)]
pub struct Decoder<'a> {
    input: &'a [u8],
    cursor: usize,
    limits: DecodeLimits,
}

impl<'a> Decoder<'a> {
    /// Creates a decoder after applying the total-input bound.
    ///
    /// # Errors
    ///
    /// Returns [`CodecError::BoundExceeded`] when the payload exceeds `max_total_bytes`.
    pub fn new(input: &'a [u8], limits: DecodeLimits) -> Result<Self, CodecError> {
        if input.len() > limits.max_total_bytes {
            return Err(CodecError::BoundExceeded {
                field: "total_bytes",
                actual: input.len(),
                maximum: limits.max_total_bytes,
            });
        }
        Ok(Self {
            input,
            cursor: 0,
            limits,
        })
    }

    /// Reads one byte.
    pub fn read_u8(&mut self) -> Result<u8, CodecError> {
        let bytes = self.read_exact(1)?;
        let [value] = bytes else {
            return Err(CodecError::UnexpectedEof {
                needed: 1,
                remaining: bytes.len(),
            });
        };
        Ok(*value)
    }

    /// Reads one canonical boolean byte.
    pub fn read_bool(&mut self) -> Result<bool, CodecError> {
        match self.read_u8()? {
            0 => Ok(false),
            1 => Ok(true),
            other => Err(CodecError::InvalidBoolean(other)),
        }
    }

    /// Reads a big-endian unsigned 16-bit integer.
    pub fn read_u16(&mut self) -> Result<u16, CodecError> {
        let bytes = self.read_exact(2)?;
        let [a, b] = bytes else {
            return Err(CodecError::UnexpectedEof {
                needed: 2,
                remaining: bytes.len(),
            });
        };
        Ok(u16::from_be_bytes([*a, *b]))
    }

    /// Reads a big-endian unsigned 32-bit integer.
    pub fn read_u32(&mut self) -> Result<u32, CodecError> {
        let bytes = self.read_exact(4)?;
        let [a, b, c, d] = bytes else {
            return Err(CodecError::UnexpectedEof {
                needed: 4,
                remaining: bytes.len(),
            });
        };
        Ok(u32::from_be_bytes([*a, *b, *c, *d]))
    }

    /// Reads a big-endian unsigned 64-bit integer.
    pub fn read_u64(&mut self) -> Result<u64, CodecError> {
        let bytes = self.read_exact(8)?;
        let [a, b, c, d, e, f, g, h] = bytes else {
            return Err(CodecError::UnexpectedEof {
                needed: 8,
                remaining: bytes.len(),
            });
        };
        Ok(u64::from_be_bytes([*a, *b, *c, *d, *e, *f, *g, *h]))
    }

    /// Reads a big-endian signed 64-bit integer.
    pub fn read_i64(&mut self) -> Result<i64, CodecError> {
        let bytes = self.read_exact(8)?;
        let [a, b, c, d, e, f, g, h] = bytes else {
            return Err(CodecError::UnexpectedEof {
                needed: 8,
                remaining: bytes.len(),
            });
        };
        Ok(i64::from_be_bytes([*a, *b, *c, *d, *e, *f, *g, *h]))
    }

    /// Reads a canonical 32-byte digest.
    pub fn read_digest(&mut self) -> Result<EvidenceDigest, CodecError> {
        let bytes = self.read_exact(SHA256_DIGEST_BYTES)?;
        let array = <[u8; SHA256_DIGEST_BYTES]>::try_from(bytes).map_err(|_| {
            CodecError::UnexpectedEof {
                needed: SHA256_DIGEST_BYTES,
                remaining: bytes.len(),
            }
        })?;
        Ok(EvidenceDigest::from_bytes(array))
    }

    /// Reads a length-prefixed byte slice under `max_blob_bytes`.
    pub fn read_bytes(&mut self) -> Result<&'a [u8], CodecError> {
        let encoded_length = self.read_u64()?;
        let length = usize::try_from(encoded_length).map_err(|_| CodecError::LengthOverflow)?;
        if length > self.limits.max_blob_bytes {
            return Err(CodecError::BoundExceeded {
                field: "blob_bytes",
                actual: length,
                maximum: self.limits.max_blob_bytes,
            });
        }
        self.read_exact(length)
    }

    /// Reads a length-prefixed UTF-8 string under `max_string_bytes`.
    pub fn read_str(&mut self) -> Result<&'a str, CodecError> {
        let encoded_length = self.read_u64()?;
        let length = usize::try_from(encoded_length).map_err(|_| CodecError::LengthOverflow)?;
        if length > self.limits.max_string_bytes {
            return Err(CodecError::BoundExceeded {
                field: "string_bytes",
                actual: length,
                maximum: self.limits.max_string_bytes,
            });
        }
        let bytes = self.read_exact(length)?;
        std::str::from_utf8(bytes).map_err(CodecError::InvalidUtf8)
    }

    /// Requires the entire input to have been consumed.
    pub fn finish(self) -> Result<(), CodecError> {
        let remaining = self.input.len().saturating_sub(self.cursor);
        if remaining == 0 {
            Ok(())
        } else {
            Err(CodecError::TrailingBytes { remaining })
        }
    }

    fn read_exact(&mut self, length: usize) -> Result<&'a [u8], CodecError> {
        let end = self
            .cursor
            .checked_add(length)
            .ok_or(CodecError::LengthOverflow)?;
        let remaining = self.input.len().saturating_sub(self.cursor);
        let bytes = self
            .input
            .get(self.cursor..end)
            .ok_or(CodecError::UnexpectedEof {
                needed: length,
                remaining,
            })?;
        self.cursor = end;
        Ok(bytes)
    }
}

/// Stable failures for canonical encoding, hashing, and decoding.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum CodecError {
    /// A platform-sized length cannot be represented by the canonical format.
    LengthOverflow,
    /// More bytes were required than remained in the payload.
    UnexpectedEof {
        /// Number of requested bytes.
        needed: usize,
        /// Number of bytes available at the cursor.
        remaining: usize,
    },
    /// A domain-separated stream did not contain its exact declared payload length.
    DomainLengthMismatch {
        /// Declared payload bytes.
        expected: u64,
        /// Observed payload bytes.
        observed: u64,
    },
    /// A decoded field exceeded its configured hard limit.
    BoundExceeded {
        /// Stable field identifier.
        field: &'static str,
        /// Observed size.
        actual: usize,
        /// Maximum admitted size.
        maximum: usize,
    },
    /// A boolean byte was neither zero nor one.
    InvalidBoolean(u8),
    /// A string was not valid UTF-8.
    InvalidUtf8(Utf8Error),
    /// Valid data remained after the expected object ended.
    TrailingBytes {
        /// Number of trailing bytes.
        remaining: usize,
    },
}

impl Display for CodecError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::LengthOverflow => formatter.write_str("canonical length field overflow"),
            Self::UnexpectedEof { needed, remaining } => write!(
                formatter,
                "canonical payload ended early: needed {needed} bytes, {remaining} remain"
            ),
            Self::DomainLengthMismatch { expected, observed } => write!(
                formatter,
                "domain-separated payload declared {expected} bytes but observed {observed}"
            ),
            Self::BoundExceeded {
                field,
                actual,
                maximum,
            } => write!(
                formatter,
                "canonical field {field} is {actual} bytes; maximum is {maximum}"
            ),
            Self::InvalidBoolean(value) => {
                write!(formatter, "canonical boolean byte must be 0 or 1; received {value}")
            }
            Self::InvalidUtf8(error) => write!(formatter, "canonical string is not UTF-8: {error}"),
            Self::TrailingBytes { remaining } => {
                write!(formatter, "canonical payload has {remaining} trailing bytes")
            }
        }
    }
}

impl Error for CodecError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::InvalidUtf8(error) => Some(error),
            _ => None,
        }
    }
}

const fn choose(x: u32, y: u32, z: u32) -> u32 {
    (x & y) ^ (!x & z)
}

const fn majority(x: u32, y: u32, z: u32) -> u32 {
    (x & y) ^ (x & z) ^ (y & z)
}

const fn big_sigma0(value: u32) -> u32 {
    value.rotate_right(2) ^ value.rotate_right(13) ^ value.rotate_right(22)
}

const fn big_sigma1(value: u32) -> u32 {
    value.rotate_right(6) ^ value.rotate_right(11) ^ value.rotate_right(25)
}

const fn small_sigma0(value: u32) -> u32 {
    value.rotate_right(7) ^ value.rotate_right(18) ^ (value >> 3)
}

const fn small_sigma1(value: u32) -> u32 {
    value.rotate_right(17) ^ value.rotate_right(19) ^ (value >> 10)
}

#[cfg(test)]
mod tests {
    use super::{
        CodecError, DecodeLimits, Decoder, DomainHasher, Encoder, Sha256, hash_bytes,
        hash_domain,
    };
    use fdgr_types::DigestDomain;

    #[test]
    fn sha256_matches_published_vectors() {
        assert_eq!(
            hash_bytes(b"").as_str(),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        );
        assert_eq!(
            hash_bytes(b"abc").as_str(),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );
        assert_eq!(
            hash_bytes(b"The quick brown fox jumps over the lazy dog").as_str(),
            "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"
        );
    }

    #[test]
    fn streaming_sha256_matches_one_shot() {
        let mut state = Sha256::new();
        state.update(b"The quick ");
        state.update(b"brown fox jumps over the lazy dog");
        assert_eq!(state.finalize(), hash_bytes(b"The quick brown fox jumps over the lazy dog"));
    }

    #[test]
    fn domain_separation_changes_identity() {
        let left = DigestDomain::parse("fdgr.left/1");
        let right = DigestDomain::parse("fdgr.right/1");
        assert!(left.is_ok());
        assert!(right.is_ok());
        if let (Ok(left), Ok(right)) = (left, right) {
            let left_hash = hash_domain(&left, b"same");
            let right_hash = hash_domain(&right, b"same");
            assert!(matches!((left_hash, right_hash), (Ok(left), Ok(right)) if left != right));
        }
    }

    #[test]
    fn streaming_domain_hash_requires_exact_length() {
        let domain = DigestDomain::parse("fdgr.stream/1");
        assert!(domain.is_ok());
        if let Ok(domain) = domain {
            let state = DomainHasher::new(&domain, 5);
            assert!(state.is_ok());
            if let Ok(mut state) = state {
                assert!(state.update(b"hello").is_ok());
                assert_eq!(state.finalize(), hash_domain(&domain, b"hello"));
            }

            let short = DomainHasher::new(&domain, 5);
            assert!(short.is_ok());
            if let Ok(mut short) = short {
                assert!(short.update(b"hell").is_ok());
                assert!(matches!(
                    short.finalize(),
                    Err(CodecError::DomainLengthMismatch {
                        expected: 5,
                        observed: 4
                    })
                ));
            }

            let long = DomainHasher::new(&domain, 4);
            assert!(long.is_ok());
            if let Ok(mut long) = long {
                assert!(matches!(
                    long.update(b"hello"),
                    Err(CodecError::DomainLengthMismatch {
                        expected: 4,
                        observed: 5
                    })
                ));
            }
        }
    }

    #[test]
    fn canonical_codec_round_trips_and_rejects_trailing_data() {
        let digest = hash_bytes(b"payload");
        let mut encoder = Encoder::new();
        encoder.put_u8(7);
        encoder.put_bool(true);
        encoder.put_u16(513);
        encoder.put_u32(70_000);
        encoder.put_u64(9_000_000);
        encoder.put_i64(-42);
        encoder.put_digest(&digest);
        assert!(encoder.put_str("hello").is_ok());
        assert!(encoder.put_bytes(b"world").is_ok());
        let bytes = encoder.into_bytes();

        let decoder = Decoder::new(&bytes, DecodeLimits::CONTROL_PLANE);
        assert!(decoder.is_ok());
        let Ok(mut decoder) = decoder else {
            return;
        };
        assert_eq!(decoder.read_u8(), Ok(7));
        assert_eq!(decoder.read_bool(), Ok(true));
        assert_eq!(decoder.read_u16(), Ok(513));
        assert_eq!(decoder.read_u32(), Ok(70_000));
        assert_eq!(decoder.read_u64(), Ok(9_000_000));
        assert_eq!(decoder.read_i64(), Ok(-42));
        assert_eq!(decoder.read_digest(), Ok(digest));
        assert_eq!(decoder.read_str(), Ok("hello"));
        assert_eq!(decoder.read_bytes(), Ok(b"world".as_ref()));
        assert_eq!(decoder.finish(), Ok(()));

        let mut with_trailer = bytes;
        with_trailer.push(0);
        let decoder = Decoder::new(&with_trailer, DecodeLimits::CONTROL_PLANE);
        assert!(decoder.is_ok());
        let Ok(mut decoder) = decoder else {
            return;
        };
        assert_eq!(decoder.read_u8(), Ok(7));
        assert!(matches!(decoder.finish(), Err(CodecError::TrailingBytes { .. })));
    }

    #[test]
    fn decoder_enforces_blob_bounds_before_slicing() {
        let mut encoder = Encoder::new();
        assert!(encoder.put_bytes(b"12345").is_ok());
        let limits = DecodeLimits {
            max_total_bytes: 64,
            max_blob_bytes: 4,
            max_string_bytes: 4,
        };
        let decoder = Decoder::new(encoder.as_bytes(), limits);
        assert!(decoder.is_ok());
        let Ok(mut decoder) = decoder else {
            return;
        };
        assert!(matches!(
            decoder.read_bytes(),
            Err(CodecError::BoundExceeded {
                field: "blob_bytes",
                actual: 5,
                maximum: 4
            })
        ));
    }
}
