#![forbid(unsafe_code)]
//! Agent-friendly deterministic CLI surfaces for FDGR.

mod args;
mod clock_args;
mod clock_render;
mod commands;
mod correspondence_cli;
mod decode_args;
mod decode_render;
mod epipolar_cli;
mod geometry_observation_cli;
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

fn command_arguments(arguments: &[String]) -> &[String] {
    arguments.split_first().map_or(&[], |(_, rest)| rest)
}

fn main() -> ExitCode {
    let arguments: Vec<String> = env::args().skip(1).collect();
    let command_arguments = command_arguments(&arguments);
    let result = if correspondence_cli::is_command(&arguments) {
        correspondence_cli::run(command_arguments)
    } else if epipolar_cli::is_command(&arguments) {
        epipolar_cli::run(command_arguments)
    } else if keyframe_cli::is_command(&arguments) {
        keyframe_cli::run(command_arguments)
    } else if relative_pose_cli::is_command(&arguments) {
        relative_pose_cli::run(command_arguments)
    } else {
        let result = commands::run(&arguments);
        if result.is_ok()
            && (arguments.is_empty()
                || arguments
                    .first()
                    .is_some_and(|value| matches!(value.as_str(), "help" | "--help" | "-h")))
        {
            correspondence_cli::print_help_line();
            epipolar_cli::print_help_line();
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

#[cfg(test)]
mod tests {
    use super::command_arguments;

    #[test]
    fn strips_only_the_command_name() {
        let arguments = vec![
            "epipolar-verify".to_owned(),
            "observations.tsv".to_owned(),
            "candidates.tsv".to_owned(),
        ];
        assert_eq!(
            command_arguments(&arguments),
            &["observations.tsv".to_owned(), "candidates.tsv".to_owned()]
        );
        assert!(command_arguments(&[]).is_empty());
    }
}
