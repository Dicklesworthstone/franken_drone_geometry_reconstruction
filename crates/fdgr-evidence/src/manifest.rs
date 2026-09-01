#![forbid(unsafe_code)]
//! Canonical object manifests and exact-byte verification.

use crate::{
    CHUNK_DOMAIN, MANIFEST_DOMAIN, MANIFEST_VERSION, MAX_CHUNKS, MAX_CHUNK_SIZE,
    MAX_MANIFEST_BYTES, OBJECT_DOMAIN,
};
use fdgr_codec::{CodecError, DecodeLimits, Decoder, DomainHasher, Encoder, hash_domain};
use fdgr_types::{DigestDomain, DomainError, EvidenceDigest};
use std::error::Error;
use std::fmt::{self, Display, Formatter};
use std::fs::File;
use std::io::{self, Read};
use std::path::Path;

/// One exact immutable object chunk.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ChunkDescriptor {
    /// Zero-based canonical chunk index.
    pub index: u64,
    /// Byte offset in the logical object.
    pub offset: u64,
    /// Number of bytes in this chunk.
    pub length: u32,
    /// Domain-separated identity of the exact chunk bytes.
    pub digest: EvidenceDigest,
}

/// Canonical object manifest binding logical bytes to ordered chunks.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ObjectManifest {
    /// Logical byte length of the complete object.
    pub object_length: u64,
    /// Nominal chunk size used for all non-final chunks.
    pub chunk_size: u32,
    /// Domain-separated identity of the complete logical object.
    pub object_digest: EvidenceDigest,
    /// Ordered, contiguous chunk descriptors.
    pub chunks: Vec<ChunkDescriptor>,
    /// Domain-separated identity of the canonical manifest body.
    pub manifest_digest: EvidenceDigest,
}

impl ObjectManifest {
    /// Builds a manifest from in-memory bytes.
    ///
    /// # Errors
    ///
    /// Returns a typed error when bounds, lengths, domains, or canonical encoding fail.
    pub fn build(bytes: &[u8], chunk_size: u32) -> Result<Self, ManifestError> {
        let length = u64::try_from(bytes.len()).map_err(|_| ManifestError::LengthOverflow)?;
        let mut reader = io::Cursor::new(bytes);
        Self::build_from_reader(&mut reader, length, chunk_size)
    }

    /// Builds a manifest while reading exactly `object_length` bytes.
    ///
    /// # Errors
    ///
    /// Returns a typed error for invalid bounds, I/O failure, early EOF, trailing input,
    /// encoding failure, or domain-hash failure.
    pub fn build_from_reader<R: Read>(
        reader: &mut R,
        object_length: u64,
        chunk_size: u32,
    ) -> Result<Self, ManifestError> {
        validate_chunk_size(chunk_size)?;
        let chunk_domain = domain(CHUNK_DOMAIN)?;
        let object_domain = domain(OBJECT_DOMAIN)?;
        let mut object_hasher = DomainHasher::new(&object_domain, object_length)?;
        let mut remaining = object_length;
        let mut offset = 0_u64;
        let mut chunks = Vec::new();

        while remaining > 0 {
            if chunks.len() >= MAX_CHUNKS {
                return Err(ManifestError::TooManyChunks {
                    actual: chunks.len().saturating_add(1),
                    maximum: MAX_CHUNKS,
                });
            }
            let target_u64 = remaining.min(u64::from(chunk_size));
            let target = usize::try_from(target_u64).map_err(|_| ManifestError::LengthOverflow)?;
            let mut bytes = vec![0_u8; target];
            read_exact_counted(reader, &mut bytes, offset)?;
            object_hasher.update(&bytes)?;
            let digest = hash_domain(&chunk_domain, &bytes)?;
            let index = u64::try_from(chunks.len()).map_err(|_| ManifestError::LengthOverflow)?;
            let length = u32::try_from(bytes.len()).map_err(|_| ManifestError::LengthOverflow)?;
            chunks.push(ChunkDescriptor {
                index,
                offset,
                length,
                digest,
            });
            offset = offset
                .checked_add(u64::from(length))
                .ok_or(ManifestError::LengthOverflow)?;
            remaining = remaining.saturating_sub(u64::from(length));
        }

        reject_trailing_input(reader)?;
        let object_digest = object_hasher.finalize()?;
        Self::from_parts(object_length, chunk_size, object_digest, chunks)
    }

