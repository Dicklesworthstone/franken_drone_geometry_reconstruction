
# Security Policy and Threat Model

## Protected assets

FDGR handles unusually sensitive material: detailed imagery and geometry of homes, entrances,
windows, utility equipment, paths, vehicles, people, neighboring property, Wi-Fi/device metadata,
cloud credentials, and possibly owner-authorized vendor sessions. Security and privacy are
architectural correctness properties, not deployment polish.

## Trust domains

1. **Safe-Rust semantic domain:** identities, ledger, claims, policy, publication, geometry
   validation, capability checks, and agent protocol.
2. **External process domain:** ffmpeg, model workers, GPU stacks, vendor tools. Untrusted even when
   locally installed.
3. **Device/vendor domain:** aircraft, controller, mobile application, radio/network, account.
4. **Archive domain:** object providers, networks, credentials, and remote retention.
5. **Untrusted content domain:** video, metadata, captions, OCR, model output, filenames, protocol
   frames, documentation, and agent arguments.
6. **Human/operator domain:** grants authority and may confirm observations; human text is still
   data, not executable code.

## Capability classes

At minimum, authority is separated into:

```text
capture.read
capture.live.observe
device.observe
device.control          # absent from initial admitted profiles
process.media.spawn
process.model.spawn
evidence.append
generation.publish
archive.read
archive.write
archive.delete
privacy.export
human.confirm
```

Read-only workers cannot recover a parent capability or manufacture a write handle. MCP
connection identity and model output grant nothing.

## Primary threats

- malicious or malformed media exploiting parsers/decoders;
- path traversal, symlink races, overwrite, partial publication, and disk exhaustion;
- sidecar descendants surviving cancellation;
- model output containing NaN, huge coordinates, decompression bombs, prompt injection, or path
  references;
- vendor/session secrets leaking into logs, packet fixtures, crashpacks, reports, or cloud;
- an agent widening capture, archive, export, or deletion scope through text;
- stale compatibility profiles sending unsupported or effectful device messages;
- remote object substitution, incomplete multipart state, accidental public bucket policy, or
  credential confusion;
- semantic overclaim revealing or fabricating critical asset locations;
- unauthorized sharing of detailed private-space geometry;
- supply-chain substitution of model weights, binaries, containers, or calibration files.

## Security rules

- All untrusted data is bounded before allocation and parsed nonrecursively where depth is
  attacker-controlled.
- Exact byte, node, frame, point, triangle, object, string, output, and diagnostic limits are part
  of each request context.
- External processes receive minimal filesystem scope, sanitized environment, no network by
  default, and explicit descendant cleanup.
- Durable manifests use canonical codecs and independent content hashes.
- Secret fields are never serialized into evidence types; redaction is structural.
- Publication destinations are preflighted. Originals are never overwritten.
- Archive deletion requires a sealed plan, independent revalidation, retention analysis, and
  surviving-root proof.
- Live adapters are read-only until a separately reviewed effect registry exists.
- Public exports require a privacy scope and may use geometry simplification, spatial cropping,
  semantic redaction, texture removal, or coordinate de-georeferencing.
- Support bundles default to manifests and diagnostics, not raw private media or packet captures.

## Vulnerability reporting

Do not open a public issue containing a live credential, private home image, proprietary packet
capture, or exploitable detail. Use a private security advisory on the GitHub repository once it
exists. Include the affected commit/profile, a minimal synthetic reproducer where possible, and
whether secrets or private imagery may have been exposed.

## Claim discipline

A passing static scanner is not proof of security. Release qualification requires malformed-input,
resource-exhaustion, cancellation, descendant-process, path, credential-leak, archive-substitution,
capability-noninterference, and privacy-export campaigns with retained receipts.
