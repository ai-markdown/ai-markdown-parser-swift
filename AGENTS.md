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

## Commit messages

- Follow [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/) using `<type>[optional scope][!]: <description>`.
- Use `feat` for new functionality, `fix` for bug fixes, and other types such as `docs`, `test`, or `refactor` as appropriate.
- Describe what the commit changes. Add a body when needed to explain the behavior or reasoning, separated from the subject by a blank line.
- Mark breaking changes with `!` after the type or scope, or with a `BREAKING CHANGE: <description>` footer.
