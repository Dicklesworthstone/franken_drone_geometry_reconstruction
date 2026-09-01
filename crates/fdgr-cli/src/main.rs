#![forbid(unsafe_code)]
//! Agent-friendly deterministic CLI surfaces for FDGR.

mod args;
mod clock_args;
mod clock_render;
mod commands;
mod decode_args;
mod decode_render;
mod recorded_args;
mod recorded_render;
mod render;
mod sample_render;
mod stored_args;
mod stored_render;

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
