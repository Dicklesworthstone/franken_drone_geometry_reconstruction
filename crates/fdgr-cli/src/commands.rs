#![forbid(unsafe_code)]
//! Command dispatch and semantic operations for the current CLI.

use crate::args::{
    parse_format, parse_import, parse_manifest_view, parse_store_verify, parse_verify,
};
use crate::render::{
    json_escape, print_capabilities, print_doctor, print_file_verification, print_help,
    print_import_receipt, print_manifest, print_plan_summary, print_store_verification,
};
use fdgr_core::{VALIDATE_ID_SCHEMA, VERSION};
use fdgr_evidence::build_file_manifest;
use fdgr_object_store::LocalObjectStore;
use fdgr_types::EvidenceDigest;

pub(crate) fn run(arguments: &[String]) -> Result<(), String> {
    let Some((command, rest)) = arguments.split_first() else {
        print_help();
        return Ok(());
    };
    match command.as_str() {
        "capabilities" => print_capabilities(parse_format(rest)?),
        "doctor" => print_doctor(parse_format(rest)?),
        "file-manifest" => file_manifest(rest),
        "import-file" => import_file(rest),
        "verify-file" => verify_file(rest),
        "verify-store" => verify_store(rest),
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

fn file_manifest(arguments: &[String]) -> Result<(), String> {
    let options = parse_manifest_view(arguments)?;
    let manifest = build_file_manifest(&options.path, options.chunk_size)
        .map_err(|error| format!("manifest build failed: {error}"))?;
    if options.chunk_offset > manifest.chunks.len() {
        return Err(format!(
            "chunk offset {} exceeds chunk count {}",
            options.chunk_offset,
            manifest.chunks.len()
        ));
    }
    print_manifest(&manifest, &options)
}

fn verify_file(arguments: &[String]) -> Result<(), String> {
    let options = parse_verify(arguments)?;
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
    print_file_verification(&manifest, options.format);
    Ok(())
}

fn import_file(arguments: &[String]) -> Result<(), String> {
    let options = parse_import(arguments)?;
    let mut store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    let receipt = store
        .import_file(&options.path, options.chunk_size)
        .map_err(|error| format!("file import failed: {error}"))?;
    print_import_receipt(receipt, options.format);
    Ok(())
}

fn verify_store(arguments: &[String]) -> Result<(), String> {
    let options = parse_store_verify(arguments)?;
    let store = LocalObjectStore::open(&options.store_root)
        .map_err(|error| format!("store open failed: {error}"))?;
    store
        .verify_manifest(&options.manifest_digest)
        .map_err(|error| format!("store verification failed: {error}"))?;
    let manifest = store
        .read_manifest(&options.manifest_digest)
        .map_err(|error| format!("manifest readback failed: {error}"))?;
    print_store_verification(&manifest, options.format);
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