    /// Decodes and validates canonical manifest bytes.
    ///
    /// # Errors
    ///
    /// Returns a typed decoding or structural error. Unknown versions fail closed.
    pub fn from_canonical_bytes(bytes: &[u8]) -> Result<Self, ManifestError> {
        let limits = DecodeLimits {
            max_total_bytes: MAX_MANIFEST_BYTES,
            max_blob_bytes: 0,
            max_string_bytes: 0,
        };
        let mut decoder = Decoder::new(bytes, limits)?;
        let version = decoder.read_u16()?;
        if version != MANIFEST_VERSION {
            return Err(ManifestError::UnsupportedVersion(version));
        }
        let object_length = decoder.read_u64()?;
        let chunk_size = decoder.read_u32()?;
        let object_digest = decoder.read_digest()?;
        let count = usize::try_from(decoder.read_u64()?)
            .map_err(|_| ManifestError::LengthOverflow)?;
        if count > MAX_CHUNKS {
            return Err(ManifestError::TooManyChunks {
                actual: count,
                maximum: MAX_CHUNKS,
            });
        }
        let mut chunks = Vec::with_capacity(count);
        for _ in 0..count {
            chunks.push(ChunkDescriptor {
                index: decoder.read_u64()?,
                offset: decoder.read_u64()?,
                length: decoder.read_u32()?,
                digest: decoder.read_digest()?,
            });
        }
        let manifest_digest = decoder.read_digest()?;
        decoder.finish()?;
        let manifest = Self {
            object_length,
            chunk_size,
            object_digest,
            chunks,
            manifest_digest,
        };
        manifest.validate_structure()?;
        Ok(manifest)
    }

    /// Encodes the complete manifest, including its manifest digest.
    ///
    /// # Errors
    ///
    /// Returns a structural or canonical-length error.
    pub fn to_canonical_bytes(&self) -> Result<Vec<u8>, ManifestError> {
        self.validate_structure()?;
        let mut encoder = Encoder::with_capacity(96 + self.chunks.len().saturating_mul(52));
        encode_body(self, &mut encoder)?;
        encoder.put_digest(&self.manifest_digest);
        Ok(encoder.into_bytes())
    }

    /// Revalidates ordering, lengths, offsets, count bounds, and manifest identity.
    ///
    /// # Errors
    ///
    /// Returns the first stable structural error in canonical chunk order.
    pub fn validate_structure(&self) -> Result<(), ManifestError> {
        validate_chunk_size(self.chunk_size)?;
        if self.chunks.len() > MAX_CHUNKS {
            return Err(ManifestError::TooManyChunks {
                actual: self.chunks.len(),
                maximum: MAX_CHUNKS,
            });
        }
        if self.object_length == 0 && !self.chunks.is_empty() {
            return Err(ManifestError::ObjectLengthMismatch {
                expected: 0,
                observed: represented_length(&self.chunks)?,
            });
        }
        if self.object_length > 0 && self.chunks.is_empty() {
            return Err(ManifestError::ObjectLengthMismatch {
                expected: self.object_length,
                observed: 0,
            });
        }

        let mut expected_offset = 0_u64;
        for (position, chunk) in self.chunks.iter().enumerate() {
            let expected_index =
                u64::try_from(position).map_err(|_| ManifestError::LengthOverflow)?;
            if chunk.index != expected_index {
                return Err(ManifestError::ChunkIndexMismatch {
                    expected: expected_index,
                    observed: chunk.index,
                });
            }
            if chunk.offset != expected_offset {
                return Err(ManifestError::ChunkOffsetMismatch {
                    index: chunk.index,
                    expected: expected_offset,
                    observed: chunk.offset,
                });
            }
            if chunk.length == 0 || chunk.length > self.chunk_size {
                return Err(ManifestError::InvalidChunkLength {
                    index: chunk.index,
                    length: chunk.length,
                    maximum: self.chunk_size,
                });
            }
            let is_final = position.saturating_add(1) == self.chunks.len();
            if !is_final && chunk.length != self.chunk_size {
                return Err(ManifestError::NonFinalChunkLength {
                    index: chunk.index,
                    expected: self.chunk_size,
                    observed: chunk.length,
                });
            }
            expected_offset = expected_offset
                .checked_add(u64::from(chunk.length))
                .ok_or(ManifestError::LengthOverflow)?;
        }
        if expected_offset != self.object_length {
            return Err(ManifestError::ObjectLengthMismatch {
                expected: self.object_length,
                observed: expected_offset,
            });
        }
        let expected = compute_manifest_digest(self)?;
        if expected != self.manifest_digest {
            return Err(ManifestError::ManifestDigestMismatch {
                expected,
                observed: self.manifest_digest.clone(),
            });
        }
        Ok(())
    }

