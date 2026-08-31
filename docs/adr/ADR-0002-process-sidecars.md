
# ADR-0002 — External media and model stacks are process sidecars

**Status:** Accepted for design

FDGR will not link ffmpeg/libav, Python, PyTorch, CUDA, or vendor-native SDKs into the Rust semantic
process. External stacks run as supervised child processes with sealed manifests,
content-addressed inputs, bounded outputs, no-network defaults, and descendant cleanup.

This preserves the safe-Rust trust domain, avoids a second async/runtime model, localizes license
and supply-chain identity, permits crash quarantine, and makes model absence a degraded mode rather
than database unavailability. Pure-Rust replacements may later be admitted behind identical
contracts and differential gates.
