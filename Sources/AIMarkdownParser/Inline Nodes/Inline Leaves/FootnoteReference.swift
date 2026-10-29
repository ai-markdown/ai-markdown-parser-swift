// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2021 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
 See https://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

/// A reference to a footnote label, without assigned display number or a borrowed C pointer.
public struct FootnoteReference: RecurringInlineMarkup {
    public var _data: _MarkupData

    init(_ data: _MarkupData) {
        self._data = data
    }

    /// Create a reference. Programmatic nodes have no source range.
    public init(label: String) {
        let raw = RawMarkup.footnoteReference(label: label, parsedRange: nil)
        self.init(_MarkupData(AbsoluteRawMarkup(markup: raw, metadata: MarkupMetadata(id: .newRoot(), indexInParent: 0))))
    }

    /// The raw label. Mutating it clears its source range.
    public var label: String {
        get {
            guard case let .footnoteReference(label) = raw.markup.data else {
                preconditionFailure("Unexpected footnote reference data")
            }
            return label
        }
        set {
            _data = _data.replacingSelf(.footnoteReference(label: newValue, parsedRange: nil))
        }
    }

    /// A source-like fallback; numbering belongs to a separate semantic planning pass.
    public var plainText: String { "[^\(label)]" }

    public func accept<V: MarkupVisitor>(_ visitor: inout V) -> V.Result {
        return visitor.visitFootnoteReference(self)
    }
}
