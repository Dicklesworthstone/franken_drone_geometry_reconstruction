#![forbid(unsafe_code)]
//! Stable text and JSON renderers for current CLI reference surfaces.

use crate::args::{ManifestViewOptions, OutputFormat};
use fdgr_core::{
    CAPABILITIES_SCHEMA, DOCTOR_SCHEMA, FILE_VERIFICATION_SCHEMA, OBJECT_MANIFEST_VIEW_SCHEMA,
    PLAN_SUMMARY_SCHEMA, STORE_VERIFICATION_SCHEMA, VERSION,
};
use fdgr_evidence::ObjectManifest;
use fdgr_object_store::{IMPORT_RECEIPT_SCHEMA, ImportReceipt};
use std::fmt::Write as _;

pub(crate) fn print_capabilities(format: OutputFormat) -> Result<(), String> {
    match format {
        OutputFormat::Text => {
            for capability in fdgr_core::capabilities() {
                println!(
                    "{}\t{}\t{}",
                    capability.id,
                    capability.status.as_str(),
                    capability.description
                );
            }
        }
        OutputFormat::Json => {
            let mut output = String::new();
            write!(
                output,
                "{{\"schema\":\"{}\",\"version\":\"{}\",\"capabilities\":[",
                json_escape(CAPABILITIES_SCHEMA),
                json_escape(VERSION)
            )
            .map_err(|error| error.to_string())?;
            for (index, capability) in fdgr_core::capabilities().iter().enumerate() {
                if index > 0 {
                    output.push(',');
                }
                write!(
                    output,
                    "{{\"id\":\"{}\",\"status\":\"{}\",\"description\":\"{}\"}}",
                    json_escape(capability.id),
                    json_escape(capability.status.as_str()),
                    json_escape(capability.description)
                )
                .map_err(|error| error.to_string())?;
            }
            output.push_str("]}");
            println!("{output}");
        }
    }
    Ok(())
}

pub(crate) fn print_doctor(format: OutputFormat) -> Result<(), String> {
    let findings = fdgr_core::doctor();
    match format {
        OutputFormat::Text => {
            for finding in findings {
                println!(
                    "{}\t{}\t{}",
                    finding.id,
                    finding.status.as_str(),
                    finding.detail
                );
            }
        }
        OutputFormat::Json => {
            let mut output = String::new();
            write!(
                output,
                "{{\"schema\":\"{}\",\"version\":\"{}\",\"findings\":[",
                json_escape(DOCTOR_SCHEMA),
                json_escape(VERSION)
            )
            .map_err(|error| error.to_string())?;
            for (index, finding) in findings.iter().enumerate() {
                if index > 0 {
                    output.push(',');
                }
                write!(
                    output,
                    "{{\"id\":\"{}\",\"status\":\"{}\",\"detail\":\"{}\"}}",
                    json_escape(finding.id),
                    json_escape(finding.status.as_str()),
                    json_escape(&finding.detail)
                )
                .map_err(|error| error.to_string())?;
            }
            output.push_str("]}");
            println!("{output}");
        }
    }
    Ok(())
}

