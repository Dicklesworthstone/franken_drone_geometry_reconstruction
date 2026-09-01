#![forbid(unsafe_code)]
//! Agent-friendly deterministic CLI surfaces for FDGR.

mod args;
mod commands;
mod render;
mod sample_render;

use std::env;
use std::process::ExitCode;

fn main() -> ExitCode {
    let arguments: Vec<String> = env::args().skip(1).collect();
    match commands::run(&arguments) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("fdgr: {error}");
            ExitCode::from(2)
        }
    }
}
