#![forbid(unsafe_code)]
#![allow(clippy::indexing_slicing)]
//! Checked seek/read and fixed-width ISO BMFF table helpers.

use super::{BoxHeader, ParseLimits};
use crate::{FourCc, MediaError};
use std::io::{ErrorKind, Read, Seek, SeekFrom};

pub(super) fn read_box_header<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
    parent_end: u64,
    allow_zero_size: bool,
) -> Result<BoxHeader, MediaError> {
    let available = parent_end.saturating_sub(offset);
    if available < 8 {
        return Err(MediaError::Truncated {
            offset,
            needed: 8,
            available,
        });
    }
    let size32 = u64::from(read_u32(reader, offset)?);
    let box_type = read_fourcc(reader, checked_add(offset, 4)?)?;
    let mut header_size = 8_u64;
    let size = if size32 == 1 {
        if available < 16 {
            return Err(MediaError::Truncated {
                offset,
                needed: 16,
                available,
            });
        }
        header_size = 16;
        read_u64(reader, checked_add(offset, 8)?)?
    } else if size32 == 0 {
        if !allow_zero_size {
            return Err(MediaError::NestedZeroSizedBox { offset, box_type });
        }
        available
    } else {
        size32
    };
    if box_type == FourCc::UUID {
        header_size = checked_add(header_size, 16)?;
    }
    if size < header_size {
        return Err(MediaError::InvalidBoxSize {
            offset,
            size,
            header_size,
            parent_end,
        });
    }
    let end = checked_add(offset, size)?;
    if end > parent_end {
        return Err(MediaError::InvalidBoxSize {
            offset,
            size,
            header_size,
            parent_end,
        });
    }
    Ok(BoxHeader {
        box_type,
        payload_start: checked_add(offset, header_size)?,
        end,
    })
}

pub(super) fn read_full_box_version<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
) -> Result<u8, MediaError> {
    require_payload(header, 4)?;
    read_u8(reader, header.payload_start)
}

pub(super) fn require_version_zero<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
) -> Result<(), MediaError> {
    let version = read_full_box_version(reader, header)?;
    if version == 0 {
        Ok(())
    } else {
        Err(MediaError::UnsupportedVersion {
            box_type: header.box_type,
            version,
        })
    }
}

pub(super) fn table_entry_count<R: Read + Seek>(
    reader: &mut R,
    header: BoxHeader,
    box_type: FourCc,
    entry_bytes: u64,
    limits: ParseLimits,
) -> Result<u64, MediaError> {
    require_payload(header, 8)?;
    let count = u64::from(read_u32(
        reader,
        checked_add(header.payload_start, 4)?,
    )?);
    enforce_entries(box_type, count, limits)?;
    let table_bytes = count
        .checked_mul(entry_bytes)
        .ok_or(MediaError::ArithmeticOverflow)?;
    enforce_table_bytes(box_type, table_bytes, limits)?;
    require_exact_payload(header, checked_add(8, table_bytes)?)?;
    Ok(count)
}

pub(super) fn enforce_entries(
    box_type: FourCc,
    count: u64,
    limits: ParseLimits,
) -> Result<(), MediaError> {
    if count > limits.max_table_entries {
        Err(MediaError::TableEntryLimitExceeded {
            box_type,
            actual: count,
            maximum: limits.max_table_entries,
        })
    } else {
        Ok(())
    }
}

pub(super) fn enforce_table_bytes(
    box_type: FourCc,
    bytes: u64,
    limits: ParseLimits,
) -> Result<(), MediaError> {
    if bytes > limits.max_table_bytes {
        Err(MediaError::TableByteLimitExceeded {
            box_type,
            actual: bytes,
            maximum: limits.max_table_bytes,
        })
    } else {
        Ok(())
    }
}

pub(super) fn require_payload(header: BoxHeader, needed: u64) -> Result<(), MediaError> {
    let available = header.end.saturating_sub(header.payload_start);
    if available < needed {
        Err(MediaError::Truncated {
            offset: header.payload_start,
            needed,
            available,
        })
    } else {
        Ok(())
    }
}

pub(super) fn require_exact_payload(
    header: BoxHeader,
    expected: u64,
) -> Result<(), MediaError> {
    let available = header.end.saturating_sub(header.payload_start);
    if available == expected {
        Ok(())
    } else {
        Err(MediaError::InvalidTableLength {
            box_type: header.box_type,
            expected,
            available,
        })
    }
}

pub(super) fn read_bounded_bytes<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
    length: u64,
    box_type: FourCc,
    limits: ParseLimits,
) -> Result<Vec<u8>, MediaError> {
    enforce_table_bytes(box_type, length, limits)?;
    let length = usize::try_from(length).map_err(|_| MediaError::ArithmeticOverflow)?;
    let mut bytes = vec![0_u8; length];
    read_exact_at(reader, offset, &mut bytes)?;
    Ok(bytes)
}

pub(super) fn read_u8<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
) -> Result<u8, MediaError> {
    let mut bytes = [0_u8; 1];
    read_exact_at(reader, offset, &mut bytes)?;
    Ok(bytes[0])
}

pub(super) fn read_u32<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
) -> Result<u32, MediaError> {
    let mut bytes = [0_u8; 4];
    read_exact_at(reader, offset, &mut bytes)?;
    Ok(u32::from_be_bytes(bytes))
}

pub(super) fn read_u64<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
) -> Result<u64, MediaError> {
    let mut bytes = [0_u8; 8];
    read_exact_at(reader, offset, &mut bytes)?;
    Ok(u64::from_be_bytes(bytes))
}

pub(super) fn read_fourcc<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
) -> Result<FourCc, MediaError> {
    let mut bytes = [0_u8; 4];
    read_exact_at(reader, offset, &mut bytes)?;
    Ok(FourCc::new(bytes))
}

fn read_exact_at<R: Read + Seek>(
    reader: &mut R,
    offset: u64,
    bytes: &mut [u8],
) -> Result<(), MediaError> {
    reader
        .seek(SeekFrom::Start(offset))
        .map_err(|source| MediaError::Io {
            operation: "seek",
            offset,
            source,
        })?;
    let mut observed = 0_usize;
    while observed < bytes.len() {
        let Some(destination) = bytes.get_mut(observed..) else {
            return Err(MediaError::ArithmeticOverflow);
        };
        match reader.read(destination) {
            Ok(0) => {
                let remaining = bytes.len().saturating_sub(observed);
                return Err(MediaError::Truncated {
                    offset: checked_add(
                        offset,
                        u64::try_from(observed).map_err(|_| MediaError::ArithmeticOverflow)?,
                    )?,
                    needed: u64::try_from(remaining)
                        .map_err(|_| MediaError::ArithmeticOverflow)?,
                    available: 0,
                });
            }
            Ok(read) => {
                observed = observed
                    .checked_add(read)
                    .ok_or(MediaError::ArithmeticOverflow)?;
            }
            Err(source) if source.kind() == ErrorKind::Interrupted => {}
            Err(source) => {
                return Err(MediaError::Io {
                    operation: "read",
                    offset: checked_add(
                        offset,
                        u64::try_from(observed).map_err(|_| MediaError::ArithmeticOverflow)?,
                    )?,
                    source,
                });
            }
        }
    }
    Ok(())
}

pub(super) fn checked_add(left: u64, right: u64) -> Result<u64, MediaError> {
    left.checked_add(right).ok_or(MediaError::ArithmeticOverflow)
}