    /// Verifies exact in-memory bytes against object and chunk identities.
    ///
    /// # Errors
    ///
    /// Returns the first structural, length, object-digest, or chunk-digest mismatch.
    pub fn verify_bytes(&self, bytes: &[u8]) -> Result<(), ManifestError> {
        let observed = u64::try_from(bytes.len()).map_err(|_| ManifestError::LengthOverflow)?;
        if observed != self.object_length {
            return Err(ManifestError::ObjectLengthMismatch {
                expected: self.object_length,
                observed,
            });
        }
        let mut reader = io::Cursor::new(bytes);
        self.verify_reader(&mut reader)
    }

    /// Verifies a reader without loading the complete object into memory.
    ///
    /// # Errors
    ///
    /// Returns the first structural, I/O, EOF, trailing-input, object, or chunk mismatch.
    pub fn verify_reader<R: Read>(&self, reader: &mut R) -> Result<(), ManifestError> {
        self.validate_structure()?;
        let object_domain = domain(OBJECT_DOMAIN)?;
        let chunk_domain = domain(CHUNK_DOMAIN)?;
        let mut object_hasher = DomainHasher::new(&object_domain, self.object_length)?;
        for chunk in &self.chunks {
            let length = usize::try_from(chunk.length)
                .map_err(|_| ManifestError::LengthOverflow)?;
            let mut bytes = vec![0_u8; length];
            read_exact_counted(reader, &mut bytes, chunk.offset)?;
            object_hasher.update(&bytes)?;
            let observed = hash_domain(&chunk_domain, &bytes)?;
            if observed != chunk.digest {
                return Err(ManifestError::ChunkDigestMismatch {
                    index: chunk.index,
                    expected: chunk.digest.clone(),
                    observed,
                });
            }
        }
        reject_trailing_input(reader)?;
        let observed = object_hasher.finalize()?;
        if observed != self.object_digest {
            return Err(ManifestError::ObjectDigestMismatch {
                expected: self.object_digest.clone(),
                observed,
            });
        }
        Ok(())
    }

    fn from_parts(
        object_length: u64,
        chunk_size: u32,
        object_digest: EvidenceDigest,
        chunks: Vec<ChunkDescriptor>,
    ) -> Result<Self, ManifestError> {
        let mut manifest = Self {
            object_length,
            chunk_size,
            object_digest,
            chunks,
            manifest_digest: EvidenceDigest::from_bytes([0_u8; 32]),
        };
        manifest.manifest_digest = compute_manifest_digest(&manifest)?;
        manifest.validate_structure()?;
        Ok(manifest)
    }
}

/// Computes a streaming manifest for a regular file using its observed metadata length.
///
/// # Errors
///
/// Returns a typed I/O or manifest error. The source is never modified.
pub fn build_file_manifest(
    path: impl AsRef<Path>,
    chunk_size: u32,
) -> Result<ObjectManifest, ManifestError> {
    let path = path.as_ref();
    let metadata = path.metadata()?;
    if !metadata.is_file() {
        return Err(ManifestError::NotRegularFile);
    }
    let mut file = File::open(path)?;
    ObjectManifest::build_from_reader(&mut file, metadata.len(), chunk_size)
}

/// Verifies a regular file against an existing manifest without modifying either.
///
/// # Errors
///
/// Returns a typed I/O or manifest mismatch.
pub fn verify_file(
    path: impl AsRef<Path>,
    manifest: &ObjectManifest,
) -> Result<(), ManifestError> {
    let path = path.as_ref();
    let metadata = path.metadata()?;
    if !metadata.is_file() {
        return Err(ManifestError::NotRegularFile);
    }
    if metadata.len() != manifest.object_length {
        return Err(ManifestError::ObjectLengthMismatch {
            expected: manifest.object_length,
            observed: metadata.len(),
        });
    }
    let mut file = File::open(path)?;
    manifest.verify_reader(&mut file)
}

