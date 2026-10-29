// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2021 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
 See https://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

#if compiler(>=6.0)
internal import AIMarkdownCAtomic
#else
@_implementationOnly import AIMarkdownCAtomic
#endif

/// A wrapper for a 64-bit unsigned atomic singleton counter.
struct AtomicCounter {
    /// The current counter value.
    static var current: UInt64 {
        return aimarkdown__cmarkup_current_unique_id()
    }

    /// Atomically increment and return the latest counter value.
    static func next() -> UInt64 {
        return aimarkdown__cmarkup_increment_and_get_unique_id()
    }
}
