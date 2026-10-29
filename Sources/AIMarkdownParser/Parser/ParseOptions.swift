// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2021 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
 See https://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

/// Options for parsing Markdown.
public struct ParseOptions: OptionSet, Sendable {
    public var rawValue: UInt

    public init(rawValue: UInt) {
        self.rawValue = rawValue
    }

    /// Preserve footnote definitions in source order and references with their raw labels.
    ///
    /// This opt-in syntax requires the companion cmark lossless-footnotes extension.
    /// It does not assign display numbers or generate a rendered footer.
    public static let parseFootnotes = ParseOptions(rawValue: 1 << 5)

    /// Enable the two-equals inline highlight extension.
    public static let parseHighlight = ParseOptions(rawValue: 1 << 6)

    /// Disable the default table, strikethrough and task list extensions.
    public static let disableGFM = ParseOptions(rawValue: 1 << 7)

    /// Observe raw adjacent characters for Web-compatible emphasis boundaries.
    public static let parseWebEmphasis = ParseOptions(rawValue: 1 << 9)

    /// Preserve Web definition-list block containers.
    public static let parseDefinitionLists = ParseOptions(rawValue: 1 << 10)

    /// Enable Web-compatible GFM literal URL tokenization before inline delimiters.
    public static let parseWebAutolinks = ParseOptions(rawValue: 1 << 8)

    /// Enable raw math syntax: inline dollar runs require at least two characters; no LaTeX preprocessing is performed.
    public static let parseMath = ParseOptions(rawValue: 1 << 11)

    /// Capture raw code/inlineCode fields without changing existing display text; programmatic nodes have no such metadata.
    public static let parseCodeMetadata = ParseOptions(rawValue: 1 << 15)

    /// Capture physical EOLs on actual SoftBreak events without changing existing display or serialization behavior.
    public static let parseSoftBreakMetadata = ParseOptions(rawValue: 1 << 17)

    /// Derive Web mdast list and list-item `spread` from block-parser events without changing display or `tight`.
    public static let parseListSpreadMetadata = ParseOptions(rawValue: 1 << 18)

    /// Accept Web code delimiter lengths without the legacy 80/255 limits.
    public static let parseWebCodeSyntax = ParseOptions(rawValue: 1 << 16)

    /// Match Web single-pass character-reference decoding in ordinary text and link fields, preserving code and TeX content.
    public static let parseWebCharacterReferences = ParseOptions(rawValue: 1 << 14)

    /// Preserve actual Link/Image reference identities and Web parsing semantics; implies parseReferenceDefinitions.
    public static let parseReferenceUsages = ParseOptions(rawValue: 1 << 13)

    /// Preserve ordinary link reference definitions in source order without rewriting link/image reference usages.
    public static let parseReferenceDefinitions = ParseOptions(rawValue: 1 << 12)

    /// Enable block directive syntax.
    public static let parseBlockDirectives = ParseOptions(rawValue: 1 << 0)

    /// Enable interpretation of symbol links from inline code spans surrounded by two backticks.
    public static let parseSymbolLinks = ParseOptions(rawValue: 1 << 1)
    
    /// Disable converting straight quotes to curly, --- to em dashes, -- to en dashes during parsing.
    public static let disableSmartOpts = ParseOptions(rawValue: 1 << 2)

    /// Parse a limited set of Doxygen commands. Requires ``parseBlockDirectives``.
    public static let parseMinimalDoxygen = ParseOptions(rawValue: 1 << 3)

    /// Disable including a `data-sourcepos` attribute on all block elements during parsing.
    public static let disableSourcePosOpts = ParseOptions(rawValue: 1 << 4)
}

