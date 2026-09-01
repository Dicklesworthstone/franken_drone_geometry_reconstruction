#![forbid(unsafe_code)]
//! Canonical event-kind vocabulary.

use crate::MAX_EVENT_KIND_BYTES;
use std::error::Error;
use std::fmt::{self, Display, Formatter};

/// Canonical lower-snake-case event family.
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct EventKind(String);

impl EventKind {
    /// Parses a bounded lower-snake-case event family.
    ///
    /// # Errors
    ///
    /// Returns a typed error for empty, oversized, or noncanonical names.
    pub fn parse(value: impl AsRef<str>) -> Result<Self, EventKindError> {
        let value = value.as_ref();
        if value.is_empty() {
            return Err(EventKindError::Empty);
        }
        if value.len() > MAX_EVENT_KIND_BYTES {
            return Err(EventKindError::TooLong {
                actual: value.len(),
                maximum: MAX_EVENT_KIND_BYTES,
            });
        }
        let mut segments = value.split('_');
        if !segments.all(|segment| {
            !segment.is_empty()
                && segment
                    .bytes()
                    .next()
                    .is_some_and(|byte| byte.is_ascii_lowercase())
                && segment
                    .bytes()
                    .all(|byte| byte.is_ascii_lowercase() || byte.is_ascii_digit())
        }) {
            return Err(EventKindError::NonCanonical);
        }
        Ok(Self(value.to_owned()))
    }

    /// Returns the canonical event-family text.
    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl Display for EventKind {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.0)
    }
}

/// Event-family validation failure.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum EventKindError {
    /// Name was empty.
    Empty,
    /// Name exceeded the hard byte bound.
    TooLong {
        /// Observed bytes.
        actual: usize,
        /// Maximum bytes.
        maximum: usize,
    },
    /// Name was not canonical lower snake case.
    NonCanonical,
}

impl Display for EventKindError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> fmt::Result {
        match self {
            Self::Empty => formatter.write_str("event kind must not be empty"),
            Self::TooLong { actual, maximum } => {
                write!(formatter, "event kind is {actual} bytes; maximum is {maximum}")
            }
            Self::NonCanonical => {
                formatter.write_str("event kind must be canonical lower snake case")
            }
        }
    }
}

impl Error for EventKindError {}

#[cfg(test)]
mod tests {
    use super::{EventKind, EventKindError};

    #[test]
    fn event_kind_is_canonical() {
        assert!(EventKind::parse("media_imported").is_ok());
        assert!(matches!(
            EventKind::parse("MediaImported"),
            Err(EventKindError::NonCanonical)
        ));
        assert!(matches!(
            EventKind::parse("media__imported"),
            Err(EventKindError::NonCanonical)
        ));
    }
}
