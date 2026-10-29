# AI Markdown Parser Swift

A custom Markdown parser for Swift, used by the Engine in [ai-markdown-swift](https://github.com/ai-markdown/ai-markdown-swift). Its module and C symbol names are isolated from the official Markdown package.

`Package.swift` provides `AIMarkdownParser`, plus `AIMarkdownCMark` and `AIMarkdownCMarkExtensions` for low-level integration tests. These are implementation dependencies of the main library; no long-term general-purpose parser API stability is promised. Minimum platforms are iOS 26 and macOS 26, with Swift tools 6.2; upstream Swift sources retain Swift 5 language mode.

## Sources and distribution

- `upstream/swift-markdown`: the organization fork containing the Swift AST, CAtomic, and the custom Swift implementation.
- `upstream/swift-cmark`: the organization fork containing the C core, extensions, and the custom C implementation.
- `Vendor/Parser`: checked-in isolated sources generated from the two pinned gitlink commits. Ordinary SwiftPM builds use these files directly without initializing submodules, accessing the workspace, running Python, or replaying patches.
- `Licenses`: all third-party licenses for the implementation; original upstream licenses and source copyright notices are preserved.
- `Tests/UpstreamMarkdownTests`: pinned upstream tests with only module imports and two qualified type names changed; assertions and resources are unchanged.
- `Tests/ParserBoundaryTests`: checks for cross-package public fields, custom syntax, metadata, and C node lifetimes.

The initial migration retains the swift-markdown / swift-cmark 0.9.0 baselines without upgrading upstream. The semantic patches were committed to the two forks in the original generation order. `scripts/generate-parser.py` exports fixed commits and applies name isolation; it **does not apply the patches again**. Implementation provenance is maintained in each fork under `AIMarkdown/provenance.json`; this package records its exact source revisions and license hashes in `Vendor/Parser/manifest.json`.

## Development and validation

Initial maintenance checkout:

```sh
git clone git@github.com:ai-markdown/ai-markdown-parser-swift.git
cd ai-markdown-parser-swift
git submodule update --init --checkout
swift test
python3 scripts/generate-parser.py --check
python3 scripts/sync-upstream-tests.py --check
```

Ordinary builds and the read-only integrity checks above do not require submodule contents. Maintainers need both fork object databases, Python, Git, Clang, and nm only for regeneration:

```sh
python3 scripts/generate-parser.py --check --rebuild-check
python3 scripts/sync-upstream-tests.py --check --rebuild-check
```

Both entries in `.gitmodules` set `update = none` so SwiftPM does not automatically fetch maintenance submodules during Git URL installation. Maintainers and regeneration CI must explicitly use `--checkout` as shown above.

An ordinary submodule checkout usually has a detached HEAD. Switch to the corresponding fork's `ai-markdown` maintenance branch before editing. Do not substitute `git submodule update --remote` for a validated upgrade, and do not run submodule updates unconditionally when development branches or uncommitted changes already exist.

For a coordinated update: commit implementation changes in the forks, stage the exact gitlinks in this repository, run the generators, execute standalone and main-library integration regression checks, then commit the pointers and outputs together. The generator reads stage-0 gitlinks from the index, not uncommitted fork working tree sources. CI regenerates from committed gitlinks and compares the output byte for byte.

Before release, ensure the referenced fork commits are available remotely, then publish the integration package version and update the main library's dependency. Do not edit generated output manually. Upstream synchronization does not automatically upgrade the main library.

## Validation

Run `swift test` and `swift test -c release` for the upstream and package-boundary suites. CI checks snapshot integrity, rebuilds from pinned forks, and runs both test configurations. `Tests/UpstreamMarkdownTests.manifest.json` records the upstream test revision and original/generated file hashes.

Maintainers can run `python3 scripts/verify-consumer.py` to recreate a standalone Git consumer with local bare mirrors and verify Debug/Release coexistence with official Markdown without initializing maintenance submodules. The comparison versions are pinned in `scripts/consumer-probe/baselines.json`. This probe needs both fork object databases; ordinary package installation does not. Use `--output` to choose a new result directory.

Keep full command logs, one-time migration archives, and validation run results outside the source tree, such as local build output or CI artifacts. The package retains reproducible checks, test sources, license records, and source manifests rather than historical run artifacts.
