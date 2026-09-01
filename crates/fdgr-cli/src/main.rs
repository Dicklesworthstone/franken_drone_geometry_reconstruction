#![forbid(unsafe_code)]
//! Agent-friendly deterministic CLI surfaces for FDGR.

use fdgr_core::{
    CAPABILITIES_SCHEMA, DOCTOR_SCHEMA, FILE_VERIFICATION_SCHEMA, OBJECT_MANIFEST_VIEW_SCHEMA,
    PLAN_SUMMARY_SCHEMA, VALIDATE_ID_SCHEMA, VERSION,
};
use fdgr_evidence::{DEFAULT_CHUNK_SIZE, ObjectManifest, build_file_manifest};
use fdgr_types::EvidenceDigest;
use std::env;
use std::fmt::Write as _;
use std::path::PathBuf;
use std::process::ExitCode;

const DEFAULT_CHUNK_VIEW_LIMIT: usize = 32;
const MAX_CHUNK_VIEW_LIMIT: usize = 4096;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum OutputFormat {
    Text,
    Json,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct ManifestViewOptions {
    path: PathBuf,
    chunk_size: u32,
    chunk_offset: usize,
    chunk_limit: usize,
    format: OutputFormat,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct VerifyOptions {
    path: PathBuf,
    chunk_size: u32,
    object_digest: EvidenceDigest,
    manifest_digest: EvidenceDigest,
    format: OutputFormat,
}

fn main() -> ExitCode {
    let arguments: Vec<String> = env::args().skip(1).collect();
    match run(&arguments) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("fdgr: {error}");
            ExitCode::from(2)
        }
    }
}

fn run(arguments: &[String]) -> Result<(), String> {
    let Some((command, rest)) = arguments.split_first() else {
        print_help();
        return Ok(());
    };
    match command.as_str() {
        "capabilities" => print_capabilities(parse_format(rest)?),
        "doctor" => print_doctor(parse_format(rest)?),
        "file-manifest" => print_file_manifest(parse_manifest_view(rest)?),
        "verify-file" => verify_file_command(parse_verify(rest)?),
        "plan-summary" => print_plan_summary(parse_format(rest)?),
        "validate-id" => validate_id(rest),
        "version" | "--version" | "-V" => {
            println!("fdgr {VERSION}");
            Ok(())
        }
        "help" | "--help" | "-h" => {
            print_help();
            Ok(())
        }
        other => Err(format!("unknown command {other:?}; run `fdgr help`")),
    }
}

fn parse_format(arguments: &[String]) -> Result<OutputFormat, String> {
    match arguments {
        [] => Ok(OutputFormat::Text),
        [flag, value] if flag == "--format" => parse_format_value(value),
        _ => Err("expected no arguments or `--format text|json`".to_owned()),
    }
}

fn parse_format_value(value: &str) -> Result<OutputFormat, String> {
    match value {
        "text" => Ok(OutputFormat::Text),
        "json" => Ok(OutputFormat::Json),
        _ => Err(format!("unknown output format {value:?}; expected text or json")),
    }
}

fn parse_manifest_view(arguments: &[String]) -> Result<ManifestViewOptions, String> {
    let mut arguments = arguments.iter();
    let path = arguments
        .next()
        .ok_or_else(|| "usage: fdgr file-manifest <path> [options]".to_owned())?;
    let mut options = ManifestViewOptions {
        path: PathBuf::from(path),
        chunk_size: DEFAULT_CHUNK_SIZE,
        chunk_offset: 0,
        chunk_limit: DEFAULT_CHUNK_VIEW_LIMIT,
        format: OutputFormat::Text,
    };
    while let Some(flag) = arguments.next() {
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--chunk-size" => options.chunk_size = parse_u32(value, "chunk size")?,
            "--chunk-offset" => options.chunk_offset = parse_usize(value, "chunk offset")?,
            "--chunk-limit" => {
                options.chunk_limit = parse_usize(value, "chunk limit")?;
                if options.chunk_limit > MAX_CHUNK_VIEW_LIMIT {
                    return Err(format!(
                        "chunk limit {} exceeds maximum {MAX_CHUNK_VIEW_LIMIT}",
                        options.chunk_limit
                    ));
                }
            }
            "--format" => options.format = parse_format_value(value)?,
            _ => return Err(format!("unknown file-manifest option {flag:?}")),
        }
    }
    Ok(options)
}

fn parse_verify(arguments: &[String]) -> Result<VerifyOptions, String> {
    let mut arguments = arguments.iter();
    let path = arguments.next().ok_or_else(|| {
        "usage: fdgr verify-file <path> --object-digest <digest> --manifest-digest <digest> [options]"
            .to_owned()
    })?;
    let mut chunk_size = DEFAULT_CHUNK_SIZE;
    let mut object_digest = None;
    let mut manifest_digest = None;
    let mut format = OutputFormat::Text;
    while let Some(flag) = arguments.next() {
        let value = arguments
            .next()
            .ok_or_else(|| format!("missing value for {flag}"))?;
        match flag.as_str() {
            "--chunk-size" => chunk_size = parse_u32(value, "chunk size")?,
            "--object-digest" => {
                object_digest = Some(
                    EvidenceDigest::parse(value).map_err(|error| error.to_string())?,
                );
            }
            "--manifest-digest" => {
                manifest_digest = Some(
                    EvidenceDigest::parse(value).map_err(|error| error.to_string())?,
                );
            }
            "--format" => format = parse_format_value(value)?,
            _ => return Err(format!("unknown verify-file option {flag:?}")),
        }
    }
    Ok(VerifyOptions {
        path: PathBuf::from(path),
        chunk_size,
        object_digest: object_digest.ok_or_else(|| "missing --object-digest".to_owned())?,
        manifest_digest: manifest_digest.ok_or_else(|| "missing --manifest-digest".to_owned())?,
        format,
    })
}