/// Stable object-manifest failures.
#[derive(Debug)]
pub enum ManifestError {
    /// The requested chunk size was zero or above the hard limit.
    InvalidChunkSize {
        /// Requested bytes.
        actual: u32,
        /// Maximum bytes.
        maximum: u32,
    },
    /// The manifest exceeded the hard chunk-count limit.
    TooManyChunks {
        /// Observed count.
        actual: usize,
        /// Maximum count.
        maximum: usize,
    },
    /// A canonical or platform length conversion overflowed.
    LengthOverflow,
    /// The source ended before the declared object length.
    UnexpectedEof {
        /// Exact byte offset at which bytes were still required.
        offset: u64,
        /// Number of bytes required by the current chunk.
        needed: usize,
        /// Number of bytes read into the current chunk.
        observed: usize,
    },
    /// Bytes remained after the exact declared object length.
    TrailingInput,
    /// The source path was not a regular file.
    NotRegularFile,
    /// Chunk index differs from canonical position.
    ChunkIndexMismatch {
        /// Expected index.
        expected: u64,
        /// Observed index.
        observed: u64,
    },
    /// Chunk offset is not contiguous.
    ChunkOffsetMismatch {
        /// Chunk index.
        index: u64,
        /// Expected byte offset.
        expected: u64,
        /// Observed byte offset.
        observed: u64,
    },
    /// Chunk length was zero or exceeded the manifest chunk size.
    InvalidChunkLength {
        /// Chunk index.
        index: u64,
        /// Observed bytes.
        length: u32,
        /// Maximum bytes.
        maximum: u32,
    },
    /// A non-final chunk was shorter than the declared chunk size.
    NonFinalChunkLength {
        /// Chunk index.
        index: u64,
        /// Expected bytes.
        expected: u32,
        /// Observed bytes.
        observed: u32,
    },
    /// Chunk lengths did not sum to the logical object length.
    ObjectLengthMismatch {
        /// Declared bytes.
        expected: u64,
        /// Observed bytes.
        observed: u64,
    },
    /// Complete object bytes did not match the manifest.
    ObjectDigestMismatch {
        /// Declared identity.
        expected: EvidenceDigest,
        /// Observed identity.
        observed: EvidenceDigest,
    },
    /// One exact chunk did not match its declared identity.
    ChunkDigestMismatch {
        /// Chunk index.
        index: u64,
        /// Declared identity.
        expected: EvidenceDigest,
        /// Observed identity.
        observed: EvidenceDigest,
    },
    /// Canonical manifest body did not match its declared identity.
    ManifestDigestMismatch {
        /// Recomputed identity.
        expected: EvidenceDigest,
        /// Encoded identity.
        observed: EvidenceDigest,
    },
    /// Canonical binary manifest version is unsupported.
    UnsupportedVersion(u16),
    /// Canonical codec failure.
    Codec(CodecError),
    /// Domain-separator validation failure.
    Domain(DomainError),
    /// Filesystem or reader failure.
    Io(io::Error),
}

impl Display for ManifestError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidChunkSize { actual, maximum } => write!(
                formatter,
                "chunk size must be in 1..={maximum} bytes; received {actual}"
            ),
            Self::TooManyChunks { actual, maximum } => {
                write!(formatter, "manifest has {actual} chunks; maximum is {maximum}")
            }
            Self::LengthOverflow => formatter.write_str("manifest length conversion overflow"),
            Self::UnexpectedEof {
                offset,
                needed,
                observed,
            } => write!(
                formatter,
                "source ended at object offset {offset}: needed {needed} bytes, observed {observed}"
            ),
            Self::TrailingInput => {
                formatter.write_str("source contains bytes beyond declared length")
            }
            Self::NotRegularFile => formatter.write_str("source path is not a regular file"),
            Self::ChunkIndexMismatch { expected, observed } => write!(
                formatter,
                "chunk index must be {expected}; observed {observed}"
            ),
            Self::ChunkOffsetMismatch {
                index,
                expected,
                observed,
            } => write!(
                formatter,
                "chunk {index} offset must be {expected}; observed {observed}"
            ),
            Self::InvalidChunkLength {
                index,
                length,
                maximum,
            } => write!(
                formatter,
                "chunk {index} length must be in 1..={maximum}; observed {length}"
            ),
            Self::NonFinalChunkLength {
                index,
                expected,
                observed,
            } => write!(
                formatter,
                "non-final chunk {index} must contain {expected} bytes; observed {observed}"
            ),
            Self::ObjectLengthMismatch { expected, observed } => write!(
                formatter,
                "object length must be {expected} bytes; observed {observed}"
            ),
            Self::ObjectDigestMismatch { expected, observed } => write!(
                formatter,
                "object digest mismatch: expected {expected}, observed {observed}"
            ),
            Self::ChunkDigestMismatch {
                index,
                expected,
                observed,
            } => write!(
                formatter,
                "chunk {index} digest mismatch: expected {expected}, observed {observed}"
            ),
            Self::ManifestDigestMismatch { expected, observed } => write!(
                formatter,
                "manifest digest mismatch: expected {expected}, observed {observed}"
            ),
            Self::UnsupportedVersion(version) => {
                write!(formatter, "unsupported object-manifest version {version}")
            }
            Self::Codec(error) => write!(formatter, "canonical codec error: {error}"),
            Self::Domain(error) => write!(formatter, "domain error: {error}"),
            Self::Io(error) => write!(formatter, "I/O error: {error}"),
        }
    }
}

