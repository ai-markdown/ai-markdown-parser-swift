# AI Markdown Parser Swift

Integration and distribution repository for the custom Markdown parser used by [ai-markdown-swift](https://github.com/ai-markdown/ai-markdown-swift).

At this stage, the two upstream fork submodules and initial migration baselines are established. The existing Vendor patches, isolation generator, generated sources, and Swift Package manifest have not yet been added. The main library still uses its own Vendor directory; this repository is not yet an installable parser package.

## Layout and pinned versions

| Directory | Source fork | Initial commit |
| --- | --- | --- |
| `upstream/swift-markdown` | [ai-markdown/swift-markdown](https://github.com/ai-markdown/swift-markdown) | `25cb61d3482054b09ae76ca4f281b1bfe7fe5a43`, upstream 0.9.0 |
| `upstream/swift-cmark` | [ai-markdown/swift-cmark](https://github.com/ai-markdown/swift-cmark) | `08ddb528923cc1a6527e02b7a1aee9e516ca749a`, upstream 0.9.0 |

The Swift Markdown layer depends on CAtomic and the CMark core and extensions. CAtomic belongs to the swift-markdown repository. The CMark extensions belong to swift-cmark and depend internally on its core; these components do not need separate repositories.

The two gitlinks are the actual version pins. This table records the initial migration baselines and does not replace subsequent submodule commit records.

## Development checkout

Clone the repository and check out the recorded versions:

```sh
git clone --recurse-submodules git@github.com:ai-markdown/ai-markdown-parser-swift.git
```

Initialize submodules in an existing clone:

```sh
git submodule update --init
```

This command checks out the commits recorded by the parent repository. Do not run it unconditionally when development branches or working tree changes already exist. The workspace initialization script initializes only missing submodule checkouts and preserves existing work.

An ordinary submodule checkout usually has a detached HEAD. Before editing, create or switch to the fork's `ai-markdown` maintenance branch from a confirmed baseline. Do not substitute `git submodule update --remote` for a validated version upgrade. The initial local maintenance branches were created during workspace setup; remote publication is tracked separately.

## Remaining migration and release work

1. Commit the main library's existing Swift/C implementation patches to the two fork maintenance branches, preserving upstream history and licenses.
2. Add the isolation generator, related tests, and provenance manifests here, then generate checked-in sources from fixed gitlinks.
3. Create a Swift Package that compiles the internal Swift/C targets together. Consumers must not need to run generators or initialize submodules.
4. Complete source regeneration, official dependency coexistence, Web semantic comparisons, and macOS/iOS integration regression checks before switching the main library's dependency.

Before publishing an integration version, ensure both referenced fork commits are available remotely; update the main library's dependency afterward. The forks are the implementation source of truth. Generated output is not a second manually maintained implementation.
