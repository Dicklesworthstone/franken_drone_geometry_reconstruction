
# Model-worker protocol boundary

Research models run out of process. The Rust trust domain never imports PyTorch, CUDA, Python,
model-specific tokenizers, or native inference libraries through FFI. A worker receives a sealed
request manifest and read-only content-addressed inputs, has network access disabled by default,
and writes outputs below an unpublished result directory. FDGR validates the output schema,
model identity, weight digests, license receipt, input basis, coordinate convention, finite-value
policy, and declared uncertainty before it can publish a proposal root.

A worker result is **never** authoritative merely because the process exited successfully. Process
acceptance, result-file presence, structural validation, numerical validation, geometric
cross-checking, and publication are distinct events.