impl Error for ManifestError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Codec(error) => Some(error),
            Self::Domain(error) => Some(error),
            Self::Io(error) => Some(error),
            _ => None,
        }
    }
}

impl From<CodecError> for ManifestError {
    fn from(error: CodecError) -> Self {
        Self::Codec(error)
    }
}

impl From<DomainError> for ManifestError {
    fn from(error: DomainError) -> Self {
        Self::Domain(error)
    }
}

impl From<io::Error> for ManifestError {
    fn from(error: io::Error) -> Self {
        Self::Io(error)
    }
}

fn validate_chunk_size(chunk_size: u32) -> Result<(), ManifestError> {
    if chunk_size == 0 || chunk_size > MAX_CHUNK_SIZE {
        return Err(ManifestError::InvalidChunkSize {
            actual: chunk_size,
            maximum: MAX_CHUNK_SIZE,
        });
    }
    Ok(())
}

fn domain(value: &str) -> Result<DigestDomain, ManifestError> {
    DigestDomain::parse(value).map_err(ManifestError::from)
}

fn reject_trailing_input<R: Read>(reader: &mut R) -> Result<(), ManifestError> {
    let mut extra = [0_u8; 1];
    if reader.read(&mut extra)? == 0 {
        Ok(())
    } else {
        Err(ManifestError::TrailingInput)
    }
}

fn read_exact_counted<R: Read>(
    reader: &mut R,
    bytes: &mut [u8],
    object_offset: u64,
) -> Result<(), ManifestError> {
    let needed = bytes.len();
    let mut observed = 0_usize;
    while observed < needed {
        let Some(destination) = bytes.get_mut(observed..) else {
            return Err(ManifestError::LengthOverflow);
        };
        let read = reader.read(destination)?;
        if read == 0 {
            let offset = object_offset
                .checked_add(u64::try_from(observed).map_err(|_| ManifestError::LengthOverflow)?)
                .ok_or(ManifestError::LengthOverflow)?;
            return Err(ManifestError::UnexpectedEof {
                offset,
                needed,
                observed,
            });
        }
        observed = observed
            .checked_add(read)
            .ok_or(ManifestError::LengthOverflow)?;
    }
    Ok(())
}

fn represented_length(chunks: &[ChunkDescriptor]) -> Result<u64, ManifestError> {
    let mut length = 0_u64;
    for chunk in chunks {
        length = length
            .checked_add(u64::from(chunk.length))
            .ok_or(ManifestError::LengthOverflow)?;
    }
    Ok(length)
}

fn encode_body(manifest: &ObjectManifest, encoder: &mut Encoder) -> Result<(), ManifestError> {
    encoder.put_u16(MANIFEST_VERSION);
    encoder.put_u64(manifest.object_length);
    encoder.put_u32(manifest.chunk_size);
    encoder.put_digest(&manifest.object_digest);
    let count = u64::try_from(manifest.chunks.len()).map_err(|_| ManifestError::LengthOverflow)?;
    encoder.put_u64(count);
    for chunk in &manifest.chunks {
        encoder.put_u64(chunk.index);
        encoder.put_u64(chunk.offset);
        encoder.put_u32(chunk.length);
        encoder.put_digest(&chunk.digest);
    }
    Ok(())
}

fn compute_manifest_digest(manifest: &ObjectManifest) -> Result<EvidenceDigest, ManifestError> {
    let mut encoder = Encoder::with_capacity(64 + manifest.chunks.len().saturating_mul(52));
    encode_body(manifest, &mut encoder)?;
    let manifest_domain = domain(MANIFEST_DOMAIN)?;
    hash_domain(&manifest_domain, encoder.as_bytes()).map_err(ManifestError::from)
}