pub(crate) fn print_manifest(
    manifest: &ObjectManifest,
    options: &ManifestViewOptions,
) -> Result<(), String> {
    let returned = manifest
        .chunks
        .len()
        .saturating_sub(options.chunk_offset)
        .min(options.chunk_limit);
    let omitted_after = manifest
        .chunks
        .len()
        .saturating_sub(options.chunk_offset.saturating_add(returned));
    match options.format {
        OutputFormat::Text => {
            println!("schema\t{OBJECT_MANIFEST_VIEW_SCHEMA}");
            println!("object_length\t{}", manifest.object_length);
            println!("chunk_size\t{}", manifest.chunk_size);
            println!("object_digest\t{}", manifest.object_digest);
            println!("manifest_digest\t{}", manifest.manifest_digest);
            println!("chunk_count\t{}", manifest.chunks.len());
            println!("chunk_offset\t{}", options.chunk_offset);
            println!("returned_chunks\t{returned}");
            println!("omitted_before\t{}", options.chunk_offset);
            println!("omitted_after\t{omitted_after}");
            for chunk in manifest
                .chunks
                .iter()
                .skip(options.chunk_offset)
                .take(options.chunk_limit)
            {
                println!(
                    "chunk\t{}\t{}\t{}\t{}",
                    chunk.index, chunk.offset, chunk.length, chunk.digest
                );
            }
        }
        OutputFormat::Json => {
            let complete = options.chunk_offset == 0 && omitted_after == 0;
            let mut output = format!(
                "{{\"schema\":\"{}\",\"object_length\":{},\"chunk_size\":{},\"object_digest\":\"{}\",\"manifest_digest\":\"{}\",\"chunk_count\":{},\"chunk_offset\":{},\"returned_chunks\":{},\"omitted_before\":{},\"omitted_after\":{},\"complete\":{},\"chunks\":[",
                json_escape(OBJECT_MANIFEST_VIEW_SCHEMA),
                manifest.object_length,
                manifest.chunk_size,
                manifest.object_digest,
                manifest.manifest_digest,
                manifest.chunks.len(),
                options.chunk_offset,
                returned,
                options.chunk_offset,
                omitted_after,
                complete
            );
            for (position, chunk) in manifest
                .chunks
                .iter()
                .skip(options.chunk_offset)
                .take(options.chunk_limit)
                .enumerate()
            {
                if position > 0 {
                    output.push(',');
                }
                write!(
                    output,
                    "{{\"index\":{},\"offset\":{},\"length\":{},\"digest\":\"{}\"}}",
                    chunk.index, chunk.offset, chunk.length, chunk.digest
                )
                .map_err(|error| error.to_string())?;
            }
            output.push_str("]}");
            println!("{output}");
        }
    }
    Ok(())
}

pub(crate) fn print_file_verification(manifest: &ObjectManifest, format: OutputFormat) {
    print_verification(FILE_VERIFICATION_SCHEMA, manifest, format);
}

pub(crate) fn print_store_verification(manifest: &ObjectManifest, format: OutputFormat) {
    print_verification(STORE_VERIFICATION_SCHEMA, manifest, format);
}

fn print_verification(schema: &str, manifest: &ObjectManifest, format: OutputFormat) {
    match format {
        OutputFormat::Text => {
            println!("schema\t{schema}");
            println!("verified\ttrue");
            println!("object_length\t{}", manifest.object_length);
            println!("chunk_size\t{}", manifest.chunk_size);
            println!("chunk_count\t{}", manifest.chunks.len());
            println!("object_digest\t{}", manifest.object_digest);
            println!("manifest_digest\t{}", manifest.manifest_digest);
        }
        OutputFormat::Json => println!(
            "{{\"schema\":\"{}\",\"verified\":true,\"object_length\":{},\"chunk_size\":{},\"chunk_count\":{},\"object_digest\":\"{}\",\"manifest_digest\":\"{}\"}}",
            json_escape(schema),
            manifest.object_length,
            manifest.chunk_size,
            manifest.chunks.len(),
            manifest.object_digest,
            manifest.manifest_digest
        ),
    }
}

pub(crate) fn print_import_receipt(receipt: ImportReceipt, format: OutputFormat) {
    match format {
        OutputFormat::Text => {
            println!("schema\t{}", receipt.schema);
            println!("stage\tpublished");
            println!("object_length\t{}", receipt.object_length);
            println!("chunk_size\t{}", receipt.chunk_size);
            println!("chunk_count\t{}", receipt.chunk_count);
            println!("object_digest\t{}", receipt.object_digest);
            println!("manifest_digest\t{}", receipt.manifest_digest);
            println!("object_created\t{}", receipt.object_created);
            println!("manifest_created\t{}", receipt.manifest_created);
            println!(
                "staging_cleanup_complete\t{}",
                receipt.staging_cleanup_complete
            );
            if let Some(entry) = receipt.staging_entry {
                println!("staging_entry\t{entry}");
            }
        }
        OutputFormat::Json => {
            let staging_entry = match receipt.staging_entry.as_deref() {
                Some(entry) => format!("\"{}\"", json_escape(entry)),
                None => "null".to_owned(),
            };
            println!(
                "{{\"schema\":\"{}\",\"stage\":\"published\",\"object_length\":{},\"chunk_size\":{},\"chunk_count\":{},\"object_digest\":\"{}\",\"manifest_digest\":\"{}\",\"object_created\":{},\"manifest_created\":{},\"staging_cleanup_complete\":{},\"staging_entry\":{}}}",
                json_escape(IMPORT_RECEIPT_SCHEMA),
                receipt.object_length,
                receipt.chunk_size,
                receipt.chunk_count,
                receipt.object_digest,
                receipt.manifest_digest,
                receipt.object_created,
                receipt.manifest_created,
                receipt.staging_cleanup_complete,
                staging_entry
            );
        }
    }
}

