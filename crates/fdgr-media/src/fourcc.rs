#![forbid(unsafe_code)]
//! Canonical four-character box and handler codes.

use std::fmt::{self, Display, Formatter};

/// Four exact bytes used as an ISO Base Media File Format type or handler code.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct FourCc([u8; 4]);

impl FourCc {
    /// Constructs a code from exact bytes.
    #[must_use]
    pub const fn new(bytes: [u8; 4]) -> Self {
        Self(bytes)
    }

    /// Returns the exact bytes.
    #[must_use]
    pub const fn as_bytes(self) -> [u8; 4] {
        self.0
    }

    pub(crate) const FTYP: Self = Self::new(*b"ftyp");
    pub(crate) const MOOV: Self = Self::new(*b"moov");
    pub(crate) const MVHD: Self = Self::new(*b"mvhd");
    pub(crate) const TRAK: Self = Self::new(*b"trak");
    pub(crate) const TKHD: Self = Self::new(*b"tkhd");
    pub(crate) const MDIA: Self = Self::new(*b"mdia");
    pub(crate) const MDHD: Self = Self::new(*b"mdhd");
    pub(crate) const HDLR: Self = Self::new(*b"hdlr");
    pub(crate) const MINF: Self = Self::new(*b"minf");
    pub(crate) const STBL: Self = Self::new(*b"stbl");
    pub(crate) const STSD: Self = Self::new(*b"stsd");
    pub(crate) const STTS: Self = Self::new(*b"stts");
    pub(crate) const CTTS: Self = Self::new(*b"ctts");
    pub(crate) const STSZ: Self = Self::new(*b"stsz");
    pub(crate) const STZ2: Self = Self::new(*b"stz2");
    pub(crate) const STCO: Self = Self::new(*b"stco");
    pub(crate) const CO64: Self = Self::new(*b"co64");
    pub(crate) const STSC: Self = Self::new(*b"stsc");
    pub(crate) const STSS: Self = Self::new(*b"stss");
    pub(crate) const EDTS: Self = Self::new(*b"edts");
    pub(crate) const DINF: Self = Self::new(*b"dinf");
    pub(crate) const DREF: Self = Self::new(*b"dref");
    pub(crate) const UDTA: Self = Self::new(*b"udta");
    pub(crate) const META: Self = Self::new(*b"meta");
    pub(crate) const MDAT: Self = Self::new(*b"mdat");
    pub(crate) const MOOF: Self = Self::new(*b"moof");
    pub(crate) const TRAF: Self = Self::new(*b"traf");
    pub(crate) const UUID: Self = Self::new(*b"uuid");
}

impl Display for FourCc {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        for byte in self.0 {
            if byte.is_ascii_graphic() || byte == b' ' {
                write!(formatter, "{}", char::from(byte))?;
            } else {
                write!(formatter, "\\x{byte:02x}")?;
            }
        }
        Ok(())
    }
}