#[cfg(test)]
mod tests {
    use super::{ChunkDescriptor, ManifestError, ObjectManifest};
    use crate::DEFAULT_CHUNK_SIZE;
    use fdgr_codec::hash_bytes;
    use fdgr_types::EvidenceDigest;
    use std::io::Cursor;

    #[test]
    fn manifest_is_deterministic_and_round_trips() {
        let first = ObjectManifest::build(b"abcdefghij", 4);
        let second = ObjectManifest::build(b"abcdefghij", 4);
        assert!(first.is_ok());
        assert!(matches!((&first, &second), (Ok(left), Ok(right)) if left == right));
        if let Ok(first) = first {
            assert_eq!(first.object_length, 10);
            assert_eq!(first.chunks.len(), 3);
            assert!(matches!(
                first.chunks.first(),
                Some(chunk) if chunk.index == 0 && chunk.offset == 0 && chunk.length == 4
            ));
            let encoded = first.to_canonical_bytes();
            assert!(encoded.is_ok());
            if let Ok(encoded) = encoded {
                assert!(matches!(
                    ObjectManifest::from_canonical_bytes(&encoded),
                    Ok(ref value) if value == &first
                ));
            }
        }
    }

    #[test]
    fn empty_object_has_no_chunks_and_still_has_identity() {
        assert!(matches!(
            ObjectManifest::build(b"", DEFAULT_CHUNK_SIZE),
            Ok(ref value)
                if value.object_length == 0
                    && value.chunks.is_empty()
                    && value.object_digest.as_str().len() == 64
        ));
    }

    #[test]
    fn manifest_rejects_reordered_or_truncated_chunks() {
        let manifest = ObjectManifest::build(b"abcdefghij", 4);
        assert!(manifest.is_ok());
        if let Ok(manifest) = manifest {
            let mut reordered = manifest.clone();
            reordered.chunks.swap(0, 1);
            assert!(matches!(
                reordered.validate_structure(),
                Err(ManifestError::ChunkIndexMismatch { .. })
                    | Err(ManifestError::ChunkOffsetMismatch { .. })
            ));
            let mut truncated = manifest;
            truncated.chunks.pop();
            assert!(matches!(
                truncated.validate_structure(),
                Err(ManifestError::ObjectLengthMismatch { .. })
                    | Err(ManifestError::ManifestDigestMismatch { .. })
            ));
        }
    }

    #[test]
    fn byte_mutation_is_detected_at_first_affected_chunk() {
        let manifest = ObjectManifest::build(b"abcdefghij", 4);
        assert!(manifest.is_ok());
        if let Ok(manifest) = manifest {
            let mut bytes = b"abcdefghij".to_vec();
            if let Some(byte) = bytes.get_mut(5) {
                *byte = b'X';
            }
            assert!(matches!(
                manifest.verify_bytes(&bytes),
                Err(ManifestError::ChunkDigestMismatch { index: 1, .. })
            ));
        }
    }

    #[test]
    fn reader_length_is_exact() {
        let mut exact = Cursor::new(&b"hello"[..]);
        assert!(ObjectManifest::build_from_reader(&mut exact, 5, 2).is_ok());
        let mut short = Cursor::new(&b"hell"[..]);
        assert!(matches!(
            ObjectManifest::build_from_reader(&mut short, 5, 2),
            Err(ManifestError::UnexpectedEof { .. })
        ));
        let mut long = Cursor::new(&b"hello!"[..]);
        assert!(matches!(
            ObjectManifest::build_from_reader(&mut long, 5, 2),
            Err(ManifestError::TrailingInput)
        ));
    }

    #[test]
    fn manifest_digest_covers_chunk_metadata() {
        let manifest = ObjectManifest::build(b"abcdefgh", 4);
        assert!(manifest.is_ok());
        if let Ok(mut manifest) = manifest {
            if let Some(chunk) = manifest.chunks.first_mut() {
                *chunk = ChunkDescriptor {
                    index: 0,
                    offset: 0,
                    length: 4,
                    digest: EvidenceDigest::from_bytes(hash_bytes(b"zzzz").to_bytes()),
                };
            }
            assert!(matches!(
                manifest.validate_structure(),
                Err(ManifestError::ManifestDigestMismatch { .. })
            ));
        }
    }
}
