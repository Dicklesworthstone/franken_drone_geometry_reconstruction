#![forbid(unsafe_code)]
//! Agent-friendly deterministic CLI surfaces for FDGR.

mod args;
mod clock_args;
mod clock_render;
mod commands;
mod correspondence_cli;
mod decode_args;
mod decode_render;
mod keyframe_cli;
mod recorded_args;
mod recorded_render;
mod relative_pose_cli;
mod render;
mod sample_render;
mod stored_args;
mod stored_render;

use std::env;
use std::process::ExitCode;

fn main() -> ExitCode {
    let arguments: Vec<String> = env::args().skip(1).collect();
    let result = if correspondence_cli::is_command(&arguments) {
        correspondence_cli::run(arguments.split_first().map_or(&[], |(_, rest)| rest))
    } else if keyframe_cli::is_command(&arguments) {
        keyframe_cli::run(arguments.split_first().map_or(&[], |(_, rest)| rest))
    } else if relative_pose_cli::is_command(&arguments) {
        relative_pose_cli::run(arguments.split_first().map_or(&[], |(_, rest)| rest))
    } else {
        let result = commands::run(&arguments);
        if result.is_ok()
            && (arguments.is_empty()
                || arguments
                    .first()
                    .is_some_and(|value| matches!(value.as_str(), "help" | "--help" | "-h")))
        {
            correspondence_cli::print_help_line();
            keyframe_cli::print_help_line();
            relative_pose_cli::print_help_line();
        }
        result
    };
    match result {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("fdgr: {error}");
            ExitCode::from(2)
        }
    }
}