fn parse_u32(value: &str, label: &str) -> Result<u32, String> {
    value
        .parse::<u32>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn parse_usize(value: &str, label: &str) -> Result<usize, String> {
    value
        .parse::<usize>()
        .map_err(|error| format!("invalid {label} {value:?}: {error}"))
}

fn print_capabilities(format: OutputFormat) -> Result<(), String> {
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

fn print_doctor(format: OutputFormat) -> Result<(), String> {
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

fn print_file_manifest(options: ManifestViewOptions) -> Result<(), String> {
    let manifest = build_file_manifest(&options.path, options.chunk_size)
        .map_err(|error| format!("manifest build failed: {error}"))?;
    if options.chunk_offset > manifest.chunks.len() {
        return Err(format!(
            "chunk offset {} exceeds chunk count {}",
            options.chunk_offset,
            manifest.chunks.len()
        ));
    }
    match options.format {
        OutputFormat::Text => print_manifest_text(&manifest, &options),
        OutputFormat::Json => print_manifest_json(&manifest, &options)?,
    }
    Ok(())
}

fn print_manifest_text(manifest: &ObjectManifest, options: &ManifestViewOptions) {
    let returned = manifest
        .chunks
        .len()
        .saturating_sub(options.chunk_offset)
        .min(options.chunk_limit);
    let omitted_after = manifest
        .chunks
        .len()
        .saturating_sub(options.chunk_offset.saturating_add(returned));
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

fn print_manifest_json(
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
    Ok(())
}

fn verify_file_command(options: VerifyOptions) -> Result<(), String> {
    let manifest = build_file_manifest(&options.path, options.chunk_size)
        .map_err(|error| format!("file verification failed: {error}"))?;
    if manifest.object_digest != options.object_digest {
        return Err(format!(
            "object digest mismatch: expected {}, observed {}",
            options.object_digest, manifest.object_digest
        ));
    }
    if manifest.manifest_digest != options.manifest_digest {
        return Err(format!(
            "manifest digest mismatch: expected {}, observed {}",
            options.manifest_digest, manifest.manifest_digest
        ));
    }
    match options.format {
        OutputFormat::Text => {
            println!("schema\t{FILE_VERIFICATION_SCHEMA}");
            println!("verified\ttrue");
            println!("object_length\t{}", manifest.object_length);
            println!("chunk_size\t{}", manifest.chunk_size);
            println!("chunk_count\t{}", manifest.chunks.len());
            println!("object_digest\t{}", manifest.object_digest);
            println!("manifest_digest\t{}", manifest.manifest_digest);
        }
        OutputFormat::Json => println!(
            "{{\"schema\":\"{}\",\"verified\":true,\"object_length\":{},\"chunk_size\":{},\"chunk_count\":{},\"object_digest\":\"{}\",\"manifest_digest\":\"{}\"}}",
            json_escape(FILE_VERIFICATION_SCHEMA),
            manifest.object_length,
            manifest.chunk_size,
            manifest.chunks.len(),
            manifest.object_digest,
            manifest.manifest_digest
        ),
    }
    Ok(())
}

fn print_plan_summary(format: OutputFormat) -> Result<(), String> {
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

fn validate_id(arguments: &[String]) -> Result<(), String> {
    let [value] = arguments else {
        return Err("usage: fdgr validate-id <64-lowercase-hex-digest>".to_owned());
    };
    match EvidenceDigest::parse(value) {
        Ok(digest) => {
            println!(
                "{{\"schema\":\"{}\",\"valid\":true,\"digest\":\"{}\"}}",
                json_escape(VALIDATE_ID_SCHEMA),
                digest.as_str()
            );
            Ok(())
        }
        Err(error) => Err(error.to_string()),
    }
}

fn json_escape(value: &str) -> String {
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

fn print_help() {
    println!(
        "fdgr {VERSION}\n\n\
         Evidence-grade drone geometry reconstruction\n\n\
         USAGE:\n  \
           fdgr capabilities [--format text|json]\n  \
           fdgr doctor [--format text|json]\n  \
           fdgr file-manifest <path> [--chunk-size bytes] [--chunk-offset n] [--chunk-limit n] [--format text|json]\n  \
           fdgr verify-file <path> --object-digest <digest> --manifest-digest <digest> [--chunk-size bytes] [--format text|json]\n  \
           fdgr plan-summary [--format text|json]\n  \
           fdgr validate-id <digest>\n  \
           fdgr version\n"
    );
}

#[cfg(test)]
mod tests {
    use super::{OutputFormat, json_escape, parse_manifest_view, parse_verify};

    #[test]
    fn json_escape_handles_control_characters() {
        assert_eq!(json_escape("a\n\"b\\c\0"), "a\\n\\\"b\\\\c\\u0000");
    }

    #[test]
    fn manifest_options_are_order_independent() {
        let arguments = vec![
            "file.mp4".to_owned(),
            "--format".to_owned(),
            "json".to_owned(),
            "--chunk-limit".to_owned(),
            "7".to_owned(),
            "--chunk-offset".to_owned(),
            "3".to_owned(),
            "--chunk-size".to_owned(),
            "1024".to_owned(),
        ];
        assert!(matches!(
            parse_manifest_view(&arguments),
            Ok(ref value)
                if value.path.to_string_lossy() == "file.mp4"
                    && value.format == OutputFormat::Json
                    && value.chunk_limit == 7
                    && value.chunk_offset == 3
                    && value.chunk_size == 1024
        ));
    }

    #[test]
    fn verify_requires_both_identities() {
        let arguments = vec!["file.mp4".to_owned()];
        assert!(parse_verify(&arguments).is_err());
    }
}
