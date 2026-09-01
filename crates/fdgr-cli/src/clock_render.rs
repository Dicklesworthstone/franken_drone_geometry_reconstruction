#![forbid(unsafe_code)]
//! Stable rendering for validated clock models.

use crate::args::OutputFormat;
use fdgr_clock::{CLOCK_MODEL_SCHEMA, ClockModel};

pub(crate) fn print_clock_model(
    model: &ClockModel,
    format: OutputFormat,
) -> Result<(), String> {
    match format {
        OutputFormat::Json => {
            println!("{}", model.to_json().map_err(|error| error.to_string())?);
            Ok(())
        }
        OutputFormat::Text => {
            let digest = model.digest().map_err(|error| error.to_string())?;
            println!("schema\t{CLOCK_MODEL_SCHEMA}");
            println!("model_digest\t{digest}");
            println!("basis_digest\t{}", model.basis.basis_digest);
            println!("source_domain\t{}", model.basis.source_domain);
            println!("reference_domain\t{}", model.basis.reference_domain);
            println!("source_epoch\t{}", model.basis.source_epoch);
            println!("reference_epoch\t{}", model.basis.reference_epoch);
            println!("model_generation\t{}", model.basis.model_generation);
            println!("source_timescale\t{}", model.basis.source_timescale);
            println!("reference_timescale\t{}", model.basis.reference_timescale);
            println!("rate_numerator\t{}", model.rate_numerator);
            println!("rate_denominator\t{}", model.rate_denominator);
            println!("offset_numerator\t{}", model.offset_numerator);
            println!("source_support_start_ticks\t{}", model.source_support_start);
            println!("source_support_end_ticks\t{}", model.source_support_end);
            println!("reference_support_start_ticks\t{}", model.reference_support_start);
            println!("reference_support_end_ticks\t{}", model.reference_support_end);
            println!("drift_ppm\t{}", model.drift_ppm);
            println!("rate_spread_ppm\t{}", model.rate_spread_ppm);
            println!(
                "median_abs_residual_ticks\t{}",
                model.median_abs_residual_ticks
            );
            println!("max_abs_residual_ticks\t{}", model.max_abs_residual_ticks);
            println!(
                "declared_uncertainty_ticks\t{}",
                model.declared_uncertainty_ticks
            );
            for anchor_id in &model.inlier_anchor_ids {
                println!("inlier_anchor_id\t{anchor_id}");
            }
            for anchor_id in &model.outlier_anchor_ids {
                println!("outlier_anchor_id\t{anchor_id}");
            }
            for group_id in &model.inlier_group_ids {
                println!("inlier_group_id\t{group_id}");
            }
            for group_id in &model.outlier_group_ids {
                println!("outlier_group_id\t{group_id}");
            }
            Ok(())
        }
    }
}
