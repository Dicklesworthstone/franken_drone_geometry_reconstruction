#[cfg(test)]
mod tests {
    use super::{CANDIDATE_HEADER, parse, read_candidate_table};
    use fdgr_codec::hash_bytes;
    use std::fs;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU64, Ordering};

    static NEXT_TEST: AtomicU64 = AtomicU64::new(1);

    fn path(label: &str) -> PathBuf {
        let id = NEXT_TEST.fetch_add(1, Ordering::Relaxed);
        std::env::temp_dir().join(format!(
            "fdgr-keyframe-cli-{label}-{}-{id}",
            std::process::id()
        ))
    }

    #[test]
    fn candidate_table_is_digest_bound_and_exact() {
        let path = path("table");
        let digest = "0101010101010101010101010101010101010101010101010101010101010101";
        let bytes = format!(
            "{CANDIDATE_HEADER}\n0\t{digest}\t-5\t900000\t800000\t10000\t20000\t30000\t700000\t1\t1\t1,2,3\n"
        );
        assert!(fs::write(&path, bytes.as_bytes()).is_ok());
        let identity = hash_bytes(bytes.as_bytes());
        assert!(matches!(
            read_candidate_table(&path, &identity),
            Ok(ref table)
                if table.digest == identity
                    && table.candidates.len() == 1
                    && table.candidates[0].presentation_tick == -5
                    && table.candidates[0].coverage_cells == vec![1, 2, 3]
        ));
        assert!(read_candidate_table(&path, &hash_bytes(b"different")).is_err());
        assert!(fs::remove_file(path).is_ok());
    }

    #[test]
    fn required_arguments_parse_without_path_leaking_into_semantics() {
        let digest = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        let arguments = vec![
            "candidates.tsv".to_owned(),
            "--candidate-basis-digest".to_owned(),
            digest.to_owned(),
            "--timeline-digest".to_owned(),
            digest.to_owned(),
            "--decoded-frame-generation-digest".to_owned(),
            digest.to_owned(),
            "--calibration-digest".to_owned(),
            digest.to_owned(),
            "--policy-digest".to_owned(),
            digest.to_owned(),
            "--generation".to_owned(),
            "2".to_owned(),
            "--max-selected".to_owned(),
            "10".to_owned(),
            "--format".to_owned(),
            "json".to_owned(),
        ];
        assert!(matches!(
            parse(&arguments),
            Ok(ref value)
                if value.selection_generation == 2
                    && value.policy.max_selected == 10
                    && value.candidate_path.to_string_lossy() == "candidates.tsv"
        ));
    }
}
