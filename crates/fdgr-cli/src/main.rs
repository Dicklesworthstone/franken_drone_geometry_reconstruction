#![forbid(unsafe_code)]
//! Agent-friendly deterministic CLI surfaces for the FDGR scaffold.

use fdgr_core::{CAPABILITIES_SCHEMA, DOCTOR_SCHEMA, VERSION};
use fdgr_types::EvidenceDigest;
use std::env;
use std::fmt::Write as _;
use std::process::ExitCode;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum OutputFormat {
    Text,
    Json,
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
        [flag, value] if flag == "--format" && value == "text" => Ok(OutputFormat::Text),
        [flag, value] if flag == "--format" && value == "json" => Ok(OutputFormat::Json),
        _ => Err("expected no arguments or `--format text|json`".to_owned()),
    }
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

fn print_plan_summary(format: OutputFormat) -> Result<(), String> {
    match format {
        OutputFormat::Text => {
            for (index, step) in fdgr_core::implementation_sequence().iter().enumerate() {
                println!("{}. {step}", index + 1);
            }
        }
        OutputFormat::Json => {
            let mut output = format!(
                "{{\"schema\":\"fdgr.plan_summary.v1\",\"version\":\"{}\",\"steps\":[",
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
                "{{\"schema\":\"fdgr.validate_id.v1\",\"valid\":true,\"digest\":\"{}\"}}",
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
         Evidence-grade drone geometry reconstruction scaffold\n\n\
         USAGE:\n  \
           fdgr capabilities [--format text|json]\n  \
           fdgr doctor [--format text|json]\n  \
           fdgr plan-summary [--format text|json]\n  \
           fdgr validate-id <digest>\n  \
           fdgr version\n"
    );
}

#[cfg(test)]
mod tests {
    use super::json_escape;

    #[test]
    fn json_escape_handles_control_characters() {
        assert_eq!(json_escape("a\n\"b\\c\0"), "a\\n\\\"b\\\\c\\u0000");
    }
}
