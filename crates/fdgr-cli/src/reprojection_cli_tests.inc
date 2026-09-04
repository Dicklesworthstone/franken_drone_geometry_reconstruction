#[cfg(test)]
mod tests {
    use super::{
        is_command, parse, parse_bool_option, parse_distortion, parse_readout,
    };
    use fdgr_calibration::{DistortionModel, ReadoutDirection};

    #[test]
    fn command_detection_is_exact() {
        assert!(is_command(&["reprojection-evaluate".to_owned()]));
        assert!(!is_command(&["reprojection".to_owned()]));
        assert!(!is_command(&[]));
    }

    #[test]
    fn distortion_grammar_is_closed_and_none_requires_zero_coefficients() {
        assert_eq!(
            parse_distortion("none", [0; 5], 2),
            Ok(DistortionModel::None)
        );
        assert!(parse_distortion("none", [1, 0, 0, 0, 0], 2).is_err());
        assert!(parse_distortion("fisheye", [0; 5], 2).is_err());
        assert!(matches!(
            parse_distortion("brown_conrady", [1, 2, 3, 4, 5], 2),
            Ok(DistortionModel::BrownConrady {
                k1_nano: 1,
                k2_nano: 2,
                p1_nano: 3,
                p2_nano: 4,
                k3_nano: 5,
            })
        ));
    }

    #[test]
    fn readout_grammar_does_not_invent_motion_semantics() {
        let global = parse_readout(false, "none", 0, 0, 0, 2);
        assert!(matches!(global, Ok(value) if !value.rolling && value.direction.is_none()));
        assert!(parse_readout(false, "top_to_bottom", 0, 0, 0, 2).is_err());
        assert!(parse_readout(false, "none", 0, 0, 1, 2).is_err());
        assert!(parse_readout(true, "none", 0, 1, 0, 2).is_err());
        assert!(parse_readout(true, "top_to_bottom", 0, 0, 0, 2).is_err());
        assert!(matches!(
            parse_readout(true, "right_to_left", 1, 10, 5, 2),
            Ok(value)
                if value.rolling
                    && value.direction == Some(ReadoutDirection::RightToLeft)
                    && value.first_observed_line_offset_ns == 1
                    && value.observed_readout_time_ns == 10
        ));
    }

    #[test]
    fn caller_cannot_supply_algorithm_identity() {
        let mut arguments = (0..9)
            .map(|index| format!("input-{index}.tsv"))
            .collect::<Vec<_>>();
        arguments.push("--reprojection-algorithm-digest".to_owned());
        arguments.push("00".repeat(32));
        assert!(matches!(
            parse(&arguments),
            Err(ref error) if error.contains("unknown reprojection-evaluate option")
        ));
    }

    #[test]
    fn boolean_grammar_is_exact() {
        assert_eq!(parse_bool_option("true", "flag"), Ok(true));
        assert_eq!(parse_bool_option("false", "flag"), Ok(false));
        assert!(parse_bool_option("1", "flag").is_err());
        assert!(parse_bool_option("TRUE", "flag").is_err());
    }
}