pub(crate) fn print_plan_summary(format: OutputFormat) -> Result<(), String> {
    match format {
        OutputFormat::Text => {
            for (index, step) in fdgr_core::implementation_sequence().iter().enumerate() {
                println!("{}. {step}", index + 1);
            }
        }
        OutputFormat::Json => {
            let mut output = format!(
                "{{\"schema\":\"{}\",\"version\":\"{}\",\"steps\":[",
                json_escape(PLAN_SUMMARY_SCHEMA),
                json_escape(VERSION)
            );
            for (index, step) in fdgr_core::implementation_sequence().iter().enumerate() {
                if index > 0 {
                    output.push(',');
                }
                write!(
                    output,
                    "{{\"order\":{},\"text\":\"{}\"}}",
                    index + 1,
                    json_escape(step)
                )
                .map_err(|error| error.to_string())?;
            }
            output.push_str("]}");
            println!("{output}");
        }
    }
    Ok(())
}

pub(crate) fn print_help() {
    println!(
        "fdgr {VERSION}\n\n\
         Evidence-grade drone geometry reconstruction\n\n\
         USAGE:\n  \
           fdgr capabilities [--format text|json]\n  \
           fdgr doctor [--format text|json]\n  \
           fdgr file-manifest <path> [--chunk-size bytes] [--chunk-offset n] [--chunk-limit n] [--format text|json]\n  \
           fdgr import-file <store-root> <path> [--chunk-size bytes] [--format text|json]\n  \
           fdgr verify-file <path> --object-digest <digest> --manifest-digest <digest> [--chunk-size bytes] [--format text|json]\n  \
           fdgr verify-store <store-root> <manifest-digest> [--format text|json]\n  \
           fdgr plan-summary [--format text|json]\n  \
           fdgr validate-id <digest>\n  \
           fdgr version\n"
    );
}

pub(crate) fn json_escape(value: &str) -> String {
    let mut escaped = String::with_capacity(value.len());
    for character in value.chars() {
        match character {
            '"' => escaped.push_str("\\\""),
            '\\' => escaped.push_str("\\\\"),
            '\n' => escaped.push_str("\\n"),
            '\r' => escaped.push_str("\\r"),
            '\t' => escaped.push_str("\\t"),
            character if character.is_control() => {
                push_json_unicode_escape(&mut escaped, u32::from(character));
            }
            character => escaped.push(character),
        }
    }
    escaped
}

fn push_json_unicode_escape(output: &mut String, value: u32) {
    output.push_str("\\u");
    output.push(hex_digit((value >> 12) & 0x0f));
    output.push(hex_digit((value >> 8) & 0x0f));
    output.push(hex_digit((value >> 4) & 0x0f));
    output.push(hex_digit(value & 0x0f));
}

const fn hex_digit(value: u32) -> char {
    match value {
        0 => '0',
        1 => '1',
        2 => '2',
        3 => '3',
        4 => '4',
        5 => '5',
        6 => '6',
        7 => '7',
        8 => '8',
        9 => '9',
        10 => 'a',
        11 => 'b',
        12 => 'c',
        13 => 'd',
        14 => 'e',
        15 => 'f',
        _ => '\u{fffd}',
    }
}

#[cfg(test)]
mod tests {
    use super::json_escape;

    #[test]
    fn json_escape_handles_control_characters() {
        assert_eq!(json_escape("a\n\"b\\c\0"), "a\\n\\\"b\\\\c\\u0000");
    }
}
