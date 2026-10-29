# Contributor Guidelines

See [README.md](README.md) for this repository's purpose, package structure, and integration boundaries.

## Repository constraints

- Preserve upstream licenses, copyright notices, and meaningful source data.
- Maintain compatible version pins, module and symbol isolation, generated snapshots, integration checks, and Swift Package distribution here. Keep semantic implementation changes in the corresponding forks.
- `upstream/swift-markdown` contains the Swift AST, C parser bridge, and CAtomic; `upstream/swift-cmark` contains the C parser core and extensions.
- Commit implementation changes on the forks' `ai-markdown` branches first, then record the matching gitlinks here. Pin exact commits rather than moving branch heads.
- Keep maintenance changes separate from upstream upgrades and retain the documented migration baselines.
- Generate and commit isolated sources from the pinned submodule commits. Do not edit generated sources directly or apply the same semantic change in both a fork and the generator.
- Preserve module, C symbol, and header isolation, and validate coexistence with the official parser after migration or upgrades.
- Maintenance tools may use submodules; ordinary SwiftPM consumption must not require submodule initialization, generators, or the workspace.
- Run checks appropriate to each change. Keep full logs and one-time migration archives outside the source tree; retain reproducible checks, tests, source manifests, and licenses in this repository. Do not present historical results as new validation.
- This revision contains only the repository skeleton and upstream baseline pins; it does not yet provide an independent package or the migrated implementation.
